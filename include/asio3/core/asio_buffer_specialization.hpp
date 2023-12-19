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
	namespace detail
	{
		template<class T, class U = ::std::remove_cvref_t<T>>
		concept is_char =
			::std::is_same_v<U, char    > ||
			::std::is_same_v<U, wchar_t > ||
			::std::is_same_v<U, char8_t > ||
			::std::is_same_v<U, char16_t> ||
			::std::is_same_v<U, char32_t>;
	}

	// asio::buffer("abc")
	template<class CharT, class Traits = ::std::char_traits<CharT>>
	requires detail::is_char<CharT>
	inline asio::const_buffer buffer(CharT*& data) noexcept
	{
		return asio::buffer(std::basic_string_view<CharT>(data));
	}

	template<class CharT, class Traits = ::std::char_traits<CharT>>
	requires detail::is_char<CharT>
	inline asio::const_buffer buffer(const CharT*& data) noexcept
	{
		return asio::buffer(std::basic_string_view<CharT>(data));
	}

	template<class CharT, class Traits = ::std::char_traits<CharT>>
	requires detail::is_char<CharT>
	inline asio::const_buffer buffer(CharT* const& data) noexcept
	{
		return asio::buffer(std::basic_string_view<CharT>(data));
	}

	template<class CharT, class Traits = ::std::char_traits<CharT>>
	requires detail::is_char<CharT>
	inline asio::const_buffer buffer(const CharT* const& data) noexcept
	{
		return asio::buffer(std::basic_string_view<CharT>(data));
	}

	// 
	template <typename T, ::std::size_t Capacity>
	inline asio::mutable_buffer buffer(::std::experimental::fixed_capacity_vector<T, Capacity>& data) noexcept
	{
		return asio::buffer((void*)(data.data()), sizeof(T) * data.size());
	}

	template <typename T, ::std::size_t Capacity>
	inline asio::const_buffer buffer(const ::std::experimental::fixed_capacity_vector<T, Capacity>& data) noexcept
	{
		return asio::buffer((const void*)(data.data()), sizeof(T) * data.size());
	}

	// 
	template <typename T1, typename T2>
	inline auto buffer(std::pair<T1, T2>& data) noexcept
	{
		// asio::buffer(std::basic_string_view<...>) == asio::const_buffer
		return ::std::array<asio::const_buffer, 2>
		{
			asio::const_buffer(asio::buffer(data.first)),
			asio::const_buffer(asio::buffer(data.second))
		};
	}

	template <typename T1, typename T2>
	inline auto buffer(const ::std::pair<T1, T2>& data) noexcept
	{
		return ::std::array<asio::const_buffer, 2>{ asio::buffer(data.first), asio::buffer(data.second)};
	}
}
