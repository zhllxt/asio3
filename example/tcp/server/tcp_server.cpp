#include <asio3/tcp/tcp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> echo(net::tcp_socket sock)
{
	std::array<char, 1024> data;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_some(sock, net::buffer(data));
		if (e1)
			co_return;

		auto [e2, n2] = co_await net::async_write(sock, net::buffer(data, n1));
		if (e2)
			co_return;
	}
}

int main()
{
	net::context_worker worker;

	net::tcp_server server(worker.get_executor(), {
		.listen_address = "127.0.0.1",
		.listen_port = 8028,
		.accept_function = echo,
	});

	server.async_start([](net::error_code ec)
	{
		if (ec)
			fmt::print("listen success: {}\n", ec.message());
		else
			fmt::print("listen failure: {}\n", ec.message());
	});

	worker.run();
}
