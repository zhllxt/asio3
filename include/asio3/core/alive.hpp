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
		auto operator()(auto state, auto sock_ref, std::chrono::system_clock::time_point& alive_time) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			while (sock.is_open())
			{
				alive_time = std::chrono::system_clock::now();

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
			std::chrono::system_clock::time_point& alive_time,
			std::chrono::system_clock::duration idle_timeout,
			asio::cancellation_signal& sig) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			struct auto_emiter
			{
				auto_emiter(asio::cancellation_signal& s) : sig(s)
				{
				}
				~auto_emiter()
				{
					sig.emit(asio::cancellation_type::terminal);
				}
				asio::cancellation_signal& sig;
			} tmp_auto_emiter{ sig };

			steady_timer t(sock.get_executor());

			auto idled_duration = std::chrono::system_clock::now() - alive_time;

			while (sock.is_open() && idled_duration < idle_timeout)
			{
				t.expires_after(idle_timeout - idled_duration);

				auto [ec] = co_await t.async_wait(use_nothrow_deferred);
				if (ec)
					co_return ec;

				idled_duration = std::chrono::system_clock::now() - alive_time;
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
		std::chrono::system_clock::time_point& alive_time,
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
		std::chrono::system_clock::time_point& alive_time,
		std::chrono::system_clock::duration idle_timeout,
		asio::cancellation_signal& sig,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_check_idle_op{}, sock),
			token, std::ref(sock), std::ref(alive_time), idle_timeout, std::ref(sig));
	}
}

namespace asio
{
	/**
	 * You need update the alive time manual by youself.
	 */
	awaitable<error_code> wait_error_or_idle_timeout(
		auto& sock, std::chrono::system_clock::time_point& alive_time,
		std::chrono::system_clock::duration idle_timeout)
	{
		if (!sock.is_open())
			co_return asio::error::operation_aborted;

		asio::cancellation_signal sig;

		auto result = co_await
		(
			async_check_error(sock, asio::bind_cancellation_slot(sig.slot(), use_nothrow_awaitable)) ||
			async_check_idle(sock, alive_time, idle_timeout, sig, use_nothrow_awaitable)
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
	struct async_wait_error_or_idle_timeout_op
	{
		static awaitable<error_code> do_check(
			auto& sock,
			std::chrono::system_clock::time_point& alive_time,
			std::chrono::system_clock::duration idle_timeout, auto& ch)
		{
			error_code ec{};

			ec = co_await wait_error_or_idle_timeout(sock, alive_time, idle_timeout);

			co_await ch.async_send(ec, asio::use_nothrow_awaitable);

			co_return ec;
		}

		auto operator()(auto state, auto sock_ref,
			std::chrono::system_clock::time_point& alive_time,
			std::chrono::system_clock::duration idle_timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			experimental::channel<void(error_code)> ch{ sock.get_executor(), 1 };

			asio::co_spawn(sock.get_executor(), do_check(sock, alive_time, idle_timeout, ch), asio::detached);

			auto [e1] = co_await ch.async_receive(asio::use_nothrow_deferred);

			co_return asio::error::timed_out;
		}
	};
}

namespace asio
{
	/**
	 * @brief Start a asynchronously error or idle timeout check operation.
	 * @param sock - The socket reference to be connected.
	 * @param idle_timeout - The idle timeout.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<typename AsyncStream, typename CheckToken = asio::default_token_type<AsyncStream>>
	inline auto async_wait_error_or_idle_timeout(
		AsyncStream& sock,
		std::chrono::system_clock::time_point& alive_time,
		std::chrono::system_clock::duration idle_timeout,
		CheckToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<CheckToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_wait_error_or_idle_timeout_op{}, sock),
			token, std::ref(sock), alive_time, idle_timeout);
	}
}
