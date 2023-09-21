#include <asio3/tcp/tcp_client.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> work(net::tcp_client& client)
{
	while (!client.is_aborted())
	{
		auto [e1] = co_await client.async_connect();
		if (e1)
		{
			co_await net::delay(std::chrono::seconds(1));
			continue;
		}

		client.async_send("<0123456789>");

		std::array<char, 1024> data;

		for (;;)
		{
			auto [e2, n2] = co_await net::async_read_some(client.socket, net::buffer(data));
			if (e2)
				break;

			auto [e3, n3] = co_await net::async_write(client.socket, net::buffer(data, n2));
			if (e3)
				break;

			fmt::print("{}\n", std::string_view{ data.data(), n3 });
		}
	}

	client.socket.close();
}

int main()
{
	net::context_worker worker;

	net::tcp_client client(worker.get_executor(), {
		.server_address = "127.0.0.1",
		.server_port = 8028,
		//.socks5_option =
		//{
		//	.proxy_address = "127.0.0.1",
		//	.proxy_port = 10808,
		//	.method = {socks5::auth_method::anonymous}
		//},
	});

	net::co_spawn(worker.get_executor(), work(client), net::detached);

	//net::signal_set sigset(worker.get_executor(), SIGINT);
	//sigset.async_wait([&client](net::error_code, int) mutable
	//{
	//	client.stop();
	//});
	//
	//worker.run();

	worker.start();

	while (std::getchar() != '\n');
}
