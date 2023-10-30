#include <asio3/udp/udp_server.hpp>
#include <asio3/core/fmt.hpp>

namespace net = ::asio;

net::awaitable<void> client_join(net::udp_server& server, std::shared_ptr<net::udp_connection> client)
{
	co_await client->async_send("<123456789>");

	co_await net::wait_error_or_idle_timeout(client->socket, client->alive_time, net::udp_idle_timeout);

	co_await server.connection_map.async_erase(client);
}

net::awaitable<void> start_server(net::udp_server& server)
{
	auto [ec, ep] = co_await server.async_open("0.0.0.0", 8035);
	if (ec)
	{
		fmt::print("listen failure: {}\n", ec.message());
		co_return;
	}

	fmt::print("listen success: {} {}\n", server.get_listen_address(), server.get_listen_port());

	while (!server.is_aborted())
	{
		std::vector<std::uint8_t> recv_buffer(1024);

		asio::ip::udp::endpoint remote_endpoint{};

		auto [e1, n1] = co_await net::async_receive_from(
			server.socket, asio::buffer(recv_buffer), remote_endpoint);
		if (e1)
			break;

		static auto make_conn = [&server, &remote_endpoint]()
		{
			return std::make_shared<net::udp_connection>(server.socket, remote_endpoint);
		};

		auto [client, is_new_client] = co_await server.connection_map.async_find_or_emplace(
			remote_endpoint, make_conn);
		if (is_new_client)
		{
			net::co_spawn(server.get_executor(), client_join(server, client), net::detached);
		}

		client->update_alive_time();

		auto data = net::buffer(recv_buffer.data(), n1);

		fmt::print("{} {} {} {}\n", std::chrono::system_clock::now(),
			client->get_remote_address(), client->get_remote_port(), data);
	}
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
