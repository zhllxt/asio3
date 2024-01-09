// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2023.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string>
#include <filesystem>
#include <system_error>

#include <asio3/core/asio.hpp>
#include <asio3/core/predef.h>

#include <asio3/core/impl/program_location.ipp>

#if ASIO3_OS_WINDOWS
namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code& ec) {
        return asio3::dll::detail::path_from_handle(NULL, ec);
    }
}}}
#endif

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{

    /*!
    * On success returns full path and name of the currently running program (the one which contains the `main()` function).
    * 
    * Return value can be used as a parameter for shared_library. See Tutorial "Linking plugin into the executable"
    * for usage example. Flag '-rdynamic' must be used when linking the plugin into the executable
    * on Linux OS.
    *
    * \param ec Variable that will be set to the result of the operation.
    * \throws std::bad_alloc in case of insufficient memory. Overload that does not accept \forcedlinkfs{error_code} also throws \forcedlinkfs{system_error}.
    */
    template<typename = void>
    inline std::filesystem::path program_location(std::error_code& ec) {
        ec.clear();
        return ::asio3::dll::detail::program_location_impl(ec);
    }

    //! \overload program_location(boost::dll::fs::error_code& ec) {
    template<typename = void>
    inline std::filesystem::path program_location() {
        std::filesystem::path ret;
        std::error_code ec;
        ret = ::asio3::dll::detail::program_location_impl(ec);

        if (ec) {
            throw std::filesystem::filesystem_error("asio::program_location() failed", ec);
        }

        return ret;
    }
}
