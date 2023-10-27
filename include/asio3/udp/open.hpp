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
#include <asio3/udp/core.hpp>

namespace asio::detail
{
	struct udp_async_start_op
	{
		template<typename AsyncStream, typename String, typename StrOrInt, typename SetOptionCallback>
		auto operator()(
			auto state, std::reference_wrapper<AsyncStream> sock_ref,
			String&& listen_address, StrOrInt&& listen_port, SetOptionCallback&& cb_set_option) -> void
		{
			using endpoint_type = typename AsyncStream::protocol_type::endpoint;
			using resolver_type = typename AsyncStream::protocol_type::resolver;

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			std::string addr = asio::to_string(std::forward<String>(listen_address));
			std::string port = asio::to_string(std::forward<StrOrInt>(listen_port));

			auto set_option = std::forward<SetOptionCallback>(cb_set_option);

			co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			resolver_type resolver(sock.get_executor());

			auto [e1, eps] = co_await resolver.async_resolve(
				addr, port, asio::ip::resolver_base::passive, use_nothrow_deferred);
			if (e1)
				co_return{ e1, endpoint_type{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, endpoint_type{} };

			endpoint_type bnd_endpoint = (*eps).endpoint();

			asio::error_code ec;

			AsyncStream tmp_sock(sock.get_executor());

			tmp_sock.open(bnd_endpoint.protocol(), ec);
			if (ec)
				co_return {ec, endpoint_type{} };

			set_option(tmp_sock);

			tmp_sock.bind(bnd_endpoint, ec);
			if (ec)
				co_return {ec, endpoint_type{} };

			sock = std::move(tmp_sock);

			co_return{ error_code{}, bnd_endpoint };
		}
	};
}

namespace asio
{
	/**
	 * @brief Asynchronously start a udp socket at the bind address and port.
	 * @param sock - The socket reference to be started.
	 * @param listen_address - The listen address. 
	 * @param listen_port - The listen port. 
	 * @param cb_set_option - The callback to set the socket options.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::udp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename String, typename StrOrInt,
		typename SetOptionCallback = asio::default_udp_socket_option_setter,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::ip::udp::endpoint)) StartToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires 
		(detail::is_template_instance_of<asio::basic_datagram_socket, std::remove_cvref_t<AsyncStream>> &&
		(std::constructible_from<std::string, String>) &&
		(std::constructible_from<std::string, StrOrInt> || std::integral<std::remove_cvref_t<StrOrInt>>))
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code, asio::ip::udp::endpoint))
	async_start(
		AsyncStream& sock,
		String&& listen_address, StrOrInt&& listen_port,
		SetOptionCallback&& cb_set_option = asio::default_udp_socket_option_setter{},
		StartToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<StartToken, void(asio::error_code, asio::ip::udp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::udp::endpoint)>(
				detail::udp_async_start_op{}, sock),
			token, std::ref(sock),
			std::forward<String>(listen_address), std::forward<StrOrInt>(listen_port),
			std::forward<SetOptionCallback>(cb_set_option));
	}
}
