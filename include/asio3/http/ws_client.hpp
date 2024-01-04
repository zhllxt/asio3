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
#include <asio3/tcp/tcp_client.hpp>
#include <asio3/http/write.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = tcp_socket>
	class basic_ws_client : public basic_tcp_client<SocketT>
	{
	public:
		using super = basic_tcp_client<SocketT>;
		using socket_type = SocketT;

		/**
		 * @brief constructor
		 */
		explicit basic_ws_client(const auto& ex)
			: super(ex)
			, ws_stream(super::socket)
		{
		}

		basic_ws_client(basic_ws_client&&) noexcept = default;
		basic_ws_client& operator=(basic_ws_client&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_ws_client()
		{
			this->close();
		}

		/**
		 * @brief Asynchronously disconnect the connection.
		 */
		template<typename StopToken = asio::default_token_type<socket_type>>
		inline auto async_stop(
			StopToken&& token = asio::default_token_type<socket_type>())
		{
			this->aborted.test_and_set();

			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						co_await self.ws_stream.async_close(websocket::close_code::normal, use_nothrow_deferred);

						co_await self.base().async_stop(use_nothrow_deferred);

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
		 * @brief Get the ws_stream.
		 */
		constexpr inline auto&& get_stream(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).ws_stream;
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		websocket::stream<socket_type&> ws_stream;
	};

	using ws_client = basic_ws_client<asio::tcp_socket>;
}
