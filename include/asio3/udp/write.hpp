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
#include <asio3/core/resolve.hpp>
#include <asio3/core/with_lock.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/core/data_persist.hpp>
#include <asio3/udp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct async_send_buffer_to_host_port_op
	{
		auto operator()(auto state, auto sock_ref, auto&& data, auto&& host, auto&& port) -> void
		{
			auto& sock = sock_ref.get();
			auto msg = std::forward_like<decltype(data)>(data);

			std::string h = asio::to_string(std::forward_like<decltype(host)>(host));
			std::string p = asio::to_string(std::forward_like<decltype(port)>(port));

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::ip::udp::resolver resolver(asio::detail::get_lowest_executor(sock));

			auto [e1, eps] = co_await asio::async_resolve(
				resolver, std::move(h), std::move(p),
				asio::ip::resolver_base::flags(), asio::use_deferred_executor(resolver));
			if (e1)
				co_return{ e1, 0 };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, 0 };

			co_await asio::async_lock(sock, asio::use_deferred_executor(sock));

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e2, n2] = co_await sock.async_send_to(
				asio::to_buffer(msg), (*eps).endpoint(), asio::use_deferred_executor(sock));
			co_return{ e2, n2 };
		}
	};

	struct async_send_data_to_endpoint_op
	{
		auto operator()(auto state, auto sock_ref, auto&& data, auto&& endpoint) -> void
		{
			auto& sock = sock_ref.get();
			auto msg = std::forward_like<decltype(data)>(data);
			auto endp = std::forward_like<decltype(endpoint)>(endpoint);

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(sock, asio::use_deferred_executor(sock));

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e2, n2] = co_await sock.async_send_to(
				asio::to_buffer(msg), endp, asio::use_deferred_executor(sock));
			co_return{ e2, n2 };
		}
	};

	struct udp_async_send_op
	{
		auto operator()(auto state, auto sock_ref, auto&& data) -> void
		{
			auto& sock = sock_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(sock, asio::use_deferred_executor(sock));

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e1, n1] = co_await sock.async_send(
				asio::to_buffer(msg), asio::use_deferred_executor(sock));

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
template <
	typename AsyncWriteStream,
	typename ConstBufferSequence,
	typename WriteToken = asio::default_token_type<AsyncWriteStream>>
requires (
	asio::is_const_buffer_sequence<ConstBufferSequence>::value ||
	asio::is_mutable_buffer_sequence<ConstBufferSequence>::value)
inline auto async_send_to(AsyncWriteStream& s, const ConstBufferSequence& buffers,
	const typename AsyncWriteStream::endpoint_type& destination,
	WriteToken&& token = asio::default_token_type<AsyncWriteStream>())
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
template <
	typename AsyncWriteStream,
	typename DataT,
	typename WriteToken = asio::default_token_type<AsyncWriteStream>>
requires (
	!asio::is_const_buffer_sequence<DataT>::value &&
	!asio::is_mutable_buffer_sequence<DataT>::value)
inline auto async_send_to(AsyncWriteStream& s, DataT&& data,
	const typename AsyncWriteStream::endpoint_type& destination,
	WriteToken&& token = asio::default_token_type<AsyncWriteStream>())
{
	return asio::async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_send_data_to_endpoint_op{}, s),
		token, std::ref(s), detail::data_persist(std::forward<DataT>(data)), destination);
}

/**
 * @brief Start an asynchronous send.
 * This function is used to asynchronously send a datagram to the specified
 * remote endpoint. It is an initiating function for an @ref
 * asynchronous_operation, and always returns immediately.
 *
 * @param data Any data.
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
template <
	typename AsyncWriteStream,
	typename WriteToken = asio::default_token_type<AsyncWriteStream>>
inline auto async_send_to(AsyncWriteStream& s, auto&& data,
	is_string auto&& host, is_string_or_integral auto&& port,
	WriteToken&& token = asio::default_token_type<AsyncWriteStream>())
{
	return asio::async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_send_buffer_to_host_port_op{}, s),
		token, std::ref(s),
		detail::data_persist(std::forward<decltype(data)>(data)),
		std::forward_like<decltype(host)>(host),
		std::forward_like<decltype(port)>(port));
}

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
requires is_udp_socket<AsyncWriteStream>
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
