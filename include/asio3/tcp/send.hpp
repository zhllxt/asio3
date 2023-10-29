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
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct tcp_async_send_op
	{
		auto operator()(auto state, auto sock_ref, auto&& data) -> void
		{
			auto& sock = sock_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e1, n1] = co_await asio::async_write(sock, asio::buffer(msg), asio::use_nothrow_deferred);

			co_return{ e1, n1 };
		}
	};

	struct tcp_async_send_all_op
	{
		template<typename ConnectionType>
		static asio::awaitable<void> do_send(asio::const_buffer msgbuf, std::size_t& total, ConnectionType conn)
		{
			auto [e1, n1] = co_await conn->async_send(msgbuf, use_nothrow_deferred);
			total += n1;
		}

		auto operator()(auto state, auto connection_map_ref, auto&& data, auto&& pred) -> void
		{
			auto& connection_map = connection_map_ref.get();

			using connection_map_type = std::remove_cvref_t<decltype(connection_map)>;
			using connection_type = typename connection_map_type::value_type;

			auto msg = std::forward_like<decltype(data)>(data);
			auto fun = std::forward_like<decltype(pred)>(pred);

			co_await asio::dispatch(connection_map.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::const_buffer msgbuf{ asio::buffer(msg) };

			std::size_t total = 0;

			co_await connection_map.async_for_each(
				[msgbuf, &fun, &total](connection_type& conn) mutable -> asio::awaitable<void>
				{
					if (fun(conn))
					{
						auto [e1, n1] = co_await conn->async_send(msgbuf, use_nothrow_deferred);
						total += n1;
					}
				}, use_nothrow_deferred);

			//asio::awaitable<void> awaiter;
			//awaiter = (awaiter && do_send(msgbuf, total, conn));
			//co_await (awaiter);

			co_return{ error_code{}, total };
		}
	};
}

namespace asio
{
	/**
	 * @brief Start an asynchronous operation to write all of the supplied data to a stream.
	 * @param sock - The stream to which the data is to be written.
	 * @param data - The written data.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
	 */
	template<
		typename AsyncStream,
		typename SendToken = asio::default_token_type<AsyncStream>>
	requires is_basic_stream_socket<AsyncStream>
	inline auto async_send(
		AsyncStream& sock,
		auto&& data,
		SendToken&& token = asio::default_token_type<AsyncStream>())
	{
		return async_initiate<SendToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::tcp_async_send_op{}, sock),
			token,
			std::ref(sock),
			detail::data_persist(std::forward_like<decltype(data)>(data))
		);
	}

	/**
	 * @brief Start an asynchronous operation to write all of the supplied data to all streams in the map.
	 * @param sock - The stream to which the data is to be written.
	 * @param data - The written data.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
	 */
	template<
		typename ConnectionMapT,
		typename SendToken = asio::default_token_type<ConnectionMapT>>
	inline auto async_send_all(
		ConnectionMapT& connection_map,
		auto&& data,
		SendToken&& token = asio::default_token_type<ConnectionMapT>())
	{
		return async_initiate<SendToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::tcp_async_send_all_op{}, connection_map),
			token,
			std::ref(connection_map),
			detail::data_persist(std::forward_like<decltype(data)>(data)),
			[](auto&) { return true; }
		);
	}

	/**
	 * @brief Start an asynchronous operation to write all of the supplied data to all streams in the map.
	 * @param sock - The stream to which the data is to be written.
	 * @param data - The written data.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
	 */
	template<
		typename ConnectionMapT,
		typename SendToken = asio::default_token_type<ConnectionMapT>>
	inline auto async_send_selected(
		ConnectionMapT& connection_map,
		auto&& data,
		auto&& pred,
		SendToken&& token = asio::default_token_type<ConnectionMapT>())
	{
		return async_initiate<SendToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::tcp_async_send_all_op{}, connection_map),
			token,
			std::ref(connection_map),
			detail::data_persist(std::forward_like<decltype(data)>(data)),
			std::forward_like<decltype(pred)>(pred)
		);
	}
}
