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
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/udp/core.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/udp/send.hpp>
#include <asio3/proxy/parser.hpp>
#include <asio3/proxy/match_condition.hpp>
#include <asio3/proxy/udp_header.hpp>

namespace asio::socks5::detail
{
	struct async_forward_to_backend_op
	{
		auto operator()(auto state, auto bound_ref, auto buffer) -> void
		{
			auto& bound = bound_ref.get();

			co_await asio::dispatch(bound.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [err, ep, domain, data] = socks5::parse_udp_packet(buffer, false);
			if (err == 0)
			{
				if (domain.empty())
				{
					auto [e1, n1] = co_await asio::async_send_to(bound, asio::buffer(data), ep);
					co_return{ e1, n1 };
				}
				else
				{
					auto [e1, n1] = co_await asio::async_send_to(bound, asio::buffer(data), std::move(domain), ep.port());
					co_return{ e1, n1 };
				}
			}
			else
			{
				co_return{ asio::error::no_data, 0 };
			}
		}
	};

	struct async_forward_to_tcp_frontend_op
	{
		auto operator()(auto state, auto front_ref, auto buffer, asio::ip::udp::endpoint sender_endpoint) -> void
		{
			auto& front = front_ref.get();

			co_await asio::dispatch(front.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), buffer.size());

			std::array<asio::const_buffer, 2> buffers
			{
				asio::buffer(head),
				asio::const_buffer{buffer},
			};

			auto [e1, n1] = co_await asio::async_write(front, buffers);
			co_return{ e1, n1 };
		}
	};

	struct async_forward_to_udp_frontend_op
	{
		auto operator()(auto state, auto bound_ref, auto buffer,
			asio::ip::udp::endpoint sender_endpoint, asio::ip::udp::endpoint frontend_endpoint) -> void
		{
			auto& bound = bound_ref.get();

			co_await asio::dispatch(bound.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto head = socks5::make_udp_header(sender_endpoint.address(), sender_endpoint.port(), 0);

			std::array<asio::const_buffer, 2> buffers
			{
				asio::buffer(head),
				asio::const_buffer{buffer},
			};

			auto [e1, n1] = co_await asio::async_send_to(bound, buffers, frontend_endpoint);
			co_return{ e1, n1 };
		}
	};
}

namespace asio::socks5
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
	requires (is_basic_datagram_socket<UdpAsyncStream>)
	inline auto async_forward_to_backend(
		UdpAsyncStream& bound,
		auto&& buffer,
		TransferToken&& token = asio::default_token_type<UdpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::async_forward_to_backend_op{}, bound),
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
	requires (is_basic_stream_socket<TcpAsyncStream>)
	inline auto async_forward_to_frontend(
		TcpAsyncStream& front,
		auto&& buffer,
		asio::ip::udp::endpoint sender_endpoint,
		TransferToken&& token = asio::default_token_type<TcpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::async_forward_to_tcp_frontend_op{}, front),
			token,
			std::ref(front),
			std::forward_like<decltype(buffer)>(buffer),
			std::move(sender_endpoint));
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
	requires (is_basic_datagram_socket<UdpAsyncStream>)
	inline auto async_forward_to_frontend(
		UdpAsyncStream& bound,
		auto&& buffer,
		asio::ip::udp::endpoint sender_endpoint,
		asio::ip::udp::endpoint frontend_endpoint,
		TransferToken&& token = asio::default_token_type<UdpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::async_forward_to_udp_frontend_op{}, bound),
			token,
			std::ref(bound),
			std::forward_like<decltype(buffer)>(buffer),
			std::move(sender_endpoint),
			std::move(frontend_endpoint));
	}
}

namespace asio::socks5::detail
{
	struct async_udp_transfer_op
	{
		auto operator()(auto state, auto bound_ref, auto front_ref,
			handshake_info& handsk_info, auto buffer, bool use_udp_channel) -> void
		{
			auto& bound = bound_ref.get();
			auto& front = front_ref.get();

			co_await asio::dispatch(front.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::ip::udp::endpoint sender_endpoint{};

			auto [e1, n1] = co_await asio::async_receive_from(bound, buffer, sender_endpoint);
			if (e1)
				co_return{ e1, 0 };

			auto data = asio::buffer(buffer.data(), n1);

			bool is_from_front = socks5::is_data_come_from_frontend(front, sender_endpoint, handsk_info);

			// recvd data from the frontend client. forward it to the backend endpoint.
			if (is_from_front)
			{
				auto [e2, n2] = co_await async_forward_to_backend(bound, data);
				co_return{ e2, n1 };
			}
			else
			{
				if (use_udp_channel)
				{
					error_code ec{};
					asio::ip::address front_addr = front.remote_endpoint(ec).address();
					if (ec)
						co_return{ ec, n1 };

					auto [e2, n2] = co_await async_forward_to_frontend(bound, data, sender_endpoint,
						asio::ip::udp::endpoint(front_addr, handsk_info.dest_port));
					co_return{ e2, n1 };
				}
				else
				{
					auto [e2, n2] = co_await async_forward_to_frontend(front, data, sender_endpoint);
					co_return{ e2, n1 };
				}
			}
		}
	};
}

namespace asio::socks5
{
	/**
	 * @brief Start an asynchronous operation to transfer data between front and back.
	 * @param bound - The bound udp socket.
	 * @param front - The front tcp client.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t);
	 */
	template<
		typename UdpAsyncStream,
		typename TcpAsyncStream,
		typename TransferToken = asio::default_token_type<TcpAsyncStream>>
	requires (is_basic_stream_socket<TcpAsyncStream> && is_basic_datagram_socket<UdpAsyncStream>)
	inline auto async_udp_transfer(
		UdpAsyncStream& bound,
		TcpAsyncStream& front,
		handshake_info& handsk_info,
		auto&& buffer,
		bool use_udp_channel,
		TransferToken&& token = asio::default_token_type<TcpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::async_udp_transfer_op{}, front),
			token,
			std::ref(bound), std::ref(front), std::ref(handsk_info),
			std::forward_like<decltype(buffer)>(buffer), use_udp_channel);
	}
}
