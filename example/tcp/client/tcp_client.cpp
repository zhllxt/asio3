#include <asio3/tcp/connect.hpp>
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/tcp_client.hpp>
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

		fmt::print("{}\n", std::string_view{ data.data(), n2 });
	}
}

net::awaitable<void> do_connect()
{
	auto executor = co_await net::this_coro::executor;

	net::tcp_socket sock(executor);

	auto [e1, ep1] = co_await net::async_connect(sock, "127.0.0.1", 20801);

	co_await net::async_write(sock, net::buffer("<0123456789>"));

	co_await echo(std::move(sock));
}

int main()
{
	net::tcp_client client({
		.server_address = "127.0.0.1",
		.server_port = 20801,
		.socket_option = {.reuse_address = true, .no_delay = true,},
	});

	client.start();

	net::io_context ctx(1);

	net::co_spawn(ctx, do_connect(), net::detached);

	ctx.run();
}
