#include <asio3/udp/udp_client.hpp>
#include <asio3/core/fmt.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

net::awaitable<void> do_recv(net::udp_client& client)
{
	std::array<char, 1024> buf;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_receive(client.socket, net::buffer(buf));
		if (e1)
			break;

		auto data = net::buffer(buf.data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await client.async_send(net::buffer(data));
		if (e2)
			break;
	}

	client.close();
}

net::awaitable<void> connect(net::udp_client& client)
{
	while (!client.is_aborted())
	{
		auto [ec, ep] = co_await client.async_connect("127.0.0.1", 8035);
		if (ec)
		{
			fmt::print("connect failure: {}\n", ec.message());
			co_await net::delay(std::chrono::seconds(1));
			continue;
		}

		fmt::print("connect success: {} {}\n", client.get_remote_address(), client.get_remote_port());

		// connect success, send some message to the server...
		co_await client.async_send("<0123456789>");

		co_await do_recv(client);
	}
}

int main()
{
	net::io_context ctx{ 1 };

	net::udp_client client(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), connect(client), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&client](net::error_code, int) mutable
	{
		client.async_stop([](auto) {});
	});

	ctx.run();
}
