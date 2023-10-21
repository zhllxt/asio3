/*
 * Copyright (c) 2017-2023 zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

	struct async_start_op
	{
		asio::awaitable<void> do_connection(auto& server, auto client)
		{
			auto& opt = server.option;

			tcp_connection_option conn_opt
			{
				.connect_timeout = opt.connect_timeout,
				.disconnect_timeout = opt.disconnect_timeout,
				.idle_timeout = opt.idle_timeout,
				.socket_option = opt.socket_option,
			};

			auto conn = std::make_shared<connection_type>(std::move(client), conn_opt);

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

		asio::awaitable<void> do_accept(auto& server)
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

		auto operator()(auto state, auto server_ref) -> void
		{
			auto& server = server_ref.get();
			auto& acp = server.acceptor;
			auto& opt = server.option;

			co_await asio::dispatch(acp.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [e1, ep1] = co_await asio::async_start(acp,
				opt.listen_address, opt.listen_port,
				opt.reuse_address, use_nothrow_deferred);
			if (e1)
				co_return e1;

			if (opt.on_connect || opt.on_disconnect)
				asio::co_spawn(acp.get_executor(), do_accept(server), asio::detached);

			co_return e1;
		}
	};

	struct batch_async_send_op
	{
		asio::awaitable<void> do_send(auto msgbuf, std::size_t& total, auto conn)
		{
			auto [e1, n1] = co_await conn->async_send(msgbuf, use_nothrow_deferred);
			total += n1;
		}

		template<typename Data>
		auto operator()(auto state, auto server_ref, Data&& data) -> void
		{
			auto& server = server_ref.get();

			Data msg = std::forward<Data>(data);

			co_await asio::dispatch(server.get_executor(), use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto msgbuf = asio::buffer(msg);

			std::size_t total = 0;

			co_await connection_map.async_for_each(
				std::bind_front(do_send, msgbuf, std::ref(total)), use_nothrow_deferred);

			//asio::awaitable<void> awaiter;
			//awaiter = (awaiter && do_send(msgbuf, total, conn));
			//co_await (awaiter);

			co_return{ error_code{}, total };
		}
	};
