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
	template<typename SessionT>
	struct basic_session_map<SessionT>::async_add_op
	{
		auto operator()(auto state, auto self_ref, value_type conn) -> void
		{
			auto& self = self_ref.get();

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			auto [it, inserted] = self.map.emplace(conn->hash_key(), conn);

			co_return inserted;
		}
	};

	template<typename SessionT>
	struct basic_session_map<SessionT>::async_find_or_add_op
	{
		auto operator()(auto state, auto self_ref, key_type key, auto&& create_func) -> void
		{
			auto& self = self_ref.get();
			auto&& create_fn = std::forward_like<decltype(create_func)>(create_func);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			bool is_new_client = false;

			auto it = self.map.find(key);
			if (it == self.map.end())
			{
				is_new_client = true;
				it = self.map.emplace(key, create_fn()).first;
			}

			co_return{ it->second, is_new_client };
		}
	};

	template<typename SessionT>
	struct basic_session_map<SessionT>::async_remove_op
	{
		auto operator()(auto state, auto self_ref, key_type key) -> void
		{
			auto& self = self_ref.get();

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			bool erased = (self.map.erase(key) > 0);

			co_return erased;
		}
	};

	template<typename SessionT>
	struct basic_session_map<SessionT>::async_find_op
	{
		auto operator()(auto state, auto self_ref, key_type key) -> void
		{
			auto& self = self_ref.get();

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			auto it = self.map.find(key);

			co_return it == self.map.end() ? nullptr : it->second;
		}
	};

	template<typename SessionT>
	struct basic_session_map<SessionT>::async_disconnect_all_op
	{
		auto operator()(auto state, auto self_ref, auto&& pred) -> void
		{
			auto& self = self_ref.get();
			auto fun = std::forward_like<decltype(pred)>(pred);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			std::size_t total = 0;

			if (!self.map.empty())
			{
				asio::experimental::channel<void(error_code)> ch(self.get_executor(), 1);

				for (auto it = self.map.begin(); it != self.map.end();)
				{
					if (!fun(it->second))
					{
						++it;
						continue;
					}

					it->second->async_disconnect(asio::bind_executor(ch.get_executor(),
					[conn = it->second, &ch](auto...) mutable
					{
						ch.async_send(error_code{}, [](auto...) {});
					}));

					it = self.map.erase(it);

					++total;
				}

				for (std::size_t i = 0; i < total; ++i)
				{
					co_await ch.async_receive(asio::use_deferred_executor(ch));
				}
			}

			co_return total;
		}
	};

	template<typename SessionT>
	struct basic_session_map<SessionT>::async_send_all_op
	{
		auto operator()(auto state, auto self_ref, auto&& data, auto&& pred) -> void
		{
			auto& self = self_ref.get();

			auto msg = std::forward_like<decltype(data)>(data);
			auto fun = std::forward_like<decltype(pred)>(pred);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			std::size_t total{ 0 };

			for (auto& [key, conn] : self.map)
			{
				if (!fun(conn))
					continue;

				auto [e1, n1] = co_await conn->async_send(
					asio::to_buffer(msg), asio::use_deferred_executor(self));
				total += n1;
			}

			co_return{ error_code{}, total };
		}
	};

	template<typename SessionT>
	struct basic_session_map<SessionT>::async_for_each_op
	{
		template<class Function>
		auto operator()(auto state, auto self_ref, Function&& func) -> void
		{
			using fun_traits_type = function_traits<std::remove_cvref_t<Function>>;
			using fun_ret_type = typename fun_traits_type::return_type;

			auto& self = self_ref.get();

			auto fn = std::forward<Function>(func);

			co_await asio::dispatch(asio::use_deferred_executor(self));

			if (!self.lock.try_send())
			{
				co_await self.lock.async_send(asio::use_deferred_executor(self.lock));
			}

			[[maybe_unused]] asio::defer_unlock defered_unlock{ self.lock };

			for (auto& [key, conn] : self.map)
			{
				if constexpr (asio::is_template_instance_of<asio::awaitable, fun_ret_type>)
				{
					co_await asio::async_call_coroutine(self.get_executor(),
						fn(conn), asio::use_deferred_executor(self));
				}
				else
				{
					fn(conn);
				}
			}

			co_return error_code{};
		}
	};
}
