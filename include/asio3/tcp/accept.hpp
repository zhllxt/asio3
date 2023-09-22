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
#include <asio3/core/timer.hpp>
#include <asio3/core/strutil.hpp>
#include <asio3/tcp/core.hpp>

namespace asio::detail
{
	struct async_create_acceptor_op
	{
		template<typename String, typename StrOrInt>
		auto operator()(auto state, auto&& executor,
			String&& listen_address, StrOrInt&& listen_port, bool reuse_addr) -> void
		{
			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			std::string addr = asio::to_string(std::forward<String>(listen_address));
			std::string port = asio::to_string(std::forward<StrOrInt>(listen_port));

			ip::tcp::resolver resolver(executor);

			auto [e1, eps] = co_await resolver.async_resolve(
				addr, port, asio::ip::tcp::resolver::passive, use_nothrow_deferred);
			if (e1)
				co_return{ e1, asio::tcp_acceptor{executor} };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted, asio::tcp_acceptor{executor} };

			if (eps.empty())
				co_return{ asio::error::host_not_found, asio::tcp_acceptor{executor} };

			try
			{
				asio::tcp_acceptor acceptor(executor, *eps, reuse_addr);

				co_return{ asio::error_code{}, std::move(acceptor) };
			}
			catch (const asio::system_error& e)
			{
				co_return{ e.code(), asio::tcp_acceptor{executor} };
			}
		}
	};
}

namespace asio
{
	/**
	 * @brief Create a tcp acceptor asynchronously.
	 * @param executor - The executor.
	 * @param listen_address - The listen ip. 
	 * @param listen_port - The listen port. 
     * @param reuse_addr  - Whether set the socket option socket_base::reuse_address.
	 * @param token - The completion handler to invoke when the operation completes.
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::tcp_acceptor acceptor);
	 */
	template<typename Executor, typename String, typename StrOrInt,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::tcp_acceptor)) CreateToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::tcp_acceptor::executor_type)>
	requires
		(std::constructible_from<std::string, String> &&
		(std::constructible_from<std::string, StrOrInt> || std::integral<std::remove_cvref_t<StrOrInt>>))
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(CreateToken, void(asio::error_code, asio::tcp_acceptor))
	async_create_acceptor(
		Executor&& executor,
		String&& listen_address, StrOrInt&& listen_port,
		bool reuse_addr = true,
		CreateToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::tcp_acceptor::executor_type))
	{
		return async_initiate<CreateToken, void(asio::error_code, asio::tcp_acceptor)>(
			experimental::co_composed<void(asio::error_code, asio::tcp_acceptor)>(
				detail::async_create_acceptor_op{}, executor),
			token, executor,
			std::forward<String>(listen_address), std::forward<StrOrInt>(listen_port), reuse_addr);
	}
}
