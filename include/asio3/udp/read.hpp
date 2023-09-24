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
#include <asio3/socks5/parser.hpp>

namespace asio::detail
{
	struct udp_async_recv_op
	{
		template<typename AsyncReadStream, typename MutableBufferSequence>
		auto operator()(auto state,
			std::reference_wrapper<AsyncReadStream> stream_ref, const MutableBufferSequence& buffers,
			bool remove_socks5_head) -> void
		{
			auto& s = stream_ref.get();

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [e1, n1] = co_await s.async_receive(buffers, use_nothrow_deferred);
			if (e1)
				co_return{ e1, std::string_view{} };

			std::string_view sv{ (std::string_view::pointer)(buffers.data()), n1 };

			if (remove_socks5_head)
			{
				auto [error, endpoint, domain, real_data] = socks5::parse_udp_packet(sv);
				if (error)
					co_return{ asio::error::no_data, std::string_view{} };
				else
					co_return{ error_code{}, real_data };
			}
			else
			{
				co_return{ error_code{}, sv };
			}
		}
	};
}

namespace asio
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
template <typename AsyncReadStream, typename MutableBufferSequence,
	ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::size_t)) ReadToken
	ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncReadStream::executor_type)>
ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, std::size_t))
async_receive_from(AsyncReadStream& s, const MutableBufferSequence& buffers,
	typename AsyncReadStream::endpoint_type& sender_endpoint,
	ReadToken&& token
	ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncReadStream::executor_type))
{
	return s.async_receive_from(buffers, sender_endpoint, std::forward<ReadToken>(token));
}

/**
 * @brief Start an asynchronous receive on a connected socket. Remove the socks5 head if exists.
 * @param buffers - One or more buffers into which the data will be received.
 * @param remove_socks5_head - If the socket has socks5 proxy, this should be true.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::string_view data);
 */
template <typename AsyncReadStream, typename MutableBufferSequence,
	ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::string_view)) ReadToken
	ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncReadStream::executor_type)>
ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, std::string_view))
async_receive(
	AsyncReadStream& s, const MutableBufferSequence& buffers, bool remove_socks5_head,
	ASIO_MOVE_ARG(ReadToken) token
	ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncReadStream::executor_type))
{
	return async_initiate<ReadToken, void(asio::error_code, std::string_view)>(
		experimental::co_composed<void(asio::error_code, std::string_view)>(
			detail::udp_async_recv_op{}, s),
		token, std::ref(s), buffers, remove_socks5_head);
}

/**
 * @brief Start an asynchronous receive on a connected socket. Remove the socks5 head if exists.
 * @param buffers - One or more buffers into which the data will be received.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::string_view data);
 */
template <typename AsyncReadStream, typename MutableBufferSequence,
	ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::string_view)) ReadToken
	ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncReadStream::executor_type)>
ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, std::string_view))
async_receive(
	AsyncReadStream& s, const MutableBufferSequence& buffers,
	ASIO_MOVE_ARG(ReadToken) token
	ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncReadStream::executor_type))
{
	return async_initiate<ReadToken, void(asio::error_code, std::string_view)>(
		experimental::co_composed<void(asio::error_code, std::string_view)>(
			detail::udp_async_recv_op{}, s),
		token, std::ref(s), buffers, false);
}

}
