#include <asio3/core/fmt.hpp>
#include <asio3/http/http_server.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

net::awaitable<void> do_http_recv(net::http_server& server, std::shared_ptr<net::http_session> session)
{
	// This buffer is required to persist across reads
	beast::flat_buffer buf;

	for (;;)
	{
		// Read a request
		http::web_request req;
		auto [e1, n1] = co_await http::async_read(session->socket, buf, req);
		if (e1)
			break;

		session->update_alive_time();

		http::web_response rep;
		bool result = co_await server.router.route(req, rep);

		// Send the response
		auto [e2, n2] = co_await beast::async_write(session->socket, std::move(rep));
		if (e2)
			break;

		if (!result || !req.keep_alive())
		{
			// This means we should close the connection, usually because
			// the response indicated the "Connection: close" semantic.
			break;
		}
	}

	session->close();
}

net::awaitable<void> client_join(net::http_server& server, std::shared_ptr<net::http_session> session)
{
	auto addr = session->get_remote_address();
	auto port = session->get_remote_port();

	fmt::print("+ client join: {} {}\n", addr, port);

	co_await server.session_map.async_add(session);

	co_await(do_http_recv(server, session) || net::watchdog(session->alive_time, net::http_idle_timeout));

	co_await server.session_map.async_remove(session);

	fmt::print("- client exit: {} {}\n", addr, port);
}

net::awaitable<void> start_server(net::http_server& server, std::string listen_address, std::uint16_t listen_port)
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
				std::make_shared<net::http_session>(std::move(client))), net::detached);
		}
	}
}

auto response_404()
{
	return http::make_error_page_response(http::status::not_found);
}

struct aop_auth
{
	net::awaitable<bool> before(http::web_request& req, http::web_response& rep)
	{
		if (req.find(http::field::authorization) == req.end())
		{
			rep = response_404();
			co_return false;
		}
		co_return true;
	}
};

int main()
{
	net::io_context_thread ctx;

	net::http_server server(ctx.get_executor());

	std::filesystem::path root = std::filesystem::current_path(); // /asio3/bin/x64
	root = root.parent_path().parent_path().append("example/wwwroot"); // /asio3/example/wwwroot
	server.webroot = std::move(root);

	server.router.add("/", [&server](http::web_request& req, http::web_response& rep) -> net::awaitable<bool>
	{
		auto res = http::make_file_response(server.webroot, "/index.html");
		if (res.has_value())
			rep = std::move(res.value());
		else
			rep = response_404();
		co_return true;
	}, http::enable_cache);

	server.router.add("/login", [](http::web_request& req, http::web_response& rep) -> net::awaitable<bool>
	{
		rep = response_404();
		co_return true;
	}, aop_auth{});

	server.router.add("*", [&server](http::web_request& req, http::web_response& rep) -> net::awaitable<bool>
	{
		auto res = http::make_file_response(server.webroot, req.target());
		if (res.has_value())
			rep = std::move(res.value());
		else
			rep = response_404();
		co_return true;
	}, http::enable_cache);

	net::co_spawn(ctx.get_executor(), start_server(server, "0.0.0.0", 8080), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	ctx.join();
}
