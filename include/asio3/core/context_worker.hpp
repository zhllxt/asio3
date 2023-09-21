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
	class context_worker : public std::enable_shared_from_this<context_worker>
	{
	public:
		explicit context_worker(int concurrency_hint = 1) : context(concurrency_hint)
		{
			started.clear();
		}

		~context_worker()
		{
			stop();
		}

		/**
		 * @brief Running work on the current thread, Blocks the calling thread
		 * until the context is stopped.
		 */
		inline void run() noexcept
		{
			if (!started.test_and_set())
			{
				asio::post(thread.get_executor(), [this]() mutable
				{
					thread_id = std::this_thread::get_id();

					context.run();

					thread.stop();
				});

				thread.attach();
			}
		}

		/**
		 * @brief Asynchronously start the work with no blocking.
		 */
		inline void start() noexcept
		{
			if (!started.test_and_set())
			{
				asio::post(thread.get_executor(), [this]() mutable
				{
					thread_id = std::this_thread::get_id();

					guard.emplace(context.get_executor());

					context.run();
				});
			}
		}

		/**
		 * @brief Blocks until the thread has no more outstanding work.
		 */
		inline void stop() noexcept
		{
			if (started.test())
			{
				guard.reset();
				thread.join();
				thread_id = {};
				started.clear();
			}
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline auto get_executor() noexcept
		{
			return context.get_executor();
		}

		/**
		 * @brief Determine whether the object is running in the current thread.
		 */
		inline bool running_in_this_thread() const noexcept
		{
			return thread_id == std::this_thread::get_id();
		}

		/**
		 * @brief return the thread id of the current object running in.
		 */
		inline std::thread::id get_thread_id() const noexcept
		{
			return thread_id;
		}

	public:
		asio::thread_pool thread{ 1 };

		asio::io_context  context;

		std::atomic_flag  started{};

		std::thread::id   thread_id{};

		std::optional<asio::executor_guard> guard{};
	};
}
