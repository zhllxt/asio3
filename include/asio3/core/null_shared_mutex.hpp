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

#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{

class null_shared_mutex
{
public:
  // Constructor.
  null_shared_mutex()
  {
  }

  // Destructor.
  ~null_shared_mutex()
  {
  }

  // Lock the mutex.
  inline void lock() noexcept
  {
  }

  // Lock the mutex.
  inline void lock_shared() noexcept
  {
  }

  // Unlock the mutex.
  inline void unlock() noexcept
  {
  }

  // Unlock the mutex.
  inline void unlock_shared() noexcept
  {
  }
};

} // namespace asio
