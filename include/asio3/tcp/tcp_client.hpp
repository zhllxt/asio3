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
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>
#include <asio3/tcp/send.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/tcp/disconnect.hpp>
#include <asio3/socks5/handshake.hpp>

namespace asio
{
	struct tcp_client_option
	{
		std::string           server_address{};
		std::uint16_t         server_port{};

		std::string           bind_address{};
		std::uint16_t         bind_port{ 0 };

		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::tcp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::tcp_handshake_timeout) };

		tcp_socket_option     socket_option{};

		socks5::option        socks5_option{};
	};

	class tcp_client : public std::enable_shared_from_this<tcp_client>
	{
	protected:
		struct async_connect_op
		{
			auto operator()(auto state, tcp_client& client) -> void
			{
				auto& sock = client.socket;
				auto& opt = client.option;

				co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

				state.reset_cancellation_state(asio::enable_terminal_cancellation());

				asio::timeout_guard tg(sock, opt.connect_timeout);

				if (asio::to_underlying(opt.socks5_option.cmd) == 0)
					opt.socks5_option.cmd = socks5::command::connect;

				if (socks5::is_option_valid(opt.socks5_option))
				{
					auto [e1, ep] = co_await asio::async_connect(
						sock,
						opt.socks5_option.proxy_address, opt.socks5_option.proxy_port,
						opt.bind_address, opt.bind_port,
						asio::default_tcp_socket_option_setter{ opt.socket_option },
						use_nothrow_deferred);
					if (e1)
						co_return e1;

					if (opt.socks5_option.dest_address.empty())
						opt.socks5_option.dest_address = opt.server_address;
					if (opt.socks5_option.dest_port == 0)
						opt.socks5_option.dest_port = opt.server_port;

					auto [e2] = co_await socks5::async_handshake(
						sock, opt.socks5_option, use_nothrow_deferred);
					if (e2)
						co_return e2;
				}
				else
				{
					auto [e1, ep] = co_await asio::async_connect(
						sock,
						opt.server_address, opt.server_port,
						opt.bind_address, opt.bind_port,
						asio::default_tcp_socket_option_setter{ opt.socket_option },
						use_nothrow_deferred);
					if (e1)
						co_return e1;
				}

				co_return asio::error_code{};
			}
		};

	public:
		template<class Executor>
		explicit tcp_client(const Executor& ex, tcp_client_option opt)
			: option(std::move(opt))
			, socket(ex)
		{
			aborted.clear();
		}

		~tcp_client()
		{
			stop();
		}

		template<
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) ConnectToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::tcp_socket::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ConnectToken, void(asio::error_code))
		async_connect(
			ConnectToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::tcp_socket::executor_type))
		{
			return asio::async_initiate<ConnectToken, void(asio::error_code)>(
				asio::experimental::co_composed<void(asio::error_code)>(
					async_connect_op{}, socket),
				token, std::ref(*this));
		}

		/**
		 * @brief Abort the object, disconnect the connection, this function does not block.
		 */
		inline void stop() noexcept
		{
			if (!aborted.test_and_set())
			{
				asio::async_disconnect(socket, option.disconnect_timeout, [](auto) {});
			}
		}

		/**
		 * @brief Set the aborted flag to false.
		 */
		inline void restart() noexcept
		{
			aborted.clear();
		}

		/**
		 * @brief Check whether the client is aborted or not.
		 */
		inline bool is_aborted() const noexcept
		{
			return aborted.test();
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data, typename WriteToken = default_tcp_write_token>
		inline auto async_send(Data&& data, WriteToken&& token = default_tcp_write_token{})
		{
			return asio::async_send(socket, std::forward<Data>(data), std::forward<WriteToken>(token));
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor() noexcept
		{
			return socket.get_executor();
		}

		/**
		 * @brief Get the local address.
		 */
		inline std::string get_local_address() noexcept
		{
			error_code ec{};
			return socket.local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the local port number.
		 */
		inline ip::port_type get_local_port() noexcept
		{
			error_code ec{};
			return socket.local_endpoint(ec).port();
		}

		/**
		 * @brief Get the remote address.
		 */
		inline std::string get_remote_address() noexcept
		{
			error_code ec{};
			return socket.remote_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the remote port number.
		 */
		inline ip::port_type get_remote_port() noexcept
		{
			error_code ec{};
			return socket.remote_endpoint(ec).port();
		}

	public:
		tcp_client_option option;

		asio::tcp_socket  socket;

		std::atomic_flag  aborted{};
	};
}
