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

net::awaitable<void> do_multicast(net::udp_socket& sock)
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

	// server:
	// send data to the multicast_endpoint
	//net::ip::udp::endpoint multicast_endpoint(net::ip::address::from_string("239.255.255.250", ec), 12345);

	// client:
	// join multicast group, recv data...
	sock.set_option(net::ip::multicast::enable_loopback(false), ec);
	sock.set_option(net::ip::multicast::join_group(net::ip::make_address("239.255.255.250")), ec);

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

net::awaitable<void> do_broadcast(net::udp_socket& sock)
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
	sock.set_option(net::ip::udp::socket::broadcast(true), ec);

	net::ip::udp::endpoint broadcast_endpoint(net::ip::address_v4::broadcast(), 12345);

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

		auto [e2, n2] = co_await sock.async_send_to(net::buffer("<data...>"), broadcast_endpoint);
		if (e2)
			break;

		auto [e1, n1] = co_await sock.async_receive_from(net::buffer(buf), sender_endpoint);
		if (e1)
			break;

		auto data = net::buffer(buf.data(), n1);

		fmt::print("{} {}\n", std::chrono::system_clock::now(), data);
	}

	sock.close(ec);
}

int main()
{
	net::io_context ctx;

	net::udp_socket sock(ctx.get_executor());

	//net::co_spawn(ctx.get_executor(), do_unicast(sock), net::detached);
	net::co_spawn(ctx.get_executor(), do_multicast(sock), net::detached);
	//net::co_spawn(ctx.get_executor(), do_broadcast(sock), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&sock](net::error_code, int) mutable
	{
		net::error_code ec;
		sock.close(ec);
	});

	ctx.run();
}
