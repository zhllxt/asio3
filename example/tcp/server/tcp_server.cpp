#include <asio3/tcp/accept.hpp>
#include <asio3/tcp/read.hpp>

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

net::awaitable<void> do_accept()
{
	auto executor = co_await net::this_coro::executor;

	auto [e1, acceptor] = co_await net::async_create_acceptor(executor, "0.0.0.0", 20801);

	for (;;)
	{
		auto [e2, client] = co_await acceptor.async_accept();
		if (e2)
			co_await net::delay(std::chrono::milliseconds(100));
		else
			net::co_spawn(executor, echo(std::move(client)), net::detached);
	}
}

int main()
{
	net::io_context ctx(1);

	net::co_spawn(ctx, do_accept(), net::detached);

	ctx.run();
}
