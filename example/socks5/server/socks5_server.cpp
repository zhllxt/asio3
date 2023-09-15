#include <asio3/core/fmt.hpp>
#include <asio3/socks5/accept.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/tcp/accept.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/socks5/parser.hpp>
#include <asio3/socks5/match_condition.hpp>
#include <asio3/socks5/udp_header.hpp>

namespace net = ::asio;
using time_point = std::chrono::steady_clock::time_point;

net::awaitable<void> tcp_transfer(
	net::tcp_socket& from, net::tcp_socket& to, socks5::handshake_info& info, time_point& deadline)
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

		if /**/ (info.last_read_channel == net::protocol::tcp)
		{
			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), data.size());

			reply_buffer.insert(reply_buffer.cend(), head.begin(), head.end());
			reply_buffer.insert(reply_buffer.cend(), data.begin(), data.end());

			auto [e1, n1] = co_await net::async_write(from, net::buffer(reply_buffer));
			co_return e1;
		}
		else if (info.last_read_channel == net::protocol::udp)
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
	net::tcp_socket& from, net::udp_socket& bound, socks5::handshake_info& info, time_point& deadline)
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

		info.last_read_channel = net::protocol::udp;

		net::error_code tp = co_await forward_udp_data(
			from, bound, info, reply_buffer, sender_endpoint, std::string_view{ data.data(), n1 });

		auto [e2, n2] = co_await net::async_send_to(bound, net::buffer(data, n1), sender_endpoint);
		if (e2)
			co_return;

		if (n1 == data.size())
			data.resize(data.size() * 3 / 2);
	}
}

net::awaitable<void> ext_transfer(
	net::tcp_socket& from, net::udp_socket& bound, socks5::handshake_info& info, time_point& deadline)
{
	net::streambuf buf{ 1024 * 1024 };

	for (;;)
	{
		deadline = std::chrono::steady_clock::now() + std::chrono::minutes(10);

		// recvd data from the front client by tcp, forward the data to back client.
		auto [e1, n1] = co_await net::async_read_until(from, buf, socks5::udp_match_condition);
		if (e1)
			co_return;

		info.last_read_channel = net::protocol::tcp;

		std::string_view data{ (std::string_view::value_type*)(buf.data().data()), buf.data().size() };

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

		buf.consume(n1);
	}
}

net::awaitable<void> watchdog(time_point& deadline)
{
	net::timer timer(co_await net::this_coro::executor);
	auto now = std::chrono::steady_clock::now();
	while (deadline > now)
	{
		timer.expires_at(deadline);
		co_await timer.async_wait();
		now = std::chrono::steady_clock::now();
	}
}

net::awaitable<void> proxy(net::tcp_socket front_client, socks5::auth_config& auth_cfg)
{
	auto result1 = co_await(
		socks5::async_accept(front_client, auth_cfg, net::use_nothrow_awaitable) ||
		net::timeout(std::chrono::seconds(5)));
	if (net::is_timeout(result1))
		co_return; // timed out
	auto [e1, info] = std::get<0>(std::move(result1));
	if (e1)
		co_return;

	time_point deadline{};

	if (info.cmd == socks5::command::connect)
	{
		net::ip::tcp::socket* ptr = std::any_cast<net::ip::tcp::socket>(std::addressof(info.bound_socket));
		if (ptr)
		{
			net::tcp_socket back_client = std::move(*ptr);

			co_await(
				tcp_transfer(front_client, back_client, info, deadline) ||
				tcp_transfer(back_client, front_client, info, deadline) ||
				watchdog(deadline));
		}
	}
	else if(info.cmd == socks5::command::udp_associate)
	{
		net::ip::udp::socket* ptr = std::any_cast<net::ip::udp::socket>(std::addressof(info.bound_socket));
		if (ptr)
		{
			net::udp_socket back_client = std::move(*ptr);

			co_await(
				udp_transfer(front_client, back_client, info, deadline) ||
				ext_transfer(front_client, back_client, info, deadline) ||
				watchdog(deadline));
		}
	}

	co_return;
}

net::awaitable<void> listen(std::string listen_address, std::uint16_t listen_port, socks5::auth_config auth_cfg)
{
	auto executor = co_await net::this_coro::executor;
	auto [e1, acceptor] = co_await net::async_create_acceptor(executor, listen_address, listen_port);
	if (e1)
	{
		fmt::print("Listen failure: {}\n", e1.message());
		co_return;
	}
	else
	{
		fmt::print("Listen success: {} {}\n",
			acceptor.local_endpoint().address().to_string(),
			acceptor.local_endpoint().port());
	}
	for (;;)
	{
		auto [e2, client] = co_await acceptor.async_accept();
		if (e2)
			co_await net::delay(std::chrono::milliseconds(100));
		else
			net::co_spawn(executor, proxy(std::move(client), auth_cfg), net::detached);
	}
}

bool do_auth(socks5::handshake_info& info)
{
	return info.username == "admin" && info.password == "123456";
}

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	net::io_context ctx(1);

	socks5::auth_config auth_cfg
	{
		.supported_method = { socks5::auth_method::anonymous, socks5::auth_method::password },
		.auth_function = std::bind_front(do_auth),
	};

	net::signal_set signals(ctx, SIGINT, SIGTERM);
	signals.async_wait([&](auto, auto)
	{
		ctx.stop();
	});

	net::co_spawn(ctx, listen("127.0.0.1", 20808, std::move(auth_cfg)), net::detached);

	ctx.run();

	return 0;
}
