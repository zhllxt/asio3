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
#include <asio3/core/timer.hpp>

namespace asio::detail
{
	struct async_check_error_op
	{
		auto operator()(auto state, auto sock_ref) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			if (!sock.is_open())
				co_return asio::error::not_connected;

			auto [ec] = co_await sock.async_wait(socket_base::wait_error, use_nothrow_deferred);
			co_return ec;
		}
	};

	struct async_check_read_op
	{
		auto operator()(auto state, auto sock_ref, std::chrono::steady_clock::time_point& alive_time) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			while (sock.is_open())
			{
				alive_time = std::chrono::steady_clock::now();

				auto [ec] = co_await sock.async_wait(socket_base::wait_read, use_nothrow_deferred);
				if (ec)
					co_return ec;
			}

			co_return asio::error::not_connected;
		}
	};

	struct async_check_idle_op
	{
		auto operator()(auto state, auto sock_ref,
			std::chrono::steady_clock::time_point& alive_time, timeout_duration timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			steady_timer t(sock.get_executor());

			deadline_time_point deadline = alive_time + timeout;

			while (sock.is_open() && deadline > std::chrono::steady_clock::now())
			{
				t.expires_at(deadline);

				auto [ec] = co_await t.async_wait(use_nothrow_deferred);
				if (ec)
					co_return ec;

				deadline = (std::max)(deadline, alive_time + timeout);
			}

			co_return asio::error::timed_out;
		}
	};
}

namespace asio
{
	/**
	 * @brief Start a asynchronously error check operation.
	 * @param sock - The socket reference to be connected.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream, typename CheckToken = asio::default_token_type<AsyncStream>>
	inline auto async_check_error(
		AsyncStream& sock,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_check_error_op{}, sock),
			token, std::ref(sock));
	}

	/**
	 * @brief Start a asynchronously read check operation.
	 * @param sock - The socket reference to be connected.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream, typename CheckToken = asio::default_token_type<AsyncStream>>
	inline auto async_check_read(
		AsyncStream& sock,
		std::chrono::steady_clock::time_point& alive_time,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_check_read_op{}, sock),
			token, std::ref(sock), std::ref(alive_time));
	}

	/**
	 * @brief Start a asynchronously idle check operation.
	 * @param sock - The socket reference to be connected.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream, typename CheckToken = asio::default_token_type<AsyncStream>>
	inline auto async_check_idle(
		AsyncStream& sock,
		std::chrono::steady_clock::time_point& alive_time,
		timeout_duration timeout,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_check_idle_op{}, sock),
			token, std::ref(sock), std::ref(alive_time), timeout);
	}
}

namespace asio
{
	/**
	 * the alive time will be updated automictly.
	 */
	awaitable<error_code> wait_until_idle_timeout(auto& sock, timeout_duration timeout)
	{
		std::chrono::steady_clock::time_point alive_time{};

		auto result = co_await
		(
			async_check_error(sock, use_nothrow_awaitable) ||
			async_check_read(sock, alive_time, use_nothrow_awaitable) ||
			async_check_idle(sock, alive_time, timeout, use_nothrow_awaitable)
		);

		error_code ec{};
		std::visit(asio::variant_overloaded{
			[](auto) {},
			[&ec](error_code e) mutable
			{
				ec = e;
			}}, result);

		co_return ec;
	}

	/**
	 * You need update the alive time manual by youself.
	 */
	awaitable<error_code> wait_until_idle_timeout(
		auto& sock, std::chrono::steady_clock::time_point& alive_time, timeout_duration timeout)
	{
		auto result = co_await
		(
			async_check_error(sock, use_nothrow_awaitable) ||
			async_check_idle(sock, alive_time, timeout, use_nothrow_awaitable)
		);

		error_code ec{};
		std::visit(asio::variant_overloaded{
			[](auto) {},
			[&ec](error_code e) mutable
			{
				ec = e;
			}}, result);

		co_return ec;
	}
}

namespace asio::detail
{
	struct async_wait_until_idle_timeout_op
	{
		static awaitable<error_code> do_check(
			auto& sock, std::chrono::steady_clock::time_point* alive_time, timeout_duration timeout, auto& ch)
		{
			error_code ec{};

			if (alive_time)
			{
				ec = co_await wait_until_idle_timeout(sock, *alive_time, timeout);
			}
			else
			{
				ec = co_await wait_until_idle_timeout(sock, timeout);
			}

			co_await ch.async_send(ec, asio::use_nothrow_awaitable);

			co_return ec;
		}

		auto operator()(auto state, auto sock_ref,
			std::chrono::steady_clock::time_point* alive_time, timeout_duration timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			experimental::channel<void(error_code)> ch{ sock.get_executor(), 1 };

			asio::co_spawn(sock.get_executor(), do_check(sock, alive_time, timeout, ch), asio::detached);

			auto [e1] = co_await ch.async_receive(asio::use_nothrow_deferred);

			co_return asio::error::timed_out;
		}
	};
}

namespace asio
{
	/**
	 * @brief Start a asynchronously idle timeout check operation.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream, typename CheckToken = asio::default_token_type<AsyncStream>>
	inline auto async_wait_until_idle_timeout(
		AsyncStream& sock,
		timeout_duration timeout,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_wait_until_idle_timeout_op{}, sock),
			token, std::ref(sock), nullptr, timeout);
	}

	/**
	 * @brief Start a asynchronously idle timeout check operation.
	 * @param sock - The socket reference to be connected.
	 * @param timeout - The disconnect timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream, typename CheckToken = asio::default_token_type<AsyncStream>>
	inline auto async_wait_until_idle_timeout(
		AsyncStream& sock,
		std::chrono::steady_clock::time_point& alive_time,
		timeout_duration timeout,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_wait_until_idle_timeout_op{}, sock),
			token, std::ref(sock), std::addressof(alive_time), timeout);
	}
}
