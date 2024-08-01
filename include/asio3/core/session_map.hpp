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
#include <asio3/core/with_lock.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/function_traits.hpp>
#include <asio3/core/data_persist.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT>
	class basic_session_map
	{
	public:
		using session_type = SessionT;
		using key_type   = typename session_type::key_type;
		using value_type = std::shared_ptr<session_type>;
		using map_type   = std::unordered_map<key_type, value_type>;
		using lock_type  = as_tuple_t<use_awaitable_t<>>::as_default_on_t<experimental::channel<void()>>;
		using executor_type = typename lock_type::executor_type;

		struct async_add_op;
		struct async_find_or_add_op;
		struct async_remove_op;
		struct async_find_op;
		struct async_disconnect_all_op;
		struct async_send_all_op;
		struct async_for_each_op;

	public:
		template<class Executor>
		explicit basic_session_map(const Executor& ex) : lock(ex, 1)
		{
			map.reserve(64);
		}

		basic_session_map(basic_session_map&&) noexcept = default;
		basic_session_map& operator=(basic_session_map&&) noexcept = default;

		~basic_session_map()
		{
		}

		/**
		 * @brief add session
		 */
		template<typename AddToken = asio::default_token_type<lock_type>>
		inline auto async_add(
			value_type conn,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<AddToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_add_op{}, lock),
				token, std::ref(*this), std::move(conn));
		}

		/**
		 * @brief find the session, if it's not exists, add a new session
		 */
		template<typename AddToken = asio::default_token_type<lock_type>>
		inline auto async_find_or_add(
			key_type key,
			auto&& create_func,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<AddToken, void(value_type, bool)>(
				asio::experimental::co_composed<void(value_type, bool)>(
					async_find_or_add_op{}, lock),
				token, std::ref(*this), std::move(key),
				std::forward_like<decltype(create_func)>(create_func));
		}

		/**
		 * @brief remove session by the key
		 */
		template<typename RemoveToken = asio::default_token_type<lock_type>>
		inline auto async_remove(
			key_type key,
			RemoveToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<RemoveToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_remove_op{}, lock),
				token, std::ref(*this), std::move(key));
		}

		/**
		 * @brief remove the specified session
		 */
		template<typename RemoveToken = asio::default_token_type<lock_type>>
		inline auto async_remove(
			std::shared_ptr<session_type>& conn,
			RemoveToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<RemoveToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_remove_op{}, lock),
				token, std::ref(*this), conn->hash_key());
		}

		/**
		 * @brief find session by the key
		 */
		template<typename FindToken = asio::default_token_type<lock_type>>
		inline auto async_find(
			key_type key,
			FindToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<FindToken, void(value_type)>(
				asio::experimental::co_composed<void(value_type)>(
					async_find_op{}, lock),
				token, std::ref(*this), std::move(key));
		}

		/**
		 * @brief disconnect all the sessions in the map.
		 */
		template<typename DisconnectToken = asio::default_token_type<lock_type>>
		inline auto async_disconnect_all(
			DisconnectToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<DisconnectToken, void(std::size_t)>(
				asio::experimental::co_composed<void(std::size_t)>(
					async_disconnect_all_op{}, lock),
				token, std::ref(*this), [](auto&) { return true; });
		}

		/**
		 * @brief disconnect the selected sessions in the map.
		 */
		template<typename DisconnectToken = asio::default_token_type<lock_type>>
		inline auto async_disconnect_selected(
			auto&& pred,
			DisconnectToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<DisconnectToken, void(std::size_t)>(
				asio::experimental::co_composed<void(std::size_t)>(
					async_disconnect_all_op{}, lock),
				token, std::ref(*this), std::forward_like<decltype(pred)>(pred));
		}

		/**
		 * @brief send data to all the sessions in the map.
		 */
		template<typename SendToken = asio::default_token_type<lock_type>>
		inline auto async_send_all(
			auto&& data,
			SendToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<SendToken, void(error_code, std::size_t)>(
				asio::experimental::co_composed<void(error_code, std::size_t)>(
					async_send_all_op{}, lock),
				token, std::ref(*this),
				detail::data_persist(std::forward_like<decltype(data)>(data)),
				[](auto&) { return true; });
		}

		/**
		 * @brief send data to the selected sessions in the map.
		 * @param data - 
		 * @param pred - [](auto& conn){ return true; }
		 */
		template<typename SendToken = asio::default_token_type<lock_type>>
		inline auto async_send_selected(
			auto&& data,
			auto&& pred,
			SendToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<SendToken, void(error_code, std::size_t)>(
				asio::experimental::co_composed<void(error_code, std::size_t)>(
					async_send_all_op{}, lock),
				token, std::ref(*this),
				detail::data_persist(std::forward_like<decltype(data)>(data)),
				std::forward_like<decltype(pred)>(pred));
		}

		/**
		 * @brief call user custom callback function for every session
		 * the custom callback function is like this :
		 * [](std::shared_ptr<tcp_session>& conn){}
		 */
		template<
			typename Function,
			typename ForEachToken = asio::default_token_type<lock_type>>
		inline auto async_for_each(
			Function&& func,
			ForEachToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<ForEachToken, void(asio::error_code)>(
				asio::experimental::co_composed<void(asio::error_code)>(
					async_for_each_op{}, lock),
				token, std::ref(*this), std::forward<Function>(func));
		}

		/**
		 * @brief get session count
		 */
		[[nodiscard]] inline std::size_t size() noexcept
		{
			return map.size();
		}

		/**
		 * @brief get session count
		 */
		[[nodiscard]] inline std::size_t count() noexcept
		{
			return map.size();
		}

		/**
		 * @brief Checks if the session container has no elements
		 */
		[[nodiscard]] inline bool empty() noexcept
		{
			return map.empty();
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor(this auto&& self) noexcept
		{
			return self.lock.get_executor();
		}

	public:
		map_type  map;

		lock_type lock;
	};

	template<typename SessionT>
	using session_map = basic_session_map<SessionT>;
}

#include <asio3/core/impl/session_map.ipp>
