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
#include <asio3/core/timer.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/udp/send.hpp>
#include <asio3/udp/connect.hpp>
#include <asio3/tcp/connect.hpp>
#include <asio3/socks5/handshake.hpp>
#include <asio3/socks5/parser.hpp>
#include <asio3/socks5/udp_header.hpp>

namespace asio
{
	struct udp_client_option
	{
		std::string           server_address{};
		std::uint16_t         server_port{};

		std::string           bind_address{};
		std::uint16_t         bind_port{ 0 };

		timeout_duration      connect_timeout{ std::chrono::milliseconds(detail::udp_connect_timeout) };
		timeout_duration      disconnect_timeout{ std::chrono::milliseconds(detail::udp_handshake_timeout) };

		udp_socket_option     socket_option{};

		socks5::option        socks5_option{};
	};

	class udp_client : public std::enable_shared_from_this<udp_client>
	{
	protected:
		struct async_connect_op
		{
			auto operator()(auto state, udp_client& client) -> void
			{
				auto& sock = client.socket;
				auto& opt = client.option;

				co_await asio::dispatch(sock.get_executor(), use_nothrow_deferred);

				state.reset_cancellation_state(asio::enable_terminal_cancellation());

				asio::timeout_guard tg(sock, opt.connect_timeout);

				if (asio::to_underlying(opt.socks5_option.cmd) == 0)
					opt.socks5_option.cmd = socks5::command::udp_associate;

				if (socks5::is_option_valid(opt.socks5_option))
				{
					asio::error_code ec{};

					ip::udp::endpoint bnd_endpoint{};

					if (!opt.bind_address.empty())
					{
						bnd_endpoint.address(asio::ip::address::from_string(opt.bind_address, ec));
						if (ec)
							co_return ec;
					}

					bnd_endpoint.port(opt.bind_port);

					udp_socket tmp_sock(sock.get_executor());

					tmp_sock.open(bnd_endpoint.protocol(), ec);
					if (ec)
						co_return ec;

					default_udp_socket_option_setter{ opt.socket_option }(tmp_sock);

					tmp_sock.bind(bnd_endpoint, ec);
					if (ec)
						co_return ec;

					if (opt.socks5_option.dest_address.empty())
						opt.socks5_option.dest_address = opt.server_address;

					opt.socks5_option.dest_port = tmp_sock.local_endpoint(ec).port();
					if (ec)
						co_return ec;

					sock = std::move(tmp_sock);

					tcp_socket s5_sock(sock.get_executor());

					auto [e1, ep1] = co_await asio::async_connect(
						s5_sock,
						opt.socks5_option.proxy_address, opt.socks5_option.proxy_port,
						asio::default_tcp_socket_option_setter{},
						use_nothrow_deferred);
					if (e1)
						co_return e1;

					auto [e2] = co_await socks5::async_handshake(
						s5_sock, opt.socks5_option, use_nothrow_deferred);
					if (e2)
						co_return e2;

					auto [e3, ep3] = co_await asio::async_connect(
						sock,
						opt.server_address, opt.socks5_option.bound_port,
						asio::default_udp_socket_option_setter{ opt.socket_option },
						use_nothrow_deferred);
					if (e3)
						co_return e3;

					client.socks5_socket = std::move(s5_sock);
				}
				else
				{
					auto [e1, ep1] = co_await asio::async_connect(
						sock,
						opt.server_address, opt.server_port,
						opt.bind_address, opt.bind_port,
						asio::default_udp_socket_option_setter{ opt.socket_option },
						use_nothrow_deferred);
					if (e1)
						co_return e1;
				}

				co_return asio::error_code{};
			}
		};

	public:
		template<class Executor>
		explicit udp_client(const Executor& ex, udp_client_option opt)
			: option(std::move(opt))
			, socket(ex)
		{
			aborted.clear();
		}

		~udp_client()
		{
			stop();
		}

		template<
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code)) ConnectToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::udp_socket::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ConnectToken, void(asio::error_code))
		async_connect(
			ConnectToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::udp_socket::executor_type))
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
				asio::post(socket.get_executor(), [this]() mutable
				{
					error_code ec{};
					socket.shutdown(socket_base::shutdown_both, ec);
					socket.close(ec);
					if (socks5_socket)
					{
						socks5_socket->shutdown(socket_base::shutdown_both, ec);
						socks5_socket->close(ec);
						socks5_socket.reset();
					}
				});
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
		 * @brief Start an asynchronous receive on a connected socket. Remove the socks5 head if exists.
		 * @param buffers - One or more buffers into which the data will be received.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::string_view data);
		 */
		template<typename MutableBufferSequence,
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::string_view)) ReadToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::udp_socket::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, std::string_view))
		inline async_receive(const MutableBufferSequence& buffers, ReadToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::udp_socket::executor_type))
		{
			return asio::async_receive(socket, buffers, socks5_socket.has_value(), std::forward<ReadToken>(token));
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename Data,
			ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, std::size_t)) WriteToken
			ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::udp_socket::executor_type)>
		ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken, void(asio::error_code, std::size_t))
		inline async_send(Data&& data, WriteToken&& token
			ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::udp_socket::executor_type))
		{
			ip::udp::endpoint target_endpoint{};

			if (socks5_socket)
			{
				error_code ec{};
				target_endpoint.address(socket.remote_endpoint(ec).address());
				target_endpoint.port(option.server_port);
			}

			return asio::async_send(socket, std::forward<Data>(data), target_endpoint, std::forward<WriteToken>(token));
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
		udp_client_option option;

		asio::udp_socket  socket;

		std::atomic_flag  aborted{};

		std::optional<tcp_socket> socks5_socket;
	};
}
