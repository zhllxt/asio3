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

#include <asio3/tcp/tcp_client.hpp>

namespace asio
{
	template<typename SocketT>
	class basic_http_client : public basic_tcp_client<SocketT>
	{
	public:
		using super = basic_tcp_client<SocketT>;
		using socket_type = SocketT;

		/**
		 * @brief constructor
		 */
		explicit basic_http_client(const auto& ex) : super(ex)
		{
		}

		/**
		 * @brief destructor
		 */
		~basic_http_client()
		{
			this->close();
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}
	};

	using http_client = basic_http_client<asio::tcp_socket>;
}