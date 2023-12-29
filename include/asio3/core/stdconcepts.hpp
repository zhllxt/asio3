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

#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <type_traits>
#include <concepts>

#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	namespace detail
	{
		template<template<typename...> typename T, typename AlwaysVoid, typename... Args>
		struct is_template_instantiable_helper : std::false_type {};

		template<template<typename...> typename T, typename... Args>
		struct is_template_instantiable_helper<T, std::void_t<T<Args...>>, Args...> : std::true_type {};

		template<template<typename...> typename U, typename T>
		struct is_template_instance_of_helper : std::false_type {};

		template<template<typename...> typename U, typename...Args>
		struct is_template_instance_of_helper<U, U<Args...>> : std::true_type {};
	}

	// https://stackoverflow.com/questions/53945490/how-to-assert-that-a-constexpr-if-else-clause-never-happen
	// https://en.cppreference.com/w/cpp/utility/variant/visit
	// https://en.cppreference.com/w/cpp/language/if#Constexpr_If
	template<typename...> concept always_false_v = false;

	// example : static_assert(is_template_instantiable<std::vector, double>);
	//           static_assert(is_template_instantiable<std::optional, int, int>);
	template<template<typename...> typename T, typename... Args>
	concept is_template_instantiable = detail::is_template_instantiable_helper<T, void, Args...>::value;

	// static_assert(is_template_instance_of<std::vector, std::vector<int>>);
	template<template<typename...> typename U, typename T>
	concept is_template_instance_of = detail::is_template_instance_of_helper<U, T>::value;

	// std::tuple
	template<typename T>
	concept is_tuple = is_template_instance_of<std::tuple, T>;
	template<typename T>
	concept is_variant = is_template_instance_of<std::variant, T>;

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
	concept is_basic_string = std::is_same_v<U, std::basic_string<
		typename U::value_type, typename U::traits_type, typename U::allocator_type>>;

	// std::basic_string_view<...>
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_basic_string_view = std::is_same_v<U, std::basic_string_view<
		typename U::value_type, typename U::traits_type>>;

	// char*, const char*, not char**
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_char_pointer = 
		 std::is_pointer_v<U> &&
		!std::is_pointer_v<std::remove_cvref_t<std::remove_pointer_t<U>>> &&
		 is_char<std::remove_pointer_t<U>>;

	// char[]
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_char_array =
		std::is_array_v<U> &&
		is_char<std::remove_all_extents_t<U>>;

	// 
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_string = std::constructible_from<std::string, U>;

	// 
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_string_or_integral = std::constructible_from<std::string, U> || std::integral<std::remove_cvref_t<U>>;
	
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

	// https://github.com/MiSo1289/more_concepts
	template <typename Ret, typename Fn, typename... Args>
    struct is_callable_r : std::false_type
    {
    };

    template <typename Ret, typename Fn, typename... Args>
    requires
    requires(Fn& fn, Args&& ... args)
    {{ static_cast<Fn&>(fn)(std::forward<Args>(args)...) } -> std::same_as<Ret>; }
    struct is_callable_r<Ret, Fn, Args...> : std::true_type
    {
    };

    template <typename Ret, typename Fn, typename... Args>
    concept is_callable_r_v = is_callable_r<Ret, Fn, Args...>::value;

	/////////////////////////////////////////////////////////////////////////////
	// cinatra-master/include/cinatra/utils.hpp
	template <typename T, typename Tuple>
	struct has_type;

	template <typename T, typename... Us>
	struct has_type<T, std::tuple<Us...>> : std::disjunction<std::is_same<T, Us>...> {};
}
