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

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	using udp_resolver = as_tuple_t<use_awaitable_t<>>::as_default_on_t<ip::udp::resolver>;
	using udp_socket   = with_lock_t<as_tuple_t<use_awaitable_t<>>>::as_default_on_t<ip::udp::socket>;
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	struct udp_socket_option
	{
		bool reuse_address = true;
	};

	struct default_udp_socket_option_setter
	{
		udp_socket_option option{};

		inline void operator()(auto& sock) noexcept
		{
			asio::error_code ec{};
			sock.set_option(asio::socket_base::reuse_address(option.reuse_address), ec);
		}
	};
}
