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
#include <asio3/core/netutil.hpp>
#include <asio3/core/with_lock.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/core/data_persist.hpp>
#include <asio3/core/file.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http::detail
#else
namespace boost::beast::http::detail
#endif
{
	template<bool isRequest, typename Body, typename Fields>
	struct async_recv_file_op
	{
		http::message<isRequest, Body, Fields>& header;

		struct visit
		{
			std::vector<asio::const_buffer>& buffers_;
			std::size_t& buffers_bytes;

			template<class ConstBufferSequence>
			void operator()(error_code&, ConstBufferSequence const& buffers)
			{
				buffers_bytes = 0;

				for (auto buffer : buffers)
				{
					buffers_bytes += buffer.size();
					buffers_.emplace_back(buffer);
				}
			}
		};

		// /boost_1_80_0/libs/beast/example/doc/http_examples.hpp

		auto operator()(auto state, auto sock_ref, auto file_ref, auto buffer_ref, auto&& body_chunk_callback) -> void
		{
			auto& sock = sock_ref.get();
			auto& file = file_ref.get();
			auto& buffer = buffer_ref.get();

			auto chunk_callback = std::forward_like<decltype(body_chunk_callback)>(body_chunk_callback);

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::error_code ec{};
			std::size_t recvd_bytes = 0;

			// Declare the parser with an empty body since
			// we plan on capturing the chunks ourselves.
			http::parser<isRequest, http::buffer_body> parser;

			parser.body_limit((std::numeric_limits<std::size_t>::max)());

			std::vector<asio::const_buffer> buffers;
			std::size_t buffers_bytes = 0;
			http::serializer<isRequest, Body, Fields> sr{ header };
			while (!sr.is_header_done())
			{
				sr.next(ec, visit{ buffers, buffers_bytes });
				sr.consume(buffers_bytes);
			}

			parser.put(buffers, ec);
			if (ec)
			{
				co_return{ ec, recvd_bytes };
			}

			if (parser.get().chunked())
			{
				// This container will hold the extensions for each chunk
				http::chunk_extensions ce;

				// This string will hold the body of each chunk
				//std::string chunk;

				// Declare our chunk header callback  This is invoked
				// after each chunk header and also after the last chunk.
				auto header_cb = [&](
					std::uint64_t size,          // Size of the chunk, or zero for the last chunk
					std::string_view extensions, // The raw chunk-extensions string. Already validated.
					error_code& ev)              // We can set this to indicate an error
					{
						// Parse the chunk extensions so we can access them easily
						ce.parse(extensions, ev);
						if (ev)
							return;

						// See if the chunk is too big
						if (size > (std::numeric_limits<std::size_t>::max)())
						{
							ev = http::error::body_limit;
							return;
						}

						// Make sure we have enough storage, and
						// reset the container for the upcoming chunk
						//chunk.reserve(static_cast<std::size_t>(size));
						//chunk.clear();
					};

				// Set the callback. The function requires a non-const reference so we
				// use a local variable, since temporaries can only bind to const refs.
				parser.on_chunk_header(header_cb);

				std::string content;
				content.reserve(1024);

				// Declare the chunk body callback. This is called one or
				// more times for each piece of a chunk body.
				auto body_cb = [&](
					std::uint64_t remain,   // The number of bytes left in this chunk
					std::string_view body,  // A buffer holding chunk body data
					error_code& ev)         // We can set this to indicate an error
					{
						// If this is the last piece of the chunk body,
						// set the error so that the call to `read` returns
						// and we can process the chunk.
						if (remain == body.size())
							ev = http::error::end_of_chunk;

						// Append this piece to our container
						//chunk.append(body.data(), body.size());
						if (!chunk_callback(body))
							ev = asio::error::operation_aborted;

						content += body;

						// The return value informs the parser of how much of the body we
						// consumed. We will indicate that we consumed everything passed in.
						return body.size();
					};
				parser.on_chunk_body(body_cb);

				while (!parser.is_done())
				{
					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, recvd_bytes };

					// Read as much as we can. When we reach the end of the chunk, the chunk
					// body callback will make the read return with the end_of_chunk error.
					auto [e1, n1] = co_await http::async_read(
						sock, buffer, parser, asio::use_deferred_executor(sock));
					if (e1 == http::error::end_of_chunk)
					{
						e1 = {};
					}
					else if (e1)
					{
						co_return{ e1, recvd_bytes };
					}

					recvd_bytes += n1;

					auto [e2, n2] = co_await asio::async_write(
						file, asio::buffer(content), asio::use_deferred_executor(file));
					if (e2)
					{
						co_return{ e2, recvd_bytes };
					}
					content.clear();
				}
			}
			else
			{
				std::array<char, 512> buf;

				while (!parser.is_done())
				{
					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, recvd_bytes };

					parser.get().body().data = buf.data();
					parser.get().body().size = buf.size();

					auto [e1, n1] = co_await http::async_read(
						sock, buffer, parser, asio::use_deferred_executor(sock));
					if (e1 == http::error::need_buffer)
					{
						e1 = {};
					}
					else if (e1)
					{
						co_return{ e1, recvd_bytes };
					}

					recvd_bytes += n1;

					std::string_view chunk_data{ buf.data(), n1 };
					if (!chunk_callback(chunk_data))
					{
						co_return{ asio::error::operation_aborted, recvd_bytes };
					}

					auto [e2, n2] = co_await asio::async_write(
						file, asio::buffer(buf.data(), n1), asio::use_deferred_executor(file));
					if (e2)
					{
						co_return{ e2, recvd_bytes };
					}

					assert(n1 == buf.size() - parser.get().body().size);
				}
			}

			co_return{ error_code{}, recvd_bytes };
		}
	};
}

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
/**
 * @brief Start an asynchronous operation to read all the data of the stream to a file.
 * @param stream - The socket stream to which the data is to be read.
 * @param file - The file stream to which the data is to be written.
 * @param header - The http message header which was already readed from the stream.
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
 *    void handler(const asio::error_code& ec, std::size_t recvd_bytes);
 */
template<
	bool isRequest,
	typename SockStream,
	typename FileStream,
	typename DynamicBuffer,
	typename Body, typename Fields,
	typename BodyChunkCallback,
	typename SendToken = asio::default_token_type<SockStream>>
requires (
	std::invocable<BodyChunkCallback, std::string_view>
)
inline auto async_recv_file(
	SockStream& stream,
	FileStream& file,
	DynamicBuffer& buffer,
	http::message<isRequest, Body, Fields>& header,
	BodyChunkCallback&& chunk_callback,
	SendToken&& token = asio::default_token_type<SockStream>())
{
	return asio::async_initiate<SendToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_recv_file_op<isRequest, Body, Fields>{ header }, stream),
		token,
		std::ref(stream),
		std::ref(file),
		std::ref(buffer),
		std::forward<BodyChunkCallback>(chunk_callback));
}

/**
 * @brief Start an asynchronous operation to read all the data of the stream to a file.
 * @param stream - The stream to which the data is to be written.
 * @param file - The file stream to which the data is to be written.
 * @param header - The http message header which was already readed from the stream.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::size_t recvd_bytes);
 */
template<
	bool isRequest,
	typename SockStream,
	typename FileStream,
	typename DynamicBuffer,
	typename Body, typename Fields,
	typename SendToken = asio::default_token_type<SockStream>>
requires (!std::invocable<SendToken, std::string_view>)
inline auto async_recv_file(
	SockStream& stream,
	FileStream& file,
	DynamicBuffer& buffer,
	http::message<isRequest, Body, Fields>& header,
	SendToken&& token = asio::default_token_type<SockStream>())
{
	return asio::async_initiate<SendToken, void(asio::error_code, std::size_t)>(
		asio::experimental::co_composed<void(asio::error_code, std::size_t)>(
			detail::async_recv_file_op<isRequest, Body, Fields>{ header }, stream),
		token,
		std::ref(stream),
		std::ref(file),
		std::ref(buffer),
		[](auto) { return true; });
}

}
