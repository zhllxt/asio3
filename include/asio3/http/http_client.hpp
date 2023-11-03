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

#include <fstream>

#include <asio3/core/detail/push_options.hpp>
#include <asio3/tcp/tcp_client.hpp>

namespace asio
{
	class http_client : public tcp_client
	{
	public:
		using super = tcp_client;

		/**
		 * @brief constructor
		 */
		explicit http_client(const auto& ex) : super(ex)
		{
		}

		/**
		 * @brief destructor
		 */
		~http_client()
		{
			close();
		}
	};
}

#include <asio3/core/detail/pop_options.hpp>
