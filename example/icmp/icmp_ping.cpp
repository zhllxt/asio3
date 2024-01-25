#include <asio3/icmp/ping.hpp>
#include <asio3/core/fmt.hpp>
#include <iostream>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

net::awaitable<void> do_ping(std::string host)
{
	for (;;)
	{
		auto [ec, rep] = co_await net::co_ping(host);

		if (rep.is_timeout())
			fmt::print("request timed out: {}\n", ec.message());
		else
			fmt::print("{}\n", fmt::kvformat(
				"{}", rep.total_length() - rep.header_length(),
				" bytes from {}", rep.source_address().to_string(),
				": icmp_seq={}", rep.sequence_number(),
				", ttl={}", rep.time_to_live(),
				", time={}ms", rep.milliseconds()
			));

		co_await net::delay(std::chrono::seconds(1));
	}
}

int main()
{
	net::io_context ctx;

	net::co_spawn(ctx.get_executor(), do_ping("www.baidu.com"), net::detached);

	ctx.run();
}
