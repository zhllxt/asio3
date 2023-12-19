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

#include <asio3/tcp/tcps_server.hpp>
#include <asio3/http/wss_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT = wss_session>
	class basic_wss_server : public basic_tcps_server<SessionT>
	{
	public:
		using super = basic_tcps_server<SessionT>;
		using socket_type = typename SessionT::socket_type;

		explicit basic_wss_server(const auto& ex, ssl::context&& sslctx)
			: super(ex, std::move(sslctx))
		{
		}

		basic_wss_server(basic_wss_server&&) noexcept = default;
		basic_wss_server& operator=(basic_wss_server&&) noexcept = default;

		~basic_wss_server()
		{
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}
	};

	using wss_server = basic_wss_server<wss_session>;
}
