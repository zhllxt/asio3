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

#include <asio3/core/io_context_thread.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/udp/send.hpp>
#include <asio3/udp/connect.hpp>
#include <asio3/udp/disconnect.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/proxy/handshake.hpp>
#include <asio3/proxy/parser.hpp>
#include <asio3/proxy/udp_header.hpp>

namespace asio
{
	struct udp_connect_option
	{
		std::string           server_address{};
		std::uint16_t         server_port{};

		std::string           bind_address{};
		std::uint16_t         bind_port{ 0 };

		bool                  reuse_address{ true };
	};

	class udp_client
	{
	public:
		explicit udp_client(const auto& ex) : socket(ex)
		{
			aborted.clear();
		}

		~udp_client()
		{
		}

		template<typename ConnectToken = asio::default_token_type<asio::udp_socket>>
		inline auto async_connect(
			this auto&& self,
			udp_connect_option opt,
			ConnectToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			udp_socket_option socket_opt{
				.reuse_address = opt.reuse_address,
			};
			return asio::async_connect(self.get_socket(),
				std::move(opt.server_address),
				opt.server_port,
				std::move(opt.bind_address),
				opt.bind_port,
				asio::default_udp_socket_option_setter{ .option = socket_opt },
				std::forward<ConnectToken>(token));
		}

		/**
		 * @brief Abort the object, disconnect the connection, this function does not block.
		 */
		template<typename StopToken = asio::default_token_type<asio::udp_socket>>
		inline auto async_stop(
			this auto&& self,
			StopToken&& token = asio::default_token_type<asio::udp_socket>())
		{
			self.aborted.test_and_set();

			return asio::async_disconnect(self.get_socket(), std::forward<StopToken>(token));
		}

		/**
		 * @brief Set the aborted flag to false.
		 */
		inline void restart(this auto&& self) noexcept
		{
			self.aborted.clear();
		}

		/**
		 * @brief Check whether the client is aborted or not.
		 */
		inline bool is_aborted(this auto&& self) noexcept
		{
			return self.aborted.test();
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
				std::forward_like<decltype(data)>(data),
				std::forward<WriteToken>(token));
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
			return self.get_socket().remote_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the remote port number.
		 */
		inline ip::port_type get_remote_port(this auto&& self) noexcept
		{
			error_code ec{};
			return self.get_socket().remote_endpoint(ec).port();
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
		asio::udp_socket  socket;

		std::atomic_flag  aborted{};
	};
}

