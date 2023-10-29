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

#include <string_view>
#include <tuple>

#include <asio3/proxy/core.hpp>
#include <asio3/core/netutil.hpp>

namespace asio::socks5
{
namespace
{
	using iterator = asio::buffers_iterator<asio::streambuf::const_buffers_type>;
	using diff_type = typename iterator::difference_type;

	std::pair<iterator, bool> udp_match_condition(iterator begin, iterator end) noexcept
	{
		// +----+------+------+----------+----------+----------+
		// |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
		// +----+------+------+----------+----------+----------+
		// | 2  |  1   |  1   | Variable |    2     | Variable |
		// +----+------+------+----------+----------+----------+

		for (iterator p = begin; p < end;)
		{
			if (end - p < static_cast<diff_type>(5))
				break;

			std::uint16_t data_size = asio::network_to_host(
				std::uint16_t(*(reinterpret_cast<const std::uint16_t*>(p.operator->()))));

			std::uint8_t atyp = std::uint8_t(p[3]);

			diff_type need = 0;

			// ATYP
			if /**/ (atyp == std::uint8_t(0x01))
				need = static_cast<diff_type>(2 + 1 + 1 + 4 + 2 + data_size);
			else if (atyp == std::uint8_t(0x03))
				need = static_cast<diff_type>(2 + 1 + 1 + p[4] + 2 + data_size);
			else if (atyp == std::uint8_t(0x04))
				need = static_cast<diff_type>(2 + 1 + 1 + 16 + 2 + data_size);
			else
				return std::pair(begin, true);

			if (end - p < need)
				break;

			return std::pair(p + need, true);
		}
		return std::pair(begin, false);
	}
}
}
