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

#include <asio3/core/asio.hpp>
#include <asio3/core/beast.hpp>
#include <asio3/http/core.hpp>
#include <asio3/tcp/tcps_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = tcp_socket>
	class basic_https_session : public basic_tcps_session<SocketT>
	{
	public:
		using super = basic_tcps_session<SocketT>;
		using socket_type = SocketT;
		using request_type = http::web_request;
		using response_type = http::web_response;

		explicit basic_https_session(socket_type sock, ssl::context& sslctx)
			: super(std::move(sock), sslctx)
		{
			this->disconnect_timeout = asio::http_disconnect_timeout;
			this->ssl_shutdown_timeout = asio::http_disconnect_timeout;
		}

		basic_https_session(basic_https_session&&) noexcept = default;
		basic_https_session& operator=(basic_https_session&&) noexcept = default;

		~basic_https_session()
		{
			this->close();
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}
	};

	using https_session = basic_https_session<asio::tcp_socket>;
}
