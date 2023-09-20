#pragma once

#include <asio3/core/detail/push_options.hpp>
#include <asio3/core/asio.hpp>
#include <asio3/core/with_lock.hpp>

namespace asio {

#if !defined(GENERATING_DOCUMENTATION)

template <typename CompletionToken, typename... Signatures>
struct async_result<with_lock_t<CompletionToken>, Signatures...>
{
  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static auto
  initiate(
      ASIO_MOVE_ARG(Initiation) initiation,
      ASIO_MOVE_ARG(RawCompletionToken) token,
      ASIO_MOVE_ARG(Args)... args)
  {
    return asio::async_initiate<CompletionToken, Signatures...>(
          ASIO_MOVE_CAST(Initiation)(initiation),
        token.token_, ASIO_MOVE_CAST(Args)(args)...);
  }
};

#if defined(ASIO_MSVC)

// Workaround for MSVC internal compiler error.

template <typename CompletionToken, typename Signature>
struct async_result<with_lock_t<CompletionToken>, Signature>
{
  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static auto
  initiate(
      ASIO_MOVE_ARG(Initiation) initiation,
      ASIO_MOVE_ARG(RawCompletionToken) token,
      ASIO_MOVE_ARG(Args)... args)
  {
    return async_initiate<CompletionToken, Signature>(
          ASIO_MOVE_CAST(Initiation)(initiation),
        token.token_, ASIO_MOVE_CAST(Args)(args)...);
  }
};

#endif // defined(ASIO_MSVC)

#endif // !defined(GENERATING_DOCUMENTATION)
}

#include <asio3/core/detail/pop_options.hpp>
