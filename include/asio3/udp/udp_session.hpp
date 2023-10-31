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
	class udp_session : public std::enable_shared_from_this<udp_session>
	{
	public:
		using key_type = ip::udp::endpoint;

	public:
		explicit udp_session(udp_socket& sock, ip::udp::endpoint remote_ep)
			: socket(sock)
			, remote_endpoint(std::move(remote_ep))
		{
		}

		~udp_session()
		{
		}

		/**
		 * @brief Get this object hash key, used for connection map
		 */
		inline key_type hash_key() noexcept
		{
			return remote_endpoint;
		}

		/**
		 * @brief Asynchronously graceful disconnect the connection, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<asio::udp_socket>>
		inline auto async_disconnect(
			DisconnectToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			return asio::async_initiate<DisconnectToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						asio::cancel_timer(self.alive_timer);

						co_return error_code{};
					}, socket), token, std::ref(*this));
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
			auto&& data,
			WriteToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			return asio::async_send_to(socket,
				std::forward_like<decltype(data)>(data),
				remote_endpoint,
				std::forward<WriteToken>(token));
		}

		/**
		 * @brief Start a asynchronously idle timeout check operation.
		 * @param idle_timeout - The idle timeout.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec);
		 */
		template<typename CheckToken = asio::default_token_type<asio::udp_socket>>
		inline auto async_wait_error_or_idle_timeout(
			std::chrono::system_clock::duration idle_timeout = asio::udp_idle_timeout,
			CheckToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			return asio::async_wait_error_or_idle_timeout(
				socket, alive_timer, alive_time, idle_timeout,
				std::forward<CheckToken>(token));
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
		asio::udp_socket&     socket;

		ip::udp::endpoint     remote_endpoint{};

		asio::steady_timer    alive_timer{ socket.get_executor() };

		std::chrono::system_clock::time_point alive_time{ std::chrono::system_clock::now() };
	};
}
