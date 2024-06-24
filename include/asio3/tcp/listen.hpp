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
#include <asio3/core/netutil.hpp>
#include <asio3/core/strutil.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/resolve.hpp>
#include <asio3/tcp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct tcp_async_listen_op
	{
		auto operator()(
			auto state, auto acceptor_ref,
			auto&& listen_address, auto&& listen_port, bool reuse_addr) -> void
		{
			auto& acceptor = acceptor_ref.get();

			using acceptor_type = std::remove_cvref_t<decltype(acceptor)>;
			using endpoint_type = typename acceptor_type::protocol_type::endpoint;
			using resolver_type = typename acceptor_type::protocol_type::resolver;

			std::string addr = asio::to_string(std::forward_like<decltype(listen_address)>(listen_address));
			std::string port = asio::to_string(std::forward_like<decltype(listen_port)>(listen_port));

			co_await asio::dispatch(acceptor.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			resolver_type resolver(acceptor.get_executor());

			auto [e1, eps] = co_await asio::async_resolve(
				resolver, std::move(addr), std::move(port),
				asio::ip::resolver_base::passive, use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			try
			{
				endpoint_type bnd_endpoint = (*eps).endpoint();

				acceptor_type tmp(acceptor.get_executor(), bnd_endpoint, reuse_addr);

				acceptor = std::move(tmp);

				co_return{ asio::error_code{}, bnd_endpoint };
			}
			catch (const asio::system_error& e)
			{
				co_return{ e.code(), endpoint_type{} };
			}
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 * @brief Asynchronously start a tcp acceptor for listen at the address and port.
	 * @param acceptor - The acceptor reference to be started.
	 * @param listen_address - The listen ip. 
	 * @param listen_port - The listen port. 
     * @param reuse_addr  - Whether set the socket option socket_base::reuse_address.
	 * @param token - The completion handler to invoke when the operation completes.
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncAcceptor,
		typename ListenToken = asio::default_token_type<AsyncAcceptor>>
	requires is_basic_socket_acceptor<AsyncAcceptor>
	inline auto async_listen(
		AsyncAcceptor& acceptor,
		is_string auto&& listen_address,
		is_string_or_integral auto&& listen_port,
		bool reuse_addr = true,
		ListenToken&& token = asio::default_token_type<AsyncAcceptor>())
	{
		return async_initiate<ListenToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_listen_op{}, acceptor),
			token,
			std::ref(acceptor),
			std::forward_like<decltype(listen_address)>(listen_address),
			std::forward_like<decltype(listen_port)>(listen_port),
			reuse_addr);
	}
}
