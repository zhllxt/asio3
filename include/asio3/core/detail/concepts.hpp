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

#include <cstdint>
#include <cctype>

#include <string>
#include <string_view>
#include <type_traits>
#include <memory>
#include <functional>
#include <tuple>
#include <utility>
#include <concepts>

namespace asio::detail
{
	template <typename Enumeration>
	inline constexpr auto to_underlying(Enumeration const value) noexcept ->
		typename std::underlying_type<Enumeration>::type
	{
		return static_cast<typename std::underlying_type<Enumeration>::type>(value);
	}

	template <typename... Ts>
	inline constexpr void ignore_unused(Ts const& ...) noexcept {}

	template <typename... Ts>
	inline constexpr void ignore_unused() noexcept {}

	// https://stackoverflow.com/questions/53945490/how-to-assert-that-a-constexpr-if-else-clause-never-happen
	// https://en.cppreference.com/w/cpp/utility/variant/visit
	// https://en.cppreference.com/w/cpp/language/if#Constexpr_If
	template<typename...> inline constexpr bool always_false_v = false;

	template <typename Tup, typename Fun, std::size_t... I>
	inline void for_each_tuple_impl(Tup&& t, Fun&& f, std::index_sequence<I...>)
	{
		(f(std::get<I>(std::forward<Tup>(t))), ...);
	}

	template <typename Tup, typename Fun>
	inline void for_each_tuple(Tup&& t, Fun&& f)
	{
		for_each_tuple_impl(std::forward<Tup>(t), std::forward<Fun>(f), std::make_index_sequence<
			std::tuple_size_v<std::remove_cvref_t<Tup>>>{});
	}

	// example : static_assert(is_template_instantiable<std::vector, double>);
	//           static_assert(is_template_instantiable<std::optional, int, int>);
	template<template<typename...> typename T, typename AlwaysVoid, typename... Args>
	struct is_template_instantiable_helper : std::false_type {};

	template<template<typename...> typename T, typename... Args>
	struct is_template_instantiable_helper<T, std::void_t<T<Args...>>, Args...> : std::true_type {};

	template<template<typename...> typename T, typename... Args>
	concept is_template_instantiable = is_template_instantiable_helper<T, void, Args...>::value;

	// static_assert(is_template_instance_of<std::vector, std::vector<int>>);
	template<template<typename...> typename U, typename T>
	struct is_template_instance_of_helper : std::false_type {};

	template<template<typename...> typename U, typename...Args>
	struct is_template_instance_of_helper<U, U<Args...>> : std::true_type {};

	template<template<typename...> typename U, typename T>
	concept is_template_instance_of = is_template_instance_of_helper<U, T>::value;

	// std::tuple
	template<typename T>
	concept is_tuple = is_template_instance_of<std::tuple, T>;

	// char,wchar_t...
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_char =
		std::is_same_v<U, char    > ||
		std::is_same_v<U, wchar_t > ||
		std::is_same_v<U, char8_t > ||
		std::is_same_v<U, char16_t> ||
		std::is_same_v<U, char32_t>;

	// std::basic_string<...>
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_string = std::is_same_v<U, std::basic_string<
		typename U::value_type, typename U::traits_type, typename U::allocator_type>>;

	// std::basic_string_view<...>
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_string_view = std::is_same_v<U, std::basic_string_view<
		typename U::value_type, typename U::traits_type>>;

	// char*, const char*, not char**
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_char_pointer = 
		 std::is_pointer_v<U> &&
		!std::is_pointer_v<std::remove_cvref_t<std::remove_pointer_t<U>>> &&
		 detail::is_char<std::remove_pointer_t<U>>;

	// char[]
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_char_array =
		std::is_array_v<U> &&
		detail::is_char<std::remove_all_extents_t<U>>;

	// 
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_character_string =
		detail::is_string      <U> ||
		detail::is_string_view <U> ||
		detail::is_char_pointer<U> ||
		detail::is_char_array  <U>;
	
	// char_type<char> == char, char_type<char*> == char, char_type<char[]> == char
	template<typename T, bool F = detail::is_char<T> || detail::is_char_pointer<T> || detail::is_char_array<T>>
	struct char_type;

	template<typename T>
	struct char_type<T, true>
	{
		using type = std::remove_cvref_t<std::remove_pointer_t<std::remove_all_extents_t<std::remove_cvref_t<T>>>>;
	};

	template<typename T>
	struct char_type<T, false>
	{
		using type = typename T::value_type;
	};

	// 
	template<typename T, typename U>
	concept has_stream_operator = requires(T a, U b) { a << b; };

	template<typename T, typename U>
	concept has_equal_operator = requires(T a, U b) { a = b; };

	template<typename T>
	concept has_bool_operator = requires(T a) { static_cast<bool>(a); };

	// containers like std::string std::vector
	template<typename T>
	concept has_member_insert = requires(T a, std::string_view b)
	{
		a.insert(a.cbegin(), b.begin(), b.end());
	};

	// 
	template<typename T>
	concept can_convert_to_string = requires(std::string a, T b)
	{
		std::string(b);
		a = b;
	};

	template<typename T>
	struct shared_ptr_adapter
	{
		using rawt = typename std::remove_cvref_t<T>;
		using type = std::conditional_t<detail::is_template_instance_of<std::shared_ptr, rawt>,
			rawt, std::shared_ptr<rawt>>;
	};

	template<typename T>
	typename detail::shared_ptr_adapter<T>::type to_shared_ptr(T&& t)
	{
		using rawt = typename std::remove_cvref_t<T>;

		if constexpr (detail::is_template_instance_of<std::shared_ptr, rawt>)
		{
			return std::forward<T>(t);
		}
		else
		{
			return std::make_shared<rawt>(std::forward<T>(t));
		}
	}

	// element_type_adapter<char> == char
	// element_type_adapter<char*> == char
	// element_type_adapter<std::shared_ptr<char>> == char
	// element_type_adapter<std::unique_ptr<char>> == char
	template<typename T>
	struct element_type_adapter
	{
		using type = typename std::remove_cvref_t<T>;
	};

	template<typename T>
	struct element_type_adapter<std::shared_ptr<T>>
	{
		using type = typename std::remove_cvref_t<T>;
	};

	template<typename T>
	struct element_type_adapter<std::unique_ptr<T>>
	{
		using type = typename std::remove_cvref_t<T>;
	};

	template<typename T>
	struct element_type_adapter<T*>
	{
		using type = typename std::remove_cvref_t<T>;
	};

	// std::string -> std::string&
	// std::string* -> std::string&
	// std::shared_ptr<std::string> -> std::string&
	// std::unique_ptr<std::string> -> std::string&
	template<typename T>
	inline typename element_type_adapter<T>::type& to_element_ref(T& value) noexcept
	{
		using rawt = typename std::remove_cvref_t<T>;

		if /**/ constexpr (detail::is_template_instance_of<std::shared_ptr, rawt>)
			return *value;
		else if constexpr (detail::is_template_instance_of<std::unique_ptr, rawt>)
			return *value;
		else if constexpr (std::is_pointer_v<rawt>)
			return *value;
		else
			return value;
	}
}

namespace asio
{
	using detail::to_underlying;
	using detail::ignore_unused;
	using detail::to_shared_ptr;

	// helper type for the visitor #4
	template<class... Ts>
	struct variant_overloaded : Ts... { using Ts::operator()...; };

	// explicit deduction guide (not needed as of C++20)
	template<class... Ts>
	variant_overloaded(Ts...) -> variant_overloaded<Ts...>;
}
