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

#include <asio3/core/timer.hpp>
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct tcp_async_disconnect_op
	{
		auto operator()(auto state, auto sock_ref, timeout_duration timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			if (!sock.is_open())
				co_return asio::error::operation_aborted;

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			asio::error_code ec{};

			asio::socket_base::linger linger{};

			sock.get_option(linger, ec);

			if (timeout > timeout_duration::zero() && !ec && !(linger.enabled() == true && linger.timeout() == 0))
			{
				sock.shutdown(asio::socket_base::shutdown_send, ec);

				if (!ec)
				{
					[[maybe_unused]] detail::call_func_when_timeout wt(sock.get_executor(), timeout,
					[&sock]() mutable
					{
						error_code ec{};
						sock.close(ec);
					});

					co_await sock.async_wait(asio::socket_base::wait_error, use_nothrow_deferred);

					sock.close(ec);
				}
				else
				{
					sock.shutdown(asio::socket_base::shutdown_receive, ec);
					sock.close(ec);
				}
			}
			else
			{
				sock.shutdown(asio::socket_base::shutdown_both, ec);
				sock.close(ec);
			}

			co_return ec;
		}
	};

	struct tcp_async_disconnect_all_op
	{
		auto operator()(auto state, auto connection_map_ref, auto&& pred, timeout_duration timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& connection_map = connection_map_ref.get();
			auto fun = std::forward_like<decltype(pred)>(pred);

			using connection_map_type = std::remove_cvref_t<decltype(connection_map)>;
			using connection_type = typename connection_map_type::value_type;

			co_await asio::dispatch(connection_map.get_executor(), asio::use_nothrow_deferred);

			co_await connection_map.async_for_each(
				[timeout, &fun](connection_type& conn) mutable -> asio::awaitable<void>
				{
					if (fun(conn))
					{
						co_await conn->async_disconnect(timeout, use_nothrow_deferred);
					}
				}, use_nothrow_deferred);

			co_return error_code{};
		}
	};
}

namespace asio
{
	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		typename DisconnectToken = asio::default_token_type<AsyncStream>>
	requires is_tcp_socket<AsyncStream>
	inline auto async_disconnect(
		AsyncStream& sock,
		DisconnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_op{}, sock),
			token, std::ref(sock), asio::tcp_disconnect_timeout);
	}

	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		typename DisconnectToken = asio::default_token_type<AsyncStream>>
	requires is_tcp_socket<AsyncStream>
	inline auto async_disconnect(
		AsyncStream& sock,
		timeout_duration timeout,
		DisconnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_op{}, sock),
			token, std::ref(sock), timeout);
	}

	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename ConnectionMapT,
		typename DisconnectToken = asio::default_token_type<ConnectionMapT>>
	inline auto async_disconnect_all(
		ConnectionMapT& connection_map,
		DisconnectToken&& token = asio::default_token_type<ConnectionMapT>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_all_op{}, connection_map),
			token, std::ref(connection_map), [](auto&) { return true; },
			asio::tcp_disconnect_timeout);
	}

	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename ConnectionMapT,
		typename DisconnectToken = asio::default_token_type<ConnectionMapT>>
	inline auto async_disconnect_all(
		ConnectionMapT& connection_map,
		timeout_duration timeout,
		DisconnectToken&& token = asio::default_token_type<ConnectionMapT>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_all_op{}, connection_map),
			token, std::ref(connection_map), [](auto&) { return true; }, timeout);
	}

	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename ConnectionMapT,
		typename DisconnectToken = asio::default_token_type<ConnectionMapT>>
	inline auto async_disconnect_selected(
		ConnectionMapT& connection_map,
		auto&& pred,
		DisconnectToken&& token = asio::default_token_type<ConnectionMapT>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_all_op{}, connection_map),
			token, std::ref(connection_map),
			std::forward_like<decltype(pred)>(pred), asio::tcp_disconnect_timeout);
	}

	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename ConnectionMapT,
		typename DisconnectToken = asio::default_token_type<ConnectionMapT>>
	inline auto async_disconnect_selected(
		ConnectionMapT& connection_map,
		timeout_duration timeout,
		auto&& pred,
		DisconnectToken&& token = asio::default_token_type<ConnectionMapT>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_all_op{}, connection_map),
			token, std::ref(connection_map),
			std::forward_like<decltype(pred)>(pred), timeout);
	}
}
