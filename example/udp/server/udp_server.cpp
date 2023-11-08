#include <asio3/udp/udp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> client_join(net::udp_server& server, std::shared_ptr<net::udp_session> session)
{
	co_await net::watchdog(session->watchdog_timer, session->alive_time, net::udp_idle_timeout);
	co_await session->async_disconnect();

	co_await server.session_map.async_remove(session);
}

net::awaitable<void> start_server(net::udp_server& server, std::string listen_address, std::uint16_t listen_port)
{
	auto [ec, ep] = co_await server.async_open(listen_address, listen_port);
	if (ec)
	{
		fmt::print("listen failure: {}\n", ec.message());
		co_return;
	}

	fmt::print("listen success: {} {}\n", server.get_listen_address(), server.get_listen_port());

	std::array<char, 1024> buf;
	net::ip::udp::endpoint remote_endpoint{};

	while (!server.is_aborted())
	{
		auto [e1, n1] = co_await net::async_receive_from(
			server.socket, net::buffer(buf), remote_endpoint);
		if (e1)
			break;

		auto [session] = co_await server.session_map.async_find(remote_endpoint);
		if (!session)
		{
			session = net::udp_session::create(server.socket, remote_endpoint);
			co_await server.session_map.async_add(session);
			net::co_spawn(server.get_executor(), client_join(server, session), net::detached);
		}

		session->update_alive_time();

		auto data = net::buffer(buf.data(), n1);

		fmt::print("{} {} {} {}\n", std::chrono::system_clock::now(),
			session->get_remote_address(), session->get_remote_port(), data);

		co_await session->async_send(data);
	}
}

int main()
{
	net::io_context_thread ctx;

	net::udp_server server(ctx.get_executor());

	net::co_spawn(server.get_executor(), start_server(server, "0.0.0.0", 8035), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	ctx.join();
}
