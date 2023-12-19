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
#include <asio3/tcp/tcps_client.hpp>
#include <asio3/http/core.hpp>
#include <asio3/http/write.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = tcp_socket>
	class basic_https_client : public basic_tcps_client<SocketT>
	{
	public:
		using super = basic_tcps_client<SocketT>;
		using socket_type = SocketT;

		/**
		 * @brief constructor
		 */
		explicit basic_https_client(const auto& ex, ssl::context&& sslctx)
			: super(ex, std::move(sslctx))
		{
		}

		basic_https_client(basic_https_client&&) noexcept = default;
		basic_https_client& operator=(basic_https_client&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_https_client()
		{
			this->close();
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}
	};

	using https_client = basic_https_client<asio::tcp_socket>;
}
