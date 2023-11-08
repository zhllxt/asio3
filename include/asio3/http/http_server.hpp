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

namespace asio
{
	template<typename SessionT = http_session>
	class basic_http_server : public basic_tcp_server<SessionT>
	{
	public:
		using super = basic_tcp_server<SessionT>;
		using request_type = typename SessionT::request_type;
		using response_type = typename SessionT::response_type;

	public:
		/**
		 * @brief constructor
		 */
		explicit basic_http_server(const auto& ex) : super(ex)
		{
		}

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
		std::filesystem::path root_directory{ std::filesystem::current_path() };

		http::basic_router<request_type, response_type> router{};
	};

	using http_server = basic_http_server<http_session>;
}
