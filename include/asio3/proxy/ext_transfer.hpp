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
#include <asio3/tcp/core.hpp>
#include <asio3/udp/core.hpp>
#include <asio3/udp/read.hpp>
#include <asio3/udp/write.hpp>
#include <asio3/udp/send.hpp>
#include <asio3/proxy/parser.hpp>
#include <asio3/proxy/match_condition.hpp>
#include <asio3/proxy/udp_header.hpp>

namespace asio::socks5::detail
{
	struct async_ext_transfer_op
	{
		auto operator()(auto state, auto from_ref, auto to_ref, auto dynamic_buf) -> void
		{
			auto& from = from_ref.get();
			auto& to = to_ref.get();

			co_await asio::dispatch(from.get_executor(), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			// recvd data from the front client by tcp, forward the data to back client.
			auto [e1, n1] = co_await asio::async_read_until(from, dynamic_buf, socks5::udp_match_condition);
			if (e1)
				co_return{ e1, n1 };

			std::string_view data{ dynamic_buf.data(), n1 };

			// this packet is a extension protocol base of below:
			// +----+------+------+----------+----------+----------+
			// |RSV | FRAG | ATYP | DST.ADDR | DST.PORT |   DATA   |
			// +----+------+------+----------+----------+----------+
			// | 2  |  1   |  1   | Variable |    2     | Variable |
			// +----+------+------+----------+----------+----------+
			// the RSV field is the real data length of the field DATA.
			// so we need unpacket this data, and send the real data to the back client.
			auto [err, ep, domain, real_data] = socks5::parse_udp_packet(data, true);
			if (err == 0)
			{
				if (domain.empty())
				{
					co_await asio::async_send_to(to, asio::buffer(real_data), ep);
				}
				else
				{
					co_await asio::async_send_to(to, asio::buffer(real_data), std::move(domain), ep.port());
				}
			}
			else
			{
				co_return{ err, n1 };
			}

			dynamic_buf.consume(n1);

			co_return{ error_code{}, n1 };
		}
	};
}

namespace asio::socks5
{
	/**
	 * @brief Start an asynchronous operation to transfer data between front and back.
	 * @param from - The front tcp client.
	 * @param to - The back tcp client.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, std::size_t);
	 */
	template<
		typename TcpAsyncStream,
		typename UdpAsyncStream,
		typename TransferToken = asio::default_token_type<TcpAsyncStream>>
	requires (is_basic_stream_socket<TcpAsyncStream> && is_basic_datagram_socket<UdpAsyncStream>)
	inline auto async_ext_transfer(
		TcpAsyncStream& from,
		UdpAsyncStream& to,
		auto&& dynamic_buf,
		TransferToken&& token = asio::default_token_type<TcpAsyncStream>())
	{
		return async_initiate<TransferToken, void(asio::error_code, std::size_t)>(
			experimental::co_composed<void(asio::error_code, std::size_t)>(
				detail::async_ext_transfer_op{}, from),
			token,
			std::ref(from), std::ref(to),
			std::forward_like<decltype(dynamic_buf)>(dynamic_buf));
	}
}
