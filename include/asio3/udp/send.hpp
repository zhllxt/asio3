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
#include <asio3/udp/core.hpp>
#include <asio3/proxy/udp_header.hpp>

namespace asio::detail
{
	struct udp_async_send_op
	{
		template<typename AsyncWriteStream, typename Data>
		auto operator()(auto state,
			std::reference_wrapper<AsyncWriteStream> stream_ref, Data&& data,
			std::optional<ip::udp::endpoint> dest_endpoint) -> void
		{
			auto& s = stream_ref.get();

			Data msg = std::forward<Data>(data);

			co_await asio::dispatch(s.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(s, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ s };

			error_code ec{};
			std::size_t n{ 0 };

			if (dest_endpoint.has_value())
			{
				auto head = socks5::make_udp_header(dest_endpoint.value(), 0);
				auto buffers = std::array<asio::const_buffer, 2>
				{
					asio::const_buffer(asio::buffer(head)),
					asio::const_buffer(asio::buffer(msg))
				};
				auto [e1, n1] = co_await s.async_send(buffers, asio::use_nothrow_deferred);
				ec = e1;
				n = n1;
			}
			else
			{
				auto [e1, n1] = co_await s.async_send(asio::buffer(msg), asio::use_nothrow_deferred);
				ec = e1;
				n = n1;
			}

			co_return{ ec, n };
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
	requires detail::is_template_instance_of<asio::basic_datagram_socket, std::remove_cvref_t<AsyncWriteStream>>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken, void(asio::error_code, std::size_t))
	async_send(
		AsyncWriteStream& s,
		Data&& data,
		WriteToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncWriteStream::executor_type))
	{
		return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::udp_async_send_op{}, s),
			token, std::ref(s), detail::data_persist(std::forward<Data>(data)), std::nullopt);
	}

	/**
	 * @brief Start an asynchronous operation to write all of the supplied data to a stream.
	 * @param s - The stream to which the data is to be written.
	 * @param data - The written data.
	 * @param dest_endpoint - The desired destination endpoint, used for socks5 proxy.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
	 */
	template<typename AsyncWriteStream, typename Data,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::size_t)) WriteToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncWriteStream::executor_type)>
	requires detail::is_template_instance_of<asio::basic_datagram_socket, std::remove_cvref_t<AsyncWriteStream>>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken, void(asio::error_code, std::size_t))
	async_send(
		AsyncWriteStream& s,
		Data&& data,
		std::optional<ip::udp::endpoint> dest_endpoint,
		WriteToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncWriteStream::executor_type))
	{
		return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::udp_async_send_op{}, s),
			token, std::ref(s), detail::data_persist(std::forward<Data>(data)), std::move(dest_endpoint));
	}
}
