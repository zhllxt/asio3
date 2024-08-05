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

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<
		typename TimerHandle,
		typename TimerObject,
		typename TimerHandleHash,
		typename TimerHandleEqual,
		typename TimerMap
	>
	struct basic_timer_map<TimerHandle, TimerObject, TimerHandleHash, TimerHandleEqual, TimerMap>::async_add_op
	{
		auto operator()(auto state, auto self_ref,
			timer_handle&& id,
			asio::timer::duration first_delay,
			asio::timer::duration interval, auto repeat_times,
			auto&& fun) -> void
		{
			auto& self = self_ref.get();

			assert(repeat_times != 0);

			auto handle = std::move(id);
			auto callback = std::forward_like<decltype(fun)>(fun);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			if (interval > (asio::timer::duration::max)())
				interval = (asio::timer::duration::max)();

			if (first_delay > (asio::timer::duration::max)())
				first_delay = (asio::timer::duration::max)();

			// if the timer is already exists, cancel it first.
			auto iter = self.map.find(handle);
			if (iter != self.map.end())
			{
				asio::cancel_timer(*(iter->second));
			}

			// the asio::timer's constructor may be throw some exception.
			value_type timer_ptr = asio::create_timer(
				self.get_executor(), first_delay, interval, repeat_times,
				std::move(callback),
				[&self, handle]() mutable
				{
					self.map.erase(handle);
				});

			self.map[std::move(handle)] = std::move(timer_ptr);

			co_return true;
		}
	};

	template<
		typename TimerHandle,
		typename TimerObject,
		typename TimerHandleHash,
		typename TimerHandleEqual,
		typename TimerMap
	>
	struct basic_timer_map<TimerHandle, TimerObject, TimerHandleHash, TimerHandleEqual, TimerMap>::async_remove_op
	{
		auto operator()(auto state, auto self_ref, timer_handle&& id) -> void
		{
			auto& self = self_ref.get();

			auto handle = std::move(id);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			auto iter = self.map.find(handle);
			if (iter != self.map.end())
			{
				asio::cancel_timer(*(iter->second));

				self.map.erase(iter);

				co_return true;
			}

			co_return false;
		}
	};

	template<
		typename TimerHandle,
		typename TimerObject,
		typename TimerHandleHash,
		typename TimerHandleEqual,
		typename TimerMap
	>
	struct basic_timer_map<TimerHandle, TimerObject, TimerHandleHash, TimerHandleEqual, TimerMap>::async_clear_op
	{
		auto operator()(auto state, auto self_ref) -> void
		{
			auto& self = self_ref.get();

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			for (auto& [id, timer_ptr] : self.map)
			{
				asio::ignore_unused(id);

				asio::cancel_timer(*timer_ptr);
			}

			self.map.clear();

			co_return true;
		}
	};

	template<
		typename TimerHandle,
		typename TimerObject,
		typename TimerHandleHash,
		typename TimerHandleEqual,
		typename TimerMap
	>
	struct basic_timer_map<TimerHandle, TimerObject, TimerHandleHash, TimerHandleEqual, TimerMap>::async_find_op
	{
		auto operator()(auto state, auto self_ref, timer_handle&& id) -> void
		{
			auto& self = self_ref.get();

			auto handle = std::move(id);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			auto it = self.map.find(handle);

			co_return it == self.map.end() ? nullptr : it->second;
		}
	};
}
