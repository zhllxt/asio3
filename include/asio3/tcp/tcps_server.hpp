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
#include <asio3/tcp/tcps_session.hpp>

namespace asio
{
	template<typename SessionT>
	class tcps_server_t : public tcp_server_t<SessionT>
	{
	public:
		using super = tcp_server_t<SessionT>;

		explicit tcps_server_t(const auto& ex, ssl::context&& sslctx)
			: super(ex)
			, ssl_context(std::move(sslctx))
		{
		}

		~tcps_server_t()
		{
		}

	public:
		asio::ssl::context                   ssl_context;
	};

	using tcps_server = tcps_server_t<tcps_session>;
}
