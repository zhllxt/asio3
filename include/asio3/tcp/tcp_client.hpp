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
#include <asio3/tcp/core.hpp>
#include <asio3/socks5/core.hpp>

namespace asio
{
	struct tcp_client_option
	{
		std::string                              server_address{};
		std::uint16_t                            server_port{};

		tcp_socket_option                        socket_option{};

		std::optional<asio::any_io_executor>     executor{};

		std::optional<socks5::option>            socks5_option{};
	};

	class tcp_client
	{
	public:
		explicit tcp_client(tcp_client_option opt) : option_(std::move(opt))
		{

		}

		bool start()
		{
			return true;
		}

		void run()
		{
		}

	protected:
		tcp_client_option option_{};
	};
}
