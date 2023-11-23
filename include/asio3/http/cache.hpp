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

#include <asio3/core/detail/push_options.hpp>

#include <cstdint>
#include <memory>
#include <chrono>
#include <functional>
#include <atomic>
#include <string>
#include <string_view>
#include <unordered_map>
#include <type_traits>
#include <shared_mutex>

#include <asio3/core/function_traits.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/null_shared_mutex.hpp>

#include <asio3/core/beast.hpp>
#include <asio3/http/core.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
	struct enable_cache_t {};

	constexpr enable_cache_t enable_cache;

	template<bool isRequest, class Body, class Fields = fields>
	inline bool is_cache_enabled(http::message<isRequest, Body, Fields>& msg)
	{
		if constexpr (isRequest)
		{
			if (msg.method() == http::verb::get)
			{
				return true;
			}
			return false;
		}
		else
		{
			return true;
		}
	}

	template<class MessageT, class MutexT>
	class basic_cache
	{
	protected:
		struct cache_node
		{
			std::chrono::steady_clock::time_point alive;

			MessageT msg;

			cache_node(std::chrono::steady_clock::time_point t, MessageT m)
				: alive(t), msg(std::move(m))
			{
			}

			inline void update_alive_time() noexcept
			{
				alive = std::chrono::steady_clock::now();
			}
		};

	public:
		using message_type = MessageT;
		using mutex_type = MutexT;

		/**
		 * @brief constructor
		 */
		basic_cache()
		{
		}

		/**
		 * @brief destructor
		 */
		~basic_cache() = default;

		/**
		 * @brief Add a element into the cache.
		 */
		template<class StringT>
		inline cache_node* add(StringT&& url, MessageT msg)
		{
			std::unique_lock guard(this->cache_mutex_);

			if (this->cache_map_.size() >= cache_max_count_)
				return nullptr;

			// can't use insert_or_assign, it maybe cause the msg was changed in multithread, and 
			// other thread are using the msg at this time.
			return std::addressof(this->cache_map_.emplace(
				asio::to_string(std::forward<StringT>(url)),
				cache_node{ std::chrono::steady_clock::now(), std::move(msg) }).first->second);
		}

		/**
		 * @brief Checks if the cache has no elements.
		 */
		inline bool empty() const noexcept
		{
			std::shared_lock guard(this->cache_mutex_);

			return this->cache_map_.empty();
		}

		/**
		 * @brief Checks if the cache is full.
		 */
		inline bool full() const noexcept
		{
			std::shared_lock guard(this->cache_mutex_);

			return this->cache_map_.size() >= cache_max_count_;
		}

		/**
		 * @brief Finds the cache with key equivalent to url.
		 */
		template<class StringT>
		inline cache_node* find(const StringT& url)
		{
			std::shared_lock guard(this->cache_mutex_);

			if (this->cache_map_.empty())
				return nullptr;

			// If rehashing occurs due to the insertion, all iterators are invalidated.
			// Otherwise iterators are not affected. References are not invalidated. 
			std::string str = asio::to_string(url);
			if (auto it = this->cache_map_.find(str); it != this->cache_map_.end())
				return std::addressof(it->second);

			return nullptr;
		}

		/**
		 * @brief Set the max number of elements in the container.
		 */
		inline void set_cache_max_count(std::size_t count) noexcept
		{
			this->cache_max_count_ = count;
		}

		/**
		 * @brief Get the max number of elements in the container.
		 */
		inline std::size_t get_cache_max_count() const noexcept
		{
			return this->cache_max_count_;
		}

		/**
		 * @brief Get the current number of elements in the container.
		 */
		inline std::size_t get_cache_count() const noexcept
		{
			std::shared_lock guard(this->cache_mutex_);

			return this->cache_map_.size();
		}

		/**
		 * @brief Requests the removal of expired elements.
		 */
		inline void shrink_to_fit()
		{
			std::unique_lock guard(this->cache_mutex_);

			if (this->cache_map_.size() < cache_max_count_)
				return;

			std::multimap<std::chrono::steady_clock::duration::rep, const std::string*> mms;
			for (auto& [url, node] : this->cache_map_)
			{
				mms.emplace(node.alive.time_since_epoch().count(), std::addressof(url));
			}

			std::size_t i = 0, n = mms.size() / 3;
			for (auto& [t, purl] : mms)
			{
				asio::ignore_unused(t);
				if (++i > n)
					break;
				this->cache_map_.erase(*purl);
			}
		}

		/**
		 * @brief Erases all elements from the container.
		 */
		inline void clear() noexcept
		{
			std::unique_lock guard(this->cache_mutex_);
			this->cache_map_.clear();
		}

	protected:
		mutable MutexT                              cache_mutex_;

		std::unordered_map<std::string, cache_node> cache_map_;

		std::size_t                                 cache_max_count_ = 0xffff;
	};

	using cache = basic_cache<http::response<http::string_body>, asio::null_shared_mutex>;
}

#include <asio3/core/detail/pop_options.hpp>
