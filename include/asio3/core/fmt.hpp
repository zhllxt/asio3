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
#include <asio3/core/strutil.hpp>

#include <string_view>

#ifndef ASIO3_DISABLE_AUTO_HEADER_ONLY
#ifndef FMT_HEADER_ONLY
#define FMT_HEADER_ONLY
#endif
#endif

// used to compatible with the UE4 "check" macro
#pragma push_macro("check")
#undef check

#include <fmt/format.h>
#include <fmt/args.h>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/compile.h>
#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/ostream.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <fmt/xchar.h>

// https://fmt.dev/latest/api.html#udt

// Custom format for MFC/ATL CString
#if defined(__AFXSTR_H__) || defined(__ATLSTR_H__)
#if __has_include(<afxstr.h>) || __has_include(<atlstr.h>)
template <>
struct fmt::formatter<CStringA, char>
{
	template<typename FormatParseContext>
	constexpr auto parse(FormatParseContext& ctx) -> decltype(ctx.begin())
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();

		// Check if reached the end of the range:
		if (it != end && *it != '}') throw format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(const CStringA& s, FormatContext& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), "{}", (LPCSTR)s);
	}
};

// CStringA s;
// fmt::format(L"{}", s);
// above code will compile failed, beacuse the CStringA can be implicitly converted to const char*
// then then fmt::format(L"{}", (const char*)...); will compile failed.
template <>
struct fmt::formatter<CStringA, wchar_t>
{
	template<typename FormatParseContext>
	constexpr auto parse(FormatParseContext& ctx) -> decltype(ctx.begin())
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();

		// Check if reached the end of the range:
		if (it != end && *it != '}') throw format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(const CStringA& s, FormatContext& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), L"{}", (LPCWSTR)CStringW(s));
	}
};

template <>
struct fmt::formatter<CStringW, char>
{
	template<typename FormatParseContext>
	constexpr auto parse(FormatParseContext& ctx) -> decltype(ctx.begin())
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();

		// Check if reached the end of the range:
		if (it != end && *it != '}') throw format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(const CStringW& s, FormatContext& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), "{}", (LPCSTR)CStringA(s));
	}
};

template <>
struct fmt::formatter<CStringW, wchar_t>
{
	template<typename FormatParseContext>
	constexpr auto parse(FormatParseContext& ctx) -> decltype(ctx.begin())
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();

		// Check if reached the end of the range:
		if (it != end && *it != '}') throw format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(const CStringW& s, FormatContext& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), L"{}", (LPCWSTR)s);
	}
};
#endif
#endif

// Custom format for wxWidgets wxString
// beacuse the wxString can be implicitly converted to const char* and const wchar_t*
// so the wxString can be use fmt::format("{}", wxString()); directly.
#if defined(_WX_WXSTRING_H__) && defined(ASIO3_ENABLE_WXSTRING_FORMATTER)
#if __has_include(<wx/string.h>)
template <>
struct fmt::formatter<wxString, char>
{
	template<typename FormatParseContext>
	constexpr auto parse(FormatParseContext& ctx) -> decltype(ctx.begin())
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();

		// Check if reached the end of the range:
		if (it != end && *it != '}') throw format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(const wxString& s, FormatContext& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), "{}", (const char*)s);
	}
};

template <>
struct fmt::formatter<wxString, wchar_t>
{
	template<typename FormatParseContext>
	constexpr auto parse(FormatParseContext& ctx) -> decltype(ctx.begin())
	{
		// Parse the presentation format and store it in the formatter:
		auto it = ctx.begin(), end = ctx.end();

		// Check if reached the end of the range:
		if (it != end && *it != '}') throw format_error("invalid format");

		// Return an iterator past the end of the parsed range:
		return it;
	}

	template <typename FormatContext>
	auto format(const wxString& s, FormatContext& ctx) -> decltype(ctx.out())
	{
		return format_to(ctx.out(), L"{}", (const wchar_t*)s);
	}
};
#endif
#endif

// Custom format for asio::buffer
#if __has_include(<asio3/core/asio.hpp>)
#include <asio3/core/asio.hpp>
#ifdef ASIO_STANDALONE
template <>
struct fmt::formatter<asio::const_buffer> : formatter<std::string_view>
{
	auto format(const asio::const_buffer& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::const_pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
template <>
struct fmt::formatter<asio::mutable_buffer> : formatter<std::string_view>
{
	auto format(const asio::mutable_buffer& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
#if !defined(ASIO_NO_DEPRECATED)
template <>
struct fmt::formatter<asio::const_buffers_1> : formatter<std::string_view>
{
	auto format(const asio::const_buffers_1& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::const_pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
template <>
struct fmt::formatter<asio::mutable_buffers_1> : formatter<std::string_view>
{
	auto format(const asio::mutable_buffers_1& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
#endif
#else
template <>
struct fmt::formatter<boost::asio::const_buffer> : formatter<std::string_view>
{
	auto format(const boost::asio::const_buffer& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::const_pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
template <>
struct fmt::formatter<boost::asio::mutable_buffer> : formatter<std::string_view>
{
	auto format(const boost::asio::mutable_buffer& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
#if !defined(BOOST_ASIO_NO_DEPRECATED)
template <>
struct fmt::formatter<boost::asio::const_buffers_1> : formatter<std::string_view>
{
	auto format(const boost::asio::const_buffers_1& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::const_pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
template <>
struct fmt::formatter<boost::asio::mutable_buffers_1> : formatter<std::string_view>
{
	auto format(const boost::asio::mutable_buffers_1& b, format_context& ctx) const
	{
		std::string_view sv{ reinterpret_cast<std::string_view::pointer>(b.data()), b.size() };
		return formatter<std::string_view>::format(sv, ctx);
	}
};
#endif
#endif
#endif

FMT_BEGIN_NAMESPACE
namespace detail {
template<class AllFmt, class Tuple, class FmtStr, class T, class... Args>
auto kvformat_cat(AllFmt& allfmt, Tuple&& tp_vals, FmtStr&& fmtstr, T&& val, Args&&... args)
{
	allfmt += ::std::forward<FmtStr>(fmtstr);

	if constexpr (sizeof...(Args) > ::std::size_t(0))
	{
		return detail::kvformat_cat(allfmt,
			::std::tuple_cat(::std::forward<Tuple>(tp_vals), ::std::tuple(::std::forward<T>(val))),
			::std::forward<Args>(args)...);
	}
	else
	{
		return ::std::tuple_cat(::std::forward<Tuple>(tp_vals), ::std::tuple(::std::forward<T>(val)));
	}
}

template<class CharT, class Tuple, ::std::size_t... I>
auto kvformat_do(::std::basic_string<CharT>& allfmt, Tuple&& tp_vals, ::std::index_sequence<I...>)
{
	if constexpr (::std::is_same_v<CharT, wchar_t>)
	{
		return fmt::vformat(fmt::wstring_view(allfmt),
			fmt::make_wformat_args(::std::get<I>(::std::forward<Tuple>(tp_vals))...));
	}
	else
	{
		return fmt::vformat(allfmt,
			fmt::make_format_args(::std::get<I>(::std::forward<Tuple>(tp_vals))...));
	}
}

template<class CharT, class Tuple>
auto kvformat_do(::std::basic_string<CharT>& allfmt, Tuple&& tp_vals)
{
	return detail::kvformat_do(allfmt, ::std::forward<Tuple>(tp_vals),
		::std::make_index_sequence<::std::tuple_size_v<::std::remove_cv_t<::std::remove_reference_t<Tuple>>>>{});
}
}  // namespace detail

template<class FmtStr, class T, class... Args>
auto kvformat(FmtStr&& fmtstr, T&& val, Args&&... args)
{
#ifdef ASIO_STANDALONE
	auto allfmt = asio::to_basic_string(::std::forward<FmtStr>(fmtstr));
#else
	auto allfmt = boost::asio::to_basic_string(::std::forward<FmtStr>(fmtstr));
#endif

	if constexpr (sizeof...(Args) > ::std::size_t(0))
	{
		return detail::kvformat_do(allfmt, detail::kvformat_cat(allfmt,
			::std::tuple(::std::forward<T>(val)), ::std::forward<Args>(args)...));
	}
	else
	{
		// this will compile failed on c++ 20
		//return fmt::format(::std::forward<FmtStr>(fmtstr), ::std::forward<T>(val));

		return detail::kvformat_do(allfmt, ::std::tuple(::std::forward<T>(val)));
	}
}
FMT_END_NAMESPACE

#pragma pop_macro("check")

#include <asio3/core/detail/pop_options.hpp>
