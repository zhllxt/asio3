#include <asio3/tcp/tcp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> do_recv(std::shared_ptr<net::tcp_connection> connection)
{
	net::streambuf strbuf{ 1024 * 1024 };

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(connection->socket, strbuf, '\n');
		if (e1)
			break;

		auto data = asio::buffer(strbuf.data().data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await net::async_write(connection->socket, data);
		if (e2)
			break;

		strbuf.consume(n1);
	}

	connection->stop();
}

net::awaitable<void> client_join(std::shared_ptr<net::tcp_connection> connection)
{
	net::co_spawn(connection->get_executor(), do_recv(connection), net::detached);

	fmt::print("{} client join: {} {}\n", std::chrono::system_clock::now(),
		connection->get_remote_address(), connection->get_remote_port());
	co_return;
}

net::awaitable<void> client_exit(std::shared_ptr<net::tcp_connection> connection)
{
	fmt::print("{} client exit: {} {}\n", std::chrono::system_clock::now(),
		connection->get_remote_address(), connection->get_remote_port());
	co_return;
}

net::awaitable<void> start_server(net::tcp_server& server)
{
	auto [ec] = co_await server.async_start();
	if (ec)
		fmt::print("listen failure: {}\n", ec.message());
	else
		fmt::print("listen success: {} {}\n",
			server.get_listen_address(), server.get_listen_port());
}

int main()
{
	net::io_context_thread ctx;

	net::tcp_server server(ctx.get_executor(), {
		.listen_address = "127.0.0.1",
		.listen_port = 8028,
		.on_connect = client_join,
		.on_disconnect = client_exit,
	});

	net::co_spawn(server.get_executor(), start_server(server), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.stop();
	});

	ctx.join();
}
