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
#include <asio3/core/session_map.hpp>
#include <asio3/udp/open.hpp>
#include <asio3/udp/udp_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT = udp_session>
	class basic_udp_server
	{
	public:
		using session_type = SessionT;
		using socket_type = typename SessionT::socket_type;

		explicit basic_udp_server(const auto& ex)
			: socket(ex)
			, session_map(ex)
		{
		}

		basic_udp_server(basic_udp_server&&) noexcept = default;
		basic_udp_server& operator=(basic_udp_server&&) noexcept = default;

		~basic_udp_server()
		{
		}

		template<typename OpenToken = asio::default_token_type<socket_type>>
		inline auto async_open(
			is_string auto&& listen_address, is_string_or_integral auto&& listen_port,
			OpenToken&& token = asio::default_token_type<socket_type>())
		{
			return asio::async_open(socket,
				std::forward_like<decltype(listen_address)>(listen_address),
				std::forward_like<decltype(listen_port)>(listen_port),
				std::forward<OpenToken>(token));
		}

		/**
		 * @brief Stop the server, this function does not block.
		 */
		template<typename StopToken = asio::default_token_type<socket_type>>
		inline auto async_stop(
			StopToken&& token = asio::default_token_type<socket_type>())
		{
			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto server_ref) -> void
					{
						auto& server = server_ref.get();

						co_await asio::dispatch(
							asio::detail::get_lowest_executor(server.socket), use_nothrow_deferred);

						state.reset_cancellation_state(asio::enable_terminal_cancellation());

						error_code ec{};
						server.socket.shutdown(asio::socket_base::shutdown_both, ec);
						server.socket.close(ec);
						asio::reset_lock(server.socket);

						co_await server.session_map.async_disconnect_all(use_nothrow_deferred);

						co_return ec;
					}, socket),
				token, std::ref(*this));
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data to all clients.
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
			return session_map.async_send_all(
				std::forward_like<decltype(data)>(data),
				std::forward<WriteToken>(token));
		}

		/**
		 * @brief Check whether the socket is stopped or not.
		 */
		inline bool is_aborted() noexcept
		{
			return !socket.is_open();
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline auto get_executor() noexcept
		{
			return asio::detail::get_lowest_executor(socket);
		}

		/**
		 * @brief Get the listen address.
		 */
		inline std::string get_listen_address() noexcept
		{
			return asio::get_local_address(socket);
		}

		/**
		 * @brief Get the listen port number.
		 */
		inline ip::port_type get_listen_port() noexcept
		{
			return asio::get_local_port(socket);
		}

		/**
		 * @brief Get the socket
		 * https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/
		 */
		constexpr inline auto&& get_socket(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).socket;
		}

		/**
		 * @brief Get the session map.
		 */
		constexpr inline auto&& get_session_map(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).session_map;
		}

	public:
		socket_type    socket;

		session_map<session_type> session_map;
	};

	using udp_server = basic_udp_server<udp_session>;
}
