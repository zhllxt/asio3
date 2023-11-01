#include <asio3/tcp/tcps_client.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> do_recv(net::tcps_client& client)
{
	std::string strbuf;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(client.ssl_stream, net::dynamic_buffer(strbuf), '\n');
		if (e1)
			break;

		auto data = net::buffer(strbuf.data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await net::async_send(client.ssl_stream, data);
		if (e2)
			break;

		strbuf.erase(0, n1);
	}

	//co_await client.ssl_stream.async_shutdown(net::use_nothrow_awaitable);
	client.socket.shutdown(net::socket_base::shutdown_both);
	client.socket.close();
}

net::awaitable<void> connect(net::tcps_client& client)
{
	while (!client.is_aborted())
	{
		auto [ec, ep] = co_await client.async_connect("127.0.0.1", 8002);
		if (ec)
		{
			// connect failed, reconnect...
			fmt::print("connect failure: {}\n", ec.message());
			co_await net::delay(std::chrono::seconds(1));
			client.socket.close();
			continue;
		}

		auto [e2] = co_await client.ssl_stream.async_handshake(
			net::ssl::stream_base::handshake_type::client);
		if (e2)
		{
			// handshake failed, reconnect...
			fmt::print("handshake failure: {}\n", ec.message());
			co_await net::delay(std::chrono::seconds(1));
			client.socket.close();
			continue;
		}

		fmt::print("connect success: {} {}\n", client.get_remote_address(), client.get_remote_port());

		// connect success, send some message to the server...
		client.async_send("<0123456789>\n", [](auto...) {});

		co_await do_recv(client);
	}
}

int main()
{
	net::io_context ctx;

	net::tcps_client client(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), connect(client), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&client](net::error_code, int) mutable
	{
		client.async_stop([](auto) {});
	});

	ctx.run();
}
