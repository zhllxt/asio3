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

#include <asio3/tcp/tcp_session.hpp>

namespace asio
{
	class http_session : public tcp_session
	{
	public:
		using super = tcp_session;

		/**
		 * @brief constructor
		 */
		explicit http_session(tcp_socket sock) : super(std::move(sock))
		{
		}

		/**
		 * @brief destructor
		 */
		~http_session()
		{
			close();
		}

	public:
		bool is_websocket{ false };
	};
}
