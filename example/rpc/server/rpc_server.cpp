#include <asio3/core/fmt.hpp>
#include <asio3/core/match_condition.hpp>
#include <asio3/rpc/rpc_server.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

decltype(std::chrono::steady_clock::now()) time1 = std::chrono::steady_clock::now();
decltype(std::chrono::steady_clock::now()) time2 = std::chrono::steady_clock::now();
std::size_t qps = 0;
bool _first = true;
net::awaitable<std::string> echo(std::string a)
{
	if (_first)
	{
		_first = false;
		time1 = std::chrono::steady_clock::now();
		time2 = std::chrono::steady_clock::now();
	}

	qps++;
	decltype(std::chrono::steady_clock::now()) time3 = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::seconds>(time3 - time2).count();
	if (ms > 1)
	{
		time2 = time3;
		ms = std::chrono::duration_cast<std::chrono::seconds>(time3 - time1).count();
		double speed = (double)qps / (double)ms;
		fmt::print("{:.1f}\n", speed);
	}
	co_return a;
}

net::awaitable<void> do_recv(net::rpc_server& server, std::shared_ptr<net::rpc_session> session)
{
	std::string strbuf;
	rpc::serializer& sr = session->serializer;
	rpc::deserializer& dr = session->deserializer;

	for (;;)
	{
		auto [e1, n1] = co_await net::async_read_until(
			session->get_stream(), net::dynamic_buffer(strbuf), net::length_payload_match_condition{});
		if (e1)
			break;
		if (n1 == 0)
			break;

		session->update_alive_time();

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
			auto [e2, resp] = co_await server.invoker.invoke(sr, dr, std::move(head), data);

			if (!resp.empty())
			{
				std::string len = asio::length_payload_match_condition::generate_length(asio::buffer(resp));
				std::array<asio::const_buffer, 2> buffers
				{
					asio::buffer(len),
					asio::buffer(resp),
				};
				co_await session->async_send(buffers);
			}

			if (e2)
				break;
		}
		else if (head.is_response())
		{
			co_await session->async_notify(sr, dr, std::move(head), data);
		}
		else
		{
			break;
		}

		strbuf.erase(0, n1);
	}

	fmt::print("client exit\n");

	session->close();
}

net::awaitable<void> client_join(net::rpc_server& server, std::shared_ptr<net::rpc_session> session)
{
	co_await server.session_map.async_add(session);
	co_await(do_recv(server, session) || net::watchdog(session->alive_time, net::tcp_idle_timeout));
	co_await server.session_map.async_remove(session);
}

net::awaitable<void> start_server(
	std::vector<net::io_context_thread>& session_ctxs,
	net::rpc_server& server, std::string listen_address, std::uint16_t listen_port)
{
	auto [ec, ep] = co_await server.async_listen(listen_address, listen_port);
	if (ec)
	{
		fmt::print("listen failure: {}\n", ec.message());
		co_return;
	}

	fmt::print("listen success: {} {}\n", server.get_listen_address(), server.get_listen_port());

	std::size_t next = 0;

	while (!server.is_aborted())
	{
		const auto& ex = session_ctxs.at((next++) % session_ctxs.size()).get_executor();
		auto [e1, client] = co_await server.acceptor.async_accept(ex);
		if (e1)
		{
			co_await net::delay(std::chrono::milliseconds(100));
		}
		else
		{
			net::co_spawn(ex, client_join(server,
				std::make_shared<net::rpc_session>(std::move(client))), net::detached);
		}
	}
}

int main()
{
	net::io_context listen_ctx;
	std::vector<net::io_context_thread> session_ctxs{ std::thread::hardware_concurrency() * 2 };

	net::rpc_server server(listen_ctx.get_executor());

	server.invoker.bind("echo", echo);

	net::co_spawn(listen_ctx.get_executor(), start_server(session_ctxs, server, "0.0.0.0", 8038), net::detached);

	net::signal_set sigset(listen_ctx.get_executor(), SIGINT);
	sigset.async_wait([&server](net::error_code, int) mutable
	{
		server.async_stop([](auto) {});
	});

	listen_ctx.run();

	for (auto& session_ctx : session_ctxs)
	{
		session_ctx.join();
	}
}
