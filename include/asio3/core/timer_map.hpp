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

namespace asio
{
	struct timer_handle
	{
		template<typename T>
		requires (!std::same_as<std::remove_cvref_t<T>, timer_handle>)
		timer_handle(T&& id)
		{
			bind(std::forward<T>(id));
		}

		timer_handle(timer_handle&& other) = default;
		timer_handle(timer_handle const& other) = default;

		timer_handle& operator=(timer_handle&& other) = default;
		timer_handle& operator=(timer_handle const& other) = default;

		template<typename T>
		requires (!std::same_as<std::remove_cvref_t<T>, timer_handle>)
		void operator=(T&& id)
		{
			bind(std::forward<T>(id));
		}

		inline bool operator==(const timer_handle& r) const noexcept
		{
			return (r.handle == this->handle);
		}

		template<typename T>
		inline void bind(T&& id)
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
			else if constexpr (std::is_integral_v<type>)
			{
				handle.resize(sizeof(type));
				std::memcpy((void*)handle.data(), (const void*)&id, sizeof(type));
			}
			else if constexpr (std::is_floating_point_v<type>)
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

	class timer_map
	{
	public:
		using map_type = std::unordered_map<timer_handle, std::shared_ptr<asio::timer>,
			detail::timer_handle_hash, detail::timer_handle_equal>;

		explicit timer_map(const auto& ex) : executor(ex)
		{
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - An integer millisecond value for the amount of time between set and firing.
		 * @param fun - The callback function signature : [](){}
		 */
		template<class TimerId, class IntegerMilliseconds, class Fun, class... Args>
		requires (asio::is_callable<Fun> && std::integral<std::remove_cvref_t<IntegerMilliseconds>>)
		void start_timer(TimerId&& timer_id, IntegerMilliseconds interval, Fun&& fun, Args&&... args)
		{
			this->start_timer(
				std::forward<TimerId>(timer_id),
				std::chrono::milliseconds(interval),
				(std::numeric_limits<std::size_t>::max)(),
				std::chrono::milliseconds(interval),
				std::forward<Fun>(fun), std::forward<Args>(args)...);
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - An integer millisecond value for the amount of time between set and firing.
		 * @param repeat - An integer value to indicate the number of times the timer is repeated.
		 * @param fun - The callback function signature : [](){}
		 */
		template<class TimerId, class IntegerMilliseconds, class Integer, class Fun, class... Args>
		requires (asio::is_callable<Fun>
			&& std::integral<std::remove_cvref_t<IntegerMilliseconds>>
			&& std::integral<std::remove_cvref_t<Integer>>)
		void start_timer(TimerId&& timer_id, IntegerMilliseconds interval, Integer repeat, Fun&& fun, Args&&... args)
		{
			this->start_timer(
				std::forward<TimerId>(timer_id),
				std::chrono::milliseconds(interval), repeat,
				std::chrono::milliseconds(interval),
				std::forward<Fun>(fun), std::forward<Args>(args)...);
		}

		// ----------------------------------------------------------------------------------------

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param fun - The callback function signature : [](){}
		 */
		template<class TimerId, class Rep, class Period, class Fun, class... Args>
		requires (asio::is_callable<Fun>)
		void start_timer(TimerId&& timer_id, std::chrono::duration<Rep, Period> interval, Fun&& fun, Args&&... args)
		{
			this->start_timer(
				std::forward<TimerId>(timer_id),
				interval,
				(std::numeric_limits<std::size_t>::max)(),
				interval,
				std::forward<Fun>(fun), std::forward<Args>(args)...);
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param repeat - An integer value to indicate the number of times the timer is repeated.
		 * @param fun - The callback function signature : [](){}
		 */
		template<class TimerId, class Rep, class Period, class Integer, class Fun, class... Args>
		requires (asio::is_callable<Fun> && std::integral<std::remove_cvref_t<Integer>>)
		void start_timer(TimerId&& timer_id, std::chrono::duration<Rep, Period> interval, Integer repeat,
			Fun&& fun, Args&&... args)
		{
			this->start_timer(
				std::forward<TimerId>(timer_id),
				interval,
				repeat,
				interval,
				std::forward<Fun>(fun), std::forward<Args>(args)...);
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param first_delay - The timeout for the first execute of timer.
		 * @param fun - The callback function signature : [](){}
		 */
		template<class TimerId, class Rep1, class Period1, class Rep2, class Period2, class Fun, class... Args>
		requires (asio::is_callable<Fun>)
		void start_timer(TimerId&& timer_id,
			std::chrono::duration<Rep1, Period1> interval,
			std::chrono::duration<Rep2, Period2> first_delay,
			Fun&& fun, Args&&... args)
		{
			this->start_timer(
				std::forward<TimerId>(timer_id),
				interval,
				(std::numeric_limits<std::size_t>::max)(),
				first_delay,
				std::forward<Fun>(fun), std::forward<Args>(args)...);
		}

		/**
		 * @brief start a timer
		 * @param timer_id - The timer id can be integer or string, example : 1,2,3 or "id1" "id2",
		 *  If a timer with this id already exists, that timer will be forced to canceled.
		 * @param interval - The amount of time between set and firing.
		 * @param repeat - Total number of times the timer is executed.
		 * @param first_delay - The timeout for the first execute of timer. 
		 * @param fun - The callback function signature : [](){}
		 */
		template<class TimerId, class Rep1, class Period1, class Rep2, class Period2, class Integer, class Fun, class... Args>
		requires (asio::is_callable<Fun> && std::integral<std::remove_cvref_t<Integer>>)
		void start_timer(TimerId&& timer_id,
			std::chrono::duration<Rep1, Period1> interval, Integer repeat,
			std::chrono::duration<Rep2, Period2> first_delay, Fun&& fun, Args&&... args)
		{
			if (repeat == static_cast<std::size_t>(0))
			{
				assert(false);
				return;
			}

			if (interval > std::chrono::duration_cast<
				std::chrono::duration<Rep1, Period1>>((asio::steady_timer::duration::max)()))
				interval = std::chrono::duration_cast<std::chrono::duration<Rep1, Period1>>(
					(asio::steady_timer::duration::max)());

			if (first_delay > std::chrono::duration_cast<
				std::chrono::duration<Rep2, Period2>>((asio::steady_timer::duration::max)()))
				first_delay = std::chrono::duration_cast<std::chrono::duration<Rep2, Period2>>(
					(asio::steady_timer::duration::max)());

			timer_handle handle{ std::forward<TimerId>(timer_id) };

			// if the timer is already exists, cancel it first.
			auto iter = this->map.find(handle);
			if (iter != this->map.end())
			{
				asio::cancel_timer(*(iter->second));
			}

			// the asio::steady_timer's constructor may be throw some exception.
			std::shared_ptr<asio::timer> timer_ptr = asio::create_timer(
				get_executor(), first_delay, interval, repeat,
				std::bind_front(std::forward<Fun>(fun), std::forward<Args>(args)...),
				[this, handle]() mutable
				{
					this->map.erase(handle);
				});

			this->map[std::move(handle)] = std::move(timer_ptr);
		}

		/**
		 * @brief stop the timer by timer id
		 */
		template<class TimerId>
		inline void stop_timer(TimerId&& timer_id)
		{
			auto iter = this->map.find(timer_id);
			if (iter != this->map.end())
			{
				asio::cancel_timer(*(iter->second));

				this->map.erase(iter);
			}
		}

		/**
		 * @brief stop all timers
		 */
		inline void stop_all_timers()
		{
			// close user custom timers
			for (auto& [id, timer_ptr] : this->map)
			{
				asio::ignore_unused(id);

				asio::cancel_timer(*timer_ptr);
			}

			this->map.clear();
		}

		/**
		 * @brief Returns true if the specified timer exists
		 */
		template<class TimerId>
		inline bool contains(TimerId&& timer_id)
		{
			return this->map.contains(timer_id);
		}

		/**
		 * @brief Finds an element with key equivalent to key.
		 */
		template<class TimerId>
		inline auto find(TimerId&& timer_id)
		{
			return this->map.find(timer_id);
		}

		/**
		 * @brief Get the executor.
		 */
		constexpr inline auto&& get_executor(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).executor;
		}

	public:
		asio::any_io_executor executor;

		map_type map;
	};
}
