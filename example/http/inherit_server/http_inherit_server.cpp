#include <asio3/core/fmt.hpp>
#include <asio3/http/http_server.hpp>

namespace net = ::asio;

// how to get the session ip and port in the http router function ?

// 1. declare a custom http request class that derived from web_request
class web_request_ex : public http::web_request
{
public:
	using http::web_request::web_request;

	// 2. declare a member variable. of course you can declared any variable at here.
	std::shared_ptr<class http_session_ex> session;
};

// 3. declare a custom http session class that derived from http_session
class http_session_ex : public net::http_session
{
public:
	using net::http_session::http_session;

	// 4. [important] redeclare the request_type
	using request_type = web_request_ex;
};

// 5. declare a custom http server.
using http_server_ex = net::basic_http_server<http_session_ex>;

net::awaitable<void> do_http_recv(http_server_ex& server, std::shared_ptr<http_session_ex> session)
{
	// This buffer is required to persist across reads
	beast::flat_buffer buf;

	for (;;)
	{
		// Read a request
		web_request_ex req;
		auto [e1, n1] = co_await http::async_read(session->socket, buf, req);
		if (e1)
			break;

		// 6. save http session into the member variable of http request.
		req.session = session;

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

net::awaitable<void> client_join(http_server_ex& server, std::shared_ptr<http_session_ex> session)
{
	auto addr = session->get_remote_address();
	auto port = session->get_remote_port();

	fmt::print("+ client join: {} {}\n", addr, port);

	co_await server.session_map.async_add(session);

	co_await(do_http_recv(server, session) || net::watchdog(session->alive_time, net::http_idle_timeout));

	co_await server.session_map.async_remove(session);

	fmt::print("- client exit: {} {}\n", addr, port);
}

net::awaitable<void> start_server(http_server_ex& server, std::string listen_address, std::uint16_t listen_port)
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
				std::make_shared<http_session_ex>(std::move(client))), net::detached);
		}
	}
}

auto response_404()
{
	return http::make_error_page_response(http::status::not_found);
}

struct aop_auth
{
	net::awaitable<bool> before(web_request_ex& req, http::web_response& rep)
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

	http_server_ex server(ctx.get_executor());

	std::filesystem::path root = std::filesystem::current_path(); // /asio3/bin/x64
	root = root.parent_path().parent_path().append("example/wwwroot"); // /asio3/example/wwwroot
	server.root_directory = std::move(root);

	server.router.add("/", [&server](web_request_ex& req, http::web_response& rep) -> net::awaitable<bool>
	{
		auto res = http::make_file_response(server.root_directory, "/index.html");
		if (res.has_value())
			rep = std::move(res.value());
		else
			rep = response_404();
		co_return true;
	}, http::enable_cache);

	server.router.add("/login", [](web_request_ex& req, http::web_response& rep) -> net::awaitable<bool>
	{
		// 7. use the http session
		if (req.session->get_remote_address() == "192.168.0.1")
			co_return false;
		rep = response_404();
		co_return true;
	}, aop_auth{});

	server.router.add("*", [&server](web_request_ex& req, http::web_response& rep) -> net::awaitable<bool>
	{
		auto res = http::make_file_response(server.root_directory, req.target());
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
