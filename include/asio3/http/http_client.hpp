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
#include <asio3/http/core.hpp>
#include <asio3/http/read.hpp>
#include <asio3/http/write.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = tcp_socket>
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

		basic_http_client(basic_http_client&&) noexcept = default;
		basic_http_client& operator=(basic_http_client&&) noexcept = default;

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
