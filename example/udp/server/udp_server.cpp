#include <asio3/udp/udp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> start_server(net::udp_server& server)
{
	auto [ec, ep] = co_await server.async_open({ .listen_address = "0.0.0.0", .listen_port = 8035 });
	if (ec)
		fmt::print("listen failure: {}\n", ec.message());
	else
		fmt::print("listen success: {} {}\n",
			server.get_listen_address(), server.get_listen_port());
}

int main()
{
	net::io_context_thread ctx;

	net::udp_server server(ctx.get_executor());

	net::co_spawn(server.get_executor(), start_server(server), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	ctx.join();
}
