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

#include <variant>

#include <asio3/proxy/accept.hpp>
#include <asio3/proxy/tcp_transfer.hpp>
#include <asio3/proxy/udp_transfer.hpp>
#include <asio3/proxy/ext_transfer.hpp>
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

		bool load_backend_client_from_handshake_info()
		{
			if (handshake_info.cmd == socks5::command::connect)
			{
				auto* p = std::any_cast<asio::ip::tcp::socket>(std::addressof(handshake_info.bound_socket));
				if (p)
				{
					backend_client = std::move(*p);
					return true;
				}
			}
			else if (handshake_info.cmd == socks5::command::udp_associate)
			{
				auto* p = std::any_cast<asio::ip::udp::socket>(std::addressof(handshake_info.bound_socket));
				if (p)
				{
					backend_client = std::move(*p);
					return true;
				}
			}
			return false;
		}

	public:
		asio::protocol         last_read_channel{};

		socks5::auth_config    auth_config{};

		socks5::handshake_info handshake_info{};

		std::variant<std::monostate, asio::tcp_socket, asio::udp_socket> backend_client;
	};
}
