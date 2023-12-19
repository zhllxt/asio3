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
#include <asio3/http/flex_wss_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<
		typename HttpsSessionT = https_session,
		typename WssSessionT = flex_wss_session,
		typename RouterT = http::basic_router<
			typename HttpsSessionT::request_type, typename HttpsSessionT::response_type>
	>
	class basic_httpwss_server : public basic_httpws_server<HttpsSessionT, WssSessionT, RouterT>
	{
	public:
		using super = basic_httpws_server<HttpsSessionT, WssSessionT, RouterT>;
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

		basic_httpwss_server(basic_httpwss_server&&) noexcept = default;
		basic_httpwss_server& operator=(basic_httpwss_server&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_httpwss_server()
		{
		}

	public:
		asio::ssl::context                   ssl_context;
	};

	using httpwss_server = basic_httpwss_server<https_session, flex_wss_session>;
}
