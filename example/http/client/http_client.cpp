#include <asio3/http/http_client.hpp>
#include <asio3/core/fmt.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

net::awaitable<void> do_upload(net::http_client& client)
{
	beast::flat_buffer buffer;

	auto on_chunk = [](auto) { return true; };

	net::error_code ec{};
	net::stream_file file(client.get_executor());
	file.open("D:/Programs/HeidiSQL_12.5_64_Portable.zip", net::file_base::read_only, ec);
	if (ec)
		co_return;

	http::request<http::string_body> req{ http::verb::post, "/", 11 };
	req.target("/upload/HeidiSQL_12.5_64_Portable.zip");
	req.set(http::field::content_type, http::extension_to_mimetype("zip"));
	req.set(http::field::host, "127.0.0.1:8080");
	req.chunked(true);
	//req.content_length(file.size());

	auto [e0, n0] = co_await http::async_send_file(client.socket, file, req, on_chunk);

	fmt::print("upload finished: {}\n", e0.message());

	http::response<http::string_body> res;
	auto [e1, n1] = co_await http::async_read(client.socket, buffer, res);

	client.close();
	client.async_stop([](auto...) {});
}

net::awaitable<void> do_download(net::http_client& client)
{
	beast::flat_buffer buffer;

	auto on_chunk = [](auto) { return true; };

	net::error_code ec{};
	net::stream_file file(client.get_executor());
	file.open("asio-master.zip",
		net::file_base::write_only | net::file_base::create | net::file_base::truncate, ec);
	if (ec)
		co_return;

	http::request<http::empty_body> req{ http::verb::get, "/", 11 };
	req.target("/download/asio-master.zip");
	req.set(http::field::host, "127.0.0.1:8080");

	auto [e1, n1] = co_await http::async_write(client.socket, req);
	if (e1)
		co_return;

	http::response_parser<http::string_body> parser;
	parser.body_limit((std::numeric_limits<std::size_t>::max)());
	auto [e2, n2] = co_await http::async_read_header(client.socket, buffer, parser);
	if (e2)
		co_return;

	auto [e3, n3] = co_await http::async_recv_file(client.socket, file, buffer, parser.get(), on_chunk);

	fmt::print("download finished: {}\n", e3.message());

	client.close();
	client.async_stop([](auto...) {});
}

net::awaitable<void> connect(net::http_client& client)
{
	while (!client.is_aborted())
	{
		auto [ec, ep] = co_await client.async_connect("127.0.0.1", 8080);
		if (ec)
		{
			// connect failed, reconnect...
			fmt::print("connect failure: {}\n", ec.message());
			co_await net::delay(std::chrono::seconds(1));
			client.close();
			continue;
		}

		fmt::print("connect success: {} {}\n", client.get_remote_address(), client.get_remote_port());

		//co_await do_upload(client);
		co_await do_download(client);
	}
}

int main()
{
	net::io_context ctx;

	net::http_client client(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), connect(client), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&client](net::error_code, int) mutable
	{
		client.async_stop([](auto) {});
	});

	ctx.run();
}
