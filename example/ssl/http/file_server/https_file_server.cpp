#ifndef ASIO3_ENABLE_SSL
#define ASIO3_ENABLE_SSL
#endif

#include <asio3/core/fmt.hpp>
#include <asio3/http/https_server.hpp>
#include <asio3/core/defer.hpp>
#include <unordered_map>
#include "../../certs.hpp"

namespace net = ::asio;

struct userdata
{
	std::shared_ptr<net::https_session>& session;
	beast::flat_buffer& buffer;
	http::request_parser<http::string_body>& parser;
	bool& need_response;
};

using http_server_ex = net::basic_https_server<
	net::https_session, http::basic_router<http::web_request, http::web_response, userdata>>;

net::awaitable<void> do_http_recv(http_server_ex& server, std::shared_ptr<net::https_session> session)
{
	// This buffer is required to persist across reads
	beast::flat_buffer buffer;

	for (;;)
	{
		http::request_parser<http::string_body> parser;

		parser.body_limit((std::numeric_limits<std::size_t>::max)());

		auto [e0, n0] = co_await http::async_read_header(session->ssl_stream, buffer, parser);
		if (e0)
			break;

		bool need_response = true;
		http::web_response rep;
		bool result = co_await server.router.route(parser.get(), rep,
			userdata{ session, buffer, parser, need_response });

		session->update_alive_time();

		// Send the response
		if (need_response)
		{
			auto [e2, n2] = co_await beast::async_write(session->ssl_stream, std::move(rep));
			if (e2)
				break;
		}

		if (!result || !parser.get().keep_alive())
		{
			// This means we should close the connection, usually because
			// the response indicated the "Connection: close" semantic.
			break;
		}
	}

	co_await session->ssl_stream.async_shutdown();

	session->close();
}

net::awaitable<void> client_join(http_server_ex& server, std::shared_ptr<net::https_session> session)
{
	auto addr = session->get_remote_address();
	auto port = session->get_remote_port();

	auto [e2] = co_await net::async_handshake(session->ssl_stream, net::ssl::stream_base::handshake_type::server);
	if (e2)
	{
		fmt::print("handshake failure: {}\n", e2.message());
		co_return;
	}

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
				std::make_shared<net::https_session>(std::move(client), server.ssl_context)), net::detached);
		}
	}
}

auto response_404()
{
	return http::make_error_page_response(http::status::not_found);
}

struct aop_auth
{
	net::awaitable<bool> before(http::web_request& req, http::web_response& rep, userdata data)
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

	net::ssl::context sslctx(net::ssl::context::tlsv12);
	sslctx.set_options(
		net::ssl::context::default_workarounds |
		net::ssl::context::no_sslv2 |
		net::ssl::context::single_dh_use);
	net::load_cert_from_string(sslctx, net::ssl::verify_none,
		ca_crt, server_crt, server_key, "123456", dh);
	http_server_ex server(ctx.get_executor(), std::move(sslctx));

	std::unordered_map<net::cancellation_signal*, std::unique_ptr<net::cancellation_signal>> sigs;

	std::filesystem::path root = std::filesystem::current_path(); // /asio3/bin/x64
	root = root.parent_path().parent_path().append("example/wwwroot"); // /asio3/example/wwwroot
	server.root_directory = std::move(root);

	server.router.add("/", [&server](http::web_request& req, http::web_response& rep, userdata data)
		-> net::awaitable<bool>
	{
		auto [e1, n1] = co_await http::async_read(data.session->ssl_stream, data.buffer, data.parser);
		if (e1)
			co_return false;

		std::filesystem::path filepath = server.root_directory / "index.html";
		auto [ec, file, content] = co_await net::async_read_file_content(filepath.string());
		if (ec)
		{
			rep = response_404();
			co_return true;
		}

		rep = http::make_html_response(std::move(content));
		co_return true;
	}, http::enable_cache);

	server.router.add("*", [&server](http::web_request& req, http::web_response& rep, userdata data)
		-> net::awaitable<bool>
	{
		auto [e1, n1] = co_await http::async_read(data.session->ssl_stream, data.buffer, data.parser);
		if (e1)
			co_return false;

		std::filesystem::path filepath = net::make_filepath(server.root_directory, req.target());
		auto [ec, file, content] = co_await net::async_read_file_content(filepath.string());
		if (ec)
		{
			rep = response_404();
			co_return true;
		}

		auto res = http::make_text_response(std::move(content));
		res.set(http::field::content_type, http::extension_to_mimetype(filepath.extension().string()));
		rep = std::move(res);
		co_return true;
	}, http::enable_cache);

	server.router.add("/download/*", [&server, &sigs](http::web_request& req, http::web_response& rep, userdata data)
		-> net::awaitable<bool>
	{
		auto [e1, n1] = co_await http::async_read(data.session->ssl_stream, data.buffer, data.parser);
		if (e1)
			co_return false;

		if (server.is_aborted())
		{
			rep = http::make_error_page_response(http::status::service_unavailable);
			co_return false;
		}

		std::filesystem::path filepath = net::make_filepath(
			server.root_directory, req.target().substr(std::strlen("/download")));

		std::error_code ec{};
		net::stream_file file(data.session->get_executor());
		file.open(filepath.string(), asio::file_base::read_only, ec);
		if (ec)
		{
			rep = http::make_error_page_response(http::status::internal_server_error);
			co_return true;
		}

		http::response<http::string_body> res{ http::status::ok, req.version() };
		res.set(http::field::server, BEAST_VERSION_STRING);
		res.set(http::field::content_type, http::extension_to_mimetype(filepath.string()));
		//res.chunked(true);
		res.content_length(file.size());

		// used to interrupt the file operation, otherwise when close this application,
		// it maybe wait for a long time until the file operation finished.
		std::unique_ptr<net::cancellation_signal> sig = std::make_unique<net::cancellation_signal>();
		net::cancellation_signal* psig = sig.get();
		sigs.emplace(psig, std::move(sig));
		std::defer remove_sig = [&sigs, psig]() mutable
		{
			sigs.erase(psig);
		};

		auto [e2, n2] = co_await http::async_send_file(data.session->ssl_stream, file, std::move(res),
			net::bind_cancellation_slot(psig->slot(), net::use_nothrow_awaitable));
		if (e2)
		{
			rep = http::make_error_page_response(http::status::internal_server_error);
			co_return true;
		}

		data.need_response = false;

		co_return true;
	}, aop_auth{});

	server.router.add("/upload/*", [&server, &sigs](http::web_request& req, http::web_response& rep, userdata data)
		-> net::awaitable<bool>
	{
		if (server.is_aborted())
		{
			rep = http::make_error_page_response(http::status::service_unavailable);
			co_return false;
		}

		std::filesystem::path filepath = net::make_filepath(
			server.root_directory, req.target().substr(std::strlen("/upload")));

		std::error_code ec{};
		net::stream_file file(data.session->get_executor());
		file.open(filepath.string(),
			asio::file_base::write_only | asio::file_base::create | asio::file_base::truncate, ec);
		if (ec)
		{
			rep = http::make_error_page_response(http::status::internal_server_error);
			co_return true;
		}

		// used to interrupt the file operation, otherwise when close this application,
		// it maybe wait for a long time until the file operation finished.
		std::unique_ptr<net::cancellation_signal> sig = std::make_unique<net::cancellation_signal>();
		net::cancellation_signal* psig = sig.get();
		sigs.emplace(psig, std::move(sig));
		std::defer remove_sig = [&sigs, psig]() mutable
		{
			sigs.erase(psig);
		};

		auto [e2, n2] = co_await http::async_recv_file(data.session->ssl_stream, file, data.buffer, req,
			net::bind_cancellation_slot(psig->slot(), net::use_nothrow_awaitable));
		if (e2)
		{
			rep = http::make_error_page_response(http::status::internal_server_error);
			co_return true;
		}

		rep = http::make_text_response("upload successed", http::status::ok);
		co_return true;
	}, aop_auth{});

	net::co_spawn(ctx.get_executor(), start_server(server, "0.0.0.0", 8443), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server, &sigs](net::error_code, int) mutable
	{
		for (auto& [key, sig] : sigs)
		{
			sig->emit(net::cancellation_type::terminal);
		}
		net::error_code ec{};
		server.acceptor.close(ec);
		server.async_stop([](auto) {});
	});

	ctx.join();
}
