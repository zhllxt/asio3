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

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	template<std::integral T, bool SkipZero = true>
	class id_generator
	{
	public:
		id_generator(T init = static_cast<T>(1)) noexcept : id(init)
		{
		}

		inline T next() noexcept
		{
			if constexpr (SkipZero)
			{
				T r = id.fetch_add(static_cast<T>(1));
				return (r == 0 ? id.fetch_add(static_cast<T>(1)) : r);
			}
			else
			{
				return id.fetch_add(static_cast<T>(1));
			}
		}

		inline T zero() noexcept
		{
			return static_cast<T>(0);
		}

	protected:
		std::atomic<T> id;
	};
}
