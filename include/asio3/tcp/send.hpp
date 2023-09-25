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
		template<typename AsyncWriteStream, typename Data>
		auto operator()(auto state, std::reference_wrapper<AsyncWriteStream> stream_ref, Data&& data) -> void
		{
			auto& s = stream_ref.get();

			Data msg = std::forward<Data>(data);

			co_await asio::dispatch(s.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			if constexpr (has_member_channel_lock<std::remove_cvref_t<AsyncWriteStream>>)
			{
				auto& lock = s.get_executor().lock;

				if (!lock->try_send())
				{
					co_await lock->async_send(asio::deferred);
				}
			}

			auto [e1, n1] = co_await asio::async_write(s, asio::buffer(msg), asio::use_nothrow_deferred);

			if constexpr (has_member_channel_lock<std::remove_cvref_t<AsyncWriteStream>>)
			{
				auto& lock = s.get_executor().lock;

				lock->try_receive([](auto...) {});
			}

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
	template<typename AsyncWriteStream, typename Data,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::size_t)) WriteToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncWriteStream::executor_type)>
	requires detail::is_template_instance_of<asio::basic_stream_socket, std::remove_cvref_t<AsyncWriteStream>>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken, void(asio::error_code, std::size_t))
	async_send(
		AsyncWriteStream& s,
		Data&& data,
		WriteToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncWriteStream::executor_type))
	{
		return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::tcp_async_send_op{}, s),
			token, std::ref(s), detail::data_persist(std::forward<Data>(data)));
	}
}
