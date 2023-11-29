#include <asio3/core/fmt.hpp>
#include <asio3/core/timer_map.hpp>
#include <asio3/tcp/tcp_server.hpp>

namespace net = ::asio;

net::awaitable<void> do_recv(std::shared_ptr<net::tcp_session> session)
{
	std::string strbuf;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(session->socket, net::dynamic_buffer(strbuf), '\n');
		if (e1)
			break;

		session->update_alive_time();

		auto data = net::buffer(strbuf.data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await net::async_send(session->socket, data);
		if (e2)
			break;

		strbuf.erase(0, n1);
	}

	session->close();
}

net::awaitable<void> client_join(net::tcp_server& server, std::shared_ptr<net::tcp_session> session)
{
	co_await server.session_map.async_add(session);

	session->socket.set_option(net::ip::tcp::no_delay(true));
	session->socket.set_option(net::socket_base::keep_alive(true));

	co_await(do_recv(session) || net::watchdog(session->alive_time, net::tcp_idle_timeout));

	co_await server.session_map.async_remove(session);
}

net::awaitable<void> start_server(net::tcp_server& server, std::string listen_address, std::uint16_t listen_port)
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
			net::co_spawn(server.get_executor(), client_join(server,
				std::make_shared<net::tcp_session>(std::move(client))), net::detached);
		}
	}
}

int main()
{
	net::io_context_thread ctx;

	net::tcp_server server(ctx.get_executor());
	net::timer_map timers(ctx.get_executor());

	timers.start_timer(1, 1000, []() mutable
	{
		fmt::print("timer 1 running...\n");

		return false; // return false to exit the timer.
	});

	net::co_spawn(ctx.get_executor(), start_server(server, "0.0.0.0", 8028), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server, &timers](net::error_code, int) mutable
	{
		timers.stop_all_timers();
		server.async_stop([](auto) {});
	});

	ctx.join();
}
