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

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http::detail
#else
namespace boost::beast::http::detail
#endif
{
	template<bool isRequest>
	struct async_relay_op
	{
		// /boost_1_83_0/libs/beast/example/doc/http_examples.hpp
		// relay(

		auto operator()(
			auto state, auto input_ref, auto output_ref,
			auto buffer_ref, auto parser_ref, auto&& transform) -> void
		{
			auto& input = input_ref.get();
			auto& output = output_ref.get();
			auto& buffer = buffer_ref.get();
			auto& p = parser_ref.get();

			auto header_callback = std::forward_like<decltype(transform)>(transform);

			co_await asio::dispatch(asio::detail::get_lowest_executor(input), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			std::size_t readed_bytes = 0, written_bytes = 0;
			std::uintptr_t pin = reinterpret_cast<std::uintptr_t>(std::addressof(input));
			std::uintptr_t pout = reinterpret_cast<std::uintptr_t>(std::addressof(output));
			std::uintptr_t pnull = std::uintptr_t(0);

			// A small buffer for relaying the body piece by piece
			std::array<char, asio::tcp_frame_size> buf;

			// Create a parser with a buffer body to read from the input.
			// This will cause crash on release mode, so change to a user passed Parser.
			//http::parser<isRequest, http::buffer_body> p;

			// Create a serializer from the message contained in the parser.
			http::serializer<isRequest, http::buffer_body, http::fields> sr{p.get()};

			// Read just the header from the input
			auto [e1, n1] = co_await http::async_read_header(input, buffer, p, asio::use_nothrow_deferred);
			readed_bytes += n1;
			if (e1)
				co_return{ e1, pin, readed_bytes, written_bytes };

			// Apply the caller's header transformation
			header_callback(p.get(), e1);
			if (e1)
				co_return{ e1, pnull, readed_bytes, written_bytes };

			// Send the transformed message to the output
			auto [e2, n2] = co_await http::async_write_header(output, sr, asio::use_nothrow_deferred);
			written_bytes += n2;
			if(e2)
				co_return{ e2, pout, readed_bytes, written_bytes };

			// Loop over the input and transfer it to the output
			do
			{
				if (!!state.cancelled())
					co_return{ asio::error::operation_aborted, pnull, readed_bytes, written_bytes };

				if (!p.is_done())
				{
					// Set up the body for writing into our small buffer
					p.get().body().data = buf.data();
					p.get().body().size = buf.size();

					// Read as much as we can
					auto [e3, n3] = co_await http::async_read(input, buffer, p, asio::use_nothrow_deferred);

					readed_bytes += n3;

					// This error is returned when buffer_body uses up the buffer
					if (e3 == http::error::need_buffer)
						e3 = {};
					if (e3)
						co_return{ e3, pin, readed_bytes, written_bytes };

					assert(n3 == buf.size() - p.get().body().size);

					// Set up the body for reading.
					// This is how much was parsed:
					p.get().body().size = buf.size() - p.get().body().size;
					p.get().body().data = buf.data();
					p.get().body().more = ! p.is_done();
				}
				else
				{
					p.get().body().data = nullptr;
					p.get().body().size = 0;
				}

				// Write everything in the buffer (which might be empty)
				auto [e4, n4] = co_await http::async_write(output, sr, asio::use_nothrow_deferred);

				written_bytes += n4;

				// This error is returned when buffer_body uses up the buffer
				if (e4 == http::error::need_buffer)
					e4 = {};
				if (e4)
					co_return{ e4, pout, readed_bytes, written_bytes };

			} while (!p.is_done() && !sr.is_done());

			co_return{ asio::error_code{}, pnull, readed_bytes, written_bytes };
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
 * @brief Relay an HTTP message.
 *   This function efficiently relays an HTTP message from a downstream
 *   client to an upstream server, or from an upstream server to a
 *   downstream client. After the message header is read from the input,
 *   a user provided transformation function is invoked which may change
 *   the contents of the header before forwarding to the output. This may
 *   be used to adjust fields such as Server, or proxy fields.
 * @param input The stream to read from.
 * @param output The stream to write to.
 * @param buffer The buffer to use for the input.
 * @param transform The header transformation to apply. The function will
 * be called with this signature:
 * @code
 *     template<class Body>
 *     void transform(message<
 *         isRequest, Body, Fields>&,  // The message to transform
 *         asio::error_code&);         // Set to the error, if any
 * @endcode
 * @tparam isRequest `true` to relay a request.
 * @tparam Fields The type of fields to use for the message.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec, std::uintptr_t errored_stream,
                   std::size_t readed_bytes, std::size_t written_bytes);
 */
template<
	bool isRequest,
	typename Body,
	typename AsyncReadStream,
	typename AsyncWriteStream,
	typename DynamicBuffer,
	typename Transform,
	typename RelayToken = asio::default_token_type<AsyncReadStream>
>
inline auto async_relay(
	AsyncReadStream& input,
	AsyncWriteStream& output,
	DynamicBuffer& buffer,
	http::parser<isRequest, Body>& parser,
	Transform&& transform,
	RelayToken&& token = asio::default_token_type<AsyncReadStream>())
{
	return asio::async_initiate<RelayToken,
		void(asio::error_code, std::uintptr_t, std::size_t, std::size_t)>(
			asio::experimental::co_composed<
			void(asio::error_code, std::uintptr_t, std::size_t, std::size_t)>(
				detail::async_relay_op<isRequest>{}, input),
			token,
			std::ref(input),
			std::ref(output),
			std::ref(buffer),
			std::ref(parser),
			std::forward<Transform>(transform));
}

template<
	bool isRequest,
	typename Body,
	typename AsyncReadStream,
	typename AsyncWriteStream,
	typename DynamicBuffer,
	typename Transform
>
net::awaitable<std::tuple<asio::error_code, std::uintptr_t, std::size_t, std::size_t>> relay(
	AsyncReadStream& input,
	AsyncWriteStream& output,
	DynamicBuffer& buffer,
	http::parser<isRequest, Body>& parser,
	Transform&& transform)
{
	http::parser<isRequest, Body>& p = parser;

	auto header_callback = std::forward_like<decltype(transform)>(transform);

	co_await asio::dispatch(asio::detail::get_lowest_executor(input), asio::use_nothrow_awaitable);

	std::size_t readed_bytes = 0, written_bytes = 0;
	std::uintptr_t pin = reinterpret_cast<std::uintptr_t>(std::addressof(input));
	std::uintptr_t pout = reinterpret_cast<std::uintptr_t>(std::addressof(output));
	std::uintptr_t pnull = std::uintptr_t(0);

	// A small buffer for relaying the body piece by piece
	std::array<char, asio::tcp_frame_size> buf;

	// Create a parser with a buffer body to read from the input.
	// This will cause crash on release mode, so change to a user passed Parser.
	//http::parser<isRequest, http::buffer_body> p;

	// Create a serializer from the message contained in the parser.
	http::serializer<isRequest, http::buffer_body, http::fields> sr{p.get()};

	// Read just the header from the input
	auto [e1, n1] = co_await http::async_read_header(input, buffer, p);
	readed_bytes += n1;
	if (e1)
		co_return std::tuple{ e1, pin, readed_bytes, written_bytes };

	// Apply the caller's header transformation
	header_callback(p.get(), e1);
	if (e1)
		co_return std::tuple{ e1, pnull, readed_bytes, written_bytes };

	// Send the transformed message to the output
	auto [e2, n2] = co_await http::async_write_header(output, sr);
	written_bytes += n2;
	if(e2)
		co_return std::tuple{ e2, pout, readed_bytes, written_bytes };

	// Loop over the input and transfer it to the output
	do
	{
		if (!p.is_done())
		{
			// Set up the body for writing into our small buffer
			p.get().body().data = buf.data();
			p.get().body().size = buf.size();

			// Read as much as we can
			auto [e3, n3] = co_await http::async_read(input, buffer, p);

			readed_bytes += n3;

			// This error is returned when buffer_body uses up the buffer
			if (e3 == http::error::need_buffer)
				e3 = {};
			if (e3)
				co_return std::tuple{ e3, pin, readed_bytes, written_bytes };

			assert(n3 == buf.size() - p.get().body().size);

			// Set up the body for reading.
			// This is how much was parsed:
			p.get().body().size = buf.size() - p.get().body().size;
			p.get().body().data = buf.data();
			p.get().body().more = ! p.is_done();
		}
		else
		{
			p.get().body().data = nullptr;
			p.get().body().size = 0;
		}

		// Write everything in the buffer (which might be empty)
		auto [e4, n4] = co_await http::async_write(output, sr);

		written_bytes += n4;

		// This error is returned when buffer_body uses up the buffer
		if (e4 == http::error::need_buffer)
			e4 = {};
		if (e4)
			co_return std::tuple{ e4, pout, readed_bytes, written_bytes };

	} while (!p.is_done() && !sr.is_done());

	co_return std::tuple{ asio::error_code{}, pnull, readed_bytes, written_bytes };
}

template<
	bool isRequest,
	typename Body,
	typename AsyncReadStream,
	typename AsyncWriteStream,
	typename DynamicBuffer
>
net::awaitable<std::tuple<asio::error_code, std::uintptr_t, std::size_t, std::size_t>> relay(
	AsyncReadStream& input,
	AsyncWriteStream& output,
	DynamicBuffer& buffer,
	http::parser<isRequest, Body>& parser)
{
	http::parser<isRequest, Body>& p = parser;

	co_await asio::dispatch(asio::detail::get_lowest_executor(input), asio::use_nothrow_awaitable);

	std::size_t readed_bytes = 0, written_bytes = 0;
	std::uintptr_t pin = reinterpret_cast<std::uintptr_t>(std::addressof(input));
	std::uintptr_t pout = reinterpret_cast<std::uintptr_t>(std::addressof(output));
	std::uintptr_t pnull = std::uintptr_t(0);

	// A small buffer for relaying the body piece by piece
	std::array<char, asio::tcp_frame_size> buf;

	// Create a parser with a buffer body to read from the input.
	// This will cause crash on release mode, so change to a user passed Parser.
	//http::parser<isRequest, http::buffer_body> p;

	// Create a serializer from the message contained in the parser.
	http::serializer<isRequest, http::buffer_body, http::fields> sr{p.get()};

	// Send the transformed message to the output
	auto [e2, n2] = co_await http::async_write_header(output, sr);
	written_bytes += n2;
	if(e2)
		co_return std::tuple{ e2, pout, readed_bytes, written_bytes };

	// Loop over the input and transfer it to the output
	do
	{
		if (!p.is_done())
		{
			// Set up the body for writing into our small buffer
			p.get().body().data = buf.data();
			p.get().body().size = buf.size();

			// Read as much as we can
			auto [e3, n3] = co_await http::async_read(input, buffer, p);

			readed_bytes += n3;

			// This error is returned when buffer_body uses up the buffer
			if (e3 == http::error::need_buffer)
				e3 = {};
			if (e3)
				co_return std::tuple{ e3, pin, readed_bytes, written_bytes };

			assert(n3 == buf.size() - p.get().body().size);

			// Set up the body for reading.
			// This is how much was parsed:
			p.get().body().size = buf.size() - p.get().body().size;
			p.get().body().data = buf.data();
			p.get().body().more = ! p.is_done();
		}
		else
		{
			p.get().body().data = nullptr;
			p.get().body().size = 0;
		}

		// Write everything in the buffer (which might be empty)
		auto [e4, n4] = co_await http::async_write(output, sr);

		written_bytes += n4;

		// This error is returned when buffer_body uses up the buffer
		if (e4 == http::error::need_buffer)
			e4 = {};
		if (e4)
			co_return std::tuple{ e4, pout, readed_bytes, written_bytes };

	} while (!p.is_done() && !sr.is_done());

	co_return std::tuple{ asio::error_code{}, pnull, readed_bytes, written_bytes };
}
}
