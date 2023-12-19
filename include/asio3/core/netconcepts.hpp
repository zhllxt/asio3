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

#include <concepts>

#include <asio3/core/asio.hpp>
#include <asio3/core/stdconcepts.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_tcp_socket = std::is_same_v<typename U::lowest_layer_type::protocol_type, asio::ip::tcp>;

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_udp_socket = std::is_same_v<typename U::lowest_layer_type::protocol_type, asio::ip::udp>;

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_basic_socket_acceptor = is_template_instance_of<asio::basic_socket_acceptor, U>;

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_basic_stream_socket = is_template_instance_of<asio::basic_stream_socket, U>;

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept is_basic_datagram_socket = is_template_instance_of<asio::basic_datagram_socket, U>;

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept convertible_to_const_buffer_sequence_adapter = requires(U & a)
	{
		{ asio::detail::buffer_sequence_adapter<asio::const_buffer, U>{a}.count() } -> std::integral;
		{ asio::const_buffer(*asio::buffer_sequence_begin(a)).size() } -> std::integral;
		{ asio::const_buffer(*asio::buffer_sequence_end(a)).size() } -> std::integral;
	};

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept convertible_to_mutable_buffer_sequence_adapter = requires(U & a)
	{
		{ asio::detail::buffer_sequence_adapter<asio::mutable_buffer, U>{a}.count() } -> std::integral;
		{ asio::mutable_buffer(*asio::buffer_sequence_begin(a)).size() } -> std::integral;
		{ asio::mutable_buffer(*asio::buffer_sequence_end(a)).size() } -> std::integral;
	};

	template<typename T, typename U = std::remove_cvref_t<T>>
	concept convertible_to_buffer_sequence_adapter =
		convertible_to_const_buffer_sequence_adapter<U> ||
		convertible_to_mutable_buffer_sequence_adapter<U>;
}
