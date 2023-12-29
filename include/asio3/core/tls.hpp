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

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	namespace detail
	{
		template<class T>
		struct external_linkaged_tls
		{
			static T& get() noexcept
			{
				thread_local static T value{};
				return value;
			}
		};

		template<class T>
		inline T& get_tls() noexcept
		{
			return external_linkaged_tls<T>::get();
		}
	}
}
