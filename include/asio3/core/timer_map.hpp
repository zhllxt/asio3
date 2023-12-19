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

#include <functional>
#include <limits>

#include <asio3/core/strutil.hpp>
#include <asio3/core/function_traits.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/with_lock.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	struct timer_handle
	{
		template<typename T>
		requires (!std::same_as<std::remove_cvref_t<T>, timer_handle>)
		timer_handle(T&& id)
		{
			to_handle(std::forward<T>(id));
		}

		timer_handle(timer_handle&& other) = default;
		timer_handle(timer_handle const& other) = default;

		timer_handle& operator=(timer_handle&& other) = default;
		timer_handle& operator=(timer_handle const& other) = default;

		template<typename T>
		requires (!std::same_as<std::remove_cvref_t<T>, timer_handle>)
		void operator=(T&& id)
		{
			to_handle(std::forward<T>(id));
		}

		inline bool operator==(const timer_handle& r) const noexcept
		{
			return (r.handle == this->handle);
		}

		template<typename T>
		inline void to_handle(T&& id)
		{
			using type = std::remove_cv_t<std::remove_reference_t<T>>;
			using rtype = std::remove_pointer_t<std::remove_all_extents_t<type>>;

			if /**/ constexpr (std::same_as<std::string, type>)
			{
				handle = std::forward<T>(id);
			}
			else if constexpr (asio::is_basic_string_view<type>)
			{
				handle.resize(sizeof(typename type::value_type) * id.size());
				std::memcpy((void*)handle.data(), (const void*)id.data(),
					sizeof(typename type::value_type) * id.size());
			}
			else if constexpr (asio::is_basic_string<type>)
			{
				handle.resize(sizeof(typename type::value_type) * id.size());
				std::memcpy((void*)handle.data(), (const void*)id.data(),
					sizeof(typename type::value_type) * id.size());
			}
			else if constexpr (std::integral<type>)
			{
				handle = std::to_string(id);
			}
			else if constexpr (std::floating_point<type>)
			{
				handle.resize(sizeof(type));
				std::memcpy((void*)handle.data(), (const void*)&id, sizeof(type));
			}
			else if constexpr (asio::is_char_pointer<type>)
			{
				assert(id && (*id));
				if (id)
				{
					std::basic_string_view<rtype> sv{ id };
					handle.resize(sizeof(rtype) * sv.size());
					std::memcpy((void*)handle.data(), (const void*)sv.data(), sizeof(rtype) * sv.size());
				}
			}
			else if constexpr (asio::is_char_array<type>)
			{
				std::basic_string_view<rtype> sv{ id };
				handle.resize(sizeof(rtype) * sv.size());
				std::memcpy((void*)handle.data(), (const void*)sv.data(), sizeof(rtype) * sv.size());
			}
			else if constexpr (std::is_pointer_v<type>)
			{
				handle = std::to_string(std::size_t(id));
			}
			else if constexpr (std::is_array_v<type>)
			{
				static_assert(asio::always_false_v<T>);
			}
			else if constexpr (std::is_same_v<timer_handle, type>)
			{
				static_assert(asio::always_false_v<T>);
			}
			else
			{
				static_assert(asio::always_false_v<T>);
			}
		}

		std::string handle;
	};

	namespace detail
	{
		struct timer_handle_hash
		{
			inline std::size_t operator()(const timer_handle& k) const noexcept
			{
				return std::hash<std::string>{}(k.handle);
			}
		};

		struct timer_handle_equal
		{
			inline bool operator()(const timer_handle& lhs, const timer_handle& rhs) const noexcept
			{
				return lhs.handle == rhs.handle;
			}
		};
	}

	template<
		typename TimerHandle,
		typename TimerObject,
		typename TimerHandleHash,
		typename TimerHandleEqual,
		typename TimerMap = std::unordered_map<
			TimerHandle, std::shared_ptr<TimerObject>, TimerHandleHash, TimerHandleEqual>
	>
	class basic_timer_map
	{
	public:
		using timer_handle_type = typename TimerHandle;
		using timer_object_type = typename TimerObject;
		using key_type = timer_handle_type;
		using value_type = std::shared_ptr<timer_object_type>;
		using map_type = typename TimerMap;
		using lock_type = as_tuple_t<use_awaitable_t<>>::as_default_on_t<experimental::channel<void()>>;
		using executor_type = typename lock_type::executor_type;

		struct async_add_op;
		struct async_remove_op;
		struct async_clear_op;
		struct async_find_op;

		explicit basic_timer_map(const auto& ex) : lock(ex, 1)
		{
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param repeat_times - Total number of times the timer is executed.
		 * @param first_delay - The timeout for the first execute of timer. 
		 * @param fun - The callback function signature : [](){}
		 */
		template<typename Fun, typename AddToken = asio::default_token_type<lock_type>>
		requires (asio::is_callable<Fun>)
		inline auto async_add(
			auto&& timer_id,
			asio::timer::duration first_delay,
			asio::timer::duration interval, std::integral auto repeat_times,
			Fun&& fun,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<AddToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_add_op{}, lock),
				token, std::ref(*this),
				timer_handle(std::forward_like<decltype(timer_id)>(timer_id)),
				first_delay, interval, repeat_times,
				std::forward<Fun>(fun));
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - An integer millisecond value for the amount of time between set and firing.
		 * @param fun - The callback function signature : [](){}
		 */
		template<typename Fun, typename AddToken = asio::default_token_type<lock_type>>
		requires (asio::is_callable<Fun>)
		inline auto async_add(
			auto&& timer_id,
			std::integral auto interval,
			Fun&& fun,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return this->async_add(
				std::forward_like<decltype(timer_id)>(timer_id),
				std::chrono::milliseconds(interval),
				std::chrono::milliseconds(interval),
				(std::numeric_limits<std::size_t>::max)(),
				std::forward<Fun>(fun), std::forward<AddToken>(token));
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - An integer millisecond value for the amount of time between set and firing.
		 * @param repeat_times - An integer value to indicate the number of times the timer is repeated.
		 * @param fun - The callback function signature : [](){}
		 */
		template<typename Fun, typename AddToken = asio::default_token_type<lock_type>>
		requires (asio::is_callable<Fun>)
		inline auto async_add(
			auto&& timer_id,
			std::integral auto interval,
			std::integral auto repeat_times,
			Fun&& fun,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return this->async_add(
				std::forward_like<decltype(timer_id)>(timer_id),
				std::chrono::milliseconds(interval),
				std::chrono::milliseconds(interval),
				repeat_times,
				std::forward<Fun>(fun), std::forward<AddToken>(token));
		}

		// ----------------------------------------------------------------------------------------

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param fun - The callback function signature : [](){}
		 */
		template<typename Fun, typename AddToken = asio::default_token_type<lock_type>>
		requires (asio::is_callable<Fun>)
		inline auto async_add(
			auto&& timer_id,
			asio::timer::duration interval,
			Fun&& fun,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return this->async_add(
				std::forward_like<decltype(timer_id)>(timer_id),
				interval,
				interval,
				(std::numeric_limits<std::size_t>::max)(),
				std::forward<Fun>(fun), std::forward<AddToken>(token));
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param repeat_times - An integer value to indicate the number of times the timer is repeated.
		 * @param fun - The callback function signature : [](){}
		 */
		template<typename Fun, typename AddToken = asio::default_token_type<lock_type>>
		requires (asio::is_callable<Fun>)
		inline auto async_add(
			auto&& timer_id,
			asio::timer::duration interval,
			std::integral auto repeat_times,
			Fun&& fun,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return this->async_add(
				std::forward_like<decltype(timer_id)>(timer_id),
				interval,
				interval,
				repeat_times,
				std::forward<Fun>(fun), std::forward<AddToken>(token));
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param first_delay - The timeout for the first execute of timer.
		 * @param fun - The callback function signature : [](){}
		 */
		template<typename Fun, typename AddToken = asio::default_token_type<lock_type>>
		requires (asio::is_callable<Fun>)
		inline auto async_add(
			auto&& timer_id,
			asio::timer::duration first_delay,
			asio::timer::duration interval,
			Fun&& fun,
			AddToken&& token = asio::default_token_type<lock_type>())
		{
			return this->async_add(
				std::forward_like<decltype(timer_id)>(timer_id),
				first_delay,
				interval,
				(std::numeric_limits<std::size_t>::max)(),
				std::forward<Fun>(fun), std::forward<AddToken>(token));
		}

		/**
		 * @brief stop the timer by timer id
		 */
		template<typename RemoveToken = asio::default_token_type<lock_type>>
		inline auto async_remove(
			auto&& timer_id,
			RemoveToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<RemoveToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_remove_op{}, lock),
				token, std::ref(*this),
				timer_handle(std::forward_like<decltype(timer_id)>(timer_id)));
		}

		/**
		 * @brief stop all timers
		 */
		template<typename RemoveToken = asio::default_token_type<lock_type>>
		inline auto async_clear(RemoveToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<RemoveToken, void(bool)>(
				asio::experimental::co_composed<void(bool)>(
					async_clear_op{}, lock),
				token, std::ref(*this));
		}

		/**
		 * @brief find session by the key
		 */
		template<typename FindToken = asio::default_token_type<lock_type>>
		inline auto async_find(
			auto&& timer_id,
			FindToken&& token = asio::default_token_type<lock_type>())
		{
			return asio::async_initiate<FindToken, void(value_type)>(
				asio::experimental::co_composed<void(value_type)>(
					async_find_op{}, lock),
				token, std::ref(*this),
				timer_handle(std::forward_like<decltype(timer_id)>(timer_id)));
		}

		/**
		 * @brief Returns true if the specified timer exists
		 */
		inline bool contains(const auto& timer_id)
		{
			return this->map.contains(timer_id);
		}

		/**
		 * @brief Finds an element with key equivalent to key.
		 */
		inline auto find(const auto& timer_id)
		{
			return this->map.find(timer_id);
		}

		/**
		 * @brief get timer count
		 */
		inline std::size_t size() noexcept
		{
			return map.size();
		}

		/**
		 * @brief get timer count
		 */
		inline std::size_t count() noexcept
		{
			return map.size();
		}

		/**
		 * @brief Checks if the timer container has no elements
		 */
		inline bool empty() noexcept
		{
			return map.empty();
		}

		/**
		 * @brief Get the executor.
		 */
		inline const auto& get_executor(this auto&& self) noexcept
		{
			return self.lock.get_executor();
		}

	public:
		lock_type lock;

		map_type  map;
	};

	using timer_map = basic_timer_map<
		timer_handle, asio::timer,
		detail::timer_handle_hash, detail::timer_handle_equal>;
}

#include <asio3/core/impl/timer_map.ipp>
