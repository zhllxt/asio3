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
	struct async_disconnect_op
	{
		template<typename AsyncStream>
		auto operator()(
			auto state, std::reference_wrapper<AsyncStream> sock_ref, timeout_duration timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			if (!sock.is_open())
				co_return asio::error::operation_aborted;

			if constexpr (has_member_channel_lock<std::remove_cvref_t<AsyncStream>>)
			{
				auto& lock = sock.get_executor().lock;

				if (!lock->try_send())
				{
					co_await lock->async_send(asio::deferred);
				}
			}

			asio::error_code ec{};

			asio::socket_base::linger linger{};

			sock.get_option(linger, ec);

			if (!ec && !(linger.enabled() == true && linger.timeout() == 0))
			{
				sock.shutdown(asio::socket_base::shutdown_send, ec);

				if (!ec)
				{
					asio::timeout_guard tg(sock, timeout);

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

			if constexpr (has_member_channel_lock<std::remove_cvref_t<AsyncStream>>)
			{
				auto& lock = sock.get_executor().lock;

				lock->try_receive([](auto...) {});
			}

			co_return ec;
		}
	};
}

namespace asio
{
	/**
	 * @brief Asynchronously graceful disconnect the socket connection.
	 * @param sock - The socket reference to be connected. The type can't be asio::ip::tcp:socket
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) DisconnectToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires std::same_as<typename std::remove_cvref_t<AsyncStream>::protocol_type, asio::ip::tcp>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(DisconnectToken, void(asio::error_code))
	async_disconnect(
		AsyncStream& sock,
		timeout_duration timeout,
		DisconnectToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_disconnect_op{}, sock),
			token, std::ref(sock), timeout);
	}
}
