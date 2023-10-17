#pragma once

#include <asio3/core/detail/push_options.hpp>
#include <asio3/core/asio.hpp>

namespace asio
{

template <typename CompletionToken>
class with_lock_t
{
public:
  /// Tag type used to prevent the "default" constructor from being used for
  /// conversions.
  struct default_constructor_tag {};

  /// Default constructor.
  /**
   * This constructor is only valid if the underlying completion token is
   * default constructible and move constructible. The underlying completion
   * token is itself defaulted as an argument to allow it to capture a source
   * location.
   */
  ASIO_CONSTEXPR with_lock_t(
      default_constructor_tag = default_constructor_tag(),
      CompletionToken token = CompletionToken())
    : token_(ASIO_MOVE_CAST(CompletionToken)(token))
  {
  }

  /// Constructor.
  template <typename T>
  ASIO_CONSTEXPR explicit with_lock_t(
      ASIO_MOVE_ARG(T) completion_token)
    : token_(ASIO_MOVE_CAST(T)(completion_token))
  {
  }

  /// Adapts an executor to add the @c with_lock_t completion token as the
  /// default.
  template <typename InnerExecutor>
  struct executor_with_default : InnerExecutor
  {
    /// Specify @c with_lock_t as the default completion token type.
    typedef with_lock_t default_completion_token_type;

    /// Construct the adapted executor from the inner executor type.
    template <typename InnerExecutor1>
    executor_with_default(const InnerExecutor1& ex,
        typename constraint<
          conditional<
            !is_same<InnerExecutor1, executor_with_default>::value,
            is_convertible<InnerExecutor1, InnerExecutor>,
            false_type
          >::type::value
        >::type = 0) ASIO_NOEXCEPT
      : InnerExecutor(ex)
    {
        lock = std::make_shared<experimental::channel<void()>>(ex, 1);
    }

    std::shared_ptr<experimental::channel<void()>> lock;
  };

  /// Type alias to adapt an I/O object to use @c with_lock_t as its
  /// default completion token type.
#if defined(ASIO_HAS_ALIAS_TEMPLATES) \
  || defined(GENERATING_DOCUMENTATION)
  template <typename T>
  using as_default_on_t = typename T::template rebind_executor<
      executor_with_default<typename T::executor_type> >::other;
#endif // defined(ASIO_HAS_ALIAS_TEMPLATES)
       //   || defined(GENERATING_DOCUMENTATION)

  /// Function helper to adapt an I/O object to use @c with_lock_t as its
  /// default completion token type.
  template <typename T>
  static typename decay<T>::type::template rebind_executor<
      executor_with_default<typename decay<T>::type::executor_type>
    >::other
  as_default_on(ASIO_MOVE_ARG(T) object)
  {
    return typename decay<T>::type::template rebind_executor<
        executor_with_default<typename decay<T>::type::executor_type>
      >::other(ASIO_MOVE_CAST(T)(object));
  }

//private:
  CompletionToken token_;
};

/// Adapt a @ref completion_token to specify that the completion handler
/// arguments should be combined into a single tuple argument.
template <typename CompletionToken>
ASIO_NODISCARD inline
ASIO_CONSTEXPR with_lock_t<typename decay<CompletionToken>::type>
with_lock(ASIO_MOVE_ARG(CompletionToken) completion_token)
{
  return with_lock_t<typename decay<CompletionToken>::type>(
      ASIO_MOVE_CAST(CompletionToken)(completion_token));
}

}

namespace asio::detail
{
	template<typename AsyncStream>
	concept has_member_variable_lock = requires(AsyncStream & s)
	{
		s.get_executor().lock->try_send();
	};

	struct async_lock_channel_op
	{
		template<typename AsyncStream>
		auto operator()(auto state, std::reference_wrapper<AsyncStream> stream_ref) -> void
		{
			auto& s = stream_ref.get();

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			if constexpr (has_member_variable_lock<std::remove_cvref_t<AsyncStream>>)
			{
				//assert(s.get_executor().running_in_this_thread());

				auto& lock = s.get_executor().lock;
				
				if (!lock->try_send())
				{
					auto [ec] = co_await lock->async_send(asio::use_nothrow_deferred);
					co_return ec;
				}
			}

			co_return error_code{};
		}
	};

	struct unlock_op
	{
		template<typename AsyncStream>
		inline auto operator()(AsyncStream& s) -> void
		{
			if constexpr (has_member_variable_lock<std::remove_cvref_t<AsyncStream>>)
			{
				//assert(s.get_executor().running_in_this_thread());

				auto& lock = s.get_executor().lock;
				
				lock->try_receive([](auto...) {});
			}
		}
	};
}

namespace asio
{
	/**
	 * @brief Start an asynchronous operation to lock the channcel of the stream.
	 * @param s - The stream to which be locked.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) LockToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(LockToken, void(asio::error_code))
	async_lock(
		AsyncStream& s,
		LockToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return async_initiate<LockToken, void(asio::error_code)>(
			experimental::co_composed<void(asio::error_code)>(
				detail::async_lock_channel_op{}, s),
			token, std::ref(s));
	}

	/**
	 * @brief Start an synchronous operation to unlock the channcel of the stream.
	 * @param s - The stream to which be locked.
	 */
	template<typename AsyncStream>
	inline void unlock(AsyncStream& s)
	{
		detail::unlock_op{}(s);
	}

	template<typename AsyncStream>
	struct unlock_guard
	{
		unlock_guard(AsyncStream& _s) noexcept : s(_s)
		{
		}
		~unlock_guard()
		{
			asio::unlock(s);
		}

		AsyncStream& s;
	};

	template<typename AsyncStream>
	unlock_guard(AsyncStream&) -> unlock_guard<AsyncStream>;
}

#include <asio3/core/detail/pop_options.hpp>
