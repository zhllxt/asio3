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
#include <asio3/udp/core.hpp>
#include <asio3/proxy/parser.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
/**
 * @brief Start an asynchronous receive.
 * This function is used to asynchronously receive a datagram. It is an
 * initiating function for an @ref asynchronous_operation, and always returns
 * immediately.
 *
 * @param buffers One or more buffers into which the data will be received.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the completion handler is called.
 *
 * @param sender_endpoint An endpoint object that receives the endpoint of
 * the remote sender of the datagram. Ownership of the sender_endpoint object
 * is retained by the caller, which must guarantee that it is valid until the
 * completion handler is called.
 *
 * @param token The @ref completion_token that will be used to produce a
 * completion handler, which will be called when the receive completes.
 * Potential completion tokens include @ref use_future, @ref use_awaitable,
 * @ref yield_context, or a function object with the correct completion
 * signature. The function signature of the completion handler must be:
 * @code void handler(
 *   const asio::error_code& error, // Result of operation.
 *   std::size_t bytes_transferred // Number of bytes received.
 * ); @endcode
 */
template <
	typename AsyncReadStream,
	typename MutableBufferSequence,
	typename ReadToken = asio::default_token_type<AsyncReadStream>>
inline auto async_receive_from(AsyncReadStream& s, const MutableBufferSequence& buffers,
	typename AsyncReadStream::endpoint_type& sender_endpoint,
	ReadToken&& token = asio::default_token_type<AsyncReadStream>())
{
	return s.async_receive_from(buffers, sender_endpoint, std::forward<ReadToken>(token));
}

/**
 * @brief Start an asynchronous receive on a connected socket. Remove the socks5 head if exists.
 * @param buffers - One or more buffers into which the data will be received.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::size_t bytes_recvd);
 */
template <
	typename AsyncReadStream,
	typename MutableBufferSequence,
	typename ReadToken = asio::default_token_type<AsyncReadStream>>
inline auto async_receive(
	AsyncReadStream& s, const MutableBufferSequence& buffers,
	ReadToken&& token = asio::default_token_type<AsyncReadStream>())
{
	return s.async_receive(buffers, std::forward<ReadToken>(token));
}
}
