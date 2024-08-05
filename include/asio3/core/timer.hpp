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

#include <atomic>

#include <asio3/core/asio.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/defer.hpp>
#include <asio3/core/with_lock.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
// Forward declaration with defaulted arguments.
template <typename Clock,
    typename WaitTraits = asio::wait_traits<Clock>,
    typename Executor = any_io_executor>
class basic_timer;

template <typename Clock, typename WaitTraits, typename Executor>
class basic_timer : public basic_waitable_timer<Clock, WaitTraits, Executor>
{
public:
  using basic_waitable_timer<Clock, WaitTraits, Executor>::basic_waitable_timer;

  /// Rebinds the timer type to another executor.
  template <typename Executor1>
  struct rebind_executor
  {
    /// The timer type when rebound to the specified executor.
    typedef basic_timer<Clock, WaitTraits, Executor1> other;
  };

  /// Cancel any asynchronous operations that are waiting on the timer.
  /**
   * This function forces the completion of any pending asynchronous wait
   * operations against the timer. The handler for each cancelled operation will
   * be invoked with the asio::error::operation_aborted error code.
   *
   * Cancelling the timer does not change the expiry time.
   *
   * @return The number of asynchronous operations that were cancelled.
   *
   * @throws asio::system_error Thrown on failure.
   *
   * @note If the timer has already expired when cancel() is called, then the
   * handlers for asynchronous wait operations will:
   *
   * @li have already been invoked; or
   *
   * @li have been queued for invocation in the near future.
   *
   * These handlers can no longer be cancelled, and therefore are passed an
   * error code that indicates the successful completion of the wait operation.
   */
  ::std::size_t cancel(::std::memory_order order = ::std::memory_order_seq_cst)
  {
	canceled_.test_and_set(order);
    return basic_waitable_timer<Clock, WaitTraits, Executor>::cancel();
  }

#if !defined(ASIO_NO_DEPRECATED)
  /// (Deprecated: Use non-error_code overload.) Cancel any asynchronous
  /// operations that are waiting on the timer.
  /**
   * This function forces the completion of any pending asynchronous wait
   * operations against the timer. The handler for each cancelled operation will
   * be invoked with the asio::error::operation_aborted error code.
   *
   * Cancelling the timer does not change the expiry time.
   *
   * @param ec Set to indicate what error occurred, if any.
   *
   * @return The number of asynchronous operations that were cancelled.
   *
   * @note If the timer has already expired when cancel() is called, then the
   * handlers for asynchronous wait operations will:
   *
   * @li have already been invoked; or
   *
   * @li have been queued for invocation in the near future.
   *
   * These handlers can no longer be cancelled, and therefore are passed an
   * error code that indicates the successful completion of the wait operation.
   */
  ::std::size_t cancel(asio::error_code& ec, ::std::memory_order order = ::std::memory_order_seq_cst)
  {
    canceled_.test_and_set(order);
    return basic_waitable_timer<Clock, WaitTraits, Executor>::cancel(ec);
  }
#endif // !defined(ASIO_NO_DEPRECATED)

  inline bool canceled(::std::memory_order order = ::std::memory_order_seq_cst) const noexcept
  {
	return canceled_.test(order);
  }

  inline void clear(::std::memory_order order = ::std::memory_order_seq_cst) noexcept
  {
	return canceled_.clear(order);
  }

protected:
  /// Why use this flag, beacuase the ec param maybe zero when the timer callback is
  /// called after the timer cancel function has called already.
  /// 
  /// Initializes ::std::atomic_flag to clear state. (since C++20)
  ::std::atomic_flag canceled_{};
};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	using timer = as_tuple_t<use_awaitable_t<>>::as_default_on_t<asio::basic_timer<::std::chrono::steady_clock>>;

	inline void cancel_timer(auto& t) noexcept
	{
		try
		{
			t.cancel();
		}
		catch (system_error const&)
		{
		}
	}
}

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct timer_tag_t {};

	constexpr timer_tag_t timer_tag{};

	struct async_sleep_op
	{
		auto operator()(auto state, auto&& executor, asio::timer::duration duration) -> void
		{
			auto ex = ::std::forward_like<decltype(executor)>(executor);

			co_await asio::dispatch(asio::use_deferred_executor(ex));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::steady_timer t(ex);

			t.expires_after(duration);

			auto [ec] = co_await t.async_wait(asio::use_deferred_executor(t));

			co_return ec;
		}
	};

	struct call_func_when_timeout
	{
		::std::shared_ptr<asio::timer> timer_ptr;

		call_func_when_timeout(auto&& executor, asio::timer::duration timeout_value, auto&& func)
		{
			timer_ptr = ::std::make_shared<asio::timer>(executor);
			timer_ptr->expires_after(timeout_value);
			timer_ptr->async_wait(
			[p = timer_ptr, f = ::std::forward_like<decltype(func)>(func)]
			(const asio::error_code& ec) mutable
			{
				if (!ec)
				{
					//assert(p->canceled() == false);
					if (p->canceled() == false)
					{
						f();
					}
				}
			});
		}

		~call_func_when_timeout()
		{
			asio::cancel_timer(*timer_ptr);
		}
	};

	struct timer_callback_helper
	{
		template<class F>
		requires (::std::invocable<::std::decay_t<F>, ::std::shared_ptr<asio::timer>&>)
		static inline asio::awaitable<bool> call(F& f, ::std::shared_ptr<asio::timer>& timer_ptr)
		{
			co_return co_await f(timer_ptr);
		}

		template<class F>
		requires (!::std::invocable<::std::decay_t<F>, ::std::shared_ptr<asio::timer>&>)
		static inline asio::awaitable<bool> call(F& f, ::std::shared_ptr<asio::timer>&)
		{
			co_return co_await f();
		}
	};

	struct timer_exit_notify_helper
	{
		template<class F>
		requires (::std::invocable<::std::decay_t<F>, ::std::shared_ptr<asio::timer>&>)
		static inline void call(F& f, ::std::shared_ptr<asio::timer>& timer_ptr)
		{
			f(timer_ptr);
		}

		template<class F>
		requires (!::std::invocable<::std::decay_t<F>, ::std::shared_ptr<asio::timer>&>)
		static inline void call(F& f, ::std::shared_ptr<asio::timer>&)
		{
			f();
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 * @brief Asynchronously sleep for a duration.
	 * @param executor - The executor.
	 * @param duration - The duration. 
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename Executor,
		typename SleepToken = asio::default_token_type<asio::timer>>
	inline auto async_sleep(
		Executor&& executor,
		asio::timer::duration duration,
		SleepToken&& token = asio::default_token_type<asio::timer>())
	{
		return asio::async_initiate<SleepToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_sleep_op{}, executor),
			token, executor, duration);
	}

	/**
	 * @brief Asynchronously sleep for a duration.
	 * @param duration - The duration. 
	 */
	template<typename = void>
	asio::awaitable<asio::error_code> async_sleep(asio::timer::duration duration)
	{
		auto ex = co_await asio::this_coro::executor;
		auto [ec] = co_await async_sleep(ex, duration, asio::use_awaitable_executor(ex));
		co_return ec;
	}

	/**
	 * @brief Asynchronously sleep for a duration.
	 * @param duration - The duration. 
	 */
	template<typename = void>
	asio::awaitable<asio::error_code> delay(asio::timer::duration duration)
	{
		co_return co_await async_sleep(duration);
	}

	/**
	 * @brief Asynchronously wait a timeout for the duration.
	 * @param duration - The duration. 
	 */
	template<typename = void>
	asio::awaitable<::std::tuple<asio::error_code, detail::timer_tag_t>> timeout(asio::timer::duration duration)
	{
		co_return ::std::tuple{ co_await async_sleep(duration), detail::timer_tag };
	}

	/**
	 * @brief Asynchronously wait until the idle timeout.
	 * @param duration - The deadline. 
	 */
	template<typename = void>
	asio::awaitable<error_code> watchdog(::std::chrono::steady_clock::time_point& deadline)
	{
		asio::steady_timer watchdog_timer(co_await asio::this_coro::executor);

		auto now = ::std::chrono::steady_clock::now();

		while (deadline > now)
		{
			watchdog_timer.expires_at(deadline);

			auto [ec] = co_await watchdog_timer.async_wait(asio::use_awaitable_executor(watchdog_timer));
			if (ec)
				co_return ec;

			now = ::std::chrono::steady_clock::now();
		}

		co_return asio::error::timed_out;
	}

	/**
	 * @brief Asynchronously wait until the idle timeout.
	 * @param duration - The deadline.
	 */
	template<typename = void>
	asio::awaitable<error_code> watchdog(
		asio::timer& watchdog_timer,
		::std::chrono::system_clock::time_point& alive_time,
		::std::chrono::system_clock::duration idle_timeout)
	{
		auto idled_duration = ::std::chrono::system_clock::now() - alive_time;

		while (idled_duration < idle_timeout)
		{
			watchdog_timer.expires_after(idle_timeout - idled_duration);

			auto [ec] = co_await watchdog_timer.async_wait(asio::use_awaitable_executor(watchdog_timer));
			if (ec)
				co_return ec;
			if (watchdog_timer.canceled())
				co_return asio::error::operation_aborted;

			idled_duration = ::std::chrono::system_clock::now() - alive_time;
		}

		co_return asio::error::timed_out;
	}

	/**
	 * @brief Asynchronously wait until the idle timeout.
	 * @param duration - The deadline. 
	 */
	template<typename = void>
	asio::awaitable<error_code> watchdog(
		::std::chrono::system_clock::time_point& alive_time,
		::std::chrono::system_clock::duration idle_timeout)
	{
		asio::timer watchdog_timer(co_await asio::this_coro::executor);

		co_return co_await watchdog(watchdog_timer, alive_time, idle_timeout);
	}

	/**
	 * @brief Check whether the result of the awaitable_operators is timeout.
	 * @param v - The result of the awaitable_operators which type is ::std::variant<...>
	 * @eg:
	 * auto result = co_await (asio::async_connect(...) || asio::timeout(...));
	 * if (asio::is_timeout(result))
	 * {
	 * }
	 */
	template<typename = void>
	constexpr bool is_timeout(is_variant auto& v) noexcept
	{
		return ::std::holds_alternative<::std::tuple<asio::error_code, detail::timer_tag_t>>(v);
	}

	/**
	 * @brief create a timer.
	 */
	::std::shared_ptr<asio::timer> create_timer(
		const auto& executor, asio::timer::duration interval, auto&& callback)
	{
		::std::shared_ptr<asio::timer> t = ::std::make_shared<asio::timer>(executor);

		asio::co_spawn(executor, [t, interval, f = ::std::forward_like<decltype(callback)>(callback)]
		() mutable -> asio::awaitable<asio::error_code>
		{
			for (;;)
			{
				t->expires_after(interval);

				auto [ec] = co_await t->async_wait(asio::use_awaitable_executor(*t));
				if (ec)
					co_return ec;
				if (t->canceled())
					co_return asio::error::operation_aborted;

				if (!(co_await detail::timer_callback_helper::call(f, t)))
					co_return asio::error::operation_aborted;
			}

			co_return error_code{};
		}, asio::detached);

		return t;
	}

	/**
	 * @brief create a timer.
	 */
	::std::shared_ptr<asio::timer> create_timer(const auto& executor,
		asio::timer::duration first_delay,
		asio::timer::duration interval, auto&& callback)
	{
		::std::shared_ptr<asio::timer> t = ::std::make_shared<asio::timer>(executor);

		asio::co_spawn(executor,
		[t, first_delay, interval, f = ::std::forward_like<decltype(callback)>(callback)]
		() mutable -> asio::awaitable<asio::error_code>
		{
			if (first_delay > asio::timer::duration::zero())
				co_await asio::delay(first_delay);

			for (;;)
			{
				auto [ec] = co_await t->async_wait(asio::use_awaitable_executor(*t));
				if (ec)
					co_return ec;
				if (t->canceled())
					co_return asio::error::operation_aborted;

				if (!(co_await detail::timer_callback_helper::call(f, t)))
					co_return asio::error::operation_aborted;

				t->expires_after(interval);
			}

			co_return error_code{};
		}, asio::detached);

		return t;
	}

	/**
	 * @brief create a timer.
	 */
	::std::shared_ptr<asio::timer> create_timer(const auto& executor,
		asio::timer::duration first_delay,
		asio::timer::duration interval, ::std::integral auto repeat_times, auto&& callback)
	{
		::std::shared_ptr<asio::timer> t = ::std::make_shared<asio::timer>(executor);

		asio::co_spawn(executor,
		[t, first_delay, interval, repeat_times, f = ::std::forward_like<decltype(callback)>(callback)]
		() mutable -> asio::awaitable<asio::error_code>
		{
			if (first_delay > asio::timer::duration::zero())
				co_await asio::delay(first_delay);

			for (decltype(repeat_times) i = 0; i < repeat_times; ++i)
			{
				auto [ec] = co_await t->async_wait(asio::use_awaitable_executor(*t));
				if (ec)
					co_return ec;
				if (t->canceled())
					co_return asio::error::operation_aborted;

				if (!(co_await detail::timer_callback_helper::call(f, t)))
					co_return asio::error::operation_aborted;

				t->expires_after(interval);
			}

			co_return error_code{};
		}, asio::detached);

		return t;
	}

	/**
	 * @brief create a timer.
	 */
	::std::shared_ptr<asio::timer> create_timer(const auto& executor,
		asio::timer::duration first_delay, asio::timer::duration interval,
		::std::integral auto repeat_times, auto&& callback, auto&& exit_notify)
	{
		::std::shared_ptr<asio::timer> t = ::std::make_shared<asio::timer>(executor);

		asio::co_spawn(executor,
		[t, first_delay, interval, repeat_times, f = ::std::forward_like<decltype(callback)>(callback),
			e = ::std::forward_like<decltype(exit_notify)>(exit_notify)]
		() mutable -> asio::awaitable<asio::error_code>
		{
			if (first_delay > asio::timer::duration::zero())
				co_await asio::delay(first_delay);

			::std::defer notify_when_destroy = [t, e = ::std::move(e)]() mutable
			{
				detail::timer_exit_notify_helper::call(e, t);
			};

			for (decltype(repeat_times) i = 0; i < repeat_times; ++i)
			{
				auto [ec] = co_await t->async_wait(asio::use_awaitable_executor(*t));
				if (ec)
					co_return ec;
				if (t->canceled())
					co_return asio::error::operation_aborted;

				if (!(co_await detail::timer_callback_helper::call(f, t)))
					co_return asio::error::operation_aborted;

				t->expires_after(interval);
			}

			co_return error_code{};
		}, asio::detached);

		return t;
	}
}
