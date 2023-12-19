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
#include <asio3/proxy/socks5_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT>
	class basic_socks5_server : public basic_tcp_server<SessionT>
	{
	public:
		using super = basic_tcp_server<SessionT>;
		using socket_type = typename SessionT::socket_type;

		explicit basic_socks5_server(const auto& ex) : super(ex)
		{
		}

		~basic_socks5_server()
		{
		}

	public:
	};

	using socks5_server = basic_socks5_server<socks5_session>;
}
