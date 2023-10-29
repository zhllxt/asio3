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
		auto operator()(auto state, auto self_ref, value_type conn) -> void
		{
			auto& self = self_ref.get();

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
		auto operator()(auto state, auto self_ref, key_type key) -> void
		{
			auto& self = self_ref.get();

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
		auto operator()(auto state, auto self_ref, key_type key) -> void
		{
			auto& self = self_ref.get();

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
	struct connection_map_t<ConnectionT>::async_disconnect_all_op
	{
		auto operator()(auto state, auto self_ref, auto&& pred) -> void
		{
			auto& self = self_ref.get();
			auto fun = std::forward_like<decltype(pred)>(pred);

			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			std::size_t total = self.map.size();

			for (auto& [key, conn] : self.map)
			{
				if (!fun(conn))
					continue;

				co_await conn->async_disconnect(asio::use_nothrow_deferred);
			}

			self.map.clear();

			self.lock.try_receive([](auto...) {});

			co_return total;
		}
	};

	template<typename ConnectionT>
	struct connection_map_t<ConnectionT>::async_send_all_op
	{
		auto operator()(auto state, auto self_ref, auto&& data, auto&& pred) -> void
		{
			auto& self = self_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);
			auto fun = std::forward_like<decltype(pred)>(pred);

			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			std::size_t total{ 0 };

			for (auto& [key, conn] : self.map)
			{
				if (!fun(conn))
					continue;

				auto [e1, n1] = co_await conn->async_send(asio::buffer(msg), asio::use_nothrow_deferred);
				total += n1;
			}

			self.lock.try_receive([](auto...) {});

			co_return{ error_code{}, total };
		}
	};

	template<typename ConnectionT>
	struct connection_map_t<ConnectionT>::async_for_each_op
	{
		template<class Function>
		auto operator()(auto state, auto self_ref, Function&& func) -> void
		{
			using fun_traits_type = function_traits<std::remove_cvref_t<Function>>;
			using fun_ret_type = typename fun_traits_type::return_type;

			auto& self = self_ref.get();

			auto fn = std::forward<Function>(func);

			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_deferred);

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::deferred);
			}

			for (auto& [key, conn] : self.map)
			{
				if constexpr (asio::is_template_instance_of<asio::awaitable, fun_ret_type>)
				{
					co_await asio::async_call_coroutine(self.get_executor(), fn(conn), asio::use_nothrow_deferred);
				}
				else
				{
					fn(conn);
				}
			}

			self.lock.try_receive([](auto...) {});

			co_return error_code{};
		}
	};
}
