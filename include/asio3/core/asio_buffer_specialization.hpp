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

#include <array>
#include <utility>

#include <asio3/core/asio.hpp>
#include <asio3/core/fixed_capacity_vector.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template <typename T, std::size_t Capacity>
	inline ASIO_MUTABLE_BUFFER buffer(std::experimental::fixed_capacity_vector<T, Capacity>& data) ASIO_NOEXCEPT
	{
		return asio::buffer((void*)(data.data()), sizeof(T) * data.size());
	}

	template <typename T, std::size_t Capacity>
	inline ASIO_CONST_BUFFER buffer(const std::experimental::fixed_capacity_vector<T, Capacity>& data) ASIO_NOEXCEPT
	{
		return asio::buffer((const void*)(data.data()), sizeof(T) * data.size());
	}

	template <typename T1, typename T2>
	inline auto buffer(std::pair<T1, T2>& data) ASIO_NOEXCEPT
	{
		// asio::buffer(std::basic_string_view<...>) == ASIO_CONST_BUFFER
		return std::array<asio::const_buffer, 2>
		{
			asio::const_buffer(asio::buffer(data.first)),
			asio::const_buffer(asio::buffer(data.second))
		};
	}

	template <typename T1, typename T2>
	inline auto buffer(const std::pair<T1, T2>& data) ASIO_NOEXCEPT
	{
		return std::array<ASIO_CONST_BUFFER, 2>{ asio::buffer(data.first), asio::buffer(data.second)};
	}
}
