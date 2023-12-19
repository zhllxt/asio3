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

#include <asio3/core/detail/push_options.hpp>

#include <cerrno>
#include <cassert>
#include <string>
#include <system_error>
#include <ios>
#include <future>

#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	namespace detail
	{
		class [[maybe_unused]] external_linkaged_last_error
		{
		public:
			[[maybe_unused]] static error_code & get() noexcept
			{
				// thread local variable of error_code
				thread_local static error_code ec_last{};

				return ec_last;
			}
		};

		namespace internal_linkaged_last_error
		{
			[[maybe_unused]] static error_code & get() noexcept
			{
				// thread local variable of error_code
				thread_local static error_code ec_last{};

				return ec_last;
			}
		}
	}
}

#include <asio3/core/detail/pop_options.hpp>
