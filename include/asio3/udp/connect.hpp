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
#include <asio3/core/with_lock.hpp>
#include <asio3/udp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct udp_async_connect_op
	{
		auto operator()(
			auto state, auto sock_ref,
			auto&& server_address, auto&& server_port, auto&& cb_set_option) -> void
		{
			auto& sock = sock_ref.get();

			auto fn_set_option = std::forward_like<decltype(cb_set_option)>(cb_set_option);

			std::string addr = asio::to_string(std::forward_like<decltype(server_address)>(server_address));
			std::string port = asio::to_string(std::forward_like<decltype(server_port)>(server_port));

			using stream_type = std::remove_cvref_t<decltype(sock)>;
			using endpoint_type = typename stream_type::protocol_type::endpoint;
			using resolver_type = typename stream_type::protocol_type::resolver;

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			resolver_type resolver(asio::detail::get_lowest_executor(sock));

			// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
			auto [e1, eps] = co_await asio::async_resolve(
				resolver, std::move(addr), std::move(port),
				asio::ip::resolver_base::flags(), asio::use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			if (sock.is_open())
			{
				for (auto&& ep : eps)
				{
					auto [e2] = co_await sock.async_connect(ep, use_nothrow_deferred);
					if (!e2)
						co_return{ e2, ep.endpoint() };

					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, endpoint_type{} };
				}
			}
			else
			{
				asio::error_code ec{};

				for (auto&& ep : eps)
				{
					stream_type tmp(sock.get_executor());

					tmp.open(ep.endpoint().protocol(), ec);
					if (ec)
						continue;

					// you can use the option callback to set the bind address and port
					fn_set_option(tmp);

					auto [e2] = co_await tmp.async_connect(ep, use_nothrow_deferred);
					if (!e2)
					{
						sock = std::move(tmp);
						co_return{ e2, ep.endpoint() };
					}
				}
			}

			co_return{ asio::error::connection_refused, endpoint_type{} };
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
	 * @brief Asynchronously establishes a socket session by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::udp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename ConnectToken = asio::default_token_type<AsyncStream>>
	requires (is_udp_socket<AsyncStream>)
	inline auto async_connectex(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::udp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::udp::endpoint)>(
				detail::udp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			asio::default_udp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously establishes a socket session by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
	 * @param cb_set_option - The callback to set the socket options.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::udp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename SetOptionFn,
		typename ConnectToken = asio::default_token_type<AsyncStream>>
	requires (is_udp_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_connectex(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		SetOptionFn&& cb_set_option,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::udp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::udp::endpoint)>(
				detail::udp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			std::forward<SetOptionFn>(cb_set_option));
	}
}
