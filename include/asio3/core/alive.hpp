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

#include <asio3/core/timer.hpp>
#include <asio3/core/asio.hpp>

namespace asio::detail
{
	struct async_alive_check_op
	{
		auto operator()(auto state, auto sock_ref, timeout_duration timeout) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			co_await asio::dispatch(sock.get_executor(), asio::use_nothrow_deferred);

			if (!sock.is_open())
				co_return asio::error::operation_aborted;

			co_await asio::async_lock(sock, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ sock };

			asio::error_code ec{};

			asio::socket_base::linger linger{};

			sock.get_option(linger, ec);

			if (!ec && !(linger.enabled() == true && linger.timeout() == 0))
			{
				sock.shutdown(asio::socket_base::shutdown_send, ec);

				if (!ec)
				{
					[[maybe_unused]] detail::close_socket_when_timeout tg(sock, timeout);

					co_await sock.async_wait(asio::socket_base::wait_error, use_nothrow_deferred);

					sock.close(ec);
				}
				else
				{
					sock.shutdown(asio::socket_base::shutdown_receive, ec);
					sock.close(ec);
				}
			}
			else
			{
				sock.shutdown(asio::socket_base::shutdown_both, ec);
				sock.close(ec);
			}

			co_return ec;
		}
	};
}

namespace asio
{
}
