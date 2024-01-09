// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright Antony Polukhin, 2015-2023.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <asio3/core/predef.h>

#if ASIO3_OS_MACOS || ASIO3_OS_IOS

#include <mach-o/dyld.h>

namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code &ec) {
        ec.clear();

        char path[1024];
        std::uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0)
            return std::filesystem::path(path);

        char *p = new char[size];
        if (_NSGetExecutablePath(p, &size) != 0) {
            ec = std::make_error_code(
                std::errc::bad_file_descriptor
            );
        }

        std::filesystem::path ret(p);
        delete[] p;
        return ret;
    }
}}} // namespace asio3::dll::detail

#elif ASIO3_OS_SOLARIS

#include <stdlib.h>
namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code& ec) {
        ec.clear();

        return std::filesystem::path(getexecname());
    }
}}} // namespace asio3::dll::detail

#elif ASIO3_OS_BSD_FREE

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>

namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code& ec) {
        ec.clear();

        int mib[4];
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PATHNAME;
        mib[3] = -1;
        char buf[10240];
        std::size_t cb = sizeof(buf);
        sysctl(mib, 4, buf, &cb, NULL, 0);

        return std::filesystem::path(buf);
    }
}}} // namespace asio3::dll::detail



#elif ASIO3_OS_BSD_NET

namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code &ec) {
        return std::filesystem::read_symlink("/proc/curproc/exe", ec);
    }
}}} // namespace asio3::dll::detail

#elif ASIO3_OS_BSD_DRAGONFLY


namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code &ec) {
        return std::filesystem::read_symlink("/proc/curproc/file", ec);
    }
}}} // namespace asio3::dll::detail

#elif ASIO3_OS_QNX

#include <fstream>
#include <string> // for std::getline
namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code &ec) {
        ec.clear();

        std::string s;
        std::ifstream ifs("/proc/self/exefile");
        std::getline(ifs, s);

        if (ifs.fail() || s.empty()) {
            ec = std::make_error_code(
                std::errc::bad_file_descriptor
            );
        }

        return std::filesystem::path(s);
    }
}}} // namespace asio3::dll::detail

#elif ASIO3_OS_WINDOWS

#include <Windows.h>
namespace asio3 { namespace dll { namespace detail {

    template<typename = void>
    inline std::error_code last_error_code() noexcept {
        DWORD err = ::GetLastError();
        return std::error_code(
            static_cast<int>(err),
            std::system_category()
        );
    }

    template<typename = void>
    inline std::filesystem::path path_from_handle(HMODULE handle, std::error_code &ec) {

        const DWORD ERROR_INSUFFICIENT_BUFFER_ = 0x7A;
        const DWORD DEFAULT_PATH_SIZE_ = MAX_PATH;

        // If `handle` parameter is NULL, GetModuleFileName retrieves the path of the
        // executable file of the current process.
        char path_hldr[DEFAULT_PATH_SIZE_];
        const DWORD ret = ::GetModuleFileNameA(handle, path_hldr, DEFAULT_PATH_SIZE_);
        if (ret && ::GetLastError() == 0) {
            // On success, GetModuleFileNameA() doesn't reset last error to ERROR_SUCCESS. Resetting it manually.
            ec.clear();
            return std::filesystem::path(path_hldr);
        }

        ec = asio3::dll::detail::last_error_code();
        for (unsigned i = 2; i < 1025 && static_cast<DWORD>(ec.value()) == ERROR_INSUFFICIENT_BUFFER_; i *= 2) {
            std::string p(std::size_t(DEFAULT_PATH_SIZE_ * i), '\0');
            const std::size_t size = ::GetModuleFileNameA(handle, &p[0], DEFAULT_PATH_SIZE_ * i);
            if (size && ::GetLastError() == 0) {
                // On success, GetModuleFileNameA() doesn't reset last error to ERROR_SUCCESS. Resetting it manually.
                ec.clear();
                p.resize(size);
                return std::filesystem::path(p);
            }

            ec = asio3::dll::detail::last_error_code();
        }

        // Error other than ERROR_INSUFFICIENT_BUFFER_ occurred or failed to allocate buffer big enough.
        return std::filesystem::path();
    }

}}} // namespace asio3::dll::detail

#else  // ASIO3_OS_LINUX || ASIO3_OS_UNIX || ASIO3_OS_HPUX || ASIO3_OS_ANDROID

namespace asio3 { namespace dll { namespace detail {
    template<typename = void>
    inline std::filesystem::path program_location_impl(std::error_code &ec) {
        // We can not use
        // asio3::dll::detail::path_from_handle(dlopen(NULL, RTLD_LAZY | RTLD_LOCAL), ignore);
        // because such code returns empty path.

        return std::filesystem::read_symlink("/proc/self/exe", ec);   // Linux specific
    }
}}} // namespace asio3::dll::detail

#endif
