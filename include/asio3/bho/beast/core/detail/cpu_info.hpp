//
// Copyright (c) 2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BHO_BEAST_DETAIL_CPU_INFO_HPP
#define BHO_BEAST_DETAIL_CPU_INFO_HPP

#include <asio3/bho/config.hpp>

#ifndef BHO_BEAST_NO_INTRINSICS
# if defined(BHO_MSVC) || ((defined(BHO_GCC) || defined(BHO_CLANG)) && defined(__SSE4_2__))
#  define BHO_BEAST_NO_INTRINSICS 0
# else
#  define BHO_BEAST_NO_INTRINSICS 1
# endif
#endif

#if ! BHO_BEAST_NO_INTRINSICS

#ifdef BHO_MSVC
#include <intrin.h> // __cpuid
#else
#include <cpuid.h>  // __get_cpuid
#endif

namespace bho {
namespace beast {
namespace detail {

/*  Portions from Boost,
    Copyright Andrey Semashev 2007 - 2015.
*/
template<class = void>
void
cpuid(
    std::uint32_t id,
    std::uint32_t& eax,
    std::uint32_t& ebx,
    std::uint32_t& ecx,
    std::uint32_t& edx)
{
#ifdef BHO_MSVC
    int regs[4];
    __cpuid(regs, id);
    eax = regs[0];
    ebx = regs[1];
    ecx = regs[2];
    edx = regs[3];
#else
    __get_cpuid(id, &eax, &ebx, &ecx, &edx);
#endif
}

struct cpu_info
{
    bool sse42 = false;

    cpu_info();
};

inline
cpu_info::
cpu_info()
{
    constexpr std::uint32_t SSE42 = 1 << 20;

    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;

    cpuid(0, eax, ebx, ecx, edx);
    if(eax >= 1)
    {
        cpuid(1, eax, ebx, ecx, edx);
        sse42 = (ecx & SSE42) != 0;
    }
}

template<class = void>
cpu_info const&
get_cpu_info()
{
    static cpu_info const ci;
    return ci;
}

} // detail
} // beast
} // bho

#endif

#endif
