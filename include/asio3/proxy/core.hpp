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

#include <any>

#include <asio3/core/asio.hpp>
#include <asio3/core/fixed_capacity_vector.hpp>
#include <asio3/core/move_only_any.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/tcp/core.hpp>
#include <asio3/udp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::socks5
#else
namespace boost::asio::socks5
#endif
{
	enum class connect_result : std::uint8_t
	{
		succeeded = 0,
		general_socks_server_failure,
		connection_not_allowed_by_ruleset,
		network_unreachable,
		host_unreachable,
		connection_refused,
		ttl_expired,
		command_not_supported,
		address_type_not_supported,
	};

	enum class address_type : std::uint8_t
	{
		unknown = 0,
		ipv4    = 1,
		domain  = 3,
		ipv6    = 4,
	};

	enum class auth_method : std::uint8_t
	{
		anonymous         = 0x00, // X'00' NO AUTHENTICATION REQUIRED
		gssapi            = 0x01, // X'01' GSSAPI
		password          = 0x02, // X'02' USERNAME/PASSWORD
		//iana              = 0x03, // X'03' to X'7F' IANA ASSIGNED
		//reserved          = 0x80, // X'80' to X'FE' RESERVED FOR PRIVATE METHODS
		noacceptable      = 0xFF, // X'FF' NO ACCEPTABLE METHODS
	};

	enum class command : std::uint8_t
	{
		connect       = 0x01, // CONNECT X'01'
		bind          = 0x02, // BIND X'02'
		udp_associate = 0x03, // UDP ASSOCIATE X'03'
	};

	using auth_method_vector = std::experimental::fixed_capacity_vector<auth_method, 8>;

	struct option
	{
		// input params
		std::string        proxy_address{};
		std::uint16_t      proxy_port{};

		auth_method_vector method{};

		std::string        username{};
		std::string        password{};

		std::string        dest_address{};
		std::uint16_t      dest_port{};

		command            cmd{};

		// output params
		std::string        bound_address{};
		std::uint16_t      bound_port{};
	};

	struct handshake_info
	{
		std::uint16_t      dest_port{};
		std::string        dest_address{};

		std::string        username{};
		std::string        password{};

		auth_method_vector method{};

		command            cmd{};

		address_type       addr_type{};

		asio::ip::tcp::endpoint client_endpoint{};

		// connect_bound_socket_type or udp_associate_bound_socket_type
		std::move_only_any bound_socket{};
	};

	struct auth_config
	{
		// you can declare a custom struct that derive from this, and redefine these two 
		// bound socket type to let the async_accept function create a custom bound socket.
		using connect_bound_socket_type = asio::tcp_socket;
		using udp_associate_bound_socket_type = asio::udp_socket;

		auth_method_vector supported_method{};

		std::function<asio::awaitable<bool>(handshake_info&)> on_auth{};
	};

	namespace
	{
		inline bool is_command_valid(command cmd) noexcept
		{
			return cmd == command::connect || cmd == command::bind || cmd == command::udp_associate;
		}

		inline bool is_method_valid(auth_method m) noexcept
		{
			return m == auth_method::anonymous || m == auth_method::gssapi || m == auth_method::password;
		}

		inline bool is_methods_valid(auth_method_vector& ms) noexcept
		{
			if (ms.empty())
				return false;
			for (auth_method m : ms)
			{
				if (!is_method_valid(m))
					return false;
			}
			return true;
		}

		inline bool is_option_valid(socks5::option& opt) noexcept
		{
			if (opt.proxy_address.empty() || opt.proxy_port == 0 || opt.method.empty())
				return false;
			if (!is_command_valid(opt.cmd))
				return false;
			//if (opt.cmd == command::udp_associate && opt.dest_port == 0)
			//	return false;
			return true;
		}

		inline bool is_data_come_from_frontend(auto& tcp_sock, auto& sender_endpoint, auto& handsk_info)
		{
			asio::error_code ec{};
			asio::ip::address front_addr = tcp_sock.remote_endpoint(ec).address();
			if (ec)
				return false;

			if (front_addr.is_loopback())
				return sender_endpoint.address() == front_addr && sender_endpoint.port() == handsk_info.dest_port;
			else
				return sender_endpoint.address() == front_addr;
		}
	}
}

#ifdef ASIO_STANDALONE
namespace socks5 = ::asio::socks5;
#else
namespace socks5 = boost::asio::socks5;
#endif
