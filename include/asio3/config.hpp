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

// define this to use standalone asio, otherwise use boost::asio.
#define ASIO3_HEADER_ONLY

// If you want to use the ssl, you need to define ASIO3_ENABLE_SSL.
// When use ssl,on windows need linker "libssl.lib;libcrypto.lib;Crypt32.lib;", on 
// linux need linker "ssl;crypto;", if link failed under gcc, try ":libssl.a;:libcrypto.a;"
// ssl must be before crypto.
//#define ASIO3_ENABLE_SSL

// Define ASIO_NO_EXCEPTIONS to disable the exception. so when the exception occurs, you can
// check the stack trace.
// If the ASIO_NO_EXCEPTIONS is defined, you can impl the throw_exception function by youself,
// see: /asio3/core/asio.hpp
//#define ASIO_NO_EXCEPTIONS
