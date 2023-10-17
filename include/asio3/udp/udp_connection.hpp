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
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/udp/send.hpp>

namespace asio
{
	struct udp_connection_option
	{
		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::udp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::udp_handshake_timeout) };
		timeout_duration      idle_timeout{ std::chrono::milliseconds(detail::udp_idle_timeout) };
	};

	class udp_connection : public std::enable_shared_from_this<udp_connection>
	{
	public:
		using key_type = ip::udp::endpoint;
		using recv_channel_type = experimental::channel<void(asio::error_code)>;
		using recv_notifier_type = as_tuple_t<deferred_t>::as_default_on_t<recv_channel_type>;

	public:
		explicit udp_connection(udp_socket& sock, udp_connection_option opt)
			: socket(sock)
			, option(std::move(opt))
		{
		}

		~udp_connection()
		{
			stop();
		}

		/**
		 * @brief Get this object hash key, used for connection map
		 */
		inline key_type hash_key() const noexcept
		{
			return remote_endpoint;
		}

		/**
		 * @brief Asynchronously graceful disconnect the connection, this function does not block.
		 */
		inline void disconnect() noexcept
		{
			stop();
		}

		/**
		 * @brief shutdown and close the socket directly.
		 */
		inline void stop() noexcept
		{
			asio::cancel_timer(idle_timer);
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data, typename WriteToken = default_udp_send_token>
		inline auto async_send(Data&& data, WriteToken&& token = default_udp_send_token{})
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
			return remote_endpoint.address().to_string(ec);
		}

		/**
		 * @brief Get the remote port number.
		 */
		inline ip::port_type get_remote_port() noexcept
		{
			return remote_endpoint.port();
		}

	public:
		udp_connection_option option{};

		asio::udp_socket&     socket;

		ip::udp::endpoint     remote_endpoint{};

		asio::steady_timer    idle_timer{ socket.get_executor() };

		recv_notifier_type    recv_notifier{ socket.get_executor(), 1 };
	};
}
