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

#include <asio3/http/http_server.hpp>
#include <asio3/http/https_session.hpp>

namespace asio
{
	template<
		typename SessionT = https_session,
		typename RequestT = typename SessionT::request_type,
		typename ResponseT = typename SessionT::response_type
	>
	class basic_https_server : public basic_http_server<SessionT, RequestT, ResponseT>
	{
	public:
		using super = basic_http_server<SessionT, RequestT, ResponseT>;

		explicit basic_https_server(const auto& ex, ssl::context&& sslctx)
			: super(ex)
			, ssl_context(std::move(sslctx))
		{
		}

		~basic_https_server()
		{
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		asio::ssl::context                   ssl_context;
	};

	using https_server = basic_https_server<https_session>;
}
