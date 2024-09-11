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
#include <asio3/udp/connect.hpp>
#include <asio3/udp/disconnect.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/proxy/handshake.hpp>
#include <asio3/proxy/parser.hpp>
#include <asio3/proxy/udp_header.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = udp_socket>
	class basic_udp_client
	{
	public:
		using socket_type = SocketT;

		explicit basic_udp_client(const auto& ex) : socket(ex)
		{
			aborted.clear();
		}

		basic_udp_client(basic_udp_client&&) noexcept = default;
		basic_udp_client& operator=(basic_udp_client&&) noexcept = default;

		~basic_udp_client()
		{
			close();
		}

		template<typename ConnectToken = asio::default_token_type<socket_type>>
		inline auto async_connect(
			is_string auto&& server_address, is_string_or_integral auto&& server_port,
			ConnectToken&& token = asio::default_token_type<socket_type>())
		{
			return asio::async_connectex(socket,
				std::forward_like<decltype(server_address)>(server_address),
				std::forward_like<decltype(server_port)>(server_port),
				std::forward<ConnectToken>(token));
		}

		/**
		 * @brief Abort the object, disconnect the session, this function does not block.
		 */
		template<typename StopToken = asio::default_token_type<socket_type>>
		inline auto async_stop(
			StopToken&& token = asio::default_token_type<socket_type>())
		{
			aborted.test_and_set();

			return asio::async_disconnect(socket, std::forward<StopToken>(token));
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
			return asio::async_send(socket,
				std::forward_like<decltype(data)>(data),
				std::forward<WriteToken>(token));
		}

		/**
		 * @brief shutdown and close the socket directly.
		 */
		inline void close()
		{
			if (socket.is_open())
			{
				assert(socket.get_executor().running_in_this_thread());
			}

			asio::error_code ec{};
			socket.shutdown(asio::socket_base::shutdown_both, ec);
			socket.close(ec);
			asio::reset_lock(socket);
		}

		/**
		 * @brief Set the aborted flag to false.
		 */
		inline void restart() noexcept
		{
			aborted.clear();
		}

		/**
		 * @brief Check whether the client is aborted or not.
		 */
		[[nodiscard]] inline bool is_aborted() noexcept
		{
			return aborted.test();
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
			return std::forward_like<decltype(self)>(self).socket;
		}

		/**
		 * @brief Get the socket.
		 */
		constexpr inline auto&& get_stream(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).socket;
		}

	public:
		socket_type       socket;

		std::atomic_flag  aborted{};
	};

	using udp_client = basic_udp_client<asio::udp_socket>;
}

