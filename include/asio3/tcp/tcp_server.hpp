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
#include <asio3/tcp/open.hpp>
#include <asio3/tcp/tcp_connection.hpp>

namespace asio
{
	class tcp_server
	{
	public:
//#include <asio3/tcp/impl/tcp_server.ipp>

	public:
		explicit tcp_server(const auto& ex) : acceptor(ex)
		{
		}

		~tcp_server()
		{
			async_stop([](auto...) {});
		}

		/**
		 * @brief Asynchronously start the server.
		 */
		template<typename StartToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_start(
			is_string auto&& listen_address,
			is_string_or_integral auto&& listen_port,
			bool reuse_addr = true,
			StartToken&& token = asio::default_token<asio::tcp_acceptor>())
		{
			return asio::async_open(
				acceptor,
				std::forward_like<decltype(listen_address)>(listen_address),
				std::forward_like<decltype(listen_port)>(listen_port),
				reuse_addr,
				std::forward<StartToken>(token));
		}

		/**
		 * @brief Asynchronously stop the server.
		 */
		template<typename StopToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_stop(StopToken&& token = asio::default_token<asio::tcp_acceptor>())
		{
			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, tcp_acceptor& acceptor) -> void
					{
						state.reset_cancellation_state(asio::enable_terminal_cancellation());

						error_code ec{};
						acceptor.close(ec);

						// disconnect all connections...

						co_return ec;
					}, acceptor),
				token, std::ref(acceptor));
		}

		/**
		 * @brief Check whether the acceptor is stopped or not.
		 */
		inline bool is_stopped() const noexcept
		{
			return !acceptor.is_open();
		}

		///**
		// * @brief Safety start an asynchronous operation to write all of the supplied data to all clients.
		// * @param data - The written data.
		// * @param token - The completion handler to invoke when the operation completes.
		// *	  The equivalent function signature of the handler must be:
		// *    @code
		// *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		// */
		//template<typename Data, typename WriteToken = default_tcp_write_token>
		//inline auto async_send(Data&& data, WriteToken&& token = default_tcp_write_token{})
		//{
		//	return async_initiate<WriteToken, void(asio::error_code, std::size_t)>(
		//		experimental::co_composed<void(asio::error_code, std::size_t)>(
		//			batch_async_send_op{}, acceptor),
		//		token, std::ref(*this), detail::data_persist(std::forward<Data>(data)));
		//}

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
		asio::tcp_acceptor  acceptor;
	};
}
