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
#include <asio3/core/resolve.hpp>
#include <asio3/udp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct udp_async_open_op
	{
		auto operator()(
			auto state, auto sock_ref,
			auto&& listen_address, auto&& listen_port, auto&& cb_set_option) -> void
		{
			auto& sock = sock_ref.get();

			auto fn_set_option = std::forward_like<decltype(cb_set_option)>(cb_set_option);

			std::string addr = asio::to_string(std::forward_like<decltype(listen_address)>(listen_address));
			std::string port = asio::to_string(std::forward_like<decltype(listen_port)>(listen_port));

			using stream_type = std::remove_cvref_t<decltype(sock)>;
			using endpoint_type = typename stream_type::protocol_type::endpoint;
			using resolver_type = typename stream_type::protocol_type::resolver;

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			resolver_type resolver(sock.get_executor());

			auto [e1, eps] = co_await asio::async_resolve(
				resolver, std::move(addr), std::move(port),
				asio::ip::resolver_base::passive, asio::use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			endpoint_type bnd_endpoint = (*eps).endpoint();

			asio::error_code ec;

			stream_type tmp(sock.get_executor());

			tmp.open(bnd_endpoint.protocol(), ec);
			if (ec)
				co_return {ec, endpoint_type{} };

			fn_set_option(tmp);

			tmp.bind(bnd_endpoint, ec);
			if (ec)
				co_return {ec, endpoint_type{} };

			sock = std::move(tmp);

			co_return{ error_code{}, bnd_endpoint };
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
	 * @brief Asynchronously start a udp socket at the bind address and port.
	 * @param sock - The socket reference to be started.
	 * @param listen_address - The listen address. 
	 * @param listen_port - The listen port. 
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::udp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename OpenToken = asio::default_token_type<AsyncStream>>
	requires (is_udp_socket<AsyncStream>)
	inline auto async_open(
		AsyncStream& sock,
		is_string auto&& listen_address, is_string_or_integral auto&& listen_port,
		OpenToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<OpenToken, void(asio::error_code, asio::ip::udp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::udp::endpoint)>(
				detail::udp_async_open_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(listen_address)>(listen_address),
			std::forward_like<decltype(listen_port)>(listen_port),
			asio::default_udp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously start a udp socket at the bind address and port.
	 * @param sock - The socket reference to be started.
	 * @param listen_address - The listen address. 
	 * @param listen_port - The listen port. 
	 * @param cb_set_option - The callback to set the socket options.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::udp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename SetOptionFn,
		typename OpenToken = asio::default_token_type<AsyncStream>>
	requires (is_udp_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_open(
		AsyncStream& sock,
		is_string auto&& listen_address, is_string_or_integral auto&& listen_port,
		SetOptionFn&& cb_set_option,
		OpenToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<OpenToken, void(asio::error_code, asio::ip::udp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::udp::endpoint)>(
				detail::udp_async_open_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(listen_address)>(listen_address),
			std::forward_like<decltype(listen_port)>(listen_port),
			std::forward<SetOptionFn>(cb_set_option));
	}
}
