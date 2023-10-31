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
#include <asio3/tcp/tcp_connection.hpp>

namespace asio
{
	class socks5_connection : public tcp_connection
	{
	public:
		using super = tcp_connection;

		explicit socks5_connection(tcp_socket sock) : tcp_connection(std::move(sock))
		{
		}

		~socks5_connection()
		{
		}

		/**
		 * @brief Safety start an asynchronous operation to transfer data between front and back.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec);
		 */
		template<typename WriteToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_transfer(
			WriteToken&& token = asio::default_token_type<asio::tcp_socket>())
		{
			return asio::async_send(socket,
				std::forward_like<decltype(data)>(data), std::forward<WriteToken>(token));
		}

	public:
		asio::protocol         last_read_channel{};

		socks5::auth_config    auth_config{};

		socks5::handshake_info handshake_info{};
	};
}
