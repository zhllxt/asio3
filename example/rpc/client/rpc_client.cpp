#include <asio3/rpc/rpc_client.hpp>
#include <asio3/core/fmt.hpp>
#include <asio3/core/match_condition.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

net::awaitable<void> do_recv(net::rpc_client& client)
{
	std::string strbuf;
	rpc::serializer& sr = client.serializer;
	rpc::deserializer& dr = client.deserializer;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(
			client.get_stream(), net::dynamic_buffer(strbuf), net::length_payload_match_condition{});
		if (e1)
			break;

		std::string_view data = net::length_payload_match_condition::get_payload(strbuf.data(), n1);

		rpc::header head{};
		try
		{
			dr.reset(data);
			dr >> head;
		}
		catch (const cereal::exception&)
		{
			break;
		}

		if (head.is_request())
		{
			auto [e2, resp] = co_await client.invoker.invoke(sr, dr, std::move(head), data);

			if (!resp.empty())
			{
				std::string len = asio::length_payload_match_condition::generate_length(asio::buffer(resp));
				std::array<asio::const_buffer, 2> buffers
				{
					asio::buffer(len),
					asio::buffer(resp),
				};
				co_await client.async_send(buffers);
			}

			if (e2)
				break;
		}
		else if (head.is_response())
		{
			co_await client.async_notify(sr, dr, std::move(head), data);
		}
		else
		{
			break;
		}

		strbuf.erase(0, n1);
	}

	client.close();
}

net::awaitable<void> connect(net::rpc_client& client)
{
	while (!client.is_aborted())
	{
		auto [ec, ep] = co_await client.async_connect("127.0.0.1", 8038);
		if (ec)
		{
			// connect failed, reconnect...
			fmt::print("connect failure: {}\n", ec.message());
			co_await net::delay(std::chrono::seconds(1));
			client.close();
			continue;
		}

		fmt::print("connect success: {} {}\n", client.get_remote_address(), client.get_remote_port());

		net::co_spawn(client.get_executor(), [&client]() mutable -> net::awaitable<void>
		{
			std::string msg;
			msg.resize(128, 'A');
			for (;;)
			{
				auto [e1, s1] = co_await client
					.set_request_option({ .timeout = std::chrono::seconds(3) })
					.async_call<std::string>("echo", msg);
				if (e1)
					break;
			}
		}, net::detached);

		co_await do_recv(client);
	}
}

int main()
{
	net::io_context ctx;

	net::rpc_client client(ctx.get_executor());

	net::co_spawn(ctx.get_executor(), connect(client), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&client](net::error_code, int) mutable
	{
		client.async_stop([](auto) {});
	});

	ctx.run();
}
