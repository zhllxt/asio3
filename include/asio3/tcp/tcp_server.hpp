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
#include <asio3/core/alive.hpp>
#include <asio3/tcp/listen.hpp>
#include <asio3/tcp/tcp_connection.hpp>

namespace asio
{
	struct tcp_listen_option
	{
		std::string           listen_address{};
		std::uint16_t         listen_port{};

		// reuse address flag for the tcp acceptor 
		bool                  reuse_address{ true };
	};

	template<typename ConnectionT>
	class tcp_server_t
	{
	public:
		using connection_type = ConnectionT;

	public:
		explicit tcp_server_t(const auto& ex) : acceptor(ex), connection_map(ex)
		{
		}

		~tcp_server_t()
		{
		}

		/**
		 * @brief Asynchronously start the server for listen at the address and port.
		 */
		template<typename StartToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_listen(
			this auto&& self,
			tcp_listen_option opt,
			StartToken&& token = asio::default_token_type<asio::tcp_acceptor>())
		{
			return asio::async_listen(
				self.get_acceptor(),
				std::move(opt.listen_address),
				opt.listen_port,
				opt.reuse_address,
				std::forward<StartToken>(token));
		}

		/**
		 * @brief Asynchronously stop the server.
		 */
		template<typename StopToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_stop(
			this auto&& self,
			StopToken&& token = asio::default_token_type<asio::tcp_acceptor>())
		{
			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						state.reset_cancellation_state(asio::enable_terminal_cancellation());

						co_await asio::dispatch(self.get_acceptor().get_executor(), use_nothrow_deferred);

						error_code ec{};
						self.get_acceptor().close(ec);

						co_await self.get_connection_map().async_disconnect_all(use_nothrow_deferred);

						co_return ec;
					}, self.get_acceptor()),
				token, std::ref(self));
		}

		/**
		 * @brief Check whether the acceptor is stopped or not.
		 */
		inline bool is_stopped(this auto&& self) noexcept
		{
			return !self.get_acceptor().is_open();
		}

		/**
		 * @brief Safety start an asynchronous operation to write all of the supplied data to all clients.
		 * @param data - The written data.
		 * @param token - The completion handler to invoke when the operation completes.
		 *	  The equivalent function signature of the handler must be:
		 *    @code
		 *    void handler(const asio::error_code& ec, std::size_t sent_bytes);
		 */
		template<typename WriteToken = asio::default_token_type<asio::tcp_socket>>
		inline auto async_send(
			this auto&& self,
			auto&& data,
			WriteToken&& token = asio::default_token_type<asio::tcp_socket>())
		{
			return self.get_connection_map().async_send_all(
				std::forward_like<decltype(data)>(data),
				std::forward<WriteToken>(token));
		}

		/**
		 * @brief Get the executor associated with the object.
		 */
		inline const auto& get_executor(this auto&& self) noexcept
		{
			return self.get_acceptor().get_executor();
		}

		/**
		 * @brief Get the listen address.
		 */
		inline std::string get_listen_address(this auto&& self) noexcept
		{
			error_code ec{};
			return self.get_acceptor().local_endpoint(ec).address().to_string(ec);
		}

		/**
		 * @brief Get the listen port number.
		 */
		inline ip::port_type get_listen_port(this auto&& self) noexcept
		{
			error_code ec{};
			return self.get_acceptor().local_endpoint(ec).port();
		}

		/**
		 * @brief Get the acceptor.
		 * https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/
		 */
		constexpr inline auto&& get_acceptor(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).acceptor;
		}

		/**
		 * @brief Get the connection map.
		 */
		constexpr inline auto&& get_connection_map(this auto&& self)
		{
			return std::forward_like<decltype(self)>(self).connection_map;
		}

	public:
		asio::tcp_acceptor  acceptor;

		connection_map_t<connection_type> connection_map;
	};

	using tcp_server = tcp_server_t<tcp_connection>;
}
