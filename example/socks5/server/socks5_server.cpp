#include <asio3/core/fmt.hpp>
#include <asio3/proxy/socks5_server.hpp>

namespace net = ::asio;
using time_point = std::chrono::steady_clock::time_point;

net::awaitable<void> tcp_transfer(
	net::tcp_socket& from, net::tcp_socket& to, socks5::handshake_info& info)
{
	std::array<char, 1024> data;

	for (;;)
	{
		deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

		auto [e1, n1] = co_await net::async_read_some(from, net::buffer(data));
		if (e1)
			co_return;

		auto [e2, n2] = co_await net::async_write(to, net::buffer(data, n1));
		if (e2)
			co_return;
	}
}

// recvd data from udp
net::awaitable<net::error_code> forward_udp_data(
	net::tcp_socket& from, net::udp_socket& bound, socks5::handshake_info& info,
	asio::protocol& last_read_channel,
	std::vector<std::uint8_t> reply_buffer,
	const net::ip::udp::endpoint& sender_endpoint, std::string_view data)
{
	net::error_code ec{};

	net::ip::address front_addr = from.remote_endpoint(ec).address();
	if (ec)
		co_return ec;

	bool is_from_front = false;

	if (front_addr.is_loopback())
		is_from_front = (sender_endpoint.address() == front_addr && sender_endpoint.port() == info.dest_port);
	else
		is_from_front = (sender_endpoint.address() == front_addr);

	// recvd data from the front client. forward it to the target endpoint.
	if (is_from_front)
	{
		auto [err, ep, domain, real_data] = socks5::parse_udp_packet(data, false);
		if (err == 0)
		{
			if (domain.empty())
			{
				auto [e1, n1] = co_await net::async_send_to(bound, net::buffer(real_data), ep);
				co_return e1;
			}
			else
			{
				auto [e1, n1] = co_await net::async_send_to(
					bound, net::buffer(real_data), std::move(domain), ep.port());
				co_return e1;
			}
		}
		else
		{
			co_return net::error::no_data;
		}
	}
	// recvd data from the back client. forward it to the front client.
	else
	{
		reply_buffer.clear();

		if /**/ (last_read_channel == net::protocol::tcp)
		{
			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), data.size());

			reply_buffer.insert(reply_buffer.cend(), head.begin(), head.end());
			reply_buffer.insert(reply_buffer.cend(), data.begin(), data.end());

			auto [e1, n1] = co_await net::async_write(from, net::buffer(reply_buffer));
			co_return e1;
		}
		else if (last_read_channel == net::protocol::udp)
		{
			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), 0);

			reply_buffer.insert(reply_buffer.cend(), head.begin(), head.end());
			reply_buffer.insert(reply_buffer.cend(), data.begin(), data.end());

			auto [e1, n1] = co_await net::async_send_to(
				bound, net::buffer(reply_buffer), net::ip::udp::endpoint(front_addr, info.dest_port));
			co_return e1;
		}
	}
}

net::awaitable<void> udp_transfer(
	net::tcp_socket& from, net::udp_socket& bound, socks5::handshake_info& info,
	asio::protocol& last_read_channel, time_point& deadline)
{
	// ############## should has a choice to set the udp recv buffer size.
	std::string data(1024, '\0');

	net::ip::udp::endpoint sender_endpoint{};

	// Define a buffer in advance to reduce memory allocation
	std::vector<std::uint8_t> reply_buffer;

	for (;;)
	{
		deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

		auto [e1, n1] = co_await net::async_receive_from(bound, net::buffer(data), sender_endpoint);
		if (e1)
			co_return;

		last_read_channel = net::protocol::udp;

		co_await forward_udp_data(
			from, bound, info, last_read_channel,
			reply_buffer, sender_endpoint, std::string_view{ data.data(), n1 });
	}
}

net::awaitable<void> ext_transfer(
	net::tcp_socket& from, net::udp_socket& bound, socks5::handshake_info& info,
	asio::protocol& last_read_channel, time_point& deadline)
{
	std::string buf;

	for (;;)
	{
		deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

		// recvd data from the front client by tcp, forward the data to back client.
		auto [e1, n1] = co_await net::async_read_until(from, net::dynamic_buffer(buf), socks5::udp_match_condition);
		if (e1)
			co_return;

		last_read_channel = net::protocol::tcp;

		std::string_view data{ buf.data(), n1 };

		// this packet is a extension protocol base of below:
		// +----+------+------+----------+----------+----------+
		// |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
		// +----+------+------+----------+----------+----------+
		// | 2  |  1   |  1   | Variable |    2     | Variable |
		// +----+------+------+----------+----------+----------+
		// the RSV field is the real data length of the field DATA.
		// so we need unpacket this data, and send the real data to the back client.
		auto [err, ep, domain, real_data] = socks5::parse_udp_packet(data, true);
		if (err == 0)
		{
			if (domain.empty())
			{
				co_await net::async_send_to(bound, net::buffer(real_data), ep);
			}
			else
			{
				co_await net::async_send_to(bound, net::buffer(real_data), std::move(domain), ep.port());
			}
		}
		else
		{
			co_return;
		}

		buf.erase(0, n1);
	}
}

net::awaitable<void> proxy(std::shared_ptr<net::socks5_connection> conn)
{
	auto handsk_result = co_await(
		socks5::async_accept(conn->socket, conn->auth_config, net::use_nothrow_awaitable) ||
		net::timeout(std::chrono::seconds(5)));
	if (net::is_timeout(handsk_result))
		co_return; // timed out
	auto [e1, handsk_info] = std::get<0>(std::move(handsk_result));
	if (e1)
	{
		co_return;
	}

	conn->handshake_info = std::move(handsk_info);

	if (!conn->load_backend_client_from_handshake_info())
		co_return;

	if (conn->handshake_info.cmd == socks5::command::connect)
	{
		asio::tcp_socket& front_client = conn->socket;
		asio::tcp_socket& back_client = std::get<asio::tcp_socket>(conn->backend_client);
		std::array<char, 1024> buf1, buf2;
		for (;;)
		{
			co_await(
				socks5::async_tcp_transfer(front_client, back_client, asio::buffer(buf1)) ||
				socks5::async_tcp_transfer(back_client, front_client, asio::buffer(buf2))
				);
		}
	}
	//else if (conn->handshake_info.cmd == socks5::command::udp_associate)
	//{
	//	co_await(
	//		udp_transfer(front_client, back_client, info, last_read_channel) ||
	//		ext_transfer(front_client, back_client, info, last_read_channel)
	//		);
	//}
}

net::awaitable<void> client_join(net::socks5_server& server, std::shared_ptr<net::socks5_connection> conn)
{
	co_await server.connection_map.async_emplace(conn);

	conn->socket.set_option(net::ip::tcp::no_delay(true));
	conn->socket.set_option(net::socket_base::keep_alive(true));

	net::co_spawn(server.get_executor(), proxy(conn), net::detached);

	co_await conn->async_wait_error_or_idle_timeout(net::proxy_idle_timeout);
	co_await conn->async_disconnect();

	co_await server.connection_map.async_erase(conn);
}

net::awaitable<void> start_server(net::socks5_server& server,
	std::string listen_address, std::uint16_t listen_port, socks5::auth_config auth_cfg)
{
	auto [ec, ep] = co_await server.async_listen(listen_address, listen_port);
	if (ec)
	{
		fmt::print("listen failure: {}\n", ec.message());
		co_return;
	}

	fmt::print("listen success: {} {}\n", server.get_listen_address(), server.get_listen_port());

	while (!server.is_aborted())
	{
		auto [e1, client] = co_await server.acceptor.async_accept();
		if (e1)
		{
			co_await net::delay(std::chrono::milliseconds(100));
		}
		else
		{
			auto conn = std::make_shared<net::socks5_connection>(std::move(client));
			conn->auth_config = auth_cfg;
			net::co_spawn(server.get_executor(), client_join(server, conn), net::detached);
		}
	}
}

net::awaitable<bool> socks5_auth(socks5::handshake_info& info)
{
	co_return info.username == "admin" && info.password == "123456";
}

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	net::io_context_thread ctx;

	socks5::auth_config auth_cfg
	{
		.supported_method = { socks5::auth_method::anonymous, socks5::auth_method::password },
		.on_auth = socks5_auth,
	};

	net::socks5_server server(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), start_server(server, "127.0.0.1", 20808, auth_cfg), net::detached);

	net::signal_set signals(ctx.get_executor(), SIGINT, SIGTERM);
	signals.async_wait([&](auto, auto)
	{
		server.async_stop([](auto) {});
	});

	ctx.join();

	return 0;
}
