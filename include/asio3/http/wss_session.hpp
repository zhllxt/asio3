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
#include <asio3/tcp/tcps_session.hpp>

namespace asio
{
	template<typename SocketT>
	class basic_wss_session : public basic_tcps_session<SocketT>
	{
	public:
		using super = basic_tcps_session<SocketT>;
		using socket_type = SocketT;

		explicit basic_wss_session(socket_type sock, ssl::context& sslctx)
			: super(std::move(sock), sslctx)
			, ws_stream(super::ssl_stream)
		{
		}

		~basic_wss_session()
		{
			this->close();
		}

		/**
		 * @brief Asynchronously graceful disconnect the connection, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<socket_type>>
		inline auto async_disconnect(
			DisconnectToken&& token = asio::default_token_type<socket_type>())
		{
			return asio::async_initiate<DisconnectToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						co_await self.ws_stream.async_close(websocket::close_code::normal, use_nothrow_deferred);

						co_await self.base().async_disconnect(use_nothrow_deferred);

						co_return error_code{};
					}, ws_stream), token, std::ref(*this));
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename WriteToken = asio::default_token_type<socket_type>>
		inline auto async_send(
			auto&& data,
			WriteToken&& token = asio::default_token_type<socket_type>())
		{
			return asio::async_send(ws_stream,
				std::forward_like<decltype(data)>(data), std::forward<WriteToken>(token));
		}

		/**
		 * @brief shutdown and close the socket directly.
		 */
		inline void close()
		{
			super::close();
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		websocket::stream<asio::ssl::stream<socket_type&>&> ws_stream;
	};

	using wss_session = basic_wss_session<asio::tcp_socket>;
}
