/*
Copyright Rene Rivera 2008-2015
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef BHO_PREDEF_OS_AMIGAOS_H
#define BHO_PREDEF_OS_AMIGAOS_H

#include <asio3/bho/predef/version_number.h>
#include <asio3/bho/predef/make.h>

/* tag::reference[]
= `BHO_OS_AMIGAOS`

http://en.wikipedia.org/wiki/AmigaOS[AmigaOS] operating system.

[options="header"]
|===
| {predef_symbol} | {predef_version}

| `AMIGA` | {predef_detection}
| `+__amigaos__+` | {predef_detection}
|===
*/ // end::reference[]

#define BHO_OS_AMIGAOS BHO_VERSION_NUMBER_NOT_AVAILABLE

#if !defined(BHO_PREDEF_DETAIL_OS_DETECTED) && ( \
    defined(AMIGA) || defined(__amigaos__) \
    )
#   undef BHO_OS_AMIGAOS
#   define BHO_OS_AMIGAOS BHO_VERSION_NUMBER_AVAILABLE
#endif

#if BHO_OS_AMIGAOS
#   define BHO_OS_AMIGAOS_AVAILABLE
#   include <asio3/bho/predef/detail/os_detected.h>
#endif

#define BHO_OS_AMIGAOS_NAME "AmigaOS"

#endif

#include <asio3/bho/predef/detail/test.h>
BHO_PREDEF_DECLARE_TEST(BHO_OS_AMIGAOS,BHO_OS_AMIGAOS_NAME)
