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

namespace asio
{
	class context_worker
	{
	public:
		explicit context_worker(int concurrency_hint = 1) : context(concurrency_hint)
		{
		}

		~context_worker()
		{
			guard.reset();
			thread.join();
		}

		/**
		 * @brief Running work on the current thread, Blocks the calling thread
		 * until the context is stopped.
		 */
		inline void run() noexcept
		{
			asio::post(thread.get_executor(), [this]() mutable
			{
				context.run();

				thread.stop();
			});

			thread.attach();
		}

		/**
		 * @brief Asynchronously start the work with no blocking.
		 */
		inline void start() noexcept
		{
			asio::post(thread.get_executor(), [this]() mutable
			{
				guard.emplace(context.get_executor());

				context.run();
			});
		}

		/**
		 * @brief Blocks until the thread has no more outstanding work.
		 */
		inline void join() noexcept
		{
			thread.join();
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline auto get_executor() noexcept
		{
			return context.get_executor();
		}

	public:
		asio::thread_pool thread{ 1 };

		asio::io_context  context;

		std::optional<asio::executor_guard> guard{};
	};
}
