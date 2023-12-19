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

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT = tcps_session>
	class basic_tcps_server : public basic_tcp_server<SessionT>
	{
	public:
		using super = basic_tcp_server<SessionT>;
		using socket_type = typename SessionT::socket_type;

		explicit basic_tcps_server(const auto& ex, ssl::context&& sslctx)
			: super(ex)
			, ssl_context(std::move(sslctx))
		{
		}

		basic_tcps_server(basic_tcps_server&&) noexcept = default;
		basic_tcps_server& operator=(basic_tcps_server&&) noexcept = default;

		~basic_tcps_server()
		{
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		asio::ssl::context                   ssl_context;
	};

	using tcps_server = basic_tcps_server<tcps_session>;
}
