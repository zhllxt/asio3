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
#include <asio3/core/with_lock.hpp>
#include <asio3/http/https_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename HttpsSessionPointerT>
	class basic_flex_wss_session
	{
	public:
		using https_session_pointer_type = HttpsSessionPointerT;
		using https_session_type = typename asio::element_type_adapter<https_session_pointer_type>::type;
		using key_type = typename https_session_type::key_type;
		using socket_type = typename https_session_type::socket_type;
		using ssl_stream_type = typename https_session_type::ssl_stream_type;

		explicit basic_flex_wss_session(HttpsSessionPointerT&& session_ptr)
			: https_session(std::move(session_ptr))
			, socket(https_session->socket)
			, ssl_context(https_session->ssl_context)
			, ssl_stream(https_session->ssl_stream)
			, ws_stream(https_session->ssl_stream)
			, alive_time(https_session->alive_time)
			, disconnect_timeout(https_session->disconnect_timeout)
			, ssl_shutdown_timeout(https_session->ssl_shutdown_timeout)
		{
		}

		basic_flex_wss_session(basic_flex_wss_session&&) noexcept = default;
		basic_flex_wss_session& operator=(basic_flex_wss_session&&) noexcept = default;

		~basic_flex_wss_session()
		{
			close();
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

						co_await asio::dispatch(asio::use_deferred_executor(self));

						co_await self.ws_stream.async_close(websocket::close_code::normal,
							asio::use_deferred_executor(self));

						co_await self.https_session->async_disconnect(asio::use_deferred_executor(self));

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
			https_session->close();
		}

		/**
		 * @brief Get this object hash key, used for session map
		 */
		[[nodiscard]] inline key_type hash_key() noexcept
		{
			return reinterpret_cast<key_type>(this);
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline auto get_executor() noexcept
		{
			return asio::detail::get_lowest_executor(socket);
		}

		/**
		 * @brief Get the local address.
		 */
		[[nodiscard]] inline std::string get_local_address() noexcept
		{
			return asio::get_local_address(socket);
		}

		/**
		 * @brief Get the local port number.
		 */
		[[nodiscard]] inline ip::port_type get_local_port() noexcept
		{
			return asio::get_local_port(socket);
		}

		/**
		 * @brief Get the remote address.
		 */
		[[nodiscard]] inline std::string get_remote_address() noexcept
		{
			return asio::get_remote_address(socket);
		}

		/**
		 * @brief Get the remote port number.
		 */
		[[nodiscard]] inline ip::port_type get_remote_port() noexcept
		{
			return asio::get_remote_port(socket);
		}

		/**
		 * @brief Get the socket.
		 * https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/
		 */
		constexpr inline auto&& get_socket(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).https_session->socket;
		}

		inline void update_alive_time() noexcept
		{
			alive_time = std::chrono::system_clock::now();
		}

	public:
		HttpsSessionPointerT https_session;

		socket_type&         socket;

		asio::ssl::context&  ssl_context;

		ssl_stream_type&     ssl_stream;

		websocket::stream<ssl_stream_type&> ws_stream;

		std::chrono::system_clock::time_point& alive_time;

		std::chrono::steady_clock::duration& disconnect_timeout;

		std::chrono::steady_clock::duration& ssl_shutdown_timeout;
	};

	using flex_wss_session = basic_flex_wss_session<std::shared_ptr<asio::https_session>>;
}
