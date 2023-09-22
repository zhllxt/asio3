#include <asio3/udp/udp_client.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> do_work(net::udp_client& client)
{
	while (!client.is_aborted())
	{
		auto [e0] = co_await client.async_connect();
		if (e0)
		{
			fmt::print("connect failure: {}\n", e0.message());
			co_await net::delay(std::chrono::seconds(1));
			continue;
		}

		fmt::print("connect success: {} {}\n", client.get_remote_address(),client.get_remote_port());

		// connect success, send some message to the server...
		client.async_send("<0123456789>");

		std::vector<char> recv_buffer(1024);

		for (;;)
		{
			auto [e1, n1] = co_await client.socket.async_receive(asio::buffer(recv_buffer));
			if (e1)
				break;

			auto data = std::string_view{ recv_buffer.data(), n1 };

			fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

			auto [e2, n2] = co_await client.socket.async_send(asio::buffer(data));
			if (e2)
				break;
		}
	}

	client.socket.shutdown(net::socket_base::shutdown_both);
	client.socket.close();
}

int main()
{
	net::io_context ctx;

	net::udp_client client(ctx.get_executor(), {
		.server_address = "127.0.0.1",
		.server_port = 8038,
		//.bind_address = "127.0.0.1",
		//.bind_port = 55555,
		//.socks5_option =
		//{
		//	.proxy_address = "127.0.0.1",
		//	.proxy_port = 10808,
		//	.method = {socks5::auth_method::anonymous}
		//},
	});

	net::co_spawn(ctx.get_executor(), do_work(client), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&client](net::error_code, int) mutable
	{
		client.stop();
	});

	ctx.run();
}
