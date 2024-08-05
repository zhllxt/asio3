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
#include <asio3/core/stdutil.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/with_lock.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/core/data_persist.hpp>
#include <asio3/core/file.hpp>
#include <asio3/http/mime_types.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct websocket_stream_async_send_op
	{
		auto operator()(auto state, auto ws_stream_ref, auto&& data) -> void
		{
			auto& ws_stream = ws_stream_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);

			co_await asio::dispatch(asio::use_deferred_executor(ws_stream));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(ws_stream, asio::use_deferred_executor(ws_stream));

			[[maybe_unused]] asio::defer_unlock defered_unlock{ ws_stream };

			auto [e1, n1] = co_await ws_stream.async_write(
				asio::to_buffer(msg), asio::use_deferred_executor(ws_stream));

			co_return{ e1, n1 };
		}
	};
}

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http::detail
#else
namespace boost::beast::http::detail
#endif
{
	template<bool isRequest, typename Body, typename Fields>
	struct async_send_file_op
	{
		http::message<isRequest, Body, Fields> header;

		// /boost_1_83_0/libs/beast/example/doc/http_examples.hpp
		// send_cgi_response

		auto operator()(auto state, auto sock_ref, auto file_ref, auto&& body_chunk_callback) -> void
		{
			auto& sock = sock_ref.get();
			auto& file = file_ref.get();

			auto chunk_callback = std::forward_like<decltype(body_chunk_callback)>(body_chunk_callback);

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::error_code ec{};
			std::size_t sent_bytes = 0;

			http::message<isRequest, http::buffer_body> msg{ std::move(header) };

			msg.body().data = nullptr;
			msg.body().more = true;

			http::serializer<isRequest, http::buffer_body> sr{ msg };

			co_await asio::async_lock(sock, asio::use_deferred_executor(sock));

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			auto [e2, n2] = co_await http::async_write_header(sock, sr, asio::use_deferred_executor(sock));
			if (e2)
			{
				co_return{ e2, sent_bytes };
			}

			sent_bytes += n2;

			std::array<char, 2048> buffer;
			do
			{
				if (!!state.cancelled())
					co_return{ asio::error::operation_aborted, sent_bytes };

				auto [e3, n3] = co_await file.async_read_some(
					asio::buffer(buffer), asio::use_deferred_executor(file));
				if (e3 == asio::error::eof)
				{
					e3 = {};

					assert(n3 == 0);

					// `nullptr` indicates there is no buffer
					msg.body().data = nullptr;

					// `false` means no more data is coming
					msg.body().more = false;
				}
				else
				{
					if (e3)
					{
						co_return{ e3, sent_bytes };
					}

					// Point to our buffer with the bytes that
					// we received, and indicate that there may
					// be some more data coming
					msg.body().data = buffer.data();
					msg.body().size = n3;
					msg.body().more = true;

					std::string_view chunk_data{ buffer.data(), n3 };
					if (!chunk_callback(chunk_data))
					{
						co_return{ asio::error::operation_aborted, sent_bytes };
					}
				}

				// Write everything in the body buffer
				auto [e4, n4] = co_await http::async_write(sock, sr, asio::use_deferred_executor(sock));

				sent_bytes += n4;

				// This error is returned by body_buffer during
				// serialization when it is done sending the data
				// provided and needs another buffer.
				if (e4 == http::error::need_buffer)
				{
					e4 = {};
				}
				else if (e4)
				{
					co_return{ e4, sent_bytes };
				}
			} while (!sr.is_done());

			co_return{ error_code{}, sent_bytes };
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
	beast::websocket::stream<AsyncStream>& stream,
	auto&& data,
	SendToken&& token = asio::default_token_type<AsyncStream>())
{
	return asio::async_initiate<SendToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::websocket_stream_async_send_op{}, stream),
		token,
		std::ref(stream),
		asio::detail::data_persist(std::forward_like<decltype(data)>(data)));
}

}

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
/**
 * @brief Start an asynchronous operation to write all the data of the supplied file to a stream.
 * @param stream - The socket stream to which the data is to be written.
 * @param file - The file stream to which the data is to be readed.
 * @param header - The http message header which will be written to the stream.
 * @param chunk_callback - The file data chunk callback function. The function will
    be called with this signature:
    @code
		// return true to continue sending the next data chunk.
		// return false will stop the sending.
        bool on_chunk(std::string_view chunk){};
    @endcode
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
 */
template<
	bool isRequest,
	typename AsyncStream,
	typename FileStream,
	typename Body, typename Fields,
	typename BodyChunkCallback,
	typename SendToken = asio::default_token_type<AsyncStream>>
requires (std::invocable<BodyChunkCallback, std::string_view>)
inline auto async_send_file(
	AsyncStream& stream,
	FileStream& file,
	http::message<isRequest, Body, Fields> header,
	BodyChunkCallback&& chunk_callback,
	SendToken&& token = asio::default_token_type<AsyncStream>())
{
	return asio::async_initiate<SendToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_send_file_op<isRequest, Body, Fields>{ std::move(header) }, stream),
		token,
		std::ref(stream),
		std::ref(file),
		std::forward<BodyChunkCallback>(chunk_callback));
}

/**
 * @brief Start an asynchronous operation to write all the data of the supplied file to a stream.
 * @param stream - The socket stream to which the data is to be written.
 * @param file - The file stream to which the data is to be readed.
 * @param header - The http message header which will be written to the stream.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
 */
template<
	bool isRequest,
	typename AsyncStream,
	typename FileStream,
	typename Body, typename Fields,
	typename SendToken = asio::default_token_type<AsyncStream>>
requires (!std::invocable<SendToken, std::string_view>)
inline auto async_send_file(
	AsyncStream& stream,
	FileStream& file,
	http::message<isRequest, Body, Fields> header,
	SendToken&& token = asio::default_token_type<AsyncStream>())
{
	return asio::async_initiate<SendToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_send_file_op<isRequest, Body, Fields>{ std::move(header) }, stream),
		token,
		std::ref(stream),
		std::ref(file),
		[](auto) { return true; });
}

}
