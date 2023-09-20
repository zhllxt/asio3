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

#include <asio3/tcp/accept.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/socks5/handshake.hpp>

namespace asio::detail
{
	struct tcp_client_async_start_op
	{
		template<typename AsyncStream, typename ClientOption>
		auto operator()(auto state,
			std::reference_wrapper<AsyncStream> sock_ref,
			std::reference_wrapper<ClientOption> option_ref) -> void
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

	struct tcp_acceptor_async_start_op
	{
		template<typename AsyncAcceptor, typename AcceptorOption>
		auto operator()(auto state,
			std::reference_wrapper<AsyncAcceptor> acceptor_ref,
			std::reference_wrapper<AcceptorOption> option_ref) -> void
		{
			auto& acceptor = acceptor_ref.get();
			auto& opt = option_ref.get();

			co_await asio::dispatch(acceptor.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [e1, a1] = co_await asio::async_create_acceptor(acceptor.get_executor(),
				opt.listen_address, opt.listen_port, opt.socket_option.reuse_address, asio::use_nothrow_deferred);
			if (e1)
				co_return e1;

			acceptor = std::move(a1);

			while (acceptor.is_open())
			{
				auto [e2, client] = co_await acceptor.async_accept(asio::use_nothrow_deferred);
				if (e2)
				{
					co_await asio::async_sleep(acceptor.get_executor(),
						std::chrono::milliseconds(100), asio::use_nothrow_deferred);
				}
				else
				{
					asio::co_spawn(acceptor.get_executor(), opt.accept_function(std::move(client)), asio::detached);
				}
			}

			co_return e1;
		}
	};
}

namespace asio
{
	template<typename AsyncStream, typename ClientOption,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) StartToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires detail::is_template_instance_of<asio::basic_stream_socket, typename std::remove_cvref_t<AsyncStream>>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code))
	async_start(
		AsyncStream& sock,
		ClientOption& option,
		StartToken&& token
		ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<StartToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_client_async_start_op{}, sock),
			token, std::ref(sock), std::ref(option));
	}

	template<typename AsyncAcceptor, typename AcceptorOption,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) StartToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncAcceptor::executor_type)>
	requires detail::is_template_instance_of<asio::basic_socket_acceptor, typename std::remove_cvref_t<AsyncAcceptor>>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code))
	async_start(
		AsyncAcceptor& acceptor,
		AcceptorOption& option,
		StartToken&& token
		ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncAcceptor::executor_type))
	{
		return asio::async_initiate<StartToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_acceptor_async_start_op{}, acceptor),
			token, std::ref(acceptor), std::ref(option));
	}
}
