#pragma once

#include <asio3/core/detail/push_options.hpp>
#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{

template <typename CompletionToken>
class with_lock_t
{
public:
  /// Tag type used to prevent the "default" constructor from being used for
  /// conversions.
  struct default_constructor_tag {};

  struct lock_info
  {
    experimental::channel<void()> ch;

  #ifndef NDEBUG
	detail::socket_type sock{ detail::invalid_socket };
  #endif

	std::thread::id     thread_id{};

	template <typename Ex>
	lock_info(const Ex& ex) : ch(ex, 1)
	{
		asio::dispatch(ex, [this]() mutable
		{
			this->thread_id = std::this_thread::get_id();
		});
	}
  };

  /// Default constructor.
  /**
   * This constructor is only valid if the underlying completion token is
   * default constructible and move constructible. The underlying completion
   * token is itself defaulted as an argument to allow it to capture a source
   * location.
   */
  constexpr with_lock_t(
      default_constructor_tag = default_constructor_tag(),
      CompletionToken token = CompletionToken())
    : token_(static_cast<CompletionToken&&>(token))
  {
  }

  /// Constructor.
  template <typename T>
  constexpr explicit with_lock_t(
      T&& completion_token)
    : token_(static_cast<T&&>(completion_token))
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
        >::type = 0) noexcept
      : InnerExecutor(ex)
    {
		lock = std::make_shared<lock_info>(ex);
    }

	inline InnerExecutor& base()
	{
		return static_cast<InnerExecutor&>(*this);
	}

	inline const InnerExecutor& base() const
	{
		return static_cast<const InnerExecutor&>(*this);
	}

	/**
	 * @brief return the thread id of the current io_context running in.
	 */
	inline std::thread::id get_thread_id() const noexcept
	{
		return this->lock->thread_id;
	}

	/**
	 * @brief Determine whether the current io_context is running in the current thread.
	 */
	inline bool running_in_this_thread() const noexcept
	{
		return std::this_thread::get_id() == this->lock->thread_id;
	}

    std::shared_ptr<lock_info> lock;
  };

  /// Type alias to adapt an I/O object to use @c with_lock_t as its
  /// default completion token type.
  template <typename T>
  using as_default_on_t = typename T::template rebind_executor<
      executor_with_default<typename T::executor_type> >::other;

  /// Function helper to adapt an I/O object to use @c with_lock_t as its
  /// default completion token type.
  template <typename T>
  static typename decay<T>::type::template rebind_executor<
      executor_with_default<typename decay<T>::type::executor_type>
    >::other
  as_default_on(T&& object)
  {
    return typename decay<T>::type::template rebind_executor<
        executor_with_default<typename decay<T>::type::executor_type>
      >::other(static_cast<T&&>(object));
  }

//private:
  CompletionToken token_;
};

/// Adapt a @ref completion_token to specify that the completion handler
/// arguments should be combined into a single tuple argument.
template <typename CompletionToken>
ASIO_NODISCARD inline
constexpr with_lock_t<typename decay<CompletionToken>::type>
with_lock(CompletionToken&& completion_token)
{
  return with_lock_t<typename decay<CompletionToken>::type>(
      static_cast<CompletionToken&&>(completion_token));
}

}

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	template<typename AsyncStream>
	concept has_member_variable_lock = requires(AsyncStream & s)
	{
		s.get_executor().lock->ch.try_send();
		s.get_executor().base();
	};

	template<typename AsyncStream>
	concept has_member_next_layer = requires(AsyncStream & s)
	{
		s.next_layer();
	};

	template<typename AsyncStream>
	concept has_member_get_executor = requires(AsyncStream & s)
	{
		s.get_executor();
	};

	struct async_lock_channel_op
	{
		template<typename AsyncStream>
		auto& lowest_layer(AsyncStream& s)
		{
			if constexpr (has_member_next_layer<std::remove_cvref_t<AsyncStream>>)
			{
				return s.next_layer().lowest_layer();
			}
			else
			{
				return s.lowest_layer();
			}
		}

		template<typename AsyncStream>
		auto operator()(auto state, ::std::reference_wrapper<AsyncStream> stream_ref) -> void
		{
			auto& s = stream_ref.get();

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			if constexpr (has_member_variable_lock<std::remove_cvref_t<AsyncStream>>)
			{
				//assert(s.get_executor().running_in_this_thread());

			#ifndef NDEBUG
				detail::socket_type& sock1 = s.get_executor().lock->sock;
				detail::socket_type  sock2 = static_cast<detail::socket_type>(lowest_layer(s).native_handle());
				if (sock1 == detail::invalid_socket)
				{
					sock1 = sock2;
				}
				else if (sock2 != detail::invalid_socket)
				{
					assert(sock1 == sock2);
				}
			#endif

				if (!s.get_executor().lock->ch.try_send())
				{
					auto [ec] = co_await s.get_executor().lock->ch.async_send(asio::use_nothrow_deferred);
					co_return ec;
				}
			}

			co_return error_code{};
		}
	};

	template<typename AsyncStream>
	inline void unlock_channel(AsyncStream& s)
	{
		if constexpr (has_member_variable_lock<std::remove_cvref_t<AsyncStream>>)
		{
			//assert(s.get_executor().running_in_this_thread());

			s.get_executor().lock->ch.try_receive([](auto...) {});
		}
	}

	template <typename... Args>
	inline void unlock_channel(experimental::basic_channel<Args...>& ch)
	{
		ch.try_receive([](auto...) {});
	}

	template<typename AsyncStream>
	inline auto get_lowest_executor(AsyncStream& s)
	{
		if constexpr (has_member_variable_lock<std::remove_cvref_t<AsyncStream>>)
		{
			return s.get_executor().base();
		}
		else if constexpr (has_member_get_executor<std::remove_cvref_t<AsyncStream>>)
		{
			return s.get_executor();
		}
		else
		{
			return s;
		}
	}
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 * @brief Start an asynchronous operation to lock the channcel of the stream.
	 *    This function is not thread safety, you must ensure that this function 
	 *    is called in the executor thread of the AsyncStream's by youself.
	 * @param s - The stream to which be locked.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		typename LockToken = asio::default_token_type<AsyncStream>>
	inline auto async_lock(
		AsyncStream& s,
		LockToken&& token = asio::default_token_type<AsyncStream>())
	{
		return async_initiate<LockToken, void(asio::error_code)>(
			experimental::co_composed<void(asio::error_code)>(
				detail::async_lock_channel_op{}, s),
			token, std::ref(s));
	}

	/**
	 * @brief Reset the lock state.
	 */
	template<typename AsyncStream>
	inline void reset_lock([[maybe_unused]] AsyncStream& s) noexcept
	{
	#ifndef NDEBUG
		if constexpr (detail::has_member_variable_lock<std::remove_cvref_t<AsyncStream>>)
		{
			s.get_executor().lock->sock = detail::invalid_socket;
		}
	#endif
	}

	template<typename AsyncStream>
	struct defer_unlock
	{
		defer_unlock(AsyncStream& _s) noexcept : s(_s)
		{
		}
		~defer_unlock()
		{
			detail::unlock_channel(s);
		}

		AsyncStream& s;
	};

	template<typename AsyncStream>
	defer_unlock(AsyncStream&) -> defer_unlock<AsyncStream>;

	// asio How to change the executor inside an awaitable
	// https://stackoverflow.com/questions/74071288/switch-context-in-coroutine-with-boostasiopost
	// https://stackoverflow.com/questions/71987021/asio-how-to-change-the-executor-inside-an-awaitable/71991876#71991876
	template<typename AsyncStream>
	inline auto use_deferred_executor(AsyncStream& s)
	{
		return asio::bind_executor(asio::detail::get_lowest_executor(s), asio::use_nothrow_deferred);
	}

	template<typename AsyncStream>
	inline auto use_awaitable_executor(AsyncStream& s)
	{
		return asio::bind_executor(asio::detail::get_lowest_executor(s), asio::use_nothrow_awaitable);
	}
}

#include <asio3/core/detail/pop_options.hpp>
