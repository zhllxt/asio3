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
#include <asio3/tcp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
template <typename AsyncReadStream, typename MutableBufferSequence>
auto async_read(AsyncReadStream& s, const MutableBufferSequence& buffers, detail::transfer_all_t completion_condition)
{
	return async_read(s, buffers, std::move(completion_condition),
		asio::default_completion_token<typename AsyncReadStream::executor_type>::type());
}

template <typename AsyncReadStream, typename Allocator>
auto async_read(AsyncReadStream& s, basic_streambuf<Allocator>& b, detail::transfer_all_t completion_condition)
{
	return async_read(s, b, std::move(completion_condition),
		asio::default_completion_token<typename AsyncReadStream::executor_type>::type());
}

template <typename AsyncReadStream, typename MutableBufferSequence>
auto async_read(AsyncReadStream& s, const MutableBufferSequence& buffers, detail::transfer_at_least_t completion_condition)
{
	return async_read(s, buffers, std::move(completion_condition),
		asio::default_completion_token<typename AsyncReadStream::executor_type>::type());
}

template <typename AsyncReadStream, typename Allocator>
auto async_read(AsyncReadStream& s, basic_streambuf<Allocator>& b, detail::transfer_at_least_t completion_condition)
{
	return async_read(s, b, std::move(completion_condition),
		asio::default_completion_token<typename AsyncReadStream::executor_type>::type());
}

template <typename AsyncReadStream, typename MutableBufferSequence>
auto async_read(AsyncReadStream& s, const MutableBufferSequence& buffers, detail::transfer_exactly_t completion_condition)
{
	return async_read(s, buffers, std::move(completion_condition),
		asio::default_completion_token<typename AsyncReadStream::executor_type>::type());
}

template <typename AsyncReadStream, typename Allocator>
auto async_read(AsyncReadStream& s, basic_streambuf<Allocator>& b, detail::transfer_exactly_t completion_condition)
{
	return async_read(s, b, std::move(completion_condition),
		asio::default_completion_token<typename AsyncReadStream::executor_type>::type());
}

/**
 * @brief Start an asynchronous read.
 * This function is used to asynchronously read data from the stream socket.
 * socket. It is an initiating function for an @ref asynchronous_operation,
 * and always returns immediately.
 *
 * @param buffers One or more buffers into which the data will be read.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the completion handler is called.
 *
 * @param token The @ref completion_token that will be used to produce a
 * completion handler, which will be called when the read completes.
 * Potential completion tokens include @ref use_future, @ref use_awaitable,
 * @ref yield_context, or a function object with the correct completion
 * signature. The function signature of the completion handler must be:
 * @code void handler(
 *   const asio::error_code& error, // Result of operation.
 *   std::size_t bytes_transferred // Number of bytes read.
 * ); @endcode
 */
template <
	typename AsyncReadStream,
	typename MutableBufferSequence,
	typename ReadToken = asio::default_token_type<AsyncReadStream>>
inline auto async_read_some(AsyncReadStream& s, const MutableBufferSequence& buffers,
	ReadToken&& token = asio::default_token_type<AsyncReadStream>())
{
	return s.async_read_some(buffers, std::forward<ReadToken>(token));
}
}
