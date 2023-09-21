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
#include <asio3/tcp/tcp_connection.hpp>

namespace asio
{
	struct tcp_server_option
	{
		std::string           listen_address{};
		std::uint16_t         listen_port{};

		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::tcp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::tcp_handshake_timeout) };
		timeout_duration      idle_timeout{ std::chrono::milliseconds(detail::tcp_idle_timeout) };

		tcp_socket_option     socket_option{};

		std::function<awaitable<void>(std::shared_ptr<tcp_connection>)> new_connection_handler{};
	};

	template<typename ConnectionT = tcp_connection>
	class tcp_server_t
	{
		template<typename> class tcp_server_impl_t;

	public:
		using connection_type = ConnectionT;

	public:
		template<class Executor>
		explicit tcp_server_t(const Executor& ex, tcp_server_option opt)
		{
			impl = std::make_shared<tcp_server_impl_t<ConnectionT>>(ex, std::move(opt));
		}

		~tcp_server_t()
		{
			impl->stop();
		}

		template<
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) StartToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::tcp_acceptor::executor_type)>
		inline auto async_start(
			StartToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::tcp_acceptor::executor_type))
		{
			return impl->async_start(std::forward<StartToken>(token));
		}

		/**
		 * @brief Abort the object, this function does not block.
		 */
		inline void stop() noexcept
		{
			impl->stop();
		}

		/**
		 * @brief Check whether the acceptor is stopped or not.
		 */
		inline bool is_stopped() const noexcept
		{
			return !impl->acceptor.is_open();
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data to all clients.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data, typename Token = default_tcp_write_token>
		inline auto async_send(Data&& data, Token&& token = default_tcp_write_token{})
		{
			return impl->async_send(std::forward<Data>(data), std::forward<Token>(token));
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return impl->acceptor.get_executor();
		}

		/**
		 * @brief Get the listen address.
		 */
		inline std::string get_listen_address() noexcept
		{
			error_code ec{};
			return impl->acceptor.local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the listen port number.
		 */
		inline ip::port_type get_listen_port() noexcept
		{
			error_code ec{};
			return impl->acceptor.local_endpoint(ec).port();
		}

	public:
		std::shared_ptr<tcp_server_impl_t<ConnectionT>> impl;
	};

	using tcp_server = tcp_server_t<>;
}

#include <asio3/tcp/impl/tcp_server.ipp>
