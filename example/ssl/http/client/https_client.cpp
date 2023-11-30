#ifndef ASIO3_ENABLE_SSL
#define ASIO3_ENABLE_SSL
#endif

#include <asio3/core/root_certificates.hpp>
#include <asio3/http/request.hpp>
#include <asio3/http/download.hpp>
#include <asio3/http/upload.hpp>

#include <asio3/core/fmt.hpp>
#include <iostream>
#include "../../certs.hpp"

namespace net = ::asio;

net::awaitable<void> do_request(net::ssl::context& sslctx)
{
	auto executor = co_await net::this_coro::executor;

	auto [e1, resp1] = co_await http::async_request(executor, {
		.url = "https://www.baidu.com/",
		.header = {
			{ "Host", "www.baidu.com" },
			{ "Connection", "keep-alive" },
		},
		.method = http::verb::get,
		.socks5_option = socks5::option{
			.proxy_address = "127.0.0.1",
			.proxy_port = 10808,
		},
		});

	std::cout << e1.message() << std::endl;
	std::cout << resp1 << std::endl;
}

net::awaitable<void> do_download(net::ssl::context& sslctx)
{
	auto executor = co_await net::this_coro::executor;

	auto on_head = [](http::response<http::string_body>& msg)
	{
		return true;
	};
	auto on_chunk = [](std::string_view data)
	{
		return true;
	};

	auto [e1] = co_await http::async_download(executor, {
		.sslctx = sslctx,
		.url = "https://www.winrar.com.cn/download/winrar-x64-624scp.exe",
		.on_head = on_head,
		.on_chunk = on_chunk,
		.saved_filepath = "winrar-x64-624scp.exe",
		.socks5_option = socks5::option{
			.proxy_address = "127.0.0.1",
			.proxy_port = 10808,
		},
		});

	std::cout << e1.message() << std::endl;
}

net::awaitable<void> do_upload(net::ssl::context& sslctx)
{
	auto executor = co_await net::this_coro::executor;

	auto on_chunk = [](std::string_view data)
	{
		return true;
	};

	auto [e1, resp] = co_await http::async_upload(executor, {
		.url = "https://127.0.0.1:8443/upload/winrar-x64-624scp.exe",
		.on_chunk = on_chunk,
		.local_filepath = "winrar-x64-624scp.exe",
		.socks5_option = socks5::option{
			.proxy_address = "127.0.0.1",
			.proxy_port = 20808,
		},
		});

	std::cout << e1.message() << std::endl;
	std::cout << resp << std::endl;
}

int main()
{
	net::io_context ctx;

	// The SSL context is required, and holds certificates
	net::ssl::context sslctx(net::ssl::context::tlsv12_client);
	// This holds the root certificate used for verification
	net::load_root_certificates(sslctx);
	// Verify the remote server's certificate
	sslctx.set_verify_mode(net::ssl::verify_peer);

	//net::co_spawn(ctx.get_executor(), do_request(sslctx), net::detached);
	net::co_spawn(ctx.get_executor(), do_download(sslctx), net::detached);
	//net::co_spawn(ctx.get_executor(), do_upload(sslctx), net::detached);

	ctx.run();
}
