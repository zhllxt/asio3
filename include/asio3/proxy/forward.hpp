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

#include <asio3/core/asio.hpp>
#include <asio3/core/netconcepts.hpp>
#include <asio3/core/with_lock.hpp>
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/udp/core.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/proxy/parser.hpp>
#include <asio3/proxy/match_condition.hpp>
#include <asio3/proxy/udp_header.hpp>

#ifdef ASIO_STANDALONE
namespace asio::socks5::detail
#else
namespace boost::asio::socks5::detail
#endif
{
	struct async_forward_data_to_backend_op
	{
		auto operator()(auto state, auto bound_ref, auto&& buf) -> void
		{
			auto& bound = bound_ref.get();
			auto  buffer = std::forward_like<decltype(buf)>(buf);

			co_await asio::dispatch(asio::use_deferred_executor(bound));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [err, ep, domain, data] = socks5::parse_udp_packet(buffer, false);
			if (err == 0)
			{
				if (domain.empty())
				{
					auto [e1, n1] = co_await bound.async_send_to(
						asio::buffer(data), ep, asio::use_deferred_executor(bound));
					co_return{ e1, n1 };
				}
				else
				{
					auto [e1, n1] = co_await asio::async_send_to(
						bound, asio::buffer(data), std::move(domain), ep.port(), asio::use_deferred_executor(bound));
					co_return{ e1, n1 };
				}
			}
			else
			{
				co_return{ asio::error::no_data, 0 };
			}
		}
	};

	struct async_forward_data_to_tcp_frontend_op
	{
		asio::ip::udp::endpoint sender_endpoint;

		auto operator()(auto state, auto front_ref, auto&& buf) -> void
		{
			auto& front = front_ref.get();
			auto  buffer = std::forward_like<decltype(buf)>(buf);

			co_await asio::dispatch(asio::use_deferred_executor(front));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), buffer.size());

			std::array<asio::const_buffer, 2> buffers
			{
				asio::buffer(head),
				asio::const_buffer{buffer},
			};

			auto [e1, n1] = co_await asio::async_write(front, buffers, asio::use_deferred_executor(front));
			co_return{ e1, n1 };
		}
	};

	struct async_forward_data_to_udp_frontend_op
	{
		asio::ip::udp::endpoint sender_endpoint;
		asio::ip::udp::endpoint frontend_endpoint;

		auto operator()(auto state, auto bound_ref, auto&& buf) -> void
		{
			auto& bound = bound_ref.get();
			auto  buffer = std::forward_like<decltype(buf)>(buf);

			co_await asio::dispatch(asio::use_deferred_executor(bound));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), 0);

			std::array<asio::const_buffer, 2> buffers
			{
				asio::buffer(head),
				asio::const_buffer{buffer},
			};

			auto [e1, n1] = co_await bound.async_send_to(
				buffers, frontend_endpoint, asio::use_deferred_executor(bound));
			co_return{ e1, n1 };
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio::socks5
#else
namespace boost::asio::socks5
#endif
{
	/**
	 * @brief Start an asynchronous operation to transfer data between front and back.
	 * @param bound - The bound udp socket.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t);
	 */
	template<
		typename UdpAsyncStream,
		typename TransferToken = asio::default_token_type<UdpAsyncStream>>
	requires (is_udp_socket<UdpAsyncStream>)
	inline auto async_forward_data_to_backend(
		UdpAsyncStream& bound,
		auto&& buffer,
		TransferToken&& token = asio::default_token_type<UdpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				socks5::detail::async_forward_data_to_backend_op{}, bound),
			token,
			std::ref(bound),
			std::forward_like<decltype(buffer)>(buffer));
	}

	/**
	 * @brief Start an asynchronous operation to transfer data between front and back.
	 * @param front - The front tcp client.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t);
	 */
	template<
		typename TcpAsyncStream,
		typename TransferToken = asio::default_token_type<TcpAsyncStream>>
	requires (is_tcp_socket<TcpAsyncStream>)
	inline auto async_forward_data_to_frontend(
		TcpAsyncStream& front,
		auto&& buffer,
		asio::ip::udp::endpoint sender_endpoint,
		TransferToken&& token = asio::default_token_type<TcpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				socks5::detail::async_forward_data_to_tcp_frontend_op{
					std::move(sender_endpoint) }, front),
			token,
			std::ref(front),
			std::forward_like<decltype(buffer)>(buffer));
	}

	/**
	 * @brief Start an asynchronous operation to transfer data between front and back.
	 * @param bound - The bound udp socket.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t);
	 */
	template<
		typename UdpAsyncStream,
		typename TransferToken = asio::default_token_type<UdpAsyncStream>>
	requires (is_udp_socket<UdpAsyncStream>)
	inline auto async_forward_data_to_frontend(
		UdpAsyncStream& bound,
		auto&& buffer,
		asio::ip::udp::endpoint sender_endpoint,
		asio::ip::udp::endpoint frontend_endpoint,
		TransferToken&& token = asio::default_token_type<UdpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				socks5::detail::async_forward_data_to_udp_frontend_op{
					std::move(sender_endpoint), std::move(frontend_endpoint) }, bound),
			token,
			std::ref(bound),
			std::forward_like<decltype(buffer)>(buffer));
	}
}
