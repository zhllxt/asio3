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

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT, typename BoundSocketT = udp_socket>
	class basic_socks5_session : public basic_tcp_session<SocketT>
	{
	public:
		using super = basic_tcp_session<SocketT>;
		using socket_type = SocketT;
		using bound_socket_type = BoundSocketT;

		explicit basic_socks5_session(socket_type sock) : super(std::move(sock))
		{
		}

		~basic_socks5_session()
		{
		}

		/**
		 * @brief Asynchronously graceful disconnect the session, this function does not block.
		 */
		template<typename DisconnectToken = asio::default_token_type<socket_type>>
		inline auto async_disconnect(
			DisconnectToken&& token = asio::default_token_type<socket_type>())
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
					}, this->socket), token, std::ref(*this));
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

		inline socket_type* get_backend_tcp_socket() noexcept
		{
			return std::any_cast<socket_type>(std::addressof(handshake_info.bound_socket));
		}

		inline bound_socket_type* get_backend_udp_socket() noexcept
		{
			return std::any_cast<bound_socket_type>(std::addressof(handshake_info.bound_socket));
		}

		inline asio::ip::udp::endpoint get_frontend_udp_endpoint() noexcept
		{
			error_code ec{};
			return { this->socket.remote_endpoint(ec).address(), handshake_info.dest_port };
		}

	public:
		asio::protocol         last_read_channel{};

		socks5::auth_config    auth_config{};

		socks5::handshake_info handshake_info{};
	};

	using socks5_session = basic_socks5_session<asio::tcp_socket>;
}
