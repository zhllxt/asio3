#include <asio3/tcp/tcp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> client_join(std::shared_ptr<net::tcp_connection> connection)
{
	std::array<char, 1024> data;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_some(connection->socket, net::buffer(data));
		if (e1)
			break;

		auto [e2, n2] = co_await net::async_write(connection->socket, net::buffer(data, n1));
		if (e2)
			break;
	}

	connection->stop();
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
	net::context_worker worker;

	net::tcp_server server(worker.get_executor(), {
		.listen_address = "127.0.0.1",
		.listen_port = 8028,
		.new_connection_handler = client_join,
	});

	net::co_spawn(server.get_executor(), start_server(server), net::detached);

	net::signal_set sigset(worker.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.stop();
	});
	
	worker.run();

	//worker.start();

	//while (std::getchar() != '\n');
}
