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

/*
 * @see /asio/read_until.hpp
 * 
 * @par Examples
 * To asynchronously read data into a @c std::string until whitespace is
 * encountered:
 * @code typedef asio::buffers_iterator<
 *     asio::const_buffers_1> iterator;
 *
 * std::pair<iterator, bool>
 * match_whitespace(iterator begin, iterator end)
 * {
 *   iterator i = begin;
 *   while (i != end)
 *     if (std::isspace(*i++))
 *       return std::make_pair(i, true);
 *   return std::make_pair(i, false);
 * }
 * ...
 * void handler(const asio::error_code& e, std::size_t size);
 * ...
 * std::string data;
 * asio::async_read_until(s, data, match_whitespace, handler);
 * @endcode
 *
 * To asynchronously read data into a @c std::string until a matching character
 * is found:
 * @code class match_char
 * {
 * public:
 *   explicit match_char(char c) : c_(c) {}
 *
 *   template <typename Iterator>
 *   std::pair<Iterator, bool> operator()(
 *       Iterator begin, Iterator end) const
 *   {
 *     Iterator i = begin;
 *     while (i != end)
 *       if (c_ == *i++)
 *         return std::make_pair(i, true);
 *     return std::make_pair(i, false);
 *   }
 *
 * private:
 *   char c_;
 * };
 *
 * namespace asio {
 *   template <> struct is_match_condition<match_char>
 *     : public boost::true_type {};
 * } // namespace asio
 * ...
 * void handler(const asio::error_code& e, std::size_t size);
 * ...
 * std::string data;
 * asio::async_read_until(s, data, match_char('a'), handler);
 * @endcode
 */

#ifdef ASIO_STANDALONE
namespace asio::socks5
#else
namespace boost::asio::socks5
#endif
{
struct udp_match_condition
{
	template <typename Iterator>
	std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const
	{
		using difference_type = typename Iterator::difference_type;

		// +----+------+------+----------+----------+----------+
		// |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
		// +----+------+------+----------+----------+----------+
		// | 2  |  1   |  1   | Variable |    2     | Variable |
		// +----+------+------+----------+----------+----------+

		for (Iterator p = begin; p < end;)
		{
			if (end - p < static_cast<difference_type>(5))
				break;

			std::uint16_t data_size = asio::network_to_host(
				std::uint16_t(*(reinterpret_cast<const std::uint16_t*>(p.operator->()))));

			std::uint8_t atyp = std::uint8_t(p[3]);

			difference_type need = 0;

			// ATYP
			if /**/ (atyp == std::uint8_t(0x01))
				need = static_cast<difference_type>(2 + 1 + 1 + 4 + 2 + data_size);
			else if (atyp == std::uint8_t(0x03))
				need = static_cast<difference_type>(2 + 1 + 1 + p[4] + 2 + data_size);
			else if (atyp == std::uint8_t(0x04))
				need = static_cast<difference_type>(2 + 1 + 1 + 16 + 2 + data_size);
			else
				return std::pair(begin, true);

			if (end - p < need)
				break;

			return std::pair(p + need, true);
		}

		return std::pair(begin, false);
	}
};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template <> struct is_match_condition<socks5::udp_match_condition> : public std::true_type {};
}
