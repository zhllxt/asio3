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
#include <asio3/udp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct udp_async_disconnect_op
	{
		auto operator()(auto state, auto sock_ref) -> void
		{
			auto& sock = sock_ref.get();

			co_await asio::dispatch(asio::detail::get_lowest_executor(sock), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			if (!sock.is_open())
				co_return asio::error::operation_aborted;

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			asio::error_code ec{};
			sock.shutdown(asio::socket_base::shutdown_both, ec);
			sock.close(ec);
			asio::reset_lock(sock);

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
	 * @brief Asynchronously graceful disconnect the socket session.
	 * @param sock - The socket reference to be connected.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		typename DisconnectToken = asio::default_token_type<AsyncStream>>
	requires (is_udp_socket<AsyncStream>)
	inline auto async_disconnect(
		AsyncStream& sock,
		DisconnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<DisconnectToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::udp_async_disconnect_op{}, sock),
			token, std::ref(sock));
	}
}
