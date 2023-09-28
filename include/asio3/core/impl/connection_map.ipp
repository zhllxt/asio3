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

namespace asio
{
	template<typename ConnectionT>
	struct connection_map_t<ConnectionT>::async_emplace_op
	{
		auto operator()(auto state, connection_map_t& self, value_type conn) -> void
		{
			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			auto [it, inserted] = self.map.emplace(conn->hash_key(), conn);

			self.lock.try_receive([](auto...) {});

			co_return inserted;
		}
	};

	template<typename ConnectionT>
	struct connection_map_t<ConnectionT>::async_erase_op
	{
		auto operator()(auto state, connection_map_t& self, key_type key) -> void
		{
			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			bool erased = (self.map.erase(key) > 0);

			self.lock.try_receive([](auto...) {});

			co_return erased;
		}
	};

	template<typename ConnectionT>
	struct connection_map_t<ConnectionT>::async_find_op
	{
		auto operator()(auto state, connection_map_t& self, key_type key) -> void
		{
			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			auto it = self.map.find(key);

			self.lock.try_receive([](auto...) {});

			co_return it == self.map.end() ? nullptr : it->second;
		}
	};

	template<typename ConnectionT>
	struct connection_map_t<ConnectionT>::async_for_each_op
	{
		template<typename Function>
		auto operator()(auto state, connection_map_t& self, Function&& func) -> void
		{
			//auto co_fn = std::forward<Function>(func);

			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			for (auto& [key, conn] : self.map)
			{
				//co_await co_fn(conn);
			}

			self.lock.try_receive([](auto...) {});

			co_return error_code{};
		}
	};
}
