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
	template<typename ConnectionT>
	class connection_map_t : public std::enable_shared_from_this<connection_map_t<ConnectionT>>
	{
	public:
		using connection_type = ConnectionT;
		using key_type   = typename connection_type::key_type;
		using value_type = std::shared_ptr<connection_type>;
		using map_type   = std::unordered_map<key_type, value_type>;
		using lock_type  = as_tuple_t<deferred_t>::as_default_on_t<experimental::channel<void()>>;

		struct async_emplace_op;
		struct async_erase_op;
		struct async_find_op;
		struct async_for_each_op;

	public:
		template<class Executor>
		explicit connection_map_t(const Executor& ex)
			: lock(ex, 1)
		{
			map.reserve(64);
		}

		~connection_map_t()
		{
		}

		/**
		 * @brief emplace connection
		 */
		template<
			ASIO_COMPLETION_TOKEN_FOR(void(bool)) EmplaceToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename lock_type::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(EmplaceToken, void(bool))
		async_emplace(
			value_type conn,
			EmplaceToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename lock_type::executor_type))
		{
			return asio::async_initiate<EmplaceToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_emplace_op{}, lock),
				token, std::ref(*this), std::move(conn));
		}

		/**
		 * @brief erase connection by the key
		 */
		template<
			ASIO_COMPLETION_TOKEN_FOR(void(bool)) EraseToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename lock_type::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(EraseToken, void(bool))
		async_erase(
			key_type key,
			EraseToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename lock_type::executor_type))
		{
			return asio::async_initiate<EraseToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_erase_op{}, lock),
				token, std::ref(*this), std::move(key));
		}

		template<
			ASIO_COMPLETION_TOKEN_FOR(void(value_type)) FindToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename lock_type::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(FindToken, void(value_type))
		async_find(
			key_type key,
			FindToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename lock_type::executor_type))
		{
			return asio::async_initiate<FindToken, void(value_type)>(
				asio::experimental::co_composed<void(value_type)>(
					async_find_op{}, lock),
				token, std::ref(*this), std::move(key));
		}

		/**
		 * @brief call user custom callback function for every connection
		 * the custom callback function is like this :
		 * [](std::shared_ptr<tcp_connection>& conn){}
		 */
		template<
			typename Function,
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) ForEachToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename lock_type::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ForEachToken, void(asio::error_code))
		async_for_each(
			Function&& func,
			ForEachToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename lock_type::executor_type))
		{
			return asio::async_initiate<ForEachToken, void(asio::error_code)>(
				asio::experimental::co_composed<void(asio::error_code)>(
					async_for_each_op{}, lock),
				token, std::ref(*this), std::forward<Function>(func));
		}

		/**
		 * @brief get connection count
		 */
		inline std::size_t size() const noexcept
		{
			return map.size();
		}

		/**
		 * @brief Checks if the connection container has no elements
		 */
		inline bool empty() const noexcept
		{
			return map.empty();
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return lock.get_executor();
		}

	public:
		map_type  map;

		lock_type lock;
	};
}

#include <asio3/core/impl/connection_map.ipp>