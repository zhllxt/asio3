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
#include <asio3/udp/core.hpp>

namespace asio::detail
{
	struct udp_async_disconnect_op
	{
		template<typename AsyncStream>
		auto operator()(
			auto state, std::reference_wrapper<AsyncStream> sock_ref, timeout_duration) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			if (!sock.is_open())
				co_return asio::error::operation_aborted;

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::unlock_guard ug{ sock };

			asio::error_code ec{};
			sock.shutdown(asio::socket_base::shutdown_both, ec);
			sock.close(ec);

			co_return ec;
		}
	};
}

namespace asio
{
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
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) DisconnectToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires std::same_as<typename std::remove_cvref_t<AsyncStream>::protocol_type, asio::ip::udp>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(DisconnectToken, void(asio::error_code))
	async_disconnect(
		AsyncStream& sock,
		timeout_duration timeout = std::chrono::steady_clock::duration::zero(),
		DisconnectToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::udp_async_disconnect_op{}, sock),
			token, std::ref(sock), timeout);
	}
}
