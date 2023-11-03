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

#include <asio3/tcp/tcp_session.hpp>
#include <asio3/tcp/sslutil.hpp>

namespace asio
{
	class tcps_session : public tcp_session
	{
	public:
		using super = tcp_session;

		explicit tcps_session(tcp_socket sock, ssl::context& sslctx)
			: tcp_session(std::move(sock))
			, ssl_context(sslctx)
			, ssl_stream(socket, ssl_context)
		{
		}

		~tcps_session()
		{
			close();
		}

		/**
		 * @brief Asynchronously graceful disconnect the connection, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_disconnect(
			DisconnectToken&& token = asio::default_token_type<asio::tcp_socket>())
		{
			return asio::async_initiate<DisconnectToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						co_await asio::async_shutdown(self.ssl_stream, use_nothrow_deferred);

						co_await self.base().async_disconnect(use_nothrow_deferred);

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
		asio::ssl::context&                  ssl_context;

		asio::ssl::stream<asio::tcp_socket&> ssl_stream;
	};
}
