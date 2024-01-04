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
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/tcp/disconnect.hpp>
#include <asio3/proxy/handshake.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = tcp_socket>
	class basic_tcp_client
	{
	public:
		using socket_type = SocketT;

		explicit basic_tcp_client(const auto& ex) : socket(ex)
		{
			aborted.clear();
		}

		basic_tcp_client(basic_tcp_client&&) noexcept = default;
		basic_tcp_client& operator=(basic_tcp_client&&) noexcept = default;

		~basic_tcp_client()
		{
			close();
		}

		/**
		 * @brief Asynchronously connect to the server.
		 */
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
		 * @brief Asynchronously disconnect the connection.
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
		 * @brief Set the aborted flag to false.
		 */
		inline void restart() noexcept
		{
			aborted.clear();
		}

		/**
		 * @brief Check whether the client is aborted or not.
		 */
		inline bool is_aborted() noexcept
		{
			return aborted.test();
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

	using tcp_client = basic_tcp_client<asio::tcp_socket>;
}

