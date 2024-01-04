/*
 * Copyright (c) 2017-2023 zhllxt
 * 
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

// No header guard

#if   __has_include(<asio/detail/push_options.hpp>)
#include <asio/detail/push_options.hpp>
#elif __has_include(<boost/asio/detail/push_options.hpp>)
#include <boost/asio/detail/push_options.hpp>
#endif

#if defined(_MSC_VER)
# pragma warning (disable:4996)
#endif

/*
 * see : https://github.com/retf/Boost.Application/pull/40
 */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || \
	defined(_WINDOWS_) || defined(__WINDOWS__) || defined(__TOS_WIN__)
#	ifndef _WIN32_WINNT
#		if __has_include(<winsdkver.h>)
#			include <winsdkver.h>
#			define _WIN32_WINNT _WIN32_WINNT_WIN7
#		endif
#		if __has_include(<SDKDDKVer.h>)
#			include <SDKDDKVer.h>
#		endif
#	endif
#	if __has_include(<crtdbg.h>)
#		include <crtdbg.h>
#	endif
#endif
