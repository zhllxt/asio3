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
#include <asio3/core/beast.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/core/data_persist.hpp>

namespace asio::detail
{
	struct websocket_stream_async_send_op
	{
		auto operator()(auto state, auto ws_stream_ref, auto&& data) -> void
		{
			auto& ws_stream = ws_stream_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);

			co_await asio::dispatch(ws_stream.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(ws_stream, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ ws_stream };

			auto [e1, n1] = co_await ws_stream.async_write(asio::to_buffer(msg), asio::use_nothrow_deferred);

			co_return{ e1, n1 };
		}
	};
}

namespace asio
{
/**
 * @brief Start an asynchronous operation to write all of the supplied data to a stream.
 * @param stream - The websocket stream to which the data is to be written.
 * @param data - The written data.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
 */
template<
	typename AsyncStream,
	typename SendToken = asio::default_token_type<AsyncStream>>
inline auto async_send(
	websocket::stream<AsyncStream>& stream,
	auto&& data,
	SendToken&& token = asio::default_token_type<AsyncStream>())
{
	return async_initiate<SendToken, void(asio::error_code, std::size_t)>(
		experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::websocket_stream_async_send_op{}, stream),
		token,
		std::ref(stream),
		detail::data_persist(std::forward_like<decltype(data)>(data)));
}

}
