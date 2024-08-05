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
#include <asio3/core/netutil.hpp>
#include <asio3/core/strutil.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename AsyncResolver>
	inline asio::awaitable<asio::error_code> resolve(
		AsyncResolver& resolver, is_string auto&& host, is_string_or_integral auto&& service,
		auto& results, asio::ip::resolver_base::flags resolve_flags)
	{
		std::string addr = asio::to_string(std::forward_like<decltype(host)>(host));
		std::string port = asio::to_string(std::forward_like<decltype(service)>(service));
		std::string_view addr_sv = addr;
		std::string_view port_sv = port;

		co_await asio::dispatch(asio::use_awaitable_executor(resolver));

		auto [e1, eps] = co_await resolver.async_resolve(
			addr_sv, port_sv, resolve_flags, asio::use_awaitable_executor(resolver));

		results = std::move(eps);

		co_return e1;
	}
}

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct async_resolve_op
	{
		//auto operator()(
		//	auto state, auto resolver_ref,
		//	auto&& host, auto&& service, asio::ip::resolver_base::flags resolve_flags) -> void
		//{
		//	auto& resolver = resolver_ref.get();

		//	std::string addr = asio::to_string(std::forward_like<decltype(host)>(host));
		//	std::string port = asio::to_string(std::forward_like<decltype(service)>(service));

		//	using resolver_type = std::remove_cvref_t<decltype(resolver)>;

		//	co_await asio::dispatch(asio::use_deferred_executor(resolver));

		//	state.reset_cancellation_state(asio::enable_terminal_cancellation());

		//	std::string_view addr_sv = addr;
		//	std::string_view port_sv = port;

		//	//asio::experimental::channel<void(asio::error_code)> ch(resolver.get_executor(), 1);

		//	//typename resolver_type::results_type eps{};

		//	// on vs2022 and release mode, the "addr_sv, port_sv" will be optimized and 
		//	// cause crash.
		//	// 
		//	//auto [e1, eps] = co_await resolver.async_resolve(
		//	//	addr_sv, port_sv, asio::ip::resolver_base::passive, asio::use_deferred_executor(resolver));

		//	//resolver.async_resolve(
		//	//	addr_sv, port_sv, resolve_flags,
		//	//	[&eps, &ch](const asio::error_code& ec, typename resolver_type::results_type results) mutable
		//	//	{
		//	//		eps = std::move(results);
		//	//		ch.try_send(ec);
		//	//	});

		//	asio::error_code e1{};
		//	auto eps = resolver.resolve(addr_sv, port_sv, resolve_flags, e1);

		//	//auto [e1] = co_await ch.async_receive(asio::use_deferred_executor(ch));

		//	co_return{ e1, std::move(eps) };
		//}

		auto operator()(
			auto state, auto resolver_ref,
			auto&& host, auto&& service, asio::ip::resolver_base::flags resolve_flags) -> void
		{
			auto& resolver = resolver_ref.get();

			using resolver_type = std::remove_cvref_t<decltype(resolver)>;

			typename resolver_type::results_type eps{};

			auto [e1] = co_await asio::async_call_coroutine(
				resolver.get_executor(), asio::resolve(
					resolver,
					std::forward_like<decltype(host)>(host),
					std::forward_like<decltype(service)>(service),
					eps,
					resolve_flags
				), asio::use_deferred_executor(resolver));

			co_return{ e1, std::move(eps) };
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/// Asynchronously perform forward resolution of a query to a list of entries.
	/**
	 * This function is used to resolve host and service names into a list of
	 * endpoint entries. It is an initiating function for an @ref
	 * asynchronous_operation, and always returns immediately.
	 *
	 * @param host A string identifying a location. May be a descriptive name or
	 * a numeric address string. If an empty string and the passive flag has been
	 * specified, the resolved endpoints are suitable for local service binding.
	 * If an empty string and passive is not specified, the resolved endpoints
	 * will use the loopback address.
	 *
	 * @param service A string identifying the requested service. This may be a
	 * descriptive name or a numeric string corresponding to a port number. May
	 * be an empty string, in which case all resolved endpoints will have a port
	 * number of 0.
	 *
	 * @param resolve_flags A set of flags that determine how name resolution
	 * should be performed. The default flags are suitable for communication with
	 * remote hosts. See the @ref resolver_base documentation for the set of
	 * available flags.
	 *
	 * @param token The @ref completion_token that will be used to produce a
	 * completion handler, which will be called when the resolve completes.
	 * Potential completion tokens include @ref use_future, @ref use_awaitable,
	 * @ref yield_context, or a function object with the correct completion
	 * signature. The function signature of the completion handler must be:
	 * @code void handler(
	 *   const asio::error_code& error, // Result of operation.
	 *   resolver::results_type results // Resolved endpoints as a range.
	 * ); @endcode
	 * Regardless of whether the asynchronous operation completes immediately or
	 * not, the completion handler will not be invoked from within this function.
	 * On immediate completion, invocation of the handler will be performed in a
	 * manner equivalent to using asio::post().
	 *
	 * A successful resolve operation is guaranteed to pass a non-empty range to
	 * the handler.
	 *
	 * @par Completion Signature
	 * @code void(asio::error_code, results_type) @endcode
	 *
	 * @note On POSIX systems, host names may be locally defined in the file
	 * <tt>/etc/hosts</tt>. On Windows, host names may be defined in the file
	 * <tt>c:\\windows\\system32\\drivers\\etc\\hosts</tt>. Remote host name
	 * resolution is performed using DNS. Operating systems may use additional
	 * locations when resolving host names (such as NETBIOS names on Windows).
	 *
	 * On POSIX systems, service names are typically defined in the file
	 * <tt>/etc/services</tt>. On Windows, service names may be found in the file
	 * <tt>c:\\windows\\system32\\drivers\\etc\\services</tt>. Operating systems
	 * may use additional locations when resolving service names.
	 */
	template<
		typename AsyncResolver,
		typename ResolveToken = asio::default_token_type<AsyncResolver>>
	inline auto async_resolve(
		AsyncResolver& resolver,
		is_string auto&& host,
		is_string_or_integral auto&& service,
		asio::ip::resolver_base::flags resolve_flags,
		ResolveToken&& token = asio::default_token_type<AsyncResolver>())
	{
		return async_initiate<ResolveToken, void(asio::error_code, typename AsyncResolver::results_type)>(
			experimental::co_composed<void(asio::error_code, typename AsyncResolver::results_type)>(
				detail::async_resolve_op{}, resolver),
			token,
			std::ref(resolver),
			std::forward_like<decltype(host)>(host),
			std::forward_like<decltype(service)>(service),
			resolve_flags);
	}
}
