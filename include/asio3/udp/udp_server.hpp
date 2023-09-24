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
#include <asio3/udp/udp_connection.hpp>

namespace asio
{
	struct udp_server_option
	{
		std::string           listen_address{};
		std::uint16_t         listen_port{};

		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::udp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::udp_handshake_timeout) };
		timeout_duration      idle_timeout{ std::chrono::milliseconds(detail::udp_idle_timeout) };

		std::size_t           init_recv_buffer_size{ detail::udp_frame_size };
		std::size_t           max_recv_buffer_size{ detail::max_buffer_size };

		udp_socket_option     socket_option{};

		std::function<void(std::shared_ptr<udp_connection>)> on_connect{};
		std::function<void(std::shared_ptr<udp_connection>)> on_disconnect{};
		std::function<void(std::shared_ptr<udp_connection>, std::string_view)> on_recv{};
	};

	template<typename ConnectionT = udp_connection>
	class udp_server_t : public std::enable_shared_from_this<udp_server_t<ConnectionT>>
	{
	protected:
		struct async_start_op
		{
			asio::awaitable<void> do_connection(udp_server_t& server, auto conn)
			{
				auto& opt = server.option;
				auto& conn_opt = conn->option;

				if (opt.on_connect)
					opt.on_connect(conn);

				std::chrono::steady_clock::time_point deadline{};

				co_await
				(
					detail::check_error(conn->socket, conn_opt.disconnect_timeout) ||
					detail::check_read (conn->socket, conn_opt.disconnect_timeout, deadline, conn_opt.idle_timeout) ||
					detail::check_idle (conn->socket, conn_opt.disconnect_timeout, deadline)
				);

				server.connection_map.erase(conn->hash_key());

				if (opt.on_disconnect)
					opt.on_disconnect(conn);
			}

			asio::awaitable<void> do_recv(udp_server_t& server)
			{
				auto& sock = server.socket;
				auto& opt = server.option;

				std::vector<std::uint8_t> recv_buffer(opt.init_recv_buffer_size);

				asio::ip::udp::endpoint remote_endpoint{};

				while (sock.is_open())
				{
					auto [e1, n1] = co_await sock.async_receive_from(
						asio::buffer(recv_buffer), remote_endpoint, use_nothrow_deferred);
					if (e1)
						break;

					auto [conn, inserted] = server.insert_or_find(remote_endpoint);
					if (inserted)
					{
						asio::co_spawn(sock.get_executor(), do_connection(server, conn), asio::detached);
					}

					std::string_view data{ (std::string_view::pointer)(recv_buffer.data()), n1 };

					opt.on_recv(conn, data);

					if (n1 == recv_buffer.size())
						recv_buffer.resize((std::min)(recv_buffer.size() * 3 / 2, opt.max_recv_buffer_size));
				}
			}

			auto operator()(auto state, udp_server_t& server) -> void
			{
				auto& sock = server.socket;
				auto& opt = server.option;

				co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

				state.reset_cancellation_state(asio::enable_terminal_cancellation());

				std::string addr = asio::to_string(opt.listen_address);
				std::string port = asio::to_string(opt.listen_port);

				ip::udp::resolver resolver(sock.get_executor());

				auto [e1, eps] = co_await resolver.async_resolve(
					addr, port, asio::ip::udp::resolver::passive, use_nothrow_deferred);
				if (e1)
					co_return e1;

				if (!!state.cancelled())
					co_return asio::error::operation_aborted;

				if (eps.empty())
					co_return asio::error::host_not_found;

				ip::udp::endpoint bnd_endpoint = (*eps).endpoint();

				asio::error_code ec;

				udp_socket tmp_sock(sock.get_executor());

				tmp_sock.open(bnd_endpoint.protocol(), ec);
				if (ec)
					co_return ec;

				default_udp_socket_option_setter{ opt.socket_option }(tmp_sock);

				tmp_sock.bind(bnd_endpoint, ec);
				if (ec)
					co_return ec;

				sock = std::move(tmp_sock);

				if (opt.on_recv)
					asio::co_spawn(sock.get_executor(), do_recv(server), asio::detached);

				co_return error_code{};
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
			auto operator()(auto state, udp_server_t& server, Data&& data) -> void
			{
				auto& acp = server.socket;

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
		explicit udp_server_t(const Executor& ex, udp_server_option opt)
			: option(std::move(opt))
			, socket(ex)
		{
		}

		~udp_server_t()
		{
			stop();
		}

		template<
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) StartToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::udp_socket::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(StartToken, void(asio::error_code))
		async_start(
			StartToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::udp_socket::executor_type))
		{
			return asio::async_initiate<StartToken, void(asio::error_code)>(
				asio::experimental::co_composed<void(asio::error_code)>(
					async_start_op{}, socket),
				token, std::ref(*this));
		}

		/**
		 * @brief Stop the server, this function does not block.
		 */
		inline void stop() noexcept
		{
			if (socket.is_open())
			{
				asio::post(socket.get_executor(), [this]() mutable
				{
					asio::error_code ec{};
					socket.shutdown(socket_base::shutdown_both, ec);
					socket.close(ec);

					for (auto& [key, conn] : connection_map)
					{
						conn->stop();
					}
				});
			}
		}

		/**
		 * @brief Check whether the socket is stopped or not.
		 */
		inline bool is_stopped() const noexcept
		{
			return !socket.is_open();
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data to all clients.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data, typename WriteToken = default_udp_send_token>
		inline auto async_send(Data&& data, WriteToken&& token = default_udp_send_token{})
		{
			return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
				experimental::co_composed<void(asio::error_code, std::size_t)>(
					batch_async_send_op{}, socket),
				token, std::ref(*this), detail::data_persist(std::forward<Data>(data)));
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return socket.get_executor();
		}

		/**
		 * @brief Get the listen address.
		 */
		inline std::string get_listen_address() noexcept
		{
			error_code ec{};
			return socket.local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the listen port number.
		 */
		inline ip::port_type get_listen_port() noexcept
		{
			error_code ec{};
			return socket.local_endpoint(ec).port();
		}

		/**
		 * @brief If a key equivalent to k already exists in the container. 
		 * If the key does not exist, inserts the new value.
		 * @return The bool component is true if the insertion took place and
		 * false if the find took place. 
		 */
		std::pair<std::shared_ptr<connection_type>, bool> insert_or_find(
			const ip::udp::endpoint& remote_endpoint)
		{
			if (auto it = connection_map.find(remote_endpoint); it != connection_map.end())
				return { it->second, false };

			udp_connection_option opt
			{
				.connect_timeout = option.connect_timeout,
				.disconnect_timeout = option.disconnect_timeout,
				.idle_timeout = option.idle_timeout,
			};
			std::shared_ptr<connection_type> conn = std::make_shared<connection_type>(socket, opt);
			auto [it, inserted] = connection_map.try_emplace(remote_endpoint, std::move(conn));
			return { it->second, inserted };
		}

	public:
		udp_server_option   option;

		asio::udp_socket    socket;

		connection_map_type connection_map;
	};

	using udp_server = udp_server_t<>;
}
