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
#include <asio3/tcp/listen.hpp>
#include <asio3/tcp/tcp_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT = tcp_session>
	class basic_tcp_server
	{
	public:
		using session_type = SessionT;
		using socket_type = typename SessionT::socket_type;

		explicit basic_tcp_server(const auto& ex) : acceptor(ex), session_map(ex)
		{
		}

		basic_tcp_server(basic_tcp_server&&) noexcept = default;
		basic_tcp_server& operator=(basic_tcp_server&&) noexcept = default;

		~basic_tcp_server()
		{
		}

		/**
		 * @brief Asynchronously start the server for listen at the address and port.
		 */
		template<typename ListenToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_listen(
			is_string auto&& listen_address,
			is_string_or_integral auto&& listen_port,
			ListenToken&& token = asio::default_token_type<asio::tcp_acceptor>())
		{
			return asio::async_listen(acceptor,
				std::forward_like<decltype(listen_address)>(listen_address),
				std::forward_like<decltype(listen_port)>(listen_port),
				true,
				std::forward<ListenToken>(token));
		}

		/**
		 * @brief Asynchronously stop the server.
		 */
		template<typename StopToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_stop(
			StopToken&& token = asio::default_token_type<asio::tcp_acceptor>())
		{
			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						error_code ec{};
						self.acceptor.close(ec);

						co_await self.session_map.async_disconnect_all(use_nothrow_deferred);

						co_return ec;
					}, acceptor), token, std::ref(*this));
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
		 * @brief Check whether the acceptor is stopped or not.
		 */
		inline bool is_aborted() noexcept
		{
			return !acceptor.is_open();
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return acceptor.get_executor();
		}

		/**
		 * @brief Get the listen address.
		 */
		inline std::string get_listen_address() noexcept
		{
			return asio::get_local_address(acceptor);
		}

		/**
		 * @brief Get the listen port number.
		 */
		inline ip::port_type get_listen_port() noexcept
		{
			return asio::get_local_port(acceptor);
		}

		/**
		 * @brief Get the acceptor.
		 * https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/
		 */
		constexpr inline auto&& get_acceptor(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).acceptor;
		}

		/**
		 * @brief Get the session map.
		 */
		constexpr inline auto&& get_session_map(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).session_map;
		}

	public:
		asio::tcp_acceptor  acceptor;

		session_map<session_type> session_map;
	};

	using tcp_server = basic_tcp_server<tcp_session>;
}
