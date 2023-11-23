#ifndef ASIO3_ENABLE_SSL
#define ASIO3_ENABLE_SSL
#endif

#include <asio3/core/fmt.hpp>
#include <asio3/tcp/tcps_server.hpp>
#include "../../certs.hpp"

namespace net = ::asio;

net::awaitable<void> do_recv(std::shared_ptr<net::tcps_session> session)
{
	std::array<char, 1024> buf;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_some(session->ssl_stream, net::buffer(buf));
		if (e1)
			break;

		session->update_alive_time();

		auto data = net::buffer(buf.data(), n1);

		fmt::print("{} {} {}\n", std::chrono::system_clock::now(), data.size(), data);

		auto [e2, n2] = co_await net::async_send(session->ssl_stream, data);
		if (e2)
			break;
	}

	session->close();
}

net::awaitable<void> client_join(net::tcps_server& server, std::shared_ptr<net::tcps_session> session)
{
	session->socket.set_option(net::ip::tcp::no_delay(true));
	session->socket.set_option(net::socket_base::keep_alive(true));

	auto [e2] = co_await net::async_handshake(session->ssl_stream, net::ssl::stream_base::handshake_type::server);
	if (e2)
	{
		fmt::print("handshake failure: {}\n", e2.message());
		co_return;
	}

	co_await server.session_map.async_add(session);

	co_await(do_recv(session) || net::watchdog(session->alive_time, net::tcp_idle_timeout));

	co_await server.session_map.async_remove(session);
}

net::awaitable<void> start_server(net::tcps_server& server, std::string listen_address, std::uint16_t listen_port)
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
				std::make_shared<net::tcps_session>(std::move(client), server.ssl_context)), net::detached);
		}
	}
}

int main()
{
	net::io_context_thread ctx;

	net::ssl::context sslctx(net::ssl::context::sslv23);
	net::load_cert_from_string(sslctx, net::ssl::verify_peer | net::ssl::verify_fail_if_no_peer_cert,
		ca_crt, server_crt, server_key, "123456", dh);
	net::tcps_server server(ctx.get_executor(), std::move(sslctx));

	net::co_spawn(ctx.get_executor(), start_server(server, "0.0.0.0", 8002), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	ctx.join();
}
