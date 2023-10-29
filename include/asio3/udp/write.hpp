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
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/udp/core.hpp>

namespace asio::detail
{
	struct async_send_to_op
	{
		auto operator()(auto state, auto sock_ref, auto buffers, auto&& host, auto&& port) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			std::string h = asio::to_string(std::forward_like<decltype(host)>(host));
			std::string p = asio::to_string(std::forward_like<decltype(port)>(port));

			asio::ip::udp::resolver resolver(sock.get_executor());

			auto [e1, eps] = co_await resolver.async_resolve(h, p, use_nothrow_deferred);
			if (e1)
				co_return{ e1, 0 };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, 0 };

			auto [e2, n2] = co_await sock.async_send_to(buffers, (*eps).endpoint(), use_nothrow_deferred);
			co_return{ e2, n2 };
		}
	};
}

namespace asio
{
/**
 * @brief Start an asynchronous send.
 * This function is used to asynchronously send a datagram to the specified
 * remote endpoint. It is an initiating function for an @ref
 * asynchronous_operation, and always returns immediately.
 *
 * @param buffers One or more data buffers to be sent to the remote endpoint.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the completion handler is called.
 *
 * @param destination The remote endpoint to which the data will be sent.
 * Copies will be made of the endpoint as required.
 *
 * @param token The @ref completion_token that will be used to produce a
 * completion handler, which will be called when the send completes. Potential
 * completion tokens include @ref use_future, @ref use_awaitable, @ref
 * yield_context, or a function object with the correct completion signature.
 * The function signature of the completion handler must be:
 * @code void handler(
 *   const asio::error_code& error, // Result of operation.
 *   std::size_t bytes_transferred // Number of bytes sent.
 * ); @endcode
 */
template <typename AsyncWriteStream, typename ConstBufferSequence,
	ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::size_t)) WriteToken
	ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncWriteStream::executor_type)>
ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken, void(asio::error_code, std::size_t))
async_send_to(AsyncWriteStream& s, const ConstBufferSequence& buffers,
	const typename AsyncWriteStream::endpoint_type& destination,
	WriteToken&& token
	ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncWriteStream::executor_type))
{
	return s.async_send_to(buffers, destination, std::forward<WriteToken>(token));
}

/**
 * @brief Start an asynchronous send.
 * This function is used to asynchronously send a datagram to the specified
 * remote endpoint. It is an initiating function for an @ref
 * asynchronous_operation, and always returns immediately.
 *
 * @param buffers One or more data buffers to be sent to the remote endpoint.
 * Although the buffers object may be copied as necessary, ownership of the
 * underlying memory blocks is retained by the caller, which must guarantee
 * that they remain valid until the completion handler is called.
 *
 * @param destination The remote endpoint to which the data will be sent.
 * Copies will be made of the endpoint as required.
 *
 * @param token The @ref completion_token that will be used to produce a
 * completion handler, which will be called when the send completes. Potential
 * completion tokens include @ref use_future, @ref use_awaitable, @ref
 * yield_context, or a function object with the correct completion signature.
 * The function signature of the completion handler must be:
 * @code void handler(
 *   const asio::error_code& error, // Result of operation.
 *   std::size_t bytes_transferred // Number of bytes sent.
 * ); @endcode
 */
template <typename AsyncWriteStream, typename ConstBufferSequence,
	typename String, typename StrOrInt,
	ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::size_t)) WriteToken
	ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncWriteStream::executor_type)>
ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken, void(asio::error_code, std::size_t))
async_send_to(AsyncWriteStream& s, const ConstBufferSequence& buffers,
	is_string auto&& host, is_string_or_integral auto&& port,
	WriteToken&& token
	ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncWriteStream::executor_type))
{
	return asio::async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_send_to_op{}, s),
		token, std::ref(s), buffers,
		std::forward_like<decltype(host)>(host),
		std::forward_like<decltype(port)>(port));
}
}
