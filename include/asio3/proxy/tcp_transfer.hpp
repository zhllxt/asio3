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
#include <asio3/core/netconcepts.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct tcp_async_transfer_op
	{
		auto operator()(auto state, auto from_ref, auto to_ref, auto buffer) -> void
		{
			auto& from = from_ref.get();
			auto& to = to_ref.get();

			co_await asio::dispatch(from.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [e1, n1] = co_await asio::async_read_some(from, buffer);
			if (e1)
				co_return{ e1, 0 };

			auto [e2, n2] = co_await asio::async_write(to, asio::buffer(buffer.data(), n1));
			if (e2)
				co_return{ e2, n1 };

			co_return{ error_code{}, n1 };
		}
	};
}

namespace asio
{
	/**
	 * @brief Start an asynchronous operation to transfer data between front and back.
	 * @param from - The front tcp client.
	 * @param to - The back tcp client.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t);
	 */
	template<
		typename TcpAsyncStream,
		typename TransferToken = asio::default_token_type<TcpAsyncStream>>
	requires is_basic_stream_socket<TcpAsyncStream>
	inline auto async_transfer(
		TcpAsyncStream& from,
		TcpAsyncStream& to,
		auto&& buffer,
		TransferToken&& token = asio::default_token_type<TcpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::tcp_async_transfer_op{}, from),
			token,
			std::ref(from), std::ref(to),
			std::forward_like<decltype(buffer)>(buffer));
	}
}
