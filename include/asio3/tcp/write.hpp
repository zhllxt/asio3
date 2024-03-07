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
#include <asio3/core/netutil.hpp>
#include <asio3/core/with_lock.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/core/data_persist.hpp>
#include <asio3/tcp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct tcp_async_send_op
	{
		auto operator()(auto state, auto sock_ref, auto&& data) -> void
		{
			auto& sock = sock_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);

			co_await asio::dispatch(asio::detail::get_lowest_executor(sock), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e1, n1] = co_await asio::async_write(sock, asio::to_buffer(msg), asio::use_nothrow_deferred);

			co_return{ e1, n1 };
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
template <typename AsyncWriteStream, typename ConstBufferSequence>
auto async_write(AsyncWriteStream& s, const ConstBufferSequence& buffers, detail::transfer_all_t completion_condition)
{
	return async_write(s, buffers, std::move(completion_condition),
		asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename Allocator>
auto async_write(AsyncWriteStream& s, basic_streambuf<Allocator>& b, detail::transfer_all_t completion_condition)
{
	return async_write(s, b, std::move(completion_condition),
		asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename ConstBufferSequence>
auto async_write(AsyncWriteStream& s, const ConstBufferSequence& buffers, detail::transfer_at_least_t completion_condition)
{
	return async_write(s, buffers, std::move(completion_condition),
		asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename Allocator>
auto async_write(AsyncWriteStream& s, basic_streambuf<Allocator>& b, detail::transfer_at_least_t completion_condition)
{
	return async_write(s, b, std::move(completion_condition),
		asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename ConstBufferSequence>
auto async_write(AsyncWriteStream& s, const ConstBufferSequence& buffers, detail::transfer_exactly_t completion_condition)
{
	return async_write(s, buffers, std::move(completion_condition),
		asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename Allocator>
auto async_write(AsyncWriteStream& s, basic_streambuf<Allocator>& b, detail::transfer_exactly_t completion_condition)
{
	return async_write(s, b, std::move(completion_condition),
		asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

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
requires is_tcp_socket<AsyncStream>
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
		detail::data_persist(std::forward_like<decltype(data)>(data)));
}

}
