#include <asio3/udp/udp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

void on_recv(std::shared_ptr<net::udp_connection> connection, std::string_view data)
{
	fmt::print("{} {}\n", std::chrono::system_clock::now(), data);
}

void client_join(std::shared_ptr<net::udp_connection> connection)
{
	fmt::print("{} client join: {} {}\n", std::chrono::system_clock::now(),
		connection->get_remote_address(), connection->get_remote_port());
}

void client_exit(std::shared_ptr<net::udp_connection> connection)
{
	fmt::print("{} client exit: {} {}\n", std::chrono::system_clock::now(),
		connection->get_remote_address(), connection->get_remote_port());
}

net::awaitable<void> start_server(net::udp_server& server)
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

	net::udp_server server(ctx.get_executor(), {
		.listen_address = "127.0.0.1",
		.listen_port = 8035,
		.on_connect = client_join,
		.on_disconnect = client_exit,
		.on_recv = on_recv,
	});

	net::co_spawn(server.get_executor(), start_server(server), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.stop();
	});

	ctx.join();
}
