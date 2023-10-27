#include <asio3/core/fmt.hpp>
#include <asio3/core/io_context_thread.hpp>
#include <asio3/tcp/open.hpp>
#include <asio3/tcp/send.hpp>
#include <asio3/tcp/tcp_server.hpp>

namespace net = ::asio;

net::awaitable<void> do_recv(net::tcp_socket client)
{
	net::streambuf strbuf{ 1024 * 1024 };

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(client, strbuf, '\n');
		if (e1)
			break;

		auto data = asio::buffer(strbuf.data().data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await net::async_send(client, data);
		if (e2)
			break;

		strbuf.consume(n1);
	}

	client.close();
}

net::awaitable<void> start_server(net::tcp_server& server)
{
	auto [ec, ep] = co_await net::async_open(server.acceptor, "0.0.0.0", 8080);
	if (ec)
	{
		fmt::print("listen failure: {}\n", ec.message());
	}
	else
	{
		fmt::print("listen success: {} {}\n", server.get_listen_address(), server.get_listen_port());

		while (!server.is_stopped())
		{
			auto [e1, client] = co_await server.acceptor.async_accept();
			if (e1)
			{
				co_await net::delay(std::chrono::milliseconds(100));
			}
			else
			{
				net::co_spawn(server.get_executor(), do_recv(std::move(client)), net::detached);
			}
		}
	}
}

int main()
{
	net::io_context_thread ctx;

	net::tcp_server server(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), start_server(server), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto...) {});
	});

	ctx.join();
}
