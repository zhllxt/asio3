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

#include <cassert>

#include <memory>
#include <functional>
#include <filesystem>

#include <asio3/core/stdconcepts.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	namespace detail
	{
		template <typename Tup, typename Fun, std::size_t... I>
		inline void for_each_tuple_impl(Tup&& t, Fun&& f, std::index_sequence<I...>)
		{
			(f(std::get<I>(std::forward<Tup>(t))), ...);
		}
	}

	template <typename Tup, typename Fun>
	inline void for_each_tuple(Tup&& t, Fun&& f)
	{
		detail::for_each_tuple_impl(std::forward<Tup>(t), std::forward<Fun>(f),
			std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<Tup>>>{});
	}
	
	// char_type<char> == char, char_type<char*> == char, char_type<char[]> == char
	template<typename T, bool F = is_char<T> || is_char_pointer<T> || is_char_array<T>>
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

	template<typename T>
	struct shared_ptr_adapter
	{
		using rawt = typename std::remove_cvref_t<T>;
		using type = std::conditional_t<asio::is_template_instance_of<std::shared_ptr, rawt>,
			rawt, std::shared_ptr<rawt>>;
	};

	template<typename T>
	typename shared_ptr_adapter<T>::type to_shared_ptr(T&& t)
	{
		using rawt = typename std::remove_cvref_t<T>;

		if constexpr (asio::is_template_instance_of<std::shared_ptr, rawt>)
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

		if /**/ constexpr (asio::is_template_instance_of<std::shared_ptr, rawt>)
			return *value;
		else if constexpr (asio::is_template_instance_of<std::unique_ptr, rawt>)
			return *value;
		else if constexpr (std::is_pointer_v<rawt>)
			return *value;
		else
			return value;
	}


	// helper type for the visitor #4
	template<class... Ts>
	struct variant_overloaded : Ts... { using Ts::operator()...; };
	// explicit deduction guide (not needed as of C++20)
	template<class... Ts>
	variant_overloaded(Ts...) -> variant_overloaded<Ts...>;

	template<typename = void>
	[[nodiscard]] bool is_subpath_of(const std::filesystem::path& base, const std::filesystem::path& p) noexcept
	{
		assert(std::filesystem::is_directory(base));

		auto it_base = base.begin();
		auto it_p = p.begin();

		while (it_base != base.end() && it_p != p.end())
		{
			if (*it_base != *it_p)
			{
				return false;
			}

			++it_base;
			++it_p;
		}

		// If all components of base are matched, and p has more components, then base is a subpath of p
		return it_base == base.end() && it_p != p.end();
	}

	template<typename = void>
	[[nodiscard]] std::filesystem::path make_filepath(const std::filesystem::path& base, const std::filesystem::path& p) noexcept
	{
		std::error_code ec{};
		std::filesystem::path b = std::filesystem::canonical(base, ec);
		if (ec)
		{
			return {};
		}

		assert(std::filesystem::is_directory(b));

		std::filesystem::path filepath = b;
		filepath += p;

		filepath = std::filesystem::canonical(filepath, ec);

		if (ec || !is_subpath_of(b, filepath))
		{
			return {};
		}

		return filepath;
	}

	template<typename = void>
	[[nodiscard]] std::filesystem::path make_filepath(const std::filesystem::path& base, ::std::string_view s) noexcept
	{
		std::filesystem::path p{ s };
		return make_filepath(base, p);
	}
}
