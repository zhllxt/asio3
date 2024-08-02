#include <asio3/udp/udp_client.hpp>
#include <asio3/core/fmt.hpp>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

// https://blog.csdn.net/qq_37733540/article/details/94552995

net::awaitable<void> do_unicast(net::udp_socket& sock)
{
	net::error_code ec;
	net::ip::udp::endpoint bind_addr{ net::ip::udp::v4(), 5900 };

	sock.open(bind_addr.protocol(), ec);
	if (ec)
	{
		fmt::print("open udp socket failed: {}\n", ec.message());
		co_return;
	}

	sock.set_option(net::socket_base::reuse_address(true), ec);

	sock.bind(bind_addr, ec);
	if (ec)
	{
		fmt::print("bind udp socket failed: {}\n", ec.message());
		co_return;
	}

	std::array<char, 1024> buf;

	for (;;)
	{
		net::ip::udp::endpoint sender_endpoint;

		auto [e1, n1] = co_await sock.async_receive_from(net::buffer(buf), sender_endpoint);
		if (e1)
			break;

		auto data = net::buffer(buf.data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);

		auto [e2, n2] = co_await sock.async_send_to(net::buffer(buf, n1), sender_endpoint);
		if (e2)
			break;
	}

	sock.close(ec);
}

net::awaitable<void> do_multicast_sender(net::udp_socket& sock)
{
	net::error_code ec;
	net::ip::udp::endpoint bind_addr{ net::ip::udp::v4(), 1900 };

	sock.open(bind_addr.protocol(), ec);
	if (ec)
	{
		fmt::print("open udp socket failed: {}\n", ec.message());
		co_return;
	}

	sock.set_option(net::socket_base::reuse_address(true), ec);

	// join multicast group or not join is both ok.
	sock.set_option(net::ip::multicast::enable_loopback(false), ec);
	sock.set_option(net::ip::multicast::join_group(net::ip::make_address("239.255.255.250")), ec);

	sock.bind(bind_addr, ec);
	if (ec)
	{
		fmt::print("bind udp socket failed: {}\n", ec.message());
		co_return;
	}

	fmt::print("start multicast sender udp socket success: {}\n", ec.message());

	net::co_spawn(sock.get_executor(), [&sock]() mutable -> net::awaitable<void>
	{
		// send data to the multicast_endpoint
		// multicast address
		// the recver must be recv data at port 1900
		net::ip::udp::endpoint multicast_endpoint(net::ip::address::from_string("239.255.255.250"), 1900);

		for (;;)
		{
			std::string msg = "<data come from multicast...>";

			auto [e2, n2] = co_await sock.async_send_to(net::buffer(msg), multicast_endpoint);
			if (e2)
			{
				fmt::print("send multicast data failed: {}\n", e2.message());
				break;
			}
			else
			{
				fmt::print("{} send multicast data success, port: {} data: {}\n",
					std::chrono::system_clock::now(), multicast_endpoint.port(), msg);
			}

			co_await net::delay(std::chrono::seconds(3));
		}

		net::error_code ec;
		sock.close(ec);
	}, net::detached);

	net::co_spawn(sock.get_executor(), [&sock]() mutable -> net::awaitable<void>
	{
		std::array<char, 1024> buf;

		for (;;)
		{
			net::ip::udp::endpoint sender_endpoint;

			auto [e1, n1] = co_await sock.async_receive_from(net::buffer(buf), sender_endpoint);
			if (e1)
			{
				fmt::print("recv failed: {}\n", e1.message());
				break;
			}

			auto data = net::buffer(buf.data(), n1);

			fmt::print("{} recv from: {}:{} {}\n", std::chrono::system_clock::now(),
				sender_endpoint.address().to_string(), sender_endpoint.port(), data);
		}

		net::error_code ec;
		sock.close(ec);
	}, net::detached);
}

net::awaitable<void> do_multicast_recver(net::udp_socket& sock)
{
	net::error_code ec;
	net::ip::udp::endpoint bind_addr{ net::ip::udp::v4(), 1900 };

	sock.open(bind_addr.protocol(), ec);
	if (ec)
	{
		fmt::print("open udp socket failed: {}\n", ec.message());
		co_return;
	}

	sock.set_option(net::socket_base::reuse_address(true), ec);

	// client:
	// must join multicast group to recv data...
	//sock.set_option(net::ip::multicast::enable_loopback(false), ec);
	sock.set_option(net::ip::multicast::join_group(net::ip::make_address("239.255.255.250")), ec);

	sock.bind(bind_addr, ec);
	if (ec)
	{
		fmt::print("bind udp socket failed: {}\n", ec.message());
		co_return;
	}

	fmt::print("start multicast recver udp socket success: {}\n", ec.message());

	std::array<char, 1024> buf;

	for (;;)
	{
		net::ip::udp::endpoint sender_endpoint;

		auto [e1, n1] = co_await sock.async_receive_from(net::buffer(buf), sender_endpoint);
		if (e1)
			break;

		auto data = net::buffer(buf.data(), n1);

		fmt::print("{} recv from: {}:{} {}\n", std::chrono::system_clock::now(),
			sender_endpoint.address().to_string(), sender_endpoint.port(), data);

		auto [e2, n2] = co_await sock.async_send_to(net::buffer(buf, n1), sender_endpoint);
		if (e2)
			break;
	}

	sock.close(ec);
}

net::awaitable<void> do_broadcast(net::udp_socket& sock)
{
	net::error_code ec;
	// the bind address can't be "192.168.255.255", must be the local machine address.
	net::ip::udp::endpoint bind_addr{ net::ip::udp::v4(), 2900 };

	sock.open(bind_addr.protocol(), ec);
	if (ec)
	{
		fmt::print("open udp socket failed: {}\n", ec.message());
		co_return;
	}

	sock.set_option(net::socket_base::reuse_address(true), ec);
	if (ec)
	{
		fmt::print("set socket option 'reuse_address' failed: {}\n", ec.message());
		co_return;
	}

	sock.set_option(net::ip::udp::socket::broadcast(true), ec);
	if (ec)
	{
		fmt::print("set socket option 'broadcast' failed: {}\n", ec.message());
		co_return;
	}

	sock.bind(bind_addr, ec);
	if (ec)
	{
		fmt::print("bind udp socket failed: {}\n", ec.message());
		co_return;
	}

	fmt::print("start broadcast udp socket success: {}\n", ec.message());

	net::co_spawn(sock.get_executor(), [&sock]() mutable -> net::awaitable<void>
	{
		// https://zhuanlan.zhihu.com/p/323918414
		// can't use the 0xffffffff broadcast address
		//net::ip::udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), 2905);
		net::ip::udp::endpoint broadcast_endpoint(net::ip::address::from_string("192.168.255.255"), 2905);

		for (;;)
		{
			std::string msg = "<data come from broadcast...>";

			auto [e2, n2] = co_await sock.async_send_to(net::buffer(msg), broadcast_endpoint);
			if (e2)
			{
				fmt::print("send broadcast data failed: {}\n", e2.message());
				break;
			}
			else
			{
				fmt::print("{} send broadcast data success, port: {} data: {}\n",
					std::chrono::system_clock::now(), broadcast_endpoint.port(), msg);
			}

			co_await net::delay(std::chrono::seconds(3));
		}

		net::error_code ec;
		sock.close(ec);
	}, net::detached);

	net::co_spawn(sock.get_executor(), [&sock]() mutable -> net::awaitable<void>
	{
		std::array<char, 1024> buf;

		for (;;)
		{
			net::ip::udp::endpoint sender_endpoint;

			auto [e1, n1] = co_await sock.async_receive_from(net::buffer(buf), sender_endpoint);
			if (e1)
			{
				fmt::print("recv failed: {}\n", e1.message());
				break;
			}

			auto data = net::buffer(buf.data(), n1);

			fmt::print("{} recv from: {}:{} {}\n", std::chrono::system_clock::now(),
				sender_endpoint.address().to_string(), sender_endpoint.port(), data);
		}

		net::error_code ec;
		sock.close(ec);
	}, net::detached);
}

int main()
{
	net::io_context ctx{ 1 };

	net::udp_socket sock(ctx.get_executor());

	//net::co_spawn(ctx.get_executor(), do_unicast(sock), net::detached);
	//net::co_spawn(ctx.get_executor(), do_multicast_sender(sock), net::detached);
	net::co_spawn(ctx.get_executor(), do_multicast_recver(sock), net::detached);
	//net::co_spawn(ctx.get_executor(), do_broadcast(sock), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&sock](net::error_code, int) mutable
	{
		net::error_code ec;
		sock.close(ec);
	});

	ctx.run();
}
