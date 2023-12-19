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
	class basic_http_session : public basic_tcp_session<SocketT>
	{
	public:
		using super = basic_tcp_session<SocketT>;
		using socket_type = SocketT;
		using request_type = http::web_request;
		using response_type = http::web_response;

		/**
		 * @brief constructor
		 */
		explicit basic_http_session(socket_type sock) : super(std::move(sock))
		{
			this->disconnect_timeout = asio::http_disconnect_timeout;
		}

		basic_http_session(basic_http_session&&) noexcept = default;
		basic_http_session& operator=(basic_http_session&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_http_session()
		{
			this->close();
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}
	};

	using http_session = basic_http_session<asio::tcp_socket>;
}
