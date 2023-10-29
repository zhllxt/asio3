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

#include <concepts>

#include <asio3/core/asio.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/strutil.hpp>
#include <asio3/core/data_persist.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/udp/core.hpp>
#include <asio3/proxy/udp_header.hpp>

namespace asio::detail
{
	struct udp_async_send_op
	{
		auto operator()(auto state, auto sock_ref, auto&& data) -> void
		{
			auto& sock = sock_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e1, n1] = co_await sock.async_send(asio::buffer(msg), asio::use_nothrow_deferred);

			co_return{ e1, n1 };
		}
	};
}

namespace asio
{
	/**
	 * @brief Start an asynchronous operation to write all of the supplied data to a stream.
	 * @param s - The stream to which the data is to be written.
	 * @param data - The written data.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
	 */
	template<
		typename AsyncWriteStream,
		typename WriteToken = asio::default_token_type<AsyncWriteStream>>
	requires is_basic_datagram_socket<AsyncWriteStream>
	inline auto async_send(
		AsyncWriteStream& s,
		auto&& data,
		WriteToken&& token = asio::default_token_type<AsyncWriteStream>())
	{
		return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::udp_async_send_op{}, s),
			token, std::ref(s),
			detail::data_persist(std::forward_like<decltype(data)>(data)));
	}
}
