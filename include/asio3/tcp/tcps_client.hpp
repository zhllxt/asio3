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

#include <asio3/tcp/tcp_client.hpp>
#include <asio3/tcp/sslutil.hpp>

namespace asio
{
	class tcps_client : public tcp_client
	{
	public:
		using super = tcp_client;

		explicit tcps_client(const auto& ex, ssl::context&& sslctx)
			: tcp_client(ex)
			, ssl_context(std::move(sslctx))
			, ssl_stream(socket, ssl_context)
		{
		}

		~tcps_client()
		{
			close();
		}

		/**
		 * @brief Asynchronously disconnect the connection.
		 */
		template<typename StopToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_stop(
			StopToken&& token = asio::default_token_type<asio::tcp_socket>())
		{
			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						co_await asio::async_lock(self.socket, use_nothrow_deferred);

						co_await self.ssl_stream.async_shutdown(use_nothrow_deferred);

						detail::unlock_channel(self.socket);

						co_await self.base().async_stop(use_nothrow_deferred);

						SSL_clear(self.ssl_stream.native_handle());

						co_return error_code{};
					}, ssl_stream), token, std::ref(*this));
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename WriteToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_send(
			auto&& data,
			WriteToken&& token = asio::default_token_type<asio::tcp_socket>())
		{
			return asio::async_send(ssl_stream,
				std::forward_like<decltype(data)>(data), std::forward<WriteToken>(token));
		}

		/**
		 * @brief shutdown and close the socket directly.
		 */
		inline void close()
		{
			super::close();

			SSL_clear(ssl_stream.native_handle());
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		asio::ssl::context                   ssl_context;

		asio::ssl::stream<asio::tcp_socket&> ssl_stream;
	};
}
