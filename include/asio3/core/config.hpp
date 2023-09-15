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

#include <asio3/config.hpp>

#if !defined(ASIO3_HEADER_ONLY) && __has_include(<boost/config.hpp>)
#include <boost/config.hpp>
#ifndef ASIO3_JOIN
#define ASIO3_JOIN BOOST_JOIN
#endif
#ifndef ASIO3_STRINGIZE
#define ASIO3_STRINGIZE BOOST_STRINGIZE
#endif
#else
#include <asio3/bho/config.hpp>
#ifndef ASIO3_JOIN
#define ASIO3_JOIN BHO_JOIN
#endif
#ifndef ASIO3_STRINGIZE
#define ASIO3_STRINGIZE BHO_STRINGIZE
#endif
#endif
