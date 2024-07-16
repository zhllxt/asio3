#include <asio/serial_port.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/fmt.hpp>
#include <iostream>

#ifdef ASIO_STANDALONE
namespace net = ::asio;
#else
namespace net = boost::asio;
#endif

using co_serial_port = net::as_tuple_t<net::use_awaitable_t<>>::as_default_on_t<net::serial_port>;

net::awaitable<void> do_send(co_serial_port& sp)
{
	for (;;)
	{
		std::string msg = "<0123456789...>\n";

		auto [e1, n1] = co_await net::async_write(sp, net::buffer(msg));
		if (e1)
			break;

		fmt::print("{} send success, data: {}", std::chrono::system_clock::now(), msg);

		co_await net::delay(std::chrono::seconds(3));
	}
}

net::awaitable<void> do_recv(co_serial_port& sp)
{
	net::error_code ec{};

	constexpr std::size_t max_size{ 4096 };

	for (std::string buffer;;)
	{
		auto [e1, n1] = co_await net::async_read_until(sp, net::dynamic_buffer(buffer, max_size), '\n');
		if (e1)
			break;

		std::string_view data{ buffer.data(), n1 };
		fmt::print("{}", data);

		buffer.erase(0, n1);
	}

	sp.close(ec);
}

net::awaitable<void> do_init(co_serial_port& sp, std::string device)
{
	net::error_code ec{};

	sp.open(device, ec);
	if (ec)
	{
		fmt::print("open serial port <{}> failed: {}\n", device, ec.message());
		co_return;
	}
	else
	{
		fmt::print("open serial port <{}> successed\n", device);
	}

	sp.set_option(net::serial_port::baud_rate(9600), ec);
	if (ec)
		fmt::print("set baud_rate failed: {}\n", ec.message());

	sp.set_option(net::serial_port::flow_control(net::serial_port::flow_control::type::none), ec);
	if (ec)
		fmt::print("set flow_control failed: {}\n", ec.message());

	sp.set_option(net::serial_port::parity(net::serial_port::parity::type::none), ec);
	if (ec)
		fmt::print("set parity failed: {}\n", ec.message());

	sp.set_option(net::serial_port::stop_bits(net::serial_port::stop_bits::type::one), ec);
	if (ec)
		fmt::print("set stop_bits failed: {}\n", ec.message());

	sp.set_option(net::serial_port::character_size(8), ec);
	if (ec)
		fmt::print("set character_size failed: {}\n", ec.message());

	net::co_spawn(co_await net::this_coro::executor, do_send(sp), net::detached);

	co_await do_recv(sp);
}

int main()
{
	net::io_context ctx;

	co_serial_port sp{ ctx.get_executor() };

	net::co_spawn(ctx.get_executor(), do_init(sp, "COM2"), net::detached);

	net::signal_set sigset(ctx.get_executor(), SIGINT);
	sigset.async_wait([&sp](net::error_code, int) mutable
	{
		net::error_code ec;
		sp.close(ec);
	});

	ctx.run();
}
