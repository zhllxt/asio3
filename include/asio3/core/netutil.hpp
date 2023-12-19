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

#include <asio3/core/netconcepts.hpp>

#if (defined(ASIO_NO_EXCEPTIONS) || defined(BOOST_ASIO_NO_EXCEPTIONS)) && !defined(ASIO3_NO_EXCEPTIONS_IMPL)
#include <cassert>
#include <iostream>
#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	template <typename Exception>
	void throw_exception(const Exception& e ASIO_SOURCE_LOCATION_PARAM)
	{
		::std::cerr << "exception occured: " << e.what() << ::std::endl;
		assert(false);
		::std::terminate();
	}
}
#endif

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	// /bho/beast/websocket/stream_base.hpp line 147
	// opt.handshake_timeout = ::std::chrono::seconds(30);

	// When there are a lot of connections, there will maybe a lot of COSE_WAIT,LAST_ACK,TIME_WAIT
	// and other problems, resulting in the client being unable to connect to the server normally.
	// Increasing the connect,handshake,shutdown timeout can effectively alleviate this problem.

	constexpr ::std::chrono::milliseconds  tcp_handshake_timeout  = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds  udp_handshake_timeout  = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds http_handshake_timeout  = ::std::chrono::milliseconds(30 * 1000);

	constexpr ::std::chrono::milliseconds  tcp_connect_timeout    = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds  udp_connect_timeout    = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds http_connect_timeout    = ::std::chrono::milliseconds(30 * 1000);

	constexpr ::std::chrono::milliseconds  tcp_disconnect_timeout = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds  udp_disconnect_timeout = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds http_disconnect_timeout = ::std::chrono::milliseconds(3  * 1000);

	constexpr ::std::chrono::milliseconds  tcp_idle_timeout       = ::std::chrono::milliseconds(60 * 60 * 1000);
	constexpr ::std::chrono::milliseconds  udp_idle_timeout       = ::std::chrono::milliseconds(60 * 1000);
	constexpr ::std::chrono::milliseconds http_idle_timeout       = ::std::chrono::milliseconds(85 * 1000);
	constexpr ::std::chrono::milliseconds mqtt_idle_timeout       = ::std::chrono::milliseconds(90 * 1000); // 60 * 1.5
	constexpr ::std::chrono::milliseconds proxy_idle_timeout      = ::std::chrono::milliseconds(5 * 60 * 1000);

	constexpr ::std::chrono::milliseconds http_request_timeout    = ::std::chrono::milliseconds(15 * 1000);
	constexpr ::std::chrono::milliseconds icmp_request_timeout    = ::std::chrono::milliseconds( 4 * 1000);

	constexpr ::std::chrono::milliseconds ssl_shutdown_timeout    = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds  ws_shutdown_timeout    = ::std::chrono::milliseconds(30 * 1000);

	constexpr ::std::chrono::milliseconds ssl_handshake_timeout   = ::std::chrono::milliseconds(30 * 1000);
	constexpr ::std::chrono::milliseconds  ws_handshake_timeout   = ::std::chrono::milliseconds(30 * 1000);

	/*
	 * The read buffer has to be at least as large
	 * as the largest possible control frame including
	 * the frame header.
	 * refrenced from beast stream.hpp
	 */
	// udp MTU : https://zhuanlan.zhihu.com/p/301276548
	constexpr ::std::size_t  tcp_frame_size = 1480;
	constexpr ::std::size_t  udp_frame_size = 548; // LAN:1472 WAN:548
	constexpr ::std::size_t http_frame_size = 1480;
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template <typename... Ts>
	inline constexpr void ignore_unused(Ts const& ...) noexcept {}

	template <typename... Ts>
	inline constexpr void ignore_unused() noexcept {}

	// ::std::thread::hardware_concurrency() is not constexpr, so use it with function form
	// @see: asio::detail::default_thread_pool_size()
	template<typename = void>
	inline ::std::size_t default_concurrency() noexcept
	{
		::std::size_t num_threads = ::std::thread::hardware_concurrency() * 2;
		num_threads = num_threads == 0 ? 2 : num_threads;
		return num_threads;
	}

	template<typename = void>
	inline ::std::string to_string(const asio::const_buffer& v) noexcept
	{
		return ::std::string{ (::std::string::pointer)(v.data()), v.size() };
	}

	template<typename = void>
	inline ::std::string to_string(const asio::mutable_buffer& v) noexcept
	{
		return ::std::string{ (::std::string::pointer)(v.data()), v.size() };
	}

#if !defined(ASIO_NO_DEPRECATED) && !defined(BOOST_ASIO_NO_DEPRECATED)
	template<typename = void>
	inline ::std::string to_string(const asio::const_buffers_1& v) noexcept
	{
		return ::std::string{ (::std::string::pointer)(v.data()), v.size() };
	}

	template<typename = void>
	inline ::std::string to_string(const asio::mutable_buffers_1& v) noexcept
	{
		return ::std::string{ (::std::string::pointer)(v.data()), v.size() };
	}
#endif

	template<typename = void>
	inline ::std::string_view to_string_view(const asio::const_buffer& v) noexcept
	{
		return ::std::string_view{ (::std::string_view::const_pointer)(v.data()), v.size() };
	}

	template<typename = void>
	inline ::std::string_view to_string_view(const asio::mutable_buffer& v) noexcept
	{
		return ::std::string_view{ (::std::string_view::const_pointer)(v.data()), v.size() };
	}

#if !defined(ASIO_NO_DEPRECATED) && !defined(BOOST_ASIO_NO_DEPRECATED)
	template<typename = void>
	inline ::std::string_view to_string_view(const asio::const_buffers_1& v) noexcept
	{
		return ::std::string_view{ (::std::string_view::const_pointer)(v.data()), v.size() };
	}

	template<typename = void>
	inline ::std::string_view to_string_view(const asio::mutable_buffers_1& v) noexcept
	{
		return ::std::string_view{ (::std::string_view::const_pointer)(v.data()), v.size() };
	}
#endif

	auto to_buffer(auto&& data)
	{
		if constexpr (convertible_to_buffer_sequence_adapter<::std::remove_cvref_t<decltype(data)>>)
		{
			return ::std::forward_like<decltype(data)>(data);
		}
		else
		{
			return asio::buffer(data);
		}
	}

	/**
	 * @brief Get the local address.
	 */
	inline ::std::string get_local_address(auto& sock) noexcept
	{
		error_code ec{};
		return sock.local_endpoint(ec).address().to_string(ec);
	}

	/**
	 * @brief Get the local port number.
	 */
	inline ip::port_type get_local_port(auto& sock) noexcept
	{
		error_code ec{};
		return sock.local_endpoint(ec).port();
	}

	/**
	 * @brief Get the remote address.
	 */
	inline ::std::string get_remote_address(auto& sock) noexcept
	{
		error_code ec{};
		return sock.remote_endpoint(ec).address().to_string(ec);
	}

	/**
	 * @brief Get the remote port number.
	 */
	inline ip::port_type get_remote_port(auto& sock) noexcept
	{
		error_code ec{};
		return sock.remote_endpoint(ec).port();
	}

	/**
	 * Swaps the order of bytes for some chunk of memory
	 * @param data - The data as a uint8_t pointer
	 * @tparam DataSize - The true size of the data
	 */
	template <::std::size_t DataSize>
	inline void swap_bytes(::std::uint8_t* data) noexcept
	{
		for (::std::size_t i = 0, end = DataSize / 2; i < end; ++i)
			::std::swap(data[i], data[DataSize - i - 1]);
	}

	/**
	 * Swaps the order of bytes for some chunk of memory
	 * @param v - The variable reference.
	 */
	template <class T>
	inline void swap_bytes(T& v) noexcept
	{
		::std::uint8_t* p = reinterpret_cast<::std::uint8_t*>(::std::addressof(v));
		swap_bytes<sizeof(T)>(p);
	}

	/**
	 * converts the value from host to TCP/IP network byte order (which is big-endian).
	 * @param v - The variable reference.
	 */
	template <class T>
	inline T host_to_network(T v) noexcept
	{
		if constexpr (::std::endian::native == ::std::endian::little)
		{
			::std::uint8_t* p = reinterpret_cast<::std::uint8_t*>(::std::addressof(v));
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
		if constexpr (::std::endian::native == ::std::endian::little)
		{
			::std::uint8_t* p = reinterpret_cast<::std::uint8_t*>(::std::addressof(v));
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
			if constexpr (::std::endian::native == ::std::endian::little)
			{
				swap_bytes<sizeof(T)>(reinterpret_cast<::std::uint8_t *>(::std::addressof(v)));
			}

			::std::memcpy((void*)p, (const void*)&v, sizeof(T));
		}
		else
		{
			static_assert(sizeof(T) == ::std::size_t(1));

			*p = ::std::decay_t<::std::remove_pointer_t<::std::remove_cvref_t<Pointer>>>(v);
		}

		p += sizeof(T);
	}

	template<class T, class Pointer>
	inline T read(Pointer& p) noexcept
	{
		T v{};

		if constexpr (int(sizeof(T)) > 1)
		{
			::std::memcpy((void*)&v, (const void*)p, sizeof(T));

			// MSDN: The htons function converts a u_short from host to TCP/IP network byte order (which is big-endian).
			// ** This mean the network byte order is big-endian **
			if constexpr (::std::endian::native == ::std::endian::little)
			{
				swap_bytes<sizeof(T)>(reinterpret_cast<::std::uint8_t *>(::std::addressof(v)));
			}
		}
		else
		{
			static_assert(sizeof(T) == ::std::size_t(1));

			v = T(*p);
		}

		p += sizeof(T);

		return v;
	}
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	enum class protocol : ::std::uint8_t
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

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	template<class T>
	struct co_await_value_type
	{
		using type = ::std::remove_cvref_t<T>;
	};

	template<>
	struct co_await_value_type<void>
	{
		using type = error_code;
	};

	struct async_call_coroutine_op
	{
		static asio::awaitable<error_code> call_coroutine(auto&& awaiter, auto& ch)
		{
			using awaiter_type = ::std::remove_cvref_t<decltype(awaiter)>;
			using value_type = typename awaiter_type::value_type;

			if constexpr (::std::is_void_v<value_type>)
			{
				co_await ::std::forward_like<decltype(awaiter)>(awaiter);

				auto [ec] = co_await ch.async_send(error_code{}, error_code{}, asio::use_nothrow_awaitable);

				co_return ec;
			}
			else
			{
				auto result = co_await ::std::forward_like<decltype(awaiter)>(awaiter);

				auto [ec] = co_await ch.async_send(error_code{}, ::std::move(result), asio::use_nothrow_awaitable);

				co_return ec;
			}
		}

		auto operator()(auto state, auto&& executor, auto&& awaiter) -> void
		{
			using awaiter_type = ::std::remove_cvref_t<decltype(awaiter)>;
			using value_type = typename awaiter_type::value_type;
			using result_type = typename detail::co_await_value_type<value_type>::type;

			auto ex = ::std::forward_like<decltype(executor)>(executor);
			auto aw = ::std::forward_like<decltype(awaiter)>(awaiter);

			co_await asio::dispatch(ex, asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			experimental::channel<void(error_code, result_type)> ch{ ex, 1 };

			asio::co_spawn(ex, call_coroutine(::std::move(aw), ch), asio::detached);

			auto [ec, result] = co_await ch.async_receive(use_nothrow_deferred);

			co_return result;
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 * @brief 
	 * @param executor - 
	 * @param awaiter - 
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
	 */
	template<typename Awaiter, typename AwaitToken>
	inline auto async_call_coroutine(auto&& executor, Awaiter&& awaiter, AwaitToken&& token)
	{
		using result_type = typename detail::co_await_value_type<typename Awaiter::value_type>::type;
		return async_initiate<AwaitToken, void(result_type)>(
			experimental::co_composed<void(result_type)>(
				detail::async_call_coroutine_op{}, executor),
			token,
			executor,
			::std::forward<Awaiter>(awaiter)
		);
	}
}
