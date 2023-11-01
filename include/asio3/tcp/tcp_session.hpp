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

#include <asio3/core/netutil.hpp>
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>
#include <asio3/tcp/disconnect.hpp>

namespace asio
{
	class tcp_session : public std::enable_shared_from_this<tcp_session>
	{
	public:
		using key_type = std::size_t;

	public:
		explicit tcp_session(tcp_socket sock) : socket(std::move(sock))
		{
		}

		~tcp_session()
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
			return asio::async_disconnect(socket,
				asio::tcp_disconnect_timeout,
				std::forward<DisconnectToken>(token));
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
			return asio::async_send(socket,
				std::forward_like<decltype(data)>(data), std::forward<WriteToken>(token));
		}

		/**
		 * @brief shutdown and close the socket directly.
		 */
		inline void close()
		{
			asio::error_code ec{};
			socket.shutdown(asio::socket_base::shutdown_both, ec);
			socket.close(ec);
		}

		/**
		 * @brief Get this object hash key, used for session map
		 */
		inline key_type hash_key() noexcept
		{
			return reinterpret_cast<key_type>(this);
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return socket.get_executor();
		}

		/**
		 * @brief Get the local address.
		 */
		inline std::string get_local_address() noexcept
		{
			return asio::get_local_address(socket);
		}

		/**
		 * @brief Get the local port number.
		 */
		inline ip::port_type get_local_port() noexcept
		{
			return asio::get_local_port(socket);
		}

		/**
		 * @brief Get the remote address.
		 */
		inline std::string get_remote_address() noexcept
		{
			return asio::get_remote_address(socket);
		}

		/**
		 * @brief Get the remote port number.
		 */
		inline ip::port_type get_remote_port() noexcept
		{
			return asio::get_remote_port(socket);
		}

		/**
		 * @brief Get the socket.
		 * https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/
		 */
		constexpr inline auto&& get_socket(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).socket;
		}

		inline void update_alive_time() noexcept
		{
			alive_time = std::chrono::system_clock::now();
		}

	public:
		asio::tcp_socket      socket;

		std::chrono::system_clock::time_point alive_time{ std::chrono::system_clock::now() };
	};
}
