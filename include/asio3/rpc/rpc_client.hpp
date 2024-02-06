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

#if __has_include(<cereal/cereal.hpp>)

#include <asio3/core/detail/push_options.hpp>

#include <asio3/tcp/tcp_client.hpp>
#include <asio3/rpc/invoker.hpp>
#include <asio3/rpc/caller.hpp>
#include <asio3/rpc/id_generator.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SocketT = tcp_socket>
	class basic_rpc_client : public basic_tcp_client<SocketT>, public rpc::async_caller
	{
	public:
		using super = basic_tcp_client<SocketT>;
		using socket_type = SocketT;

		/**
		 * @brief constructor
		 */
		explicit basic_rpc_client(const auto& ex) : super(ex)
		{
		}

		basic_rpc_client(basic_rpc_client&&) noexcept = default;
		basic_rpc_client& operator=(basic_rpc_client&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_rpc_client()
		{
			this->close();
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		rpc::invoker invoker{};
		rpc::serializer serializer;
		rpc::deserializer deserializer;
		rpc::id_generator<rpc::header::id_type> id_generator{};
	};

	using rpc_client = basic_rpc_client<asio::tcp_socket>;
}

#include <asio3/core/detail/pop_options.hpp>

#endif
