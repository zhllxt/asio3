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
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct tcp_async_connect_op
	{
		auto operator()(
			auto state, auto sock_ref,
			auto&& server_address, auto&& server_port, auto&& bind_address, auto&& bind_port,
			timeout_duration timeout, auto&& cb_set_option) -> void
		{
			auto& sock = sock_ref.get();

			auto fn_set_option = std::forward_like<decltype(cb_set_option)>(cb_set_option);

			std::string svr_addr = asio::to_string(std::forward_like<decltype(server_address)>(server_address));
			std::string bnd_addr = asio::to_string(std::forward_like<decltype(bind_address)>(bind_address));
			std::string svr_port = asio::to_string(std::forward_like<decltype(server_port)>(server_port));
			std::string bnd_port = asio::to_string(std::forward_like<decltype(bind_port)>(bind_port));

			using stream_type = std::remove_cvref_t<decltype(sock)>;
			using endpoint_type = typename stream_type::protocol_type::endpoint;
			using resolver_type = typename stream_type::protocol_type::resolver;

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			detail::call_func_when_timeout wt(sock.get_executor(), timeout, [&sock]() mutable
			{
				error_code ec{};
				sock.close(ec);
			});

			resolver_type resolver(sock.get_executor());

			// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
			auto [e1, eps] = co_await resolver.async_resolve(svr_addr, svr_port, use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (wt.ptr->timeouted)
				co_return{ asio::error::timed_out, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			asio::error_code ec{};

			endpoint_type bnd_endpoint{};

			if (!bnd_addr.empty())
			{
				bnd_endpoint.address(asio::ip::address::from_string(bnd_addr, ec));
				if (ec)
					co_return{ ec, endpoint_type{} };
			}

			std::uint16_t uport = std::strtoul(bnd_port.data(), nullptr, 10);
			if (uport != 0)
			{
				bnd_endpoint.port(uport);
			}

			if (sock.is_open())
			{
				for (auto&& ep : eps)
				{
					auto [e2] = co_await sock.async_connect(ep, use_nothrow_deferred);
					if (!e2)
						co_return{ e2, ep.endpoint() };

					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, endpoint_type{} };
				}
			}
			else
			{
				for (auto&& ep : eps)
				{
					stream_type tmp(sock.get_executor());

					tmp.open(ep.endpoint().protocol(), ec);
					if (ec)
						continue;

					fn_set_option(tmp);

					if (!bnd_addr.empty() || uport != 0)
					{
						tmp.bind(bnd_endpoint, ec);
						if (ec)
							continue;
					}

					auto [e2] = co_await tmp.async_connect(ep, use_nothrow_deferred);
					if (!e2)
					{
						sock = std::move(tmp);
						co_return{ e2, ep.endpoint() };
					}

					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, endpoint_type{} };
				}
			}

			co_return{ asio::error::connection_refused, endpoint_type{} };
		}
	};
}

namespace asio
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
	requires is_basic_stream_socket<AsyncStream>
	inline auto async_connect(
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
			"", 0,
			asio::tcp_connect_timeout,
			asio::default_tcp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
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
	requires (is_basic_stream_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		SetOptionFn&& cb_set_option,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			"", 0,
			asio::tcp_connect_timeout,
			std::forward<SetOptionFn>(cb_set_option));
	}

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
	requires is_basic_stream_socket<AsyncStream>
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		is_string auto&& bind_address, is_string_or_integral auto&& bind_port,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			std::forward_like<decltype(bind_address)>(bind_address),
			std::forward_like<decltype(bind_port)>(bind_port),
			asio::tcp_connect_timeout,
			asio::default_tcp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
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
	requires (is_basic_stream_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		is_string auto&& bind_address, is_string_or_integral auto&& bind_port,
		SetOptionFn&& cb_set_option,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			std::forward_like<decltype(bind_address)>(bind_address),
			std::forward_like<decltype(bind_port)>(bind_port),
			asio::tcp_connect_timeout,
			std::forward<SetOptionFn>(cb_set_option));
	}

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
	requires is_basic_stream_socket<AsyncStream>
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		timeout_duration timeout,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			"", 0, timeout,
			asio::default_tcp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
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
	requires (is_basic_stream_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		timeout_duration timeout,
		SetOptionFn&& cb_set_option,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			"", 0, timeout,
			std::forward<SetOptionFn>(cb_set_option));
	}

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
	requires is_basic_stream_socket<AsyncStream>
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		is_string auto&& bind_address, is_string_or_integral auto&& bind_port,
		timeout_duration timeout,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			std::forward_like<decltype(bind_address)>(bind_address),
			std::forward_like<decltype(bind_port)>(bind_port),
			timeout,
			asio::default_tcp_socket_option_setter{});
	}

	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
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
	requires (is_basic_stream_socket<AsyncStream> && std::invocable<SetOptionFn, AsyncStream&>)
	inline auto async_connect(
		AsyncStream& sock,
		is_string auto&& server_address, is_string_or_integral auto&& server_port,
		is_string auto&& bind_address, is_string_or_integral auto&& bind_port,
		timeout_duration timeout,
		SetOptionFn&& cb_set_option,
		ConnectToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward_like<decltype(server_address)>(server_address),
			std::forward_like<decltype(server_port)>(server_port),
			std::forward_like<decltype(bind_address)>(bind_address),
			std::forward_like<decltype(bind_port)>(bind_port),
			timeout,
			std::forward<SetOptionFn>(cb_set_option));
	}
}
