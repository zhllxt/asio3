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

#include <asio3/core/netutil.hpp>
#include <asio3/core/function_traits.hpp>

#include <asio3/http/core.hpp>

namespace asio::detail
{
	template<class Proxy>
	struct http_proxy_checker
	{
		static constexpr bool value = true
			&& (!std::is_same_v<std::remove_cvref_t<Proxy>, error_code>)
			&& (!detail::can_convert_to_http_request_v<std::remove_cvref_t<Proxy>>)
			&& (!detail::is_character_string_v<std::remove_cvref_t<Proxy>>)
			;
	};

	template<class T>
	inline constexpr bool http_proxy_checker_v = http_proxy_checker<std::remove_cvref_t<T>>::value;

	template<class args_t>
	struct is_http_execute_download_enabled
	{
		template<class, class = void>
		struct has_member_enabled : std::false_type {};

		template<class T>
		struct has_member_enabled<T, std::void_t<decltype(T::http_execute_download_enabled)>> : std::true_type {};

		static constexpr bool value()
		{
			if constexpr (has_member_enabled<args_t>::value)
			{
				return args_t::http_execute_download_enabled;
			}
			else
			{
				return true;
			}
		}
	};

	template<class args_t>
	struct is_https_execute_download_enabled
	{
		template<class, class = void>
		struct has_member_enabled : std::false_type {};

		template<class T>
		struct has_member_enabled<T, std::void_t<decltype(T::https_execute_download_enabled)>> : std::true_type {};

		static constexpr bool value()
		{
			if constexpr (has_member_enabled<args_t>::value)
			{
				return args_t::https_execute_download_enabled;
			}
			else
			{
				return true;
			}
		}
	};
}