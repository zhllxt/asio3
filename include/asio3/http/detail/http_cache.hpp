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
#include <map>
#include <unordered_map>
#include <type_traits>
#include <shared_mutex>

#include <asio3/core/function_traits.hpp>
#include <asio3/core/netutil.hpp>

#ifdef ASIO3_HEADER_ONLY
#include <asio/detail/null_mutex.hpp>
namespace bho::beast::http
#else
#include <boost/asio/detail/null_mutex.hpp>
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
}

namespace asio::detail
{
	template<class MessageT, class MutexT>
	class basic_http_cache_t
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
		/**
		 * @brief constructor
		 */
		basic_http_cache_t()
		{
		}

		/**
		 * @brief destructor
		 */
		~basic_http_cache_t() = default;

		/**
		 * @brief Add a element into the cache.
		 */
		template<class StringT>
		inline cache_node* emplace(StringT&& url, MessageT msg)
		{
			std::unique_lock guard(this->http_cache_mutex_);

			if (this->http_cache_map_.size() >= http_caches_max_count_)
				return nullptr;

			// can't use insert_or_assign, it maybe cause the msg was changed in multithread, and 
			// other thread are using the msg at this time.
			return std::addressof(this->http_cache_map_.emplace(
				detail::to_string(std::forward<StringT>(url)),
				cache_node{ std::chrono::steady_clock::now(), std::move(msg) }).first->second);
		}

		/**
		 * @brief Checks if the cache has no elements.
		 */
		inline bool empty() const noexcept
		{
			std::shared_lock guard(this->http_cache_mutex_);

			return this->http_cache_map_.empty();
		}

		/**
		 * @brief Finds the cache with key equivalent to url.
		 */
		template<class StringT>
		inline cache_node* find(const StringT& url)
		{
			std::shared_lock guard(this->http_cache_mutex_);

			if (this->http_cache_map_.empty())
				return nullptr;

			// If rehashing occurs due to the insertion, all iterators are invalidated.
			// Otherwise iterators are not affected. References are not invalidated. 
			if constexpr (std::is_same_v<StringT, std::string>)
			{
				if (auto it = this->http_cache_map_.find(url); it != this->http_cache_map_.end())
					return std::addressof(it->second);
			}
			else
			{
				std::string str = detail::to_string(url);
				if (auto it = this->http_cache_map_.find(str); it != this->http_cache_map_.end())
					return std::addressof(it->second);
			}

			return nullptr;
		}

		/**
		 * @brief Set the max number of elements in the container.
		 */
		inline void set_cache_max_count(std::size_t count) noexcept
		{
			this->http_caches_max_count_ = count;
		}

		/**
		 * @brief Get the max number of elements in the container.
		 */
		inline std::size_t get_cache_max_count() const noexcept
		{
			return this->http_caches_max_count_;
		}

		/**
		 * @brief Get the current number of elements in the container.
		 */
		inline std::size_t get_cache_count() const noexcept
		{
			std::shared_lock guard(this->http_cache_mutex_);

			return this->http_cache_map_.size();
		}

		/**
		 * @brief Requests the removal of expired elements.
		 */
		inline void shrink_to_fit()
		{
			std::unique_lock guard(this->http_cache_mutex_);

			if (this->http_cache_map_.size() < http_caches_max_count_)
				return (*this);

			std::multimap<std::chrono::steady_clock::duration::rep, const std::string*> mms;
			for (auto& [url, node] : this->http_cache_map_)
			{
				mms.emplace(node.alive.time_since_epoch().count(), std::addressof(url));
			}

			std::size_t i = 0, n = mms.size() / 3;
			for (auto& [t, purl] : mms)
			{
				detail::ignore_unused(t);
				if (++i > n)
					break;
				this->http_cache_map_.erase(*purl);
			}
		}

		/**
		 * @brief Erases all elements from the container.
		 */
		inline void clear() noexcept
		{
			std::unique_lock guard(this->http_cache_mutex_);
			this->http_cache_map_.clear();
		}

	protected:
		mutable MutexT                              http_cache_mutex_;

		std::unordered_map<std::string, cache_node> http_cache_map_;

		std::size_t                                 http_caches_max_count_ = 100000;
	};
}

#include <asio3/core/detail/pop_options.hpp>
