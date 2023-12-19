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
#include <asio3/http/ws_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT = ws_session>
	class basic_ws_server : public basic_tcp_server<SessionT>
	{
	public:
		using super = basic_tcp_server<SessionT>;
		using socket_type = typename SessionT::socket_type;

		explicit basic_ws_server(const auto& ex) : super(ex)
		{
		}

		basic_ws_server(basic_ws_server&&) noexcept = default;
		basic_ws_server& operator=(basic_ws_server&&) noexcept = default;

		~basic_ws_server()
		{
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}
	};

	using ws_server = basic_ws_server<ws_session>;
}
