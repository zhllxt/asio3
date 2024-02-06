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

#include <asio3/tcp/tcp_server.hpp>
#include <asio3/rpc/rpc_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<typename SessionT = rpc_session>
	class basic_rpc_server : public basic_tcp_server<SessionT>
	{
	public:
		using super = basic_tcp_server<SessionT>;
		using socket_type = typename SessionT::socket_type;

		explicit basic_rpc_server(const auto& ex) : super(ex)
		{
		}

		basic_rpc_server(basic_rpc_server&&) noexcept = default;
		basic_rpc_server& operator=(basic_rpc_server&&) noexcept = default;

		~basic_rpc_server()
		{
		}

		inline super& base() noexcept
		{
			return static_cast<super&>(*this);
		}

	public:
		rpc::invoker                      invoker{};
	};

	using rpc_server = basic_rpc_server<rpc_session>;
}

#include <asio3/core/detail/pop_options.hpp>

#endif
