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
#include <asio3/core/strutil.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	template<class T>
	[[nodiscard]] inline auto data_persist(T&& data)
	{
		using data_type = std::remove_cvref_t<T>;

		// std::string_view
		if constexpr /**/ (asio::is_basic_string_view<data_type>)
		{
			using value_type = typename data_type::value_type;
			return std::basic_string<value_type>(data.data(), data.size());
		}
		// char* , const char* , const char* const
		else if constexpr (asio::is_char_pointer<data_type>)
		{
			using value_type = typename std::remove_cvref_t<std::remove_pointer_t<data_type>>;
			return std::basic_string<value_type>(std::forward<T>(data));
		}
		// char[]
		else if constexpr (asio::is_char_array<data_type>)
		{
			using value_type = typename std::remove_cvref_t<std::remove_all_extents_t<data_type>>;
			return std::basic_string<value_type>(std::forward<T>(data));
		}
		// object like : std::string, std::vector
		else if constexpr (
			std::is_move_constructible_v          <data_type> ||
			std::is_trivially_move_constructible_v<data_type> ||
			std::is_nothrow_move_constructible_v  <data_type> ||
			std::is_move_assignable_v             <data_type> ||
			std::is_trivially_move_assignable_v   <data_type> ||
			std::is_nothrow_move_assignable_v     <data_type> )
		{
			return std::forward<T>(data);
		}
		else
		{
			// PodType (&data)[N] like : int buf[5], double buf[5]
			auto buffer = asio::buffer(data);
			return std::string(reinterpret_cast<std::string::const_pointer>(
				const_cast<const void*>(buffer.data())), buffer.size());
		}
	}

	template<class CharT, class SizeT>
	[[nodiscard]] inline auto data_persist(CharT * s, SizeT count)
	{
		using value_type = typename std::remove_cvref_t<CharT>;
			
		if (!s)
		{
			return std::basic_string<value_type>{};
		}

		return std::basic_string<value_type>(s, count);
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::const_buffer&& data) noexcept
	{
		// "data" is rvalue reference, should use std::move()
		return std::move(data);
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(const asio::const_buffer& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::const_buffer& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::mutable_buffer&& data) noexcept
	{
		return data_persist(asio::const_buffer(std::move(data)));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(const asio::mutable_buffer& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::mutable_buffer& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

#if !defined(ASIO_NO_DEPRECATED) && !defined(BOOST_ASIO_NO_DEPRECATED)
	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::const_buffers_1&& data) noexcept
	{
		return data_persist(asio::const_buffer(std::move(data)));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::const_buffers_1& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(const asio::const_buffers_1& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::mutable_buffers_1&& data) noexcept
	{
		return data_persist(asio::const_buffer(std::move(data)));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(asio::mutable_buffers_1& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}

	template<typename = void>
	[[nodiscard]] inline auto data_persist(const asio::mutable_buffers_1& data) noexcept
	{
		return data_persist(asio::const_buffer(data));
	}
#endif
}
