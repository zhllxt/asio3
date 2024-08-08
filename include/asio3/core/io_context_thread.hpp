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

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	class io_context_thread
	{
	public:
		explicit io_context_thread(int concurrency_hint = 1) : context(concurrency_hint)
		{
			thread = ::std::thread([this]() mutable
			{
				context.run();
			});
		}

		~io_context_thread()
		{
			join();
		}

		/**
		 * @brief Blocks until the thread has no more outstanding work.
		 */
		inline void join() noexcept
		{
			guard->reset();

			if (thread.joinable())
				thread.join();
		}

		/**
		 * @brief Restart the io_context and thread.
		 * if the thread is not finished, there will be no effect.
		 * 
		 */
		inline void restart()
		{
			if (thread.joinable())
				return;

			context.restart();

			guard = std::make_unique<asio::executor_guard>(context.get_executor());

			thread = ::std::thread([this]() mutable
			{
				context.run();
			});
		}

		/**
		 * @brief Get the executor associated of the io_context.
		 */
		inline asio::io_context::executor_type get_executor() noexcept
		{
			return context.get_executor();
		}

	public:
		::std::thread        thread{};

		asio::io_context     context{ 1 };

		std::unique_ptr<asio::executor_guard> guard{
			std::make_unique<asio::executor_guard>(context.get_executor()) };
	};
}
