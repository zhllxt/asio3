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

#include <concepts>

#include <asio3/config.hpp>

#ifdef ASIO3_HEADER_ONLY
#  ifndef ASIO_STANDALONE
#  define ASIO_STANDALONE
#  endif
#endif

#ifdef ASIO_STANDALONE
#  ifndef ASIO_HEADER_ONLY
#  define ASIO_HEADER_ONLY
#  endif
#endif

#include <asio3/core/detail/push_options.hpp>

#ifdef ASIO_STANDALONE
	#include <asio.hpp>
	#include <asio/experimental/co_composed.hpp>
	#include <asio/experimental/awaitable_operators.hpp>
	#include <asio/experimental/channel.hpp>
	#if defined(ASIO3_ENABLE_SSL) || defined(ASIO3_USE_SSL)
		#include <asio/ssl.hpp>
	#endif
#else
	#include <boost/asio.hpp>
	#include <boost/asio/experimental/co_composed.hpp>
	#include <boost/asio/experimental/awaitable_operators.hpp>
	#include <boost/asio/experimental/channel.hpp>
	#if defined(ASIO3_ENABLE_SSL) || defined(ASIO3_USE_SSL)
		#include <boost/asio/ssl.hpp>
	#endif
	#if !defined(ASIO_VERSION) && defined(BOOST_ASIO_VERSION)
	#define ASIO_VERSION BOOST_ASIO_VERSION
	#endif
	#if !defined(ASIO_NODISCARD) && defined(BOOST_ASIO_NODISCARD)
	#define ASIO_NODISCARD BOOST_ASIO_NODISCARD
	#endif
	#if !defined(ASIO_CONST_BUFFER) && defined(BOOST_ASIO_CONST_BUFFER)
	#define ASIO_CONST_BUFFER BOOST_ASIO_CONST_BUFFER
	#endif
	#if !defined(ASIO_MUTABLE_BUFFER) && defined(BOOST_ASIO_MUTABLE_BUFFER)
	#define ASIO_MUTABLE_BUFFER BOOST_ASIO_MUTABLE_BUFFER
	#endif
	#if !defined(ASIO_NOEXCEPT) && defined(BOOST_ASIO_NOEXCEPT)
	#define ASIO_NOEXCEPT BOOST_ASIO_NOEXCEPT
	#endif
	#if !defined(ASIO_NO_EXCEPTIONS) && defined(BOOST_ASIO_NO_EXCEPTIONS)
	#define ASIO_NO_EXCEPTIONS BOOST_ASIO_NO_EXCEPTIONS
	#endif
	#if !defined(ASIO_NO_DEPRECATED) && defined(BOOST_ASIO_NO_DEPRECATED)
	#define ASIO_NO_DEPRECATED BOOST_ASIO_NO_DEPRECATED
	#endif
	#if !defined(ASIO_CORO_REENTER) && defined(BOOST_ASIO_CORO_REENTER)
	#define ASIO_CORO_REENTER BOOST_ASIO_CORO_REENTER
	#endif
	#if !defined(ASIO_CORO_YIELD) && defined(BOOST_ASIO_CORO_YIELD)
	#define ASIO_CORO_YIELD BOOST_ASIO_CORO_YIELD
	#endif
	#if !defined(ASIO_CORO_FORK) && defined(BOOST_ASIO_CORO_FORK)
	#define ASIO_CORO_FORK BOOST_ASIO_CORO_FORK
	#endif
	#if !defined(ASIO_INITFN_AUTO_RESULT_TYPE) && defined(BOOST_ASIO_INITFN_AUTO_RESULT_TYPE)
	#define ASIO_INITFN_AUTO_RESULT_TYPE BOOST_ASIO_INITFN_AUTO_RESULT_TYPE
	#endif
	#if !defined(ASIO_COMPLETION_TOKEN_FOR) && defined(BOOST_ASIO_COMPLETION_TOKEN_FOR)
	#define ASIO_COMPLETION_TOKEN_FOR BOOST_ASIO_COMPLETION_TOKEN_FOR
	#endif
	#if !defined(ASIO_DEFAULT_COMPLETION_TOKEN) && defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN)
	#define ASIO_DEFAULT_COMPLETION_TOKEN BOOST_ASIO_DEFAULT_COMPLETION_TOKEN
	#endif
	#if !defined(ASIO_DEFAULT_COMPLETION_TOKEN_TYPE) && defined(BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE)
	#define ASIO_DEFAULT_COMPLETION_TOKEN_TYPE BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE
	#endif
	#if !defined(ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX) && defined(BOOST_ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX)
	#define ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX BOOST_ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX
	#endif
	#if !defined(ASIO_INITFN_AUTO_RESULT_TYPE_SUFFIX) && defined(BOOST_ASIO_INITFN_AUTO_RESULT_TYPE_SUFFIX)
	#define ASIO_INITFN_AUTO_RESULT_TYPE_SUFFIX BOOST_ASIO_INITFN_AUTO_RESULT_TYPE_SUFFIX
	#endif
	#if !defined(ASIO_NO_TS_EXECUTORS) && defined(BOOST_ASIO_NO_TS_EXECUTORS)
	#define ASIO_NO_TS_EXECUTORS BOOST_ASIO_NO_TS_EXECUTORS
	#endif
	#if !defined(ASIO_HANDLER_LOCATION) && defined(BOOST_ASIO_HANDLER_LOCATION)
	#define ASIO_HANDLER_LOCATION BOOST_ASIO_HANDLER_LOCATION
	#endif
	#if !defined(ASIO_ENABLE_BUFFER_DEBUGGING) && defined(BOOST_ASIO_ENABLE_BUFFER_DEBUGGING)
	#define ASIO_ENABLE_BUFFER_DEBUGGING BOOST_ASIO_ENABLE_BUFFER_DEBUGGING
	#endif
	#if !defined(ASIO_MOVE_ARG) && defined(BOOST_ASIO_MOVE_ARG)
	#define ASIO_MOVE_ARG BOOST_ASIO_MOVE_ARG
	#endif
	#if !defined(ASIO_MOVE_CAST) && defined(BOOST_ASIO_MOVE_CAST)
	#define ASIO_MOVE_CAST BOOST_ASIO_MOVE_CAST
	#endif
	#if !defined(ASIO_HANDLER_TYPE) && defined(BOOST_ASIO_HANDLER_TYPE)
	#define ASIO_HANDLER_TYPE BOOST_ASIO_HANDLER_TYPE
	#endif
	#if !defined(ASIO_HAS_STD_HASH) && defined(BOOST_ASIO_HAS_STD_HASH)
	#define ASIO_HAS_STD_HASH BOOST_ASIO_HAS_STD_HASH
	#endif
	#if !defined(ASIO_SYNC_OP_VOID) && defined(BOOST_ASIO_SYNC_OP_VOID)
	#define ASIO_SYNC_OP_VOID BOOST_ASIO_SYNC_OP_VOID
	#endif
	#if !defined(ASIO_SYNC_OP_VOID_RETURN) && defined(BOOST_ASIO_SYNC_OP_VOID_RETURN)
	#define ASIO_SYNC_OP_VOID_RETURN BOOST_ASIO_SYNC_OP_VOID_RETURN
	#endif
#endif // ASIO_STANDALONE

#ifdef ASIO_STANDALONE
	namespace asio
	{
		using error_condition = std::error_condition;
		using executor_guard  = asio::executor_work_guard<asio::io_context::executor_type>;
	}
#else
	namespace boost::asio
	{
		using error_code      = ::boost::system::error_code;
		using system_error    = ::boost::system::system_error;
		using error_condition = ::boost::system::error_condition;
		using error_category  = ::boost::system::error_category;
		using executor_guard  = asio::executor_work_guard<asio::io_context::executor_type>;
	}
	namespace asio = ::boost::asio;
	namespace bho  = ::boost; // bho means boost header only

	// [ adding definitions to namespace alias ]
	// This is currently not allowed and probably won't be in C++1Z either,
	// but note that a recent proposal is allowing
	// https://stackoverflow.com/questions/31629101/adding-definitions-to-namespace-alias?r=SearchResults
	//namespace asio
	//{
	//	using error_code   = ::boost::system::error_code;
	//	using system_error = ::boost::system::system_error;
	//}
#endif // ASIO_STANDALONE

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename T, typename U = ::std::remove_cvref_t<T>>
	using default_token_type = typename asio::default_completion_token<typename U::executor_type>::type;

	constexpr auto use_nothrow_deferred  = asio::as_tuple(asio::deferred);
	constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);
}

#ifdef ASIO_STANDALONE
using namespace asio::experimental::awaitable_operators;
#else
using namespace boost::asio::experimental::awaitable_operators;
#endif

#include <asio3/core/detail/pop_options.hpp>
