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
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/udp/send.hpp>
#include <asio3/udp/disconnect.hpp>

namespace asio
{
	class udp_connection : public std::enable_shared_from_this<udp_connection>
	{
	public:
		using key_type = ip::udp::endpoint;

	public:
		explicit udp_connection(udp_socket& sock) : socket(sock)
		{
		}

		~udp_connection()
		{
		}

		/**
		 * @brief Get this object hash key, used for connection map
		 */
		inline key_type hash_key(this auto&& self) noexcept
		{
			return self.remote_endpoint;
		}

		/**
		 * @brief Asynchronously graceful disconnect the connection, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<asio::udp_socket>>
		inline auto async_disconnect(
			this auto&& self,
			DisconnectToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			return asio::async_disconnect(self.get_socket(), std::forward<DisconnectToken>(token));
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename WriteToken = asio::default_token_type<asio::udp_socket>>
		inline auto async_send(
			this auto&& self,
			auto&& data,
			WriteToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			return asio::async_send(self.get_socket(),
				std::forward_like<decltype(data)>(data), std::forward<WriteToken>(token));
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor(this auto&& self) noexcept
		{
			return self.get_socket().get_executor();
		}

		/**
		 * @brief Get the local address.
		 */
		inline std::string get_local_address(this auto&& self) noexcept
		{
			error_code ec{};
			return self.get_socket().local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the local port number.
		 */
		inline ip::port_type get_local_port(this auto&& self) noexcept
		{
			error_code ec{};
			return self.get_socket().local_endpoint(ec).port();
		}

		/**
		 * @brief Get the remote address.
		 */
		inline std::string get_remote_address(this auto&& self) noexcept
		{
			error_code ec{};
			return self.remote_endpoint.address().to_string(ec);
		}

		/**
		 * @brief Get the remote port number.
		 */
		inline ip::port_type get_remote_port(this auto&& self) noexcept
		{
			return self.remote_endpoint.port();
		}

		/**
		 * @brief Get the socket.
		 * https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/
		 */
		constexpr inline auto&& get_socket(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).socket;
		}

	public:
		asio::udp_socket&     socket;

		ip::udp::endpoint     remote_endpoint{};
	};
}
