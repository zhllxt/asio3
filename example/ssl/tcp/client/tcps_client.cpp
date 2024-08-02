#ifndef ASIO3_ENABLE_SSL
#define ASIO3_ENABLE_SSL
#endif

#include <asio3/tcp/tcps_client.hpp>
#include <asio3/core/fmt.hpp>
#include "../../certs.hpp"

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

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

	client.close();
}

net::awaitable<void> connect(net::tcps_client& client)
{
	while (!client.is_aborted())
	{
		auto [e1, ep] = co_await client.async_connect("127.0.0.1", 8002);
		if (e1)
		{
			// connect failed, reconnect...
			fmt::print("connect failure: {}\n", e1.message());
			co_await net::delay(std::chrono::seconds(1));
			client.close();
			continue;
		}

		auto [e2] = co_await net::async_handshake(client.ssl_stream,
			net::ssl::stream_base::handshake_type::client);
		if (e2)
		{
			// handshake failed, reconnect...
			fmt::print("handshake failure: {}\n", e2.message());
			co_await net::delay(std::chrono::seconds(1));
			client.close();
			continue;
		}

		fmt::print("connect success: {} {}\n", client.get_remote_address(), client.get_remote_port());

		// connect success, send some message to the server...
		co_await client.async_send("<0123456789>\n");

		co_await do_recv(client);
	}
}

int main()
{
	net::io_context ctx{ 1 };

	net::ssl::context sslctx(net::ssl::context::sslv23);
	net::load_cert_from_string(sslctx, net::ssl::verify_peer, ca_crt, client_crt, client_key, "123456");
	net::tcps_client client(ctx.get_executor(), std::move(sslctx));

	net::co_spawn(ctx.get_executor(), connect(client), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&client](net::error_code, int) mutable
	{
		client.async_stop([](auto) {});
	});

	ctx.run();
}
