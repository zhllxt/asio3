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
#include <asio3/core/with_lock.hpp>
#include <asio3/core/impl/with_lock.hpp>

namespace asio
{
	using udp_resolver = as_tuple_t<deferred_t>::as_default_on_t<ip::udp::resolver>;
	using udp_socket   = with_lock_t<as_tuple_t<deferred_t>>::as_default_on_t<ip::udp::socket>;
}

namespace asio
{
	struct udp_socket_option
	{
		bool reuse_address = true;
		bool keep_alive    = true;
	};

	struct default_udp_socket_option_setter
	{
		udp_socket_option option{};

		inline void operator()(auto& sock) noexcept
		{
			asio::error_code ec{};
			sock.set_option(asio::socket_base::reuse_address(option.reuse_address), ec);
			sock.set_option(asio::socket_base::keep_alive(option.keep_alive), ec);
		}
	};

	struct default_udp_send_token
	{
		inline void operator()(const asio::error_code&, std::size_t) noexcept
		{
		}
	};
}
