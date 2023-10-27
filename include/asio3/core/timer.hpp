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
	using timer_duration = std::chrono::steady_clock::duration;
	using timeout_duration = std::chrono::steady_clock::duration;

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
		auto operator()(auto state, auto&& executor, timer_duration duration) -> void
		{
			co_await asio::dispatch(executor, use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::steady_timer t(executor);

			t.expires_after(duration);

			auto [ec] = co_await t.async_wait(use_nothrow_deferred);

			co_return ec;
		}
	};

	struct close_socket_when_timeout
	{
		std::shared_ptr<asio::steady_timer> t;

		close_socket_when_timeout(auto& sock, timeout_duration val) noexcept
		{
			t = std::make_shared<asio::steady_timer>(sock.get_executor(), val);

			asio::co_spawn(sock.get_executor(),
			[&sock, p = t]() mutable -> asio::awaitable<asio::error_code>
			{
				auto [ec] = co_await p->async_wait(use_nothrow_awaitable);
				if (!ec)
				{
					sock.close(ec);
				}
				co_return ec;
			}, asio::detached);
		}

		~close_socket_when_timeout()
		{
			try
			{
				t->cancel();
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
	template<typename Executor,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) SleepToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::timer::executor_type)>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(SleepToken, void(asio::error_code))
	async_sleep(
		Executor&& executor,
		timer_duration duration,
		SleepToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::timer::executor_type))
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
	asio::awaitable<asio::error_code> async_sleep(timer_duration duration)
	{
		auto [ec] = co_await async_sleep(co_await asio::this_coro::executor, duration, use_nothrow_awaitable);
		co_return ec;
	}

	/**
	 * @brief Asynchronously sleep for a duration.
	 * @param duration - The duration. 
	 */
	asio::awaitable<asio::error_code> delay(timer_duration duration)
	{
		co_return co_await async_sleep(duration);
	}

	/**
	 * @brief Asynchronously wait a timeout for the duration.
	 * @param duration - The duration. 
	 */
	asio::awaitable<std::tuple<asio::error_code, detail::timer_tag_t>> timeout(timer_duration duration)
	{
		co_return std::tuple{ co_await async_sleep(duration), detail::timer_tag };
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
