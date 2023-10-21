/*
 * Copyright (c) 2017-2023 zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

	struct async_connect_op
	{
		auto operator()(auto state, auto client_ref) -> void
		{
			auto& client = client_ref.get();
			auto& sock = client.socket;
			auto& opt = client.option;

			co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			[[maybe_unused]] detail::close_socket_when_timeout tg(sock, opt.connect_timeout);

			if (asio::to_underlying(opt.socks5_option.cmd) == 0)
				opt.socks5_option.cmd = socks5::command::connect;

			if (socks5::is_option_valid(opt.socks5_option))
			{
				auto [e1, ep] = co_await asio::async_connect(
					sock,
					opt.socks5_option.proxy_address, opt.socks5_option.proxy_port,
					opt.bind_address, opt.bind_port,
					asio::default_tcp_socket_option_setter{ opt.socket_option },
					use_nothrow_deferred);
				if (e1)
					co_return e1;

				if (opt.socks5_option.dest_address.empty())
					opt.socks5_option.dest_address = opt.server_address;
				if (opt.socks5_option.dest_port == 0)
					opt.socks5_option.dest_port = opt.server_port;

				auto [e2] = co_await socks5::async_handshake(
					sock, opt.socks5_option, use_nothrow_deferred);
				if (e2)
					co_return e2;
			}
			else
			{
				auto [e1, ep] = co_await asio::async_connect(
					sock,
					opt.server_address, opt.server_port,
					opt.bind_address, opt.bind_port,
					asio::default_tcp_socket_option_setter{ opt.socket_option },
					use_nothrow_deferred);
				if (e1)
					co_return e1;
			}

			co_return asio::error_code{};
		}
	};
