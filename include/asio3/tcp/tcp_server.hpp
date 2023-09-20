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

#include <asio3/core/context_worker.hpp>
#include <asio3/tcp/accept.hpp>
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>
#include <asio3/tcp/send.hpp>
#include <asio3/tcp/start.hpp>
#include <asio3/tcp/disconnect.hpp>

namespace asio
{
	struct tcp_server_option
	{
		std::string           listen_address{};
		std::uint16_t         listen_port{};

		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::tcp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::tcp_handshake_timeout) };

		tcp_socket_option     socket_option{};

		std::function<awaitable<void>(tcp_socket)> accept_function{};
	};

	class tcp_server
	{
	public:
		template<class Executor>
		explicit tcp_server(const Executor& ex, tcp_server_option opt)
			: option(std::move(opt))
			, acceptor(ex)
		{
		}

		~tcp_server()
		{
			stop();
		}

		template<
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) StartToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::tcp_acceptor::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code))
		async_start(
			StartToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::tcp_acceptor::executor_type))
		{
			return asio::async_start(acceptor, option, std::forward<StartToken>(token));
		}

		/**
		 * @brief Abort the object, this function does not block.
		 */
		inline void stop() noexcept
		{
			if (acceptor.is_open())
			{
				asio::dispatch(acceptor.get_executor(), [this]() mutable
				{
					asio::error_code ec{};
					acceptor.close(ec);
				});
			}
		}

		/**
		 * @brief Check whether the acceptor is stopped or not.
		 */
		inline bool is_stopped() const noexcept
		{
			return !acceptor.is_open();
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data to a stream.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		//template<typename Data, typename Token = default_tcp_write_token>
		//inline auto async_send(Data&& data, Token&& token = default_tcp_write_token{})
		//{
		//	return asio::async_send(acceptor, std::forward<Data>(data), std::forward<Token>(token));
		//}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return acceptor.get_executor();
		}

	public:
		tcp_server_option  option;

		asio::tcp_acceptor acceptor;
	};
}
