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
#include <asio3/http/http_session.hpp>
#include <asio3/http/router.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<
		typename SessionT = http_session,
		typename RouterT = http::basic_router<typename SessionT::request_type, typename SessionT::response_type>
	>
	class basic_http_server : public basic_tcp_server<SessionT>
	{
	public:
		using super = basic_tcp_server<SessionT>;
		using request_type = typename SessionT::request_type;
		using response_type = typename SessionT::response_type;

		explicit basic_http_server(const auto& ex) : super(ex)
		{
		}

		basic_http_server(basic_http_server&&) noexcept = default;
		basic_http_server& operator=(basic_http_server&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_http_server()
		{
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		std::filesystem::path webroot{ std::filesystem::current_path() };

		RouterT router{};
	};

	using http_server = basic_http_server<http_session>;
}
