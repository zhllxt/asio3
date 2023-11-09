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

#include <asio3/http/httpws_server.hpp>
#include <asio3/http/https_session.hpp>
#include <asio3/http/wss_session.hpp>

namespace asio
{
	template<
		typename HttpsSessionT = https_session,
		typename WssSessionT = wss_session,
		typename RequestT = typename HttpsSessionT::request_type,
		typename ResponseT = typename HttpsSessionT::response_type
	>
	class basic_httpwss_server : public basic_httpws_server<HttpsSessionT, WssSessionT, RequestT, ResponseT>
	{
	public:
		using super = basic_httpws_server<HttpsSessionT, WssSessionT, RequestT, ResponseT>;
		using http_session_type = HttpsSessionT;
		using ws_session_type = WssSessionT;
		using https_session_type = HttpsSessionT;
		using wss_session_type = WssSessionT;

	public:
		/**
		 * @brief constructor
		 */
		explicit basic_httpwss_server(const auto& ex, ssl::context&& sslctx)
			: super(ex)
			, ssl_context(std::move(sslctx))
		{
		}

		/**
		 * @brief destructor
		 */
		~basic_httpwss_server()
		{
		}

	public:
		asio::ssl::context                   ssl_context;
	};

	using httpwss_server = basic_httpwss_server<https_session, wss_session>;
}
