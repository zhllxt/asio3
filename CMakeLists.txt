#
# Copyright (c) 2017-2023 zhllxt
#
# author   : zhllxt
# email    : 37792738@qq.com
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required (VERSION 3.11)

# remove last end of "/"
string(REGEX REPLACE "/$" "" CMAKELISTS_DIR_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# get current relative dir name and set target name
string(REGEX REPLACE ".*/(.*)" "\\1" CMAKELISTS_DIR_NAME ${CMAKELISTS_DIR_PATH})

# set root directory
set(ASIO3_ROOT_DIR ${CMAKELISTS_DIR_PATH} CACHE PATH "asio3 root directory" FORCE)
if(NOT ASIO3_ROOT_DIR)
    message(FATAL_ERROR "asio3 root directory is not set!")
endif()
message(STATUS "ASIO3_ROOT_DIR = ${ASIO3_ROOT_DIR}")

#-------------------------------------------------------------------------------
function (DoGroupSources curdir rootdir foldername folder)
    file (GLOB children RELATIVE ${ASIO3_ROOT_DIR}/${curdir} ${ASIO3_ROOT_DIR}/${curdir}/*)
    foreach (child ${children})
        if (IS_DIRECTORY ${ASIO3_ROOT_DIR}/${curdir}/${child})
            DoGroupSources (${curdir}/${child} ${rootdir} ${foldername} ${folder})
        elseif (${child} STREQUAL "CMakeLists.txt")
            source_group("" FILES ${ASIO3_ROOT_DIR}/${curdir}/${child})
        else()
            string (REGEX REPLACE ^${rootdir} ${folder} groupname ${curdir})
            string (REPLACE "/" "\\" groupname ${groupname})
            source_group (${foldername}${groupname} FILES ${ASIO3_ROOT_DIR}/${curdir}/${child})
        endif()
    endforeach()
endfunction()

function (GroupSources curdir folder)
    string (FIND ${curdir} "/" Separator)
    if(${Separator} EQUAL -1)
        set(Separator 0)
    else ()
        math(EXPR Separator "${Separator} + 1")
    endif()
    string (SUBSTRING ${curdir} ${Separator} -1 foldername)
    DoGroupSources (${curdir} ${curdir} ${foldername} ${folder})
endfunction()

#-------------------------------------------------------------------------------
# project
#-------------------------------------------------------------------------------
project (asio3 VERSION 1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

option(BUILD_EXAMPLES "asio3: Build examples" OFF)
option(BUILD_TESTS "asio3: Build tests" OFF)

set(ASIO3_DISABLE_HEADER_ONLY OFF CACHE BOOL "asio3: Disable standalone asio (use boost::asio instead)")
set(ASIO3_ENABLE_SSL OFF CACHE BOOL "asio3: Enable SSL/TLS support (link prebuilt OpenSSL)")

set(NEED_STDCPPFS "")
set(OPENSSL_LIBS "")
set(GENERAL_LIBS "")
set(ASIO3_OPENSSL_INCS_DIR "" CACHE STRING "asio3 openssl include dir" FORCE)
set(ASIO3_OPENSSL_LIBS_DIR "" CACHE STRING "asio3 openssl lib dir" FORCE)

if(NOT CMAKE_INSTALL_INCLUDEDIR)
    set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE STRING "cmake_install_includedir" FORCE)
endif()

if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "cmake_install_libdir" FORCE)
endif()

set(ASIO3_ARCH "x64" CACHE STRING "asio3: Target architecture (x64/x86)")
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ASIO3_ARCH "x86" FORCE)
endif()
message(STATUS "asio3: target architecture = ${ASIO3_ARCH}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
    set(NEED_STDCPPFS stdc++fs)
endif()

set(ASIO3_OPENSSL_INCS_DIR /3rd/openssl/include)

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(OPENSSL_LIBS libssl.a libcrypto.a)
    set(GENERAL_LIBS -lpthread -lrt -ldl ${NEED_STDCPPFS})
    set(ASIO3_OPENSSL_LIBS_DIR /3rd/openssl/prebuilt/linux/${ASIO3_ARCH})
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(OPENSSL_LIBS libssl.lib libcrypto.lib Crypt32.lib)
    set(GENERAL_LIBS ws2_32 mswsock)
    set(ASIO3_OPENSSL_LIBS_DIR /3rd/openssl/prebuilt/windows/${ASIO3_ARCH})
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(OPENSSL_LIBS libssl.a libcrypto.a)
    set(GENERAL_LIBS -lpthread -lrt -ldl ${NEED_STDCPPFS})
    set(ASIO3_OPENSSL_LIBS_DIR /3rd/openssl/prebuilt/freebsd/${ASIO3_ARCH})
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(OPENSSL_LIBS libssl.a libcrypto.a)
    set(GENERAL_LIBS -lpthread -ldl)
    set(ASIO3_OPENSSL_LIBS_DIR /3rd/openssl/prebuilt/mac)
ELSE ()
    set(OPENSSL_LIBS libssl.a libcrypto.a)
    set(GENERAL_LIBS -lpthread -lrt -ldl ${NEED_STDCPPFS})
    set(ASIO3_OPENSSL_LIBS_DIR /3rd/openssl/prebuilt/default)
ENDIF ()

message(STATUS "ASIO3_OPENSSL_INCS_DIR = ${ASIO3_ROOT_DIR}/${ASIO3_OPENSSL_INCS_DIR}")
message(STATUS "ASIO3_OPENSSL_LIBS_DIR = ${ASIO3_ROOT_DIR}/${ASIO3_OPENSSL_LIBS_DIR}")
message(STATUS "OPENSSL_LIBS = ${OPENSSL_LIBS}")
message(STATUS "GENERAL_LIBS = ${GENERAL_LIBS}")
message(STATUS "CMAKE_INSTALL_INCLUDEDIR = ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "CMAKE_INSTALL_LIBDIR = ${CMAKE_INSTALL_LIBDIR}")

# ---------------------------------------------------------------------------------------
# interface config
# ---------------------------------------------------------------------------------------
add_library(asio3 INTERFACE)
add_library(asio3::asio3 ALIAS asio3)

set_target_properties(asio3 PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME asio3
)

target_include_directories(asio3 INTERFACE
    $<BUILD_INTERFACE:${ASIO3_ROOT_DIR}/include/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/asio3/include/>
)

if(NOT ASIO3_DISABLE_HEADER_ONLY)
    target_include_directories(asio3 INTERFACE
        $<BUILD_INTERFACE:${ASIO3_ROOT_DIR}/3rd/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/asio3/3rd/>
    )
else()
    target_compile_definitions(asio3 INTERFACE ASIO3_DISABLE_HEADER_ONLY)
    message(STATUS "asio3: ASIO3_DISABLE_HEADER_ONLY macro will defined")
    message(STATUS "asio3: standalone asio disabled (use boost::asio)")
endif()

if(ASIO3_ENABLE_SSL)
    target_compile_definitions(asio3 INTERFACE ASIO3_ENABLE_SSL)
    target_include_directories(asio3 INTERFACE
        $<BUILD_INTERFACE:${ASIO3_ROOT_DIR}/3rd/openssl/include/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/asio3/3rd/openssl/include/>
    )
    target_link_directories(asio3 INTERFACE
        $<BUILD_INTERFACE:${ASIO3_ROOT_DIR}/${ASIO3_OPENSSL_LIBS_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/asio3/${ASIO3_OPENSSL_LIBS_DIR}>
    )
    target_link_libraries(asio3 INTERFACE ${OPENSSL_LIBS})
    message(STATUS "asio3: ASIO3_ENABLE_SSL macro will defined")
    message(STATUS "asio3: SSL/TLS support enabled (linked with prebuilt OpenSSL)")
endif()

find_package(Threads REQUIRED)
target_link_libraries(asio3 INTERFACE
    Threads::Threads
    ${GENERAL_LIBS}
)

set_target_properties(asio3 PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_features(asio3 INTERFACE cxx_std_23)

#-------------------------------------------------------------------------------
install(
    DIRECTORY ${ASIO3_ROOT_DIR}/include
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/asio3
)

install(
    DIRECTORY ${ASIO3_ROOT_DIR}/3rd
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/asio3
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/asio3-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${ASIO3_ROOT_DIR}/cmake/asio3-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/asio3-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asio3
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/asio3-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/asio3-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asio3
)

set(ASIO3_INSTALL_INCLUDES
    ${CMAKE_INSTALL_INCLUDEDIR}/asio3/include/
)

if(NOT ASIO3_DISABLE_HEADER_ONLY)
    list(APPEND ASIO3_INSTALL_INCLUDES
        ${CMAKE_INSTALL_INCLUDEDIR}/asio3/3rd/
    )
endif()

if(ASIO3_ENABLE_SSL)
    list(APPEND ASIO3_INSTALL_INCLUDES
        ${CMAKE_INSTALL_INCLUDEDIR}/asio3/3rd/openssl/include/
    )
endif()

install(
    TARGETS asio3
    EXPORT asio3-targets
    INCLUDES DESTINATION ${ASIO3_INSTALL_INCLUDES}
)

install(
    EXPORT asio3-targets
    NAMESPACE asio3::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/asio3
)

export(
    EXPORT asio3-targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/asio3-targets.cmake
    NAMESPACE asio3::
)
export(PACKAGE asio3)

file (GLOB_RECURSE  ASIO3_FILES  ${ASIO3_ROOT_DIR}/include/asio3/*.*)
file (GLOB_RECURSE   ASIO_FILES  ${ASIO3_ROOT_DIR}/3rd/asio/*.*)

# exclude : asio/impl/src.cpp
list (REMOVE_ITEM    ASIO_FILES  ${ASIO3_ROOT_DIR}/3rd/asio/impl/src.cpp)

enable_testing()

if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(BUILD_TESTS)
    message(STATUS "asio3: BUILD_TESTS is ON, but tests directory not found")
endif()
