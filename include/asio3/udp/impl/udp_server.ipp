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

namespace asio
{
	template<typename ConnectionT>
	struct udp_server_t<ConnectionT>::async_start_op
	{
		asio::awaitable<void> do_connection(udp_server_t& server, auto conn)
		{
			auto& opt = server.option;

			if (opt.on_connect)
			{
				co_await opt.on_connect(conn);
			}

			co_await server.connection_map.async_emplace(conn, use_nothrow_deferred);

			std::chrono::steady_clock::time_point deadline{};

			co_await
			(
				detail::check_error(conn) ||
				detail::check_read (conn, deadline) ||
				detail::check_idle (conn, deadline)
			);

			co_await server.connection_map.async_erase(conn->hash_key(), use_nothrow_deferred);

			if (opt.on_disconnect)
			{
				co_await opt.on_disconnect(conn);
			}
		}

		asio::awaitable<void> do_accept(udp_server_t& server)
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

				std::shared_ptr<connection_type> conn;

				auto [p] = co_await server.connection_map.async_find(remote_endpoint, use_nothrow_deferred);
				if (p == nullptr)
				{
					udp_connection_option conn_opt
					{
						.connect_timeout = opt.connect_timeout,
						.disconnect_timeout = opt.disconnect_timeout,
						.idle_timeout = opt.idle_timeout,
					};

					conn = std::make_shared<connection_type>(sock, conn_opt);

					asio::co_spawn(sock.get_executor(), do_connection(server, conn), asio::detached);
				}
				else
				{
					conn = std::move(p);
				}

				if (opt.on_recv)
				{
					std::string_view data{ (std::string_view::pointer)(recv_buffer.data()), n1 };
					co_await opt.on_recv(conn, data);
				}

				if (n1 == recv_buffer.size())
					recv_buffer.resize((std::min)(recv_buffer.size() * 3 / 2, opt.max_recv_buffer_size));
			}
		}

		auto operator()(auto state, udp_server_t& server) -> void
		{
			auto& sock = server.socket;
			auto& opt = server.option;

			auto [e1, ep1] = co_await asio::async_start(
				sock, opt.listen_address, opt.listen_port,
				default_udp_socket_option_setter{ opt.socket_option }, use_nothrow_deferred);
			if (e1)
				co_return e1;

			if (opt.on_connect || opt.on_disconnect || opt.on_recv)
				asio::co_spawn(sock.get_executor(), do_accept(server), asio::detached);

			co_return error_code{};
		}
	};

	template<typename ConnectionT>
	struct udp_server_t<ConnectionT>::batch_async_send_op
	{
		asio::awaitable<void> do_send(auto msgbuf, std::size_t& total, auto conn)
		{
			auto [e1, n1] = co_await conn->async_send(msgbuf, use_nothrow_deferred);
			total += n1;
		}

		template<typename Data>
		auto operator()(auto state, udp_server_t& server, Data&& data) -> void
		{
			Data msg = std::forward<Data>(data);

			co_await asio::dispatch(server.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			error_code ec{};

			auto msgbuf = asio::buffer(msg);

			std::size_t total = 0;

			co_await connection_map.async_for_each(
				std::bind_front(do_send, msgbuf, std::ref(total)), use_nothrow_deferred);

			co_return{ error_code{}, total };
		}
	};
}
