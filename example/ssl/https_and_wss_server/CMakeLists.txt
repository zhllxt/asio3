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

set(TARGET_NAME httpwss_server)

add_executable (
    ${TARGET_NAME}
    ${ASIO3_FILES}
    ${TARGET_NAME}.cpp
)

configure(${TARGET_NAME} true)
