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

#include <asio3/core/asio.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/strutil.hpp>
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct tcp_async_start_op
	{
		template<typename AsyncStream, typename String, typename StrOrInt>
		auto operator()(
			auto state, std::reference_wrapper<AsyncStream> sock_ref,
			String&& listen_address, StrOrInt&& listen_port, bool reuse_addr) -> void
		{
			using endpoint_type = typename AsyncStream::protocol_type::endpoint;
			using resolver_type = typename AsyncStream::protocol_type::resolver;

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			std::string addr = asio::to_string(std::forward<String>(listen_address));
			std::string port = asio::to_string(std::forward<StrOrInt>(listen_port));

			co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

			resolver_type resolver(sock.get_executor());

			auto [e1, eps] = co_await resolver.async_resolve(
				addr, port, asio::ip::resolver_base::passive, use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			try
			{
				endpoint_type bnd_endpoint = (*eps).endpoint();

				AsyncStream tmp_sock(sock.get_executor(), bnd_endpoint, reuse_addr);

				sock = std::move(tmp_sock);

				co_return{ asio::error_code{}, bnd_endpoint };
			}
			catch (const asio::system_error& e)
			{
				co_return{ e.code(), endpoint_type{} };
			}
		}
	};
}

namespace asio
{
	/**
	 * @brief Asynchronously start a tcp acceptor for listen at the bind address and port.
	 * @param sock - The socket reference to be started.
	 * @param listen_address - The listen ip. 
	 * @param listen_port - The listen port. 
     * @param reuse_addr  - Whether set the socket option socket_base::reuse_address.
	 * @param token - The completion handler to invoke when the operation completes.
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename String, typename StrOrInt,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::ip::tcp::endpoint)) StartToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires
		(detail::is_template_instance_of<asio::basic_socket_acceptor, std::remove_cvref_t<AsyncStream>> &&
		(std::constructible_from<std::string, String>) &&
		(std::constructible_from<std::string, StrOrInt> || std::integral<std::remove_cvref_t<StrOrInt>>))
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code, asio::ip::tcp::endpoint))
	async_start(
		AsyncStream& sock,
		String&& listen_address, StrOrInt&& listen_port,
		bool reuse_addr = true,
		StartToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return async_initiate<StartToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_start_op{}, sock),
			token, std::ref(sock),
			std::forward<String>(listen_address), std::forward<StrOrInt>(listen_port), reuse_addr);
	}
}
