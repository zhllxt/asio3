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
		auto operator()(auto state, udp_client& client) -> void
		{
			auto& sock = client.socket;
			auto& opt = client.option;

			co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			[[maybe_unused]] detail::close_socket_when_timeout tg(sock, opt.connect_timeout);

			if (asio::to_underlying(opt.socks5_option.cmd) == 0)
				opt.socks5_option.cmd = socks5::command::udp_associate;

			if (socks5::is_option_valid(opt.socks5_option))
			{
				asio::error_code ec{};

				ip::udp::endpoint bnd_endpoint{};

				if (!opt.bind_address.empty())
				{
					bnd_endpoint.address(asio::ip::address::from_string(opt.bind_address, ec));
					if (ec)
						co_return ec;
				}

				bnd_endpoint.port(opt.bind_port);

				udp_socket tmp_sock(sock.get_executor());

				tmp_sock.open(bnd_endpoint.protocol(), ec);
				if (ec)
					co_return ec;

				default_udp_socket_option_setter{ opt.socket_option }(tmp_sock);

				tmp_sock.bind(bnd_endpoint, ec);
				if (ec)
					co_return ec;

				if (opt.socks5_option.dest_address.empty())
					opt.socks5_option.dest_address = opt.server_address;

				opt.socks5_option.dest_port = tmp_sock.local_endpoint(ec).port();
				if (ec)
					co_return ec;

				sock = std::move(tmp_sock);

				tcp_socket s5_sock(sock.get_executor());

				auto [e1, ep1] = co_await asio::async_connect(
					s5_sock,
					opt.socks5_option.proxy_address, opt.socks5_option.proxy_port,
					asio::default_tcp_socket_option_setter{},
					use_nothrow_deferred);
				if (e1)
					co_return e1;

				auto [e2] = co_await socks5::async_handshake(
					s5_sock, opt.socks5_option, use_nothrow_deferred);
				if (e2)
					co_return e2;

				auto [e3, ep3] = co_await asio::async_connect(
					sock,
					opt.server_address, opt.socks5_option.bound_port,
					asio::default_udp_socket_option_setter{ opt.socket_option },
					use_nothrow_deferred);
				if (e3)
					co_return e3;

				client.socks5_socket = std::move(s5_sock);
			}
			else
			{
				auto [e1, ep1] = co_await asio::async_connect(
					sock,
					opt.server_address, opt.server_port,
					opt.bind_address, opt.bind_port,
					asio::default_udp_socket_option_setter{ opt.socket_option },
					use_nothrow_deferred);
				if (e1)
					co_return e1;
			}

			co_return asio::error_code{};
		}
	};
