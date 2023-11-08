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
#include <asio3/tcp/tcp_session.hpp>

namespace asio
{
	template<typename SocketT>
	class basic_ws_session : public basic_tcp_session<SocketT>
	{
	public:
		using super = basic_tcp_session<SocketT>;
		using socket_type = SocketT;

		/**
		 * @brief constructor
		 */
		explicit basic_ws_session(socket_type sock)
			: super(std::move(sock))
			, ws_stream(super::socket)
		{
		}

		/**
		 * @brief destructor
		 */
		~basic_ws_session()
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
					}, this->socket), token, std::ref(*this));
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		websocket::stream<socket_type&> ws_stream;
	};

	using ws_session = basic_ws_session<asio::tcp_socket>;
}
