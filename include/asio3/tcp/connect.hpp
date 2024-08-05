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
#include <asio3/core/timer.hpp>
#include <asio3/core/resolve.hpp>
#include <asio3/core/with_lock.hpp>
#include <asio3/tcp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
	 */
	template<typename AsyncStream>
	requires is_tcp_socket<AsyncStream>
	inline asio::awaitable<asio::error_code> connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port)
	{
		std::string addr = asio::to_string(std::forward_like<decltype(server_address)>(server_address));
		std::string port = asio::to_string(std::forward_like<decltype(server_port)>(server_port));

		using sock_type = std::remove_cvref_t<decltype(sock)>;
		using endpoint_type = typename sock_type::protocol_type::endpoint;
		using resolver_type = typename sock_type::protocol_type::resolver;

		co_await asio::dispatch(asio::use_awaitable_executor(sock));

		resolver_type resolver(asio::detail::get_lowest_executor(sock));

		// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
		auto [e1, eps] = co_await resolver.async_resolve(
			addr, port, asio::ip::resolver_base::flags(), asio::use_awaitable_executor(resolver));
		if (e1)
			co_return e1;

		if (sock.is_open())
		{
			for (const auto& ep : eps)
			{
				auto [e2] = co_await sock.async_connect(ep, asio::use_awaitable_executor(sock));
				if (!e2)
					co_return e2;
			}
		}
		else
		{
			asio::error_code ec{};

			for (const auto& ep : eps)
			{
				sock_type tmp(sock.get_executor());

				tmp.open(ep.endpoint().protocol(), ec);
				if (ec)
					continue;

				auto [e2] = co_await tmp.async_connect(ep, asio::use_awaitable_executor(tmp));
				if (!e2)
				{
					sock = std::move(tmp);
					co_return e2;
				}
			}
		}

		co_return asio::error::connection_refused;
	}
}

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct tcp_async_connect_op
	{
		auto operator()(
			auto state, auto sock_ref, auto&& server_address, auto&& server_port,
			std::chrono::steady_clock::duration connect_timeout, auto&& cb_set_option) -> void
		{
			auto& sock = sock_ref.get();

			auto fn_set_option = std::forward_like<decltype(cb_set_option)>(cb_set_option);

			std::string addr = asio::to_string(std::forward_like<decltype(server_address)>(server_address));
			std::string port = asio::to_string(std::forward_like<decltype(server_port)>(server_port));

			using sock_type = std::remove_cvref_t<decltype(sock)>;
			using endpoint_type = typename sock_type::protocol_type::endpoint;
			using resolver_type = typename sock_type::protocol_type::resolver;

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			detail::call_func_when_timeout wt(
				asio::detail::get_lowest_executor(sock), connect_timeout, [&sock]() mutable
				{
					error_code ec{};
					sock.close(ec);
					asio::reset_lock(sock);
				});

			resolver_type resolver(asio::detail::get_lowest_executor(sock));

			// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
			auto [e1, eps] = co_await asio::async_resolve(
				resolver, std::move(addr), std::move(port),
				asio::ip::resolver_base::flags(), asio::use_deferred_executor(resolver));
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			if (sock.is_open())
			{
				for (const auto& ep : eps)
				{
					auto [e2] = co_await sock.async_connect(ep, asio::use_deferred_executor(sock));
					if (!e2)
						co_return{ e2, ep.endpoint() };

					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, endpoint_type{} };
				}
			}
			else
			{
				asio::error_code ec{};

				for (const auto& ep : eps)
				{
					sock_type tmp(sock.get_executor());

					tmp.open(ep.endpoint().protocol(), ec);
					if (ec)
						continue;

					// you can use the option callback to set the bind address and port
					fn_set_option(tmp);

					auto [e2] = co_await tmp.async_connect(ep, asio::use_deferred_executor(tmp));
					if (!e2)
					{
						sock = std::move(tmp);
						co_return{ e2, ep.endpoint() };
					}
				}
			}

			co_return{ asio::error::connection_refused, endpoint_type{} };
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename ConnectToken = asio::default_token_type<AsyncStream>>
	requires is_tcp_socket<AsyncStream>
	inline auto async_connectex(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			asio::tcp_connect_timeout,
			asio::default_tcp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
	 * @param connect_timeout - The connect timeout.
	 * @param cb_set_option - The callback to set the socket options. [](auto& sock){...}
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename SetOptionFn,
		typename ConnectToken = asio::default_token_type<AsyncStream>>
	requires (is_tcp_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_connectex(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		std::chrono::steady_clock::duration connect_timeout,
		SetOptionFn&& cb_set_option,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			connect_timeout,
			std::forward<SetOptionFn>(cb_set_option));
	}
}
