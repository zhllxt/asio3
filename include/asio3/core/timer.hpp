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
#include <asio3/core/netutil.hpp>

namespace asio
{
	using timer = asio::as_tuple_t<::asio::deferred_t>::as_default_on_t<::asio::steady_timer>;

	inline void cancel_timer(asio::steady_timer& t) noexcept
	{
		try
		{
			t.cancel();
		}
		catch (system_error const&)
		{
		}
	}

	struct safe_timer
	{
		explicit safe_timer(const auto& executor) : t(executor)
		{
			canceled.clear();
		}

		inline void cancel()
		{
			canceled.test_and_set();

			cancel_timer(t);
		}

		/// Timer impl
		asio::steady_timer t;

		/// Why use this flag, beacuase the ec param maybe zero when the timer callback is
		/// called after the timer cancel function has called already.
		/// Before : need reset the "canceled" flag to false, otherwise after "client.stop();"
		/// then call client.start(...) again, this reconnect timer will doesn't work .
		/// can't put this "clear" code into the timer handle function, beacuse the stop timer
		/// maybe called many times. so, when the "canceled" flag is set false in the timer handle
		/// and the stop timer is called later, then the "canceled" flag will be set true again .
		std::atomic_flag   canceled;
	};
}

namespace asio::detail
{
	struct timer_tag_t {};

	constexpr timer_tag_t timer_tag{};

	struct async_sleep_op
	{
		auto operator()(auto state, auto&& executor, std::chrono::steady_clock::duration duration) -> void
		{
			co_await asio::dispatch(executor, use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::steady_timer t(executor);

			t.expires_after(duration);

			auto [ec] = co_await t.async_wait(use_nothrow_deferred);

			co_return ec;
		}
	};

	struct call_func_when_timeout
	{
		struct storage
		{
			asio::steady_timer timer;
			bool timeouted{ false };

			storage(auto& executor) : timer(executor)
			{
			}
		};

		std::shared_ptr<storage> ptr;

		call_func_when_timeout(auto&& executor, std::chrono::steady_clock::duration timeout_value, auto&& func)
		{
			ptr = std::make_shared<storage>(executor);

			ptr->timer.expires_after(timeout_value);

			asio::co_spawn(executor, [p = ptr, f = std::forward_like<decltype(func)>(func)]
			() mutable -> asio::awaitable<asio::error_code>
			{
				auto [ec] = co_await p->timer.async_wait(use_nothrow_awaitable);
				if (!ec)
				{
					p->timeouted = true;
					f();
				}
				co_return ec;
			}, asio::detached);
		}

		~call_func_when_timeout()
		{
			try
			{
				ptr->timer.cancel();
			}
			catch (system_error const&)
			{
			}
		}
	};
}

namespace asio
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
		typename SleepToken = default_token_type<asio::timer>>
	inline auto async_sleep(
		Executor&& executor,
		std::chrono::steady_clock::duration duration,
		SleepToken&& token = default_token_type<asio::timer>())
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
	asio::awaitable<asio::error_code> async_sleep(std::chrono::steady_clock::duration duration)
	{
		auto [ec] = co_await async_sleep(co_await asio::this_coro::executor, duration, use_nothrow_awaitable);
		co_return ec;
	}

	/**
	 * @brief Asynchronously sleep for a duration.
	 * @param duration - The duration. 
	 */
	asio::awaitable<asio::error_code> delay(std::chrono::steady_clock::duration duration)
	{
		co_return co_await async_sleep(duration);
	}

	/**
	 * @brief Asynchronously wait a timeout for the duration.
	 * @param duration - The duration. 
	 */
	asio::awaitable<std::tuple<asio::error_code, detail::timer_tag_t>> timeout(
		std::chrono::steady_clock::duration duration)
	{
		co_return std::tuple{ co_await async_sleep(duration), detail::timer_tag };
	}

	/**
	 * @brief Asynchronously wait until the idle timeout.
	 * @param duration - The deadline. 
	 */
	asio::awaitable<error_code> watchdog(std::chrono::steady_clock::time_point& deadline)
	{
		asio::steady_timer watchdog_timer(co_await asio::this_coro::executor);

		auto now = std::chrono::steady_clock::now();

		while (deadline > now)
		{
			watchdog_timer.expires_at(deadline);

			auto [ec] = co_await watchdog_timer.async_wait(use_nothrow_awaitable);
			if (ec)
				co_return ec;

			now = std::chrono::steady_clock::now();
		}

		co_return asio::error::timed_out;
	}

	/**
	 * @brief Asynchronously wait until the idle timeout.
	 * @param duration - The deadline.
	 */
	asio::awaitable<error_code> watchdog(
		asio::steady_timer& watchdog_timer,
		std::chrono::system_clock::time_point& alive_time,
		std::chrono::system_clock::duration idle_timeout)
	{
		auto idled_duration = std::chrono::system_clock::now() - alive_time;

		while (idled_duration < idle_timeout)
		{
			watchdog_timer.expires_after(idle_timeout - idled_duration);

			auto [ec] = co_await watchdog_timer.async_wait(use_nothrow_awaitable);
			if (ec)
				co_return ec;

			idled_duration = std::chrono::system_clock::now() - alive_time;
		}

		co_return asio::error::timed_out;
	}

	/**
	 * @brief Asynchronously wait until the idle timeout.
	 * @param duration - The deadline. 
	 */
	asio::awaitable<error_code> watchdog(
		std::chrono::system_clock::time_point& alive_time,
		std::chrono::system_clock::duration idle_timeout)
	{
		asio::steady_timer watchdog_timer(co_await asio::this_coro::executor);

		co_return co_await watchdog(watchdog_timer, alive_time, idle_timeout);
	}

	/**
	 * @brief Check whether the result of the awaitable_operators is timeout.
	 * @param v - The result of the awaitable_operators which type is std::variant<...>
	 * @eg:
	 * auto result = co_await (asio::async_connect(...) || asio::timeout(...));
	 * if (asio::is_timeout(result))
	 * {
	 * }
	 */
	constexpr bool is_timeout(is_variant auto& v) noexcept
	{
		return std::holds_alternative<std::tuple<asio::error_code, detail::timer_tag_t>>(v);
	}
}
