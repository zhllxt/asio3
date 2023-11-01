#include <asio3/core/fmt.hpp>
#include <asio3/proxy/handshake.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/proxy/parser.hpp>
#include <asio3/proxy/match_condition.hpp>
#include <asio3/proxy/udp_header.hpp>

namespace net = ::asio;
using time_point = std::chrono::steady_clock::time_point;

net::awaitable<void> tcp_connect(socks5::option sock5_opt)
{
	auto executor = co_await net::this_coro::executor;

	net::tcp_socket client(executor);

	auto [e1, ep1] = co_await net::async_connect(client, sock5_opt.proxy_address, sock5_opt.proxy_port);
	if (e1)
	{
		fmt::print("connect failure: {}\n", e1.message());
		co_return;
	}
	else
	{
		fmt::print("connect success: {} {}\n",
			client.local_endpoint().address().to_string(),
			client.local_endpoint().port());
	}

	auto [e2] = co_await socks5::async_handshake(client, sock5_opt);
	if (e2)
		co_return;

	auto [e5, n5] = co_await net::async_write(client, net::buffer("<abc0123456789def>"));
	if (e5)
		co_return;

	std::array<char, 1024> data;

	for (;;)
	{
		auto [e3, n3] = co_await net::async_read_some(client, net::buffer(data));
		if (e3)
			break;

		auto [e4, n4] = co_await net::async_write(client, net::buffer(data, n3));
		if (e4)
			break;
	}
}

net::awaitable<void> udp_connect(socks5::option sock5_opt)
{
	auto executor = co_await net::this_coro::executor;

	net::error_code ec{};

	std::uint16_t dest_port = sock5_opt.dest_port;

	net::ip::udp::endpoint local_endp{ net::ip::udp::v4(), 0 };
	net::udp_socket cast(executor, local_endp);
	local_endp = cast.local_endpoint(ec);
	sock5_opt.dest_port = local_endp.port();

	net::tcp_socket client(executor);

	auto [e1, ep1] = co_await net::async_connect(client, sock5_opt.proxy_address, sock5_opt.proxy_port);
	if (e1)
	{
		fmt::print("connect failure: {}\n", e1.message());
		co_return;
	}
	else
	{
		fmt::print("connect success: {} {}\n",
			client.local_endpoint().address().to_string(),
			client.local_endpoint().port());
	}

	auto result2 = co_await(
		socks5::async_handshake(client, sock5_opt, net::use_nothrow_awaitable) ||
		net::timeout(std::chrono::seconds(5)));
	if (result2.index() == 1)
		co_return; // timed out
	auto [e2] = std::get<0>(std::move(result2));
	if (e2)
		co_return;

	net::ip::udp::endpoint remote_endp{ ep1.address(), sock5_opt.bound_port };
	net::ip::udp::endpoint sender_endp;

	std::string msg = "<abc0123456789def>";
	std::array<char, 1024> data;

	socks5::insert_udp_header(msg, sock5_opt.dest_address, dest_port);

	for (;;)
	{
		auto [e4, n4] = co_await net::async_send_to(cast, net::buffer(msg), remote_endp);
		if (e4)
			break;

		auto [e5, n5] = co_await net::async_receive_from(cast, net::buffer(data), sender_endp);
		if (e5)
			break;

		auto [err, ep, domain, real_data] = socks5::parse_udp_packet(std::string_view{ data.data(),n5 }, false);

		net::ignore_unused(err, ep, domain, real_data);
	}
}

int main()
{
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS_)
	// Detected memory leaks on windows system
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	net::io_context ctx(1);

	socks5::option sock5_opt
	{
		.proxy_address = "127.0.0.1",
		.proxy_port = 20808,
		.method = {socks5::auth_method::anonymous},
		.username = "admin",
		.password = "123456",
		.dest_address = "127.0.0.1",
		.dest_port = 8035,
		.cmd = socks5::command::udp_associate,
	};

	//net::co_spawn(ctx, tcp_connect(std::move(sock5_opt)), net::detached);
	net::co_spawn(ctx, udp_connect(std::move(sock5_opt)), net::detached);

	net::signal_set signals(ctx, SIGINT, SIGTERM);
	signals.async_wait([&](auto, auto)
	{
		ctx.stop();
	});

	ctx.run();

	return 0;
}
