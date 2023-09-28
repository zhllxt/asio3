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

#include <asio3/core/io_context_thread.hpp>
#include <asio3/core/connection_map.hpp>
#include <asio3/tcp/start.hpp>
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

		std::function<awaitable<void>(std::shared_ptr<tcp_connection>)> on_connect{};
		std::function<awaitable<void>(std::shared_ptr<tcp_connection>)> on_disconnect{};
	};

	template<typename ConnectionT = tcp_connection>
	class tcp_server_t : public std::enable_shared_from_this<tcp_server_t<ConnectionT>>
	{
	public:
		using connection_type = ConnectionT;

		struct async_start_op;
		struct batch_async_send_op;

	public:
		template<class Executor>
		explicit tcp_server_t(const Executor& ex, tcp_server_option opt)
			: option(std::move(opt))
			, acceptor(ex)
			, connection_map(ex)
		{
		}

		~tcp_server_t()
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
			return asio::async_initiate<StartToken, void(asio::error_code)>(
				asio::experimental::co_composed<void(asio::error_code)>(
					async_start_op{}, acceptor),
				token, std::ref(*this));
		}

		/**
		 * @brief Stop the server, this function does not block.
		 */
		inline void stop() noexcept
		{
			if (acceptor.is_open())
			{
				asio::co_spawn(get_executor(), do_stop(*this), asio::detached);
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
		 * @brief Safety start an asynchronous operation to write all of the supplied data to all clients.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data, typename WriteToken = default_tcp_write_token>
		inline auto async_send(Data&& data, WriteToken&& token = default_tcp_write_token{})
		{
			return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
				experimental::co_composed<void(asio::error_code, std::size_t)>(
					batch_async_send_op{}, acceptor),
				token, std::ref(*this), detail::data_persist(std::forward<Data>(data)));
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return acceptor.get_executor();
		}

		/**
		 * @brief Get the listen address.
		 */
		inline std::string get_listen_address() noexcept
		{
			error_code ec{};
			return acceptor.local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the listen port number.
		 */
		inline ip::port_type get_listen_port() noexcept
		{
			error_code ec{};
			return acceptor.local_endpoint(ec).port();
		}

	protected:
		static asio::awaitable<void> do_disconnect_connection(std::shared_ptr<connection_type>& conn)
		{
			conn->disconnect();
			co_return;
		}

		static asio::awaitable<void> do_stop(tcp_server_t& server)
		{
			co_await asio::post(server.get_executor(), use_nothrow_deferred);

			asio::error_code ec{};
			server.acceptor.close(ec);

			co_await server.connection_map.async_for_each(do_disconnect_connection, use_nothrow_deferred);
		}

	public:
		tcp_server_option   option;

		asio::tcp_acceptor  acceptor;

		connection_map_t<connection_type> connection_map;
	};

	using tcp_server = tcp_server_t<>;
}

#include <asio3/tcp/impl/tcp_server.ipp>
