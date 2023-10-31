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

#include <asio3/tcp/tcp_server.hpp>
#include <asio3/proxy/socks5_connection.hpp>

namespace asio
{
	template<typename ConnectionT>
	class socks5_server_t : public tcp_server_t<ConnectionT>
	{
	public:
		using super = tcp_server_t<ConnectionT>;

		explicit socks5_server_t(const auto& ex) : super(ex)
		{
		}

		~socks5_server_t()
		{
		}

	public:
	};

	using socks5_server = socks5_server_t<socks5_connection>;
}
