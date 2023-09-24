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
#include <asio3/core/detail/netutil.hpp>
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>
#include <asio3/tcp/send.hpp>
#include <asio3/tcp/disconnect.hpp>

namespace asio
{
	struct tcp_connection_option
	{
		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::tcp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::tcp_handshake_timeout) };
		timeout_duration      idle_timeout{ std::chrono::milliseconds(detail::tcp_idle_timeout) };

		tcp_socket_option     socket_option{};
	};

	class tcp_connection : public std::enable_shared_from_this<tcp_connection>
	{
	public:
		using key_type = std::size_t;

	public:
		explicit tcp_connection(tcp_socket sock, tcp_connection_option opt)
			: socket(std::move(sock))
			, option(std::move(opt))
		{
			default_tcp_socket_option_setter{ option.socket_option }(socket);
		}

		~tcp_connection()
		{
			stop();
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
		inline void disconnect() noexcept
		{
			if (socket.is_open())
			{
				asio::async_disconnect(socket, option.disconnect_timeout, [](auto) {});
			}
		}

		/**
		 * @brief shutdown and close the socket directly.
		 */
		inline void stop() noexcept
		{
			if (socket.is_open())
			{
				asio::post(socket.get_executor(), [this]() mutable
				{
					error_code ec{};
					socket.shutdown(socket_base::shutdown_both, ec);
					socket.close(ec);
				});
			}
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data, typename WriteToken = default_tcp_write_token>
		inline auto async_send(Data&& data, WriteToken&& token = default_tcp_write_token{})
		{
			return asio::async_send(socket, std::forward<Data>(data), std::forward<WriteToken>(token));
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
		tcp_connection_option option{};

		asio::tcp_socket      socket;
	};
}
