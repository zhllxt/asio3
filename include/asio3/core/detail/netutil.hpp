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

#include <bit>

#include <asio3/core/detail/type_traits.hpp>
#include <asio3/core/asio.hpp>

namespace asio::detail
{
	// /bho/beast/websocket/stream_base.hpp line 147
	// opt.handshake_timeout = std::chrono::seconds(30);

	// When there are a lot of connections, there will maybe a lot of COSE_WAIT,LAST_ACK,TIME_WAIT
	// and other problems, resulting in the client being unable to connect to the server normally.
	// Increasing the connect,handshake,shutdown timeout can effectively alleviate this problem.

	static long constexpr  tcp_handshake_timeout = 30 * 1000;
	static long constexpr  udp_handshake_timeout = 30 * 1000;
	static long constexpr http_handshake_timeout = 30 * 1000;

	static long constexpr  tcp_connect_timeout   = 30 * 1000;
	static long constexpr  udp_connect_timeout   = 30 * 1000;
	static long constexpr http_connect_timeout   = 30 * 1000;

	static long constexpr  tcp_silence_timeout   = 60 * 60 * 1000;
	static long constexpr  udp_silence_timeout   = 60 * 1000;
	static long constexpr http_silence_timeout   = 85 * 1000;
	static long constexpr mqtt_silence_timeout   = 90 * 1000; // 60 * 1.5

	static long constexpr http_execute_timeout   = 15 * 1000;
	static long constexpr icmp_execute_timeout   =  4 * 1000;

	static long constexpr ssl_shutdown_timeout   = 30 * 1000;
	static long constexpr  ws_shutdown_timeout   = 30 * 1000;

	static long constexpr ssl_handshake_timeout  = 30 * 1000;
	static long constexpr  ws_handshake_timeout  = 30 * 1000;

	/*
	 * The read buffer has to be at least as large
	 * as the largest possible control frame including
	 * the frame header.
	 * refrenced from beast stream.hpp
	 */
	// udp MTU : https://zhuanlan.zhihu.com/p/301276548
	static std::size_t constexpr  tcp_frame_size = 1536;
	static std::size_t constexpr  udp_frame_size = 1024;
	static std::size_t constexpr http_frame_size = 1536;

	static std::size_t constexpr max_buffer_size = (std::numeric_limits<std::size_t>::max)();

	// std::thread::hardware_concurrency() is not constexpr, so use it with function form
	// @see: asio::detail::default_thread_pool_size()
	template<typename = void>
	inline std::size_t default_concurrency() noexcept
	{
		std::size_t num_threads = std::thread::hardware_concurrency() * 2;
		num_threads = num_threads == 0 ? 2 : num_threads;
		return num_threads;
	}
}

namespace asio::detail
{
	/**
	 * Swaps the order of bytes for some chunk of memory
	 * @param data - The data as a uint8_t pointer
	 * @tparam DataSize - The true size of the data
	 */
	template <std::size_t DataSize>
	inline void swap_bytes(std::uint8_t* data) noexcept
	{
		for (std::size_t i = 0, end = DataSize / 2; i < end; ++i)
			std::swap(data[i], data[DataSize - i - 1]);
	}

	/**
	 * Swaps the order of bytes for some chunk of memory
	 * @param v - The variable reference.
	 */
	template <class T>
	inline void swap_bytes(T& v) noexcept
	{
		std::uint8_t* p = reinterpret_cast<std::uint8_t*>(std::addressof(v));
		swap_bytes<sizeof(T)>(p);
	}

	/**
	 * converts the value from host to TCP/IP network byte order (which is big-endian).
	 * @param v - The variable reference.
	 */
	template <class T>
	inline T host_to_network(T v) noexcept
	{
		if constexpr (std::endian::native == std::endian::little)
		{
			std::uint8_t* p = reinterpret_cast<std::uint8_t*>(std::addressof(v));
			swap_bytes<sizeof(T)>(p);
		}

		return v;
	}

	/**
	 * converts the value from TCP/IP network order to host byte order (which is little-endian on Intel processors).
	 * @param v - The variable reference.
	 */
	template <class T>
	inline T network_to_host(T v) noexcept
	{
		if constexpr (std::endian::native == std::endian::little)
		{
			std::uint8_t* p = reinterpret_cast<std::uint8_t*>(std::addressof(v));
			swap_bytes<sizeof(T)>(p);
		}

		return v;
	}

	template<class T, class Pointer>
	inline void write(Pointer& p, T v) noexcept
	{
		if constexpr (int(sizeof(T)) > 1)
		{
			// MSDN: The htons function converts a u_short from host to TCP/IP network byte order (which is big-endian).
			// ** This mean the network byte order is big-endian **
			if constexpr (std::endian::native == std::endian::little)
			{
				swap_bytes<sizeof(T)>(reinterpret_cast<std::uint8_t *>(std::addressof(v)));
			}

			std::memcpy((void*)p, (const void*)&v, sizeof(T));
		}
		else
		{
			static_assert(sizeof(T) == std::size_t(1));

			*p = std::decay_t<std::remove_pointer_t<std::remove_cvref_t<Pointer>>>(v);
		}

		p += sizeof(T);
	}

	template<class T, class Pointer>
	inline T read(Pointer& p) noexcept
	{
		T v{};

		if constexpr (int(sizeof(T)) > 1)
		{
			std::memcpy((void*)&v, (const void*)p, sizeof(T));

			// MSDN: The htons function converts a u_short from host to TCP/IP network byte order (which is big-endian).
			// ** This mean the network byte order is big-endian **
			if constexpr (std::endian::native == std::endian::little)
			{
				swap_bytes<sizeof(T)>(reinterpret_cast<std::uint8_t *>(std::addressof(v)));
			}
		}
		else
		{
			static_assert(sizeof(T) == std::size_t(1));

			v = T(*p);
		}

		p += sizeof(T);

		return v;
	}
}

namespace asio
{
	enum class protocol : std::uint8_t
	{
		unknown = 0,

		udp = 1,
		kcp,

		tcp,
		http,
		websocket,
		rpc,
		mqtt,

		tcps,
		https,
		websockets,
		rpcs,
		mqtts,

		icmp,
		serialport,

		ws = websocket,
		wss = websockets
	};
}
