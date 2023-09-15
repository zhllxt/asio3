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
	using udp_resolver = as_tuple_t<deferred_t>::as_default_on_t<ip::udp::resolver>;
	using udp_socket   = as_tuple_t<deferred_t>::as_default_on_t<ip::udp::socket>;
}
