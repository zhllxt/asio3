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

namespace asio
{
	template<typename SessionT>
	class http_server_t : public tcp_server_t<SessionT>
	{
	public:
		using super = tcp_server_t<SessionT>;

	public:
		/**
		 * @brief constructor
		 */
		explicit http_server_t(const auto& ex) : super(ex)
		{
		}

		/**
		 * @brief destructor
		 */
		~http_server_t()
		{
		}

	public:
		std::filesystem::path     root_directory{ std::filesystem::current_path() };
	};

	using http_server = http_server_t<http_session>;
}
