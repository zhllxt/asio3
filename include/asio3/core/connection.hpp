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
	asio::awaitable<void> check_error(auto conn)
	{
		auto& sock = conn->socket;

		co_await sock.async_wait(socket_base::wait_error, use_nothrow_awaitable);

		conn->stop();
	}

	asio::awaitable<void> check_read(auto conn, std::chrono::steady_clock::time_point& deadline)
	{
		auto& sock = conn->socket;

		while (sock.is_open())
		{
			deadline = (std::max)(deadline, std::chrono::steady_clock::now() + conn->option.idle_timeout);

			auto [e1] = co_await sock.async_wait(socket_base::wait_read, use_nothrow_awaitable);
			if (e1)
				break;
		}

		conn->stop();
	}

	asio::awaitable<void> check_idle(auto conn, std::chrono::steady_clock::time_point& deadline)
	{
		auto& sock = conn->socket;

		steady_timer& t = conn->idle_timer;

		auto now = std::chrono::steady_clock::now();

		while (sock.is_open() && deadline > now)
		{
			t.expires_at(deadline);

			auto [e1] = co_await t.async_wait(use_nothrow_awaitable);
			if (e1)
				break;

			now = std::chrono::steady_clock::now();
		}

		conn->disconnect();
	}
}
