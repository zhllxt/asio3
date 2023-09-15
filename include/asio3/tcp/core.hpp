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

namespace asio
{
	using tcp_acceptor = as_tuple_t<deferred_t>::as_default_on_t<ip::tcp::acceptor>;
	using tcp_resolver = as_tuple_t<deferred_t>::as_default_on_t<ip::tcp::resolver>;
	using tcp_socket   = as_tuple_t<deferred_t>::as_default_on_t<ip::tcp::socket>;
}

namespace asio
{
	struct tcp_socket_option
	{
		bool reuse_address = true;
		bool keep_alive = true;
		bool no_delay = true;
	};
}
