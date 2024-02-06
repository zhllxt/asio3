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

#include <bit>

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
namespace asio
#else
namespace boost::asio
#endif
{
struct length_payload_match_condition
{
	template <typename Iterator>
	std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const
	{
		using difference_type = typename Iterator::difference_type;

		for (Iterator p = begin; p < end;)
		{
			// If 0~253, current byte are the payload length.
			if (std::uint8_t(*p) < std::uint8_t(254))
			{
				std::uint8_t payload_size = static_cast<std::uint8_t>(*p);

				++p;

				if (end - p < static_cast<difference_type>(payload_size))
					break;

				return std::pair(p + static_cast<difference_type>(payload_size), true);
			}

			// If 254, the following 2 bytes interpreted as a 16-bit unsigned integer
			// are the payload length.
			if (std::uint8_t(*p) == std::uint8_t(254))
			{
				++p;

				if (end - p < 2)
					break;

				std::uint16_t payload_size = *(reinterpret_cast<const std::uint16_t*>(p.operator->()));

				// use little endian
				if constexpr (std::endian::native == std::endian::big)
				{
					swap_bytes<sizeof(std::uint16_t)>(reinterpret_cast<std::uint8_t*>(
						std::addressof(payload_size)));
				}

				// illegal data
				if (payload_size < static_cast<std::uint16_t>(254))
					return std::pair(begin, true);

				p += 2;
				if (end - p < static_cast<difference_type>(payload_size))
					break;

				return std::pair(p + static_cast<difference_type>(payload_size), true);
			}

			// If 255, the following 8 bytes interpreted as a 64-bit unsigned integer
			// (the most significant bit MUST be 0) are the payload length.
			if (std::uint8_t(*p) == 255)
			{
				++p;

				if (end - p < 8)
					break;

				// the most significant bit MUST be 0
				std::int64_t payload_size = *(reinterpret_cast<const std::int64_t*>(p.operator->()));

				// use little endian
				if constexpr (std::endian::native == std::endian::big)
				{
					swap_bytes<sizeof(std::int64_t)>(reinterpret_cast<std::uint8_t*>(
						std::addressof(payload_size)));
				}

				// illegal data
				if (payload_size <= static_cast<std::int64_t>((std::numeric_limits<std::uint16_t>::max)()))
					return std::pair(begin, true);

				p += 8;
				if (end - p < static_cast<difference_type>(payload_size))
					break;

				return std::pair(p + static_cast<difference_type>(payload_size), true);
			}

			assert(false);
		}

		return std::pair(begin, false);
	}

	inline static std::string_view get_payload(std::string_view buffer)
	{
		if (buffer.empty())
		{
			return {};
		}
		else if (std::uint8_t(buffer[0]) < std::uint8_t(254))
		{
			return buffer.substr(1);
		}
		else if (std::uint8_t(buffer[0]) == std::uint8_t(254))
		{
			return buffer.substr(1 + 2);
		}
		else
		{
			assert(std::uint8_t(buffer[0]) == std::uint8_t(255));
			return buffer.substr(1 + 8);
		}
	}

	inline static std::string_view get_payload(const auto* p, auto size)
	{
		return get_payload(std::string_view{
			reinterpret_cast<std::string_view::const_pointer>(p),
			static_cast<std::string_view::size_type>(size)
			});
	}
};

template <> struct is_match_condition<length_payload_match_condition> : public std::true_type {};
}
