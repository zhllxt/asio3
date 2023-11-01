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

#include <asio3/proxy/accept.hpp>
#include <asio3/proxy/forward.hpp>
#include <asio3/tcp/tcp_session.hpp>
#include <asio3/udp/udp_session.hpp>

namespace asio
{
	class socks5_session : public tcp_session
	{
	public:
		using super = tcp_session;

		explicit socks5_session(tcp_socket sock) : tcp_session(std::move(sock))
		{
		}

		~socks5_session()
		{
		}

		/**
		 * @brief Asynchronously graceful disconnect the session, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_disconnect(
			DisconnectToken&& token = asio::default_token_type<asio::tcp_socket>())
		{
			return asio::async_initiate<DisconnectToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						co_await self.base().async_disconnect(use_nothrow_deferred);

						if /**/ (auto* p1 = self.get_backend_tcp_socket(); p1)
							co_await asio::async_disconnect(*p1, use_nothrow_deferred);
						else if (auto* p2 = self.get_backend_udp_socket(); p2)
							co_await asio::async_disconnect(*p2, use_nothrow_deferred);

						co_return error_code{};
					}, socket), token, std::ref(*this));
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

		inline asio::tcp_socket* get_backend_tcp_socket() noexcept
		{
			return std::any_cast<asio::tcp_socket>(std::addressof(handshake_info.bound_socket));
		}

		inline asio::udp_socket* get_backend_udp_socket() noexcept
		{
			return std::any_cast<asio::udp_socket>(std::addressof(handshake_info.bound_socket));
		}

		inline asio::ip::udp::endpoint get_frontend_udp_endpoint() noexcept
		{
			error_code ec{};
			return { socket.remote_endpoint(ec).address(), handshake_info.dest_port };
		}

	public:
		asio::protocol         last_read_channel{};

		socks5::auth_config    auth_config{};

		socks5::handshake_info handshake_info{};
	};
}
