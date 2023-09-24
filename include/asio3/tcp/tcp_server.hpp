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

		std::function<awaitable<void>(std::shared_ptr<tcp_connection>)> on_accept{};
	};

	template<typename ConnectionT = tcp_connection>
	class tcp_server_t : public std::enable_shared_from_this<tcp_server_t<ConnectionT>>
	{
	protected:
		struct async_start_op
		{
			asio::awaitable<void> do_connection(tcp_server_t& server, tcp_socket client)
			{
				tcp_connection_option conn_opt
				{
					.connect_timeout = server.option.connect_timeout,
					.disconnect_timeout = server.option.disconnect_timeout,
					.idle_timeout = server.option.idle_timeout,
					.socket_option = server.option.socket_option,
				};

				auto conn = std::make_shared<connection_type>(std::move(client), conn_opt);

				server.connection_map[conn->hash_key()] = conn;

				std::chrono::steady_clock::time_point deadline{};
				
				co_await
				(
					(
						server.option.on_accept(conn)
					)
					&&
					(
						detail::check_error(conn->socket, conn_opt.disconnect_timeout) ||
						detail::check_read (conn->socket, conn_opt.disconnect_timeout, deadline, conn_opt.idle_timeout) ||
						detail::check_idle (conn->socket, conn_opt.disconnect_timeout, deadline)
					)
				);

				server.connection_map.erase(conn->hash_key());
			}

			asio::awaitable<void> do_accept(tcp_server_t& server)
			{
				auto& acp = server.acceptor;

				while (acp.is_open())
				{
					auto [e1, client] = co_await acp.async_accept(use_nothrow_deferred);
					if (e1 && acp.is_open())
					{
						co_await asio::async_sleep(acp.get_executor(),
							std::chrono::milliseconds(100), use_nothrow_deferred);
					}
					else
					{
						asio::co_spawn(acp.get_executor(),
							do_connection(server, std::move(client)), asio::detached);
					}
				}
			}

			auto operator()(auto state, tcp_server_t& server) -> void
			{
				auto& acp = server.acceptor;
				auto& opt = server.option;

				co_await asio::dispatch(acp.get_executor(), use_nothrow_deferred);

				state.reset_cancellation_state(asio::enable_terminal_cancellation());

				auto [e1, a1] = co_await asio::async_create_acceptor(acp.get_executor(),
					opt.listen_address, opt.listen_port,
					opt.socket_option.reuse_address, use_nothrow_deferred);
				if (e1)
					co_return e1;

				acp = std::move(a1);

				if (opt.on_accept)
					asio::co_spawn(acp.get_executor(), do_accept(server), asio::detached);

				co_return e1;
			}
		};

		struct batch_async_send_op
		{
			asio::awaitable<void> do_send(auto& conn, auto msgbuf, std::size_t& total)
			{
				auto [e1, n1] = co_await conn->async_send(msgbuf, use_nothrow_deferred);
				total += n1;
			}

			template<typename Data>
			auto operator()(auto state, tcp_server_t& server, Data&& data) -> void
			{
				auto& acp = server.acceptor;

				co_await asio::dispatch(acp.get_executor(), use_nothrow_deferred);

				state.reset_cancellation_state(asio::enable_terminal_cancellation());

				Data msg = std::forward<Data>(data);

				auto msgbuf = asio::buffer(msg);

				std::size_t total = 0;

				//asio::awaitable<void> awaiter;

				for (auto& [key, conn] : server.connection_map)
				{
					auto [e1, n1] = co_await conn->async_send(msgbuf, use_nothrow_deferred);

					total += n1;

					//awaiter = (awaiter && do_send(conn, msgbuf, total));
				}

				//co_await (awaiter);

				co_return{ error_code{}, total };
			}
		};

	public:
		using connection_type = ConnectionT;
		using connection_map_type = std::unordered_map<
			typename connection_type::key_type, std::shared_ptr<connection_type>>;

	public:
		template<class Executor>
		explicit tcp_server_t(const Executor& ex, tcp_server_option opt)
			: option(std::move(opt))
			, acceptor(ex)
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
				asio::post(acceptor.get_executor(), [this]() mutable
				{
					asio::error_code ec{};
					acceptor.close(ec);

					for (auto& [key, conn] : connection_map)
					{
						conn->disconnect();
					}
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

	public:
		tcp_server_option   option;

		asio::tcp_acceptor  acceptor;

		connection_map_type connection_map;
	};

	using tcp_server = tcp_server_t<>;
}
