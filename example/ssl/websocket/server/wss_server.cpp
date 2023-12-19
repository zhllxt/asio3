#ifndef ASIO3_ENABLE_SSL
#define ASIO3_ENABLE_SSL
#endif

#include <asio3/core/fmt.hpp>
#include <asio3/http/wss_server.hpp>
#include "../../certs.hpp"

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

net::awaitable<void> do_recv(std::shared_ptr<net::wss_session> session)
{
	beast::flat_buffer buf;

	for (;;)
	{
		// Read a message into our buffer
		auto [e1, n1] = co_await session->ws_stream.async_read(buf);
		if (e1)
			break;

		session->update_alive_time();

		// Echo the message
		session->ws_stream.text(session->ws_stream.got_text());
		auto [e2, n2] = co_await net::async_send(session->ws_stream, buf.data());
		if (e2)
			break;

		// Clear the buffer
		buf.consume(buf.size());
	}

	session->close();
}

net::awaitable<void> client_join(net::wss_server& server, std::shared_ptr<net::wss_session> session)
{
	session->socket.set_option(net::ip::tcp::no_delay(true));
	session->socket.set_option(net::socket_base::keep_alive(true));

	auto [e0] = co_await net::async_handshake(session->ssl_stream, net::ssl::stream_base::handshake_type::server);
	if (e0)
	{
		fmt::print("ssl handshake failure: {}\n", e0.message());
		co_return;
	}

	// Set suggested timeout settings for the websocket
	session->ws_stream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

	// Set a decorator to change the Server of the handshake
	session->ws_stream.set_option(websocket::stream_base::decorator(
		[](websocket::response_type& res)
		{
			res.set(http::field::server, BEAST_VERSION_STRING);
			res.set(http::field::authentication_results, "200 OK");
		}));

	// Accept the websocket handshake
	auto [e1] = co_await session->ws_stream.async_accept();
	if (e1)
	{
		fmt::print("websocket handshake failed: {} {} {}\n",
			session->get_remote_address(), session->get_remote_port(), e1.message());
		co_return;
	}

	auto addr = session->get_remote_address();
	auto port = session->get_remote_port();

	fmt::print("+ websocket client join: {} {}\n", addr, port);

	co_await server.session_map.async_add(session);

	co_await(do_recv(session) || net::watchdog(session->alive_time, net::tcp_idle_timeout));

	co_await server.session_map.async_remove(session);

	fmt::print("- websocket client exit: {} {}\n", addr, port);
}

net::awaitable<void> start_server(net::wss_server& server, std::string listen_address, std::uint16_t listen_port)
{
	auto [ec, ep] = co_await server.async_listen(listen_address, listen_port);
	if (ec)
	{
		fmt::print("listen failure: {}\n", ec.message());
		co_return;
	}

	fmt::print("listen success: {} {}\n", server.get_listen_address(), server.get_listen_port());

	while (!server.is_aborted())
	{
		auto [e1, client] = co_await server.acceptor.async_accept();
		if (e1)
		{
			co_await net::delay(std::chrono::milliseconds(100));
		}
		else
		{
			net::co_spawn(server.get_executor(), client_join(server,
				std::make_shared<net::wss_session>(std::move(client), server.ssl_context)), net::detached);
		}
	}
}

int main()
{
	net::io_context_thread ctx;

	net::ssl::context sslctx(net::ssl::context::sslv23);
	net::load_cert_from_string(sslctx, net::ssl::verify_peer | net::ssl::verify_fail_if_no_peer_cert,
		ca_crt, server_crt, server_key, "123456", dh);
	net::wss_server server(ctx.get_executor(), std::move(sslctx));

	net::co_spawn(ctx.get_executor(), start_server(server, "0.0.0.0", 8443), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	ctx.join();
}
