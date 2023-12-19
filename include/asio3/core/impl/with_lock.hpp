#pragma once

#include <asio3/core/detail/push_options.hpp>
#include <asio3/core/asio.hpp>
#include <asio3/core/with_lock.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{

#if !defined(GENERATING_DOCUMENTATION)

template <typename CompletionToken, typename... Signatures>
struct async_result<with_lock_t<CompletionToken>, Signatures...>
{
  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static auto initiate(Initiation&& initiation,
      RawCompletionToken&& token, Args&&... args)
  {
    return asio::async_initiate<typename conditional<
        is_const<typename remove_reference<RawCompletionToken>::type>::value,
          const CompletionToken, CompletionToken>::type, Signatures...>(
          static_cast<Initiation&&>(initiation),
        token.token_, static_cast<Args&&>(args)...);
  }
};

#if defined(ASIO_MSVC)

// Workaround for MSVC internal compiler error.

template <typename CompletionToken, typename Signature>
struct async_result<with_lock_t<CompletionToken>, Signature>
{
  template <typename Initiation, typename RawCompletionToken, typename... Args>
  static auto initiate(Initiation&& initiation,
      RawCompletionToken&& token, Args&&... args)
  {
    return async_initiate<typename conditional<
        is_const<typename remove_reference<RawCompletionToken>::type>::value,
          const CompletionToken, CompletionToken>::type, Signature>(
        static_cast<Initiation&&>(initiation),
        token.token_, static_cast<Args&&>(args)...);
  }
};

#endif // defined(ASIO_MSVC)

#endif // !defined(GENERATING_DOCUMENTATION)
}

#include <asio3/core/detail/pop_options.hpp>
