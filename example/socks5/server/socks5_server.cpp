#include <asio3/core/fmt.hpp>
#include <asio3/proxy/socks5_server.hpp>

namespace net = ::asio;
using time_point = std::chrono::steady_clock::time_point;

net::awaitable<void> tcp_transfer(
	std::shared_ptr<net::socks5_session> conn, net::tcp_socket& from, net::tcp_socket& to)
{
	std::array<char, 1024> data;

	for (;;)
	{
		conn->update_alive_time();

		auto [e1, n1] = co_await net::async_read_some(from, net::buffer(data));
		if (e1)
			break;

		auto [e2, n2] = co_await net::async_write(to, net::buffer(data, n1));
		if (e2)
			break;
	}
}

net::awaitable<void> udp_transfer(
	std::shared_ptr<net::socks5_session> conn, net::tcp_socket& front, net::udp_socket& bound)
{
	std::array<char, 1024> data;
	net::ip::udp::endpoint sender_endpoint{};

	for (;;)
	{
		conn->update_alive_time();

		auto [e1, n1] = co_await net::async_receive_from(bound, net::buffer(data), sender_endpoint);
		if (e1)
			break;

		if (socks5::is_data_come_from_frontend(front, sender_endpoint, conn->handshake_info))
		{
			conn->last_read_channel = net::protocol::udp;

			auto [e2, n2] = co_await socks5::async_forward_data_to_backend(bound, asio::buffer(data, n1));
			if (e2)
				break;
		}
		else
		{
			if (conn->last_read_channel == net::protocol::udp)
			{
				auto [e2, n2] = co_await socks5::async_forward_data_to_frontend(
					bound, asio::buffer(data, n1), sender_endpoint, conn->get_frontend_udp_endpoint());
				if (e2)
					break;
			}
			else
			{
				auto [e2, n2] = co_await socks5::async_forward_data_to_frontend(
					front, asio::buffer(data, n1), sender_endpoint);
				if (e2)
					break;
			}
		}
	}
}

net::awaitable<void> ext_transfer(
	std::shared_ptr<net::socks5_session> conn, net::tcp_socket& front, net::udp_socket& bound)
{
	std::string buf;

	for (;;)
	{
		conn->update_alive_time();

		// recvd data from the front client by tcp, forward the data to back client.
		auto [e1, n1] = co_await net::async_read_until(
			front, net::dynamic_buffer(buf), socks5::udp_match_condition{});
		if (e1)
			break;

		conn->last_read_channel = net::protocol::tcp;

		// this packet is a extension protocol base of below:
		// +----+------+------+----------+----------+----------+
		// |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
		// +----+------+------+----------+----------+----------+
		// | 2  |  1   |  1   | Variable |    2     | Variable |
		// +----+------+------+----------+----------+----------+
		// the RSV field is the real data length of the field DATA.
		// so we need unpacket this data, and send the real data to the back client.

		auto [err, ep, domain, real_data] = socks5::parse_udp_packet(asio::buffer(buf.data(), n1), true);
		if (err == 0)
		{
			if (domain.empty())
			{
				auto [e2, n2] = co_await net::async_send_to(bound, net::buffer(real_data), ep);
				if (e2)
					break;
			}
			else
			{
				auto [e2, n2] = co_await net::async_send_to(
					bound, net::buffer(real_data), std::move(domain), ep.port());
				if (e2)
					break;
			}
		}
		else
		{
			break;
		}

		buf.erase(0, n1);
	}

	net::error_code ec{};
	front.shutdown(asio::socket_base::shutdown_both, ec);
	front.close(ec);
}

net::awaitable<void> proxy(std::shared_ptr<net::socks5_session> conn)
{
	auto result = co_await(
		socks5::async_accept(conn->socket, conn->auth_config, net::use_nothrow_awaitable) ||
		net::timeout(std::chrono::seconds(5)));
	if (net::is_timeout(result))
		co_return;
	auto [e1, handsk_info] = std::get<0>(std::move(result));
	if (e1)
		co_return;

	conn->handshake_info = std::move(handsk_info);

	net::error_code ec{};

	if (conn->handshake_info.cmd == socks5::command::connect)
	{
		asio::tcp_socket& front_client = conn->socket;
		asio::tcp_socket& back_client = *conn->get_backend_tcp_socket();
		co_await(
			tcp_transfer(conn, front_client, back_client) ||
			tcp_transfer(conn, back_client, front_client) ||
			net::watchdog(conn->alive_time, net::proxy_idle_timeout));
		front_client.close(ec);
		back_client.close(ec);
	}
	else if (conn->handshake_info.cmd == socks5::command::udp_associate)
	{
		asio::tcp_socket& front_client = conn->socket;
		asio::udp_socket& back_client = *conn->get_backend_udp_socket();
		co_await(
			udp_transfer(conn, front_client, back_client) ||
			ext_transfer(conn, front_client, back_client) ||
			net::watchdog(conn->alive_time, net::proxy_idle_timeout));
		front_client.close(ec);
		back_client.close(ec);
	}
}

net::awaitable<void> client_join(net::socks5_server& server, std::shared_ptr<net::socks5_session> conn)
{
	co_await server.session_map.async_add(conn);

	conn->socket.set_option(net::ip::tcp::no_delay(true));
	conn->socket.set_option(net::socket_base::keep_alive(true));

	co_await proxy(conn);
	co_await conn->async_disconnect();

	co_await server.session_map.async_remove(conn);
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
			auto conn = std::make_shared<net::socks5_session>(std::move(client));
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
