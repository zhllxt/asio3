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
#include <asio3/core/with_lock.hpp>
#include <asio3/tcp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct tcp_async_disconnect_op
	{
		auto operator()(auto state, auto sock_ref, std::chrono::steady_clock::duration disconnect_timeout) -> void
		{
			auto& sock = sock_ref.get();

			co_await asio::dispatch(asio::detail::get_lowest_executor(sock), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			if (!sock.is_open())
				co_return asio::error::operation_aborted;

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			// release lock immediately, otherwise, the following situation maybe occur:
			// 1. the async wait is called(see below), and wait for timeout.
			// 2. then the async send is called, and require the lock, but the lock is 
			// holded at here, so the async send will wait for the lock forever.
			// 3. until disconnect timed out, the async wait returned, and release lock.
			// 4. then the async send get the lock.
			// so, this will take 30 seconds default to exit the application.
			{
				[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };
			}

			asio::error_code ec{};

			asio::socket_base::linger linger{};

			sock.get_option(linger, ec);

			if (disconnect_timeout > std::chrono::steady_clock::duration::zero() &&
				!ec &&
				!(linger.enabled() == true && linger.timeout() == 0))
			{
				sock.shutdown(asio::socket_base::shutdown_send, ec);

				if (!ec)
				{
					[[maybe_unused]] detail::call_func_when_timeout wt(
						asio::detail::get_lowest_executor(sock), disconnect_timeout,
						[&sock]() mutable
						{
							error_code ec{};
							sock.close(ec);
							asio::reset_lock(sock);
						});

					// https://github.com/chriskohlhoff/asio/issues/715
					// when use wait_error like below:
					// auto result = co_await
					// (
					// 	async_check_error(sock, use_nothrow_awaitable) ||
					// 	async_check_idle(sock, alive_time, idle_timeout, use_nothrow_awaitable)
					// );
					// even if the async_check_idle is returned, the async_check_error won't returned.
					co_await sock.async_wait(asio::socket_base::wait_error, use_nothrow_deferred);

					sock.close(ec);
					asio::reset_lock(sock);
				}
				else
				{
					sock.shutdown(asio::socket_base::shutdown_receive, ec);
					sock.close(ec);
					asio::reset_lock(sock);
				}
			}
			else
			{
				sock.shutdown(asio::socket_base::shutdown_both, ec);
				sock.close(ec);
				asio::reset_lock(sock);
			}

			co_return ec;
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
	 * @param disconnect_timeout - The disconnect timeout.
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
		std::chrono::steady_clock::duration disconnect_timeout,
		DisconnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::tcp_async_disconnect_op{}, sock),
			token, std::ref(sock), disconnect_timeout);
	}
}
