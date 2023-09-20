/*
 * Copyright (c) 2017-2023 zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

#include <asio3/tcp/connect.hpp>
#include <asio3/socks5/handshake.hpp>

namespace asio::detail
{
	struct tcp_async_start_op
	{
		template<typename AsyncStream, typename StartOption>
		auto operator()(auto state,
			std::reference_wrapper<AsyncStream> sock_ref,
			std::reference_wrapper<StartOption> option_ref) -> void
		{
			auto& sock = sock_ref.get();
			auto& opt = option_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::timeout_guard tg(sock, opt.connect_timeout);

			if (asio::to_underlying(opt.socks5_option.cmd) == 0)
				opt.socks5_option.cmd = socks5::command::connect;

			auto [e1, ep] = co_await asio::async_connect(
				sock,
				asio::get_server_address(opt.server_address, opt.socks5_option),
				asio::get_server_port(opt.server_port, opt.socks5_option),
				asio::default_tcp_socket_option_setter{ opt.socket_option },
				asio::use_nothrow_deferred);
			if (e1)
				co_return e1;

			if (socks5::is_option_valid(opt.socks5_option))
			{
				if (opt.socks5_option.dest_address.empty())
					opt.socks5_option.dest_address = opt.server_address;
				if (opt.socks5_option.dest_port == 0)
					opt.socks5_option.dest_port = opt.server_port;

				auto [e2] = co_await socks5::async_handshake(
					sock, opt.socks5_option, asio::use_nothrow_deferred);
				if (e2)
					co_return e2;
			}

			co_return asio::error_code{};
		}
	};
}

namespace asio
{
	template<typename AsyncStream, typename StartOption,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) StartToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires std::same_as<typename std::remove_cvref_t<AsyncStream>::protocol_type, asio::ip::tcp>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code))
	async_start(
		AsyncStream& sock,
		StartOption& option,
		StartToken&& token
		ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<StartToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_start_op{}, sock),
			token, std::ref(sock), std::ref(option));
	}
}
