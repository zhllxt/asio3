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
	struct default_set_option_callback
	{
		inline void operator()(auto& sock) noexcept
		{
			detail::ignore_unused(sock);
		}
	};

	struct async_connect_op
	{
		template<typename AsyncStream, typename String, typename StrOrInt, typename SetOptionCallback>
		auto operator()(
			auto state, std::reference_wrapper<AsyncStream> sock_ref,
			String&& host, StrOrInt&& port, SetOptionCallback&& cb_set_option) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto& sock = sock_ref.get();

			std::string h = asio::to_string(std::forward<String>(host));
			std::string p = asio::to_string(std::forward<StrOrInt>(port));

			asio::ip::tcp::resolver resolver(sock.get_executor());

			auto [e1, eps] = co_await resolver.async_resolve(h, p, use_nothrow_deferred);
			if (e1)
				co_return{ e1, asio::ip::tcp::endpoint{} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, asio::ip::tcp::endpoint{} };

			if (eps.empty())
				co_return{ asio::error::host_unreachable, asio::ip::tcp::endpoint{} };

			if (sock.is_open())
			{
				for (auto&& ep : eps)
				{
					auto [e2] = co_await sock.async_connect(ep, use_nothrow_deferred);
					if (!e2)
						co_return{ e2, ep.endpoint() };

					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, asio::ip::tcp::endpoint{} };
				}
			}
			else
			{
				asio::error_code ec{};

				for (auto&& ep : eps)
				{
					sock.close(ec);

					sock.open(ep.endpoint().protocol(), ec);

					if (ec)
						continue;

					sock.set_option(asio::socket_base::reuse_address(true), ec);
					sock.set_option(asio::socket_base::keep_alive(true), ec);
					sock.set_option(asio::ip::tcp::no_delay(true), ec);

					cb_set_option(sock);

					auto [e2] = co_await sock.async_connect(ep, use_nothrow_deferred);
					if (!e2)
						co_return{ e2, ep.endpoint() };

					if (!!state.cancelled())
						co_return{ asio::error::operation_aborted, asio::ip::tcp::endpoint{} };
				}
			}

			co_return{ asio::error::connection_refused, asio::ip::tcp::endpoint{} };
		}
	};
}

namespace asio
{
	/**
	 * @brief Asynchronously establishes a socket connection by trying each endpoint in a sequence.
	 * @param sock - The socket reference to be connected. The type can't be asio::ip::tcp:socket
	 * @param host - The target server host. 
	 * @param port - The target server port. 
	 * @param cb_set_option - The callback to set the socket options.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::ip::tcp::endpoint ep);
	 */
	template<
		typename AsyncStream,
		typename String, typename StrOrInt,
		typename SetOptionCallback = detail::default_set_option_callback,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::ip::tcp::endpoint)) ConnectToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename AsyncStream::executor_type)>
	requires 
		(std::constructible_from<std::string, String> &&
		(std::constructible_from<std::string, StrOrInt> || std::integral<std::remove_cvref_t<StrOrInt>>))
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint))
	async_connect(
		AsyncStream& sock, String&& host, StrOrInt&& port,
		SetOptionCallback&& cb_set_option = detail::default_set_option_callback{},
		ConnectToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename AsyncStream::executor_type))
	{
		return asio::async_initiate<ConnectToken, void(asio::error_code, asio::ip::tcp::endpoint)>(
			asio::experimental::co_composed<void(asio::error_code, asio::ip::tcp::endpoint)>(
				detail::async_connect_op{}, sock),
			token, std::ref(sock), std::forward<String>(host), std::forward<StrOrInt>(port),
			std::forward<SetOptionCallback>(cb_set_option));
	}
}
