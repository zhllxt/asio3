#include <asio3/core/fmt.hpp>
#include <asio3/tcp/tcp_server.hpp>

namespace net = ::asio;

net::awaitable<void> do_recv(std::shared_ptr<net::tcp_connection> conn)
{
	std::string strbuf;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(conn->socket, asio::dynamic_buffer(strbuf), '\n');
		if (e1)
			break;

		conn->update_alive_time();

		auto data = net::buffer(strbuf.data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await net::async_send(conn->socket, data);
		if (e2)
			break;

		strbuf.erase(0, n1);
	}

	conn->socket.shutdown(net::socket_base::shutdown_both);
	conn->socket.close();
}

net::awaitable<void> client_join(net::tcp_server& server, std::shared_ptr<net::tcp_connection> conn)
{
	co_await server.connection_map.async_emplace(conn);

	conn->socket.set_option(net::ip::tcp::no_delay(true));
	conn->socket.set_option(net::socket_base::keep_alive(true));

	net::co_spawn(server.get_executor(), do_recv(conn), net::detached);

	co_await conn->async_wait_error_or_idle_timeout(net::tcp_idle_timeout);
	co_await conn->async_disconnect();

	co_await server.connection_map.async_erase(conn);
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
				std::make_shared<net::tcp_connection>(std::move(client))), net::detached);
		}
	}
}

int main()
{
	net::io_context_thread ctx;

	net::tcp_server server(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), start_server(server, "0.0.0.0", 8028), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	ctx.join();
}
