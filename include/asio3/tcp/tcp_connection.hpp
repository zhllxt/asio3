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

#include <asio3/core/connection.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>
#include <asio3/tcp/send.hpp>
#include <asio3/tcp/disconnect.hpp>

namespace asio
{
	class tcp_connection : public std::enable_shared_from_this<tcp_connection>
	{
	public:
		using key_type = std::size_t;

	public:
		explicit tcp_connection(tcp_socket sock) : socket(std::move(sock))
		{
		}

		~tcp_connection()
		{
			async_disconnect(std::chrono::milliseconds(detail::tcp_handshake_timeout), [](auto...) {});
		}

		/**
		 * @brief Get this object hash key, used for connection map
		 */
		inline key_type hash_key() const noexcept
		{
			return reinterpret_cast<key_type>(this);
		}

		/**
		 * @brief Asynchronously graceful disconnect the connection, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_disconnect(
			timeout_duration timeout = std::chrono::milliseconds(detail::tcp_handshake_timeout),
			DisconnectToken&& token = asio::default_token<asio::tcp_socket>())
		{
			return asio::async_disconnect(socket, timeout, std::forward<DisconnectToken>(token));
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
		inline auto async_send(auto&& data, WriteToken&& token = asio::default_token<asio::tcp_socket>())
		{
			return asio::async_send(socket,
				std::forward_like<decltype(data)>(data), std::forward<WriteToken>(token));
		}

		inline void start_idle_check()
		{

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
			error_code ec{};
			return socket.local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the local port number.
		 */
		inline ip::port_type get_local_port() noexcept
		{
			error_code ec{};
			return socket.local_endpoint(ec).port();
		}

		/**
		 * @brief Get the remote address.
		 */
		inline std::string get_remote_address() noexcept
		{
			error_code ec{};
			return socket.remote_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the remote port number.
		 */
		inline ip::port_type get_remote_port() noexcept
		{
			error_code ec{};
			return socket.remote_endpoint(ec).port();
		}

	public:
		asio::tcp_socket      socket;

		asio::steady_timer    idle_timer{ socket.get_executor()};
	};
}
