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
#include <asio3/core/strutil.hpp>
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct tcp_async_connect_op
	{
		template<typename AsyncStream,
			typename String1, typename StrOrInt1, typename String2, typename StrOrInt2,
			typename SetOptionCallback>
		auto operator()(
			auto state, std::reference_wrapper<AsyncStream> sock_ref,
			String1&& server_address, StrOrInt1&& server_port,
			String2&& bind_address, StrOrInt2&& bind_port,
			SetOptionCallback&& cb_set_option) -> void
		{
			using endpoint_type = typename AsyncStream::protocol_type::endpoint;
			using resolver_type = typename AsyncStream::protocol_type::resolver;

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			std::string srv_addr = asio::to_string(std::forward<String1>(server_address));
			std::string bnd_addr = asio::to_string(std::forward<String2>(bind_address));

			std::string srv_port = asio::to_string(std::forward<StrOrInt1>(server_port));
			std::string bnd_port = asio::to_string(std::forward<StrOrInt2>(bind_port));

			resolver_type resolver(sock.get_executor());

			// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
			auto [e1, eps] = co_await resolver.async_resolve(srv_addr, srv_port, use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

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
					AsyncStream tmp_sock(sock.get_executor());

					tmp_sock.open(ep.endpoint().protocol(), ec);
					if (ec)
						continue;

					cb_set_option(tmp_sock);

					if (!bnd_addr.empty() || uport != 0)
					{
						tmp_sock.bind(bnd_endpoint, ec);
						if (ec)
							continue;
					}

					auto [e2] = co_await tmp_sock.async_connect(ep, use_nothrow_deferred);
					if (!e2)
					{
						sock = std::move(tmp_sock);
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
	 * @param cb_set_option - The callback to set the socket options.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename String, typename StrOrInt,
		typename SetOptionCallback = asio::default_tcp_socket_option_setter,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::ip::tcp::endpoint)) ConnectToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires 
		(detail::is_template_instance_of<asio::basic_stream_socket, std::remove_cvref_t<AsyncStream>> &&
		(std::constructible_from<std::string, String>) &&
		(std::constructible_from<std::string, StrOrInt> || std::integral<std::remove_cvref_t<StrOrInt>>))
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint))
	async_connect(
		AsyncStream& sock,
		String&& server_address, StrOrInt&& server_port,
		SetOptionCallback&& cb_set_option = asio::default_tcp_socket_option_setter{},
		ConnectToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward<String>(server_address), std::forward<StrOrInt>(server_port),
			"", 0,
			std::forward<SetOptionCallback>(cb_set_option));
	}

	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected.
	 * @param server_address - The target server address. 
	 * @param server_port - The target server port. 
	 * @param cb_set_option - The callback to set the socket options.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename String1, typename StrOrInt1,
		typename String2, typename StrOrInt2,
		typename SetOptionCallback = asio::default_tcp_socket_option_setter,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::ip::tcp::endpoint)) ConnectToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires 
		(detail::is_template_instance_of<asio::basic_stream_socket, std::remove_cvref_t<AsyncStream>> &&
		(std::constructible_from<std::string, String1>) &&
		(std::constructible_from<std::string, StrOrInt1> || std::integral<std::remove_cvref_t<StrOrInt1>>) &&
		(std::constructible_from<std::string, String2>) &&
		(std::constructible_from<std::string, StrOrInt2> || std::integral<std::remove_cvref_t<StrOrInt2>>))
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint))
	async_connect(
		AsyncStream& sock,
		String1&& server_address, StrOrInt1&& server_port,
		String2&& bind_address, StrOrInt2&& bind_port,
		SetOptionCallback&& cb_set_option = asio::default_tcp_socket_option_setter{},
		ConnectToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::tcp_async_connect_op{}, sock),
			token, std::ref(sock),
			std::forward<String1>(server_address), std::forward<StrOrInt1>(server_port),
			std::forward<String2>(bind_address), std::forward<StrOrInt2>(bind_port),
			std::forward<SetOptionCallback>(cb_set_option));
	}
}
