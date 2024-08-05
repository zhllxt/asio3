/*
 * Copyright (c) 2017-2023 zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * socks5 protocol : https://www.ddhigh.com/2019/08/24/socks5-protocol.html
 * UDP Associate : https://blog.csdn.net/whatday/article/details/40183555
 */

#pragma once

#include <asio3/core/error.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/resolve.hpp>
#include <asio3/core/with_lock.hpp>

#include <asio3/proxy/core.hpp>
#include <asio3/proxy/error.hpp>

#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>

#include <asio3/udp/core.hpp>

#ifdef ASIO_STANDALONE
namespace asio::socks5
#else
namespace boost::asio::socks5
#endif
{
	/**
	 * @brief Perform the socks5 handshake asynchronously in the server role.
	 * @param socket - The read/write stream object reference.
	 * @param auth_cfg - The socks5 auth option reference.
	 */
	template<typename AsyncStream, typename AuthConfig>
	requires std::derived_from<std::remove_cvref_t<AuthConfig>, socks5::auth_config>
	inline asio::awaitable<asio::error_code> accept(
		AsyncStream& sock, AuthConfig& auth_cfg, handshake_info& handsk_info)
	{
	#ifdef ASIO_STANDALONE
		using ::asio::read;
		using ::asio::write;
		using ::std::to_underlying;
	#else
		using boost::asio::read;
		using boost::asio::write;
		using ::std::to_underlying;
	#endif

		asio::error_code ec{};

		asio::streambuf strbuf{};
		asio::mutable_buffer buffer{};
		std::size_t bytes{};
		char* p{};

		asio::ip::tcp::endpoint dst_endpoint{};

		co_await asio::dispatch(asio::use_awaitable_executor(sock));

		handsk_info.client_endpoint = sock.remote_endpoint(ec);

		std::string  & dst_addr = handsk_info.dest_address;
		std::uint16_t& dst_port = handsk_info.dest_port;

		// The client connects to the server, and sends a version
		// identifier / method selection message :

		// +----+----------+----------+
		// |VER | NMETHODS | METHODS  |
		// +----+----------+----------+
		// | 1  |    1     | 1 to 255 |
		// +----+----------+----------+

		strbuf.consume(strbuf.size());

		auto [e1, n1] = co_await asio::async_read(
			sock, strbuf, asio::transfer_exactly(1 + 1), asio::use_awaitable_executor(sock));
		if (e1)
			co_return e1;

		p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

		if (std::uint8_t version = read<std::uint8_t>(p); version != std::uint8_t(0x05))
		{
			ec = socks5::make_error_code(socks5::error::unsupported_version);
			co_return ec;
		}

		std::uint8_t nmethods{};
		if (nmethods = read<std::uint8_t>(p); nmethods == std::uint8_t(0))
		{
			ec = socks5::make_error_code(socks5::error::no_acceptable_methods);
			co_return ec;
		}

		strbuf.consume(strbuf.size());

		auto [e2, n2] = co_await asio::async_read(
			sock, strbuf, asio::transfer_exactly(nmethods), asio::use_awaitable_executor(sock));
		if (e2)
			co_return e2;

		p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

		auth_method method = auth_method::noacceptable;

		for (std::uint8_t i = 0; method == auth_method::noacceptable && i < nmethods; ++i)
		{
			auth_method m1 = static_cast<auth_method>(read<std::uint8_t>(p));

			for(auth_method m2 : auth_cfg.supported_method)
			{
				if (m1 == m2)
				{
					method = m1;
					break;
				}
			}
		}

		handsk_info.method.emplace_back(method);

        // +----+--------+
        // |VER | METHOD |
        // +----+--------+
        // | 1  |   1    |
        // +----+--------+

		strbuf.consume(strbuf.size());

		bytes  = 2;
		buffer = strbuf.prepare(bytes);
		p      = static_cast<char*>(buffer.data());

		write(p, std::uint8_t(0x05));                  // VER 
		write(p, std::uint8_t(to_underlying(method))); // METHOD 

		strbuf.commit(bytes);

		auto [e3, n3] = co_await asio::async_write(
			sock, strbuf, asio::transfer_exactly(bytes), asio::use_awaitable_executor(sock));
		if (e3)
			co_return e3;

		if (method == auth_method::noacceptable)
		{
			ec = socks5::make_error_code(socks5::error::no_acceptable_methods);
			co_return ec;
		}
		if (method == auth_method::password)
		{
			std::string& username = handsk_info.username;
			std::string& password = handsk_info.password;
			std::uint8_t ulen{}, plen{};

			//         +----+------+----------+------+----------+
			//         |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
			//         +----+------+----------+------+----------+
			//         | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
			//         +----+------+----------+------+----------+
					
			strbuf.consume(strbuf.size());

			auto [e4, n4] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(1 + 1), asio::use_awaitable_executor(sock));
			if (e4)
				co_return e4;

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			// The VER field contains the current version of the subnegotiation, which is X'01'.
			if (std::uint8_t version = read<std::uint8_t>(p); version != std::uint8_t(0x01))
			{
				ec = socks5::make_error_code(socks5::error::unsupported_authentication_version);
				co_return ec;
			}

			if (ulen = read<std::uint8_t>(p); ulen == std::uint8_t(0))
			{
				ec = socks5::make_error_code(socks5::error::authentication_failed);
				co_return ec;
			}

			// read username
			strbuf.consume(strbuf.size());

			auto [e5, n5] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(ulen), asio::use_awaitable_executor(sock));
			if (e5)
				co_return e5;

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			username.assign(p, ulen);

			// read password len
			strbuf.consume(strbuf.size());

			auto [e6, n6] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(1), asio::use_awaitable_executor(sock));
			if (e6)
				co_return e6;

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			if (plen = read<std::uint8_t>(p); plen == std::uint8_t(0))
			{
				ec = socks5::make_error_code(socks5::error::authentication_failed);
				co_return ec;
			}

			// read password
			strbuf.consume(strbuf.size());

			auto [e7, n7] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(plen), asio::use_awaitable_executor(sock));
			if (e7)
				co_return e7;

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			password.assign(p, plen);

			auto auth_result = co_await auth_cfg.on_auth(handsk_info);

			// compare username and password
			// failed
			if (!auth_result)
			{
				strbuf.consume(strbuf.size());

				bytes  = 2;
				buffer = strbuf.prepare(bytes);
				p      = static_cast<char*>(buffer.data());

				write(p, std::uint8_t(0x01));                                                // VER 
				write(p, std::uint8_t(to_underlying(socks5::error::authentication_failed))); // STATUS  

				strbuf.commit(bytes);

				auto [e8, n8] = co_await asio::async_write(
					sock, strbuf, asio::transfer_exactly(bytes), asio::use_awaitable_executor(sock));

				ec = socks5::make_error_code(socks5::error::authentication_failed);
				co_return ec;
			}
			else
			{
				strbuf.consume(strbuf.size());

				bytes  = 2;
				buffer = strbuf.prepare(bytes);
				p      = static_cast<char*>(buffer.data());

				write(p, std::uint8_t(0x01)); // VER 
				write(p, std::uint8_t(0x00)); // STATUS  

				strbuf.commit(bytes);

				auto [e9, n9] = co_await asio::async_write(
					sock, strbuf, asio::transfer_exactly(bytes), asio::use_awaitable_executor(sock));
				if (e9)
					co_return e9;
			}
		}

		//  +----+-----+-------+------+----------+----------+
		//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
		//  +----+-----+-------+------+----------+----------+
		//  | 1  |  1  | X'00' |  1   | Variable |    2     |
		//  +----+-----+-------+------+----------+----------+

		strbuf.consume(strbuf.size());

		// 1. read the first 5 bytes : VER REP RSV ATYP [LEN]
		auto [ea, na] = co_await asio::async_read(
			sock, strbuf, asio::transfer_exactly(5), asio::use_awaitable_executor(sock));
		if (ea)
			co_return ea;

		p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

		// VER
		if (std::uint8_t ver = read<std::uint8_t>(p); ver != std::uint8_t(0x05))
		{
			ec = socks5::make_error_code(socks5::error::unsupported_version);
			co_return ec;
		}

		// CMD
		socks5::command cmd = static_cast<socks5::command>(read<std::uint8_t>(p));

		handsk_info.cmd = cmd;

		// skip RSV.
		read<std::uint8_t>(p);

		socks5::address_type atyp = static_cast<socks5::address_type>(read<std::uint8_t>(p)); // ATYP
		std::uint8_t         alen =                                   read<std::uint8_t>(p);  // [LEN]

		handsk_info.addr_type = atyp;

		// ATYP
		switch (atyp)
		{
		case socks5::address_type::ipv4  : bytes = 4    + 2 - 1; break; // IP V4 address: X'01'
		case socks5::address_type::domain: bytes = alen + 2 - 0; break; // DOMAINNAME: X'03'
		case socks5::address_type::ipv6  : bytes = 16   + 2 - 1; break; // IP V6 address: X'04'
		default:
		{
			ec = socks5::make_error_code(socks5::error::address_type_not_supported);
			co_return ec;
		}
		}

		strbuf.consume(strbuf.size());

		auto [eb, nb] = co_await asio::async_read(
			sock, strbuf, asio::transfer_exactly(bytes), asio::use_awaitable_executor(sock));
		if (eb)
			co_return eb;

		p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

		dst_addr = "";
		dst_port = 0;

		switch (atyp)
		{
		case socks5::address_type::ipv4: // IP V4 address: X'01'
		{
			asio::ip::address_v4::bytes_type addr{};
			addr[0] = alen;
			addr[1] = read<std::uint8_t>(p);
			addr[2] = read<std::uint8_t>(p);
			addr[3] = read<std::uint8_t>(p);
			dst_endpoint.address(asio::ip::address_v4(addr));
			dst_addr = dst_endpoint.address().to_string(ec);
			dst_port = read<std::uint16_t>(p);
			dst_endpoint.port(dst_port);
		}
		break;
		case socks5::address_type::domain: // DOMAINNAME: X'03'
		{
			std::string addr;
			addr.resize(alen);
			std::copy(p, p + alen, addr.data());
			p += alen;
			dst_addr = std::move(addr);
			dst_port = read<std::uint16_t>(p);
			dst_endpoint.port(dst_port);
		}
		break;
		case socks5::address_type::ipv6: // IP V6 address: X'04'
		{
			asio::ip::address_v6::bytes_type addr{};
			addr[0] = alen;
			for (int i = 1; i < 16; i++)
			{
				addr[i] = read<std::uint8_t>(p);
			}
			dst_endpoint.address(asio::ip::address_v6(addr));
			dst_addr = dst_endpoint.address().to_string(ec);
			dst_port = read<std::uint16_t>(p);
			dst_endpoint.port(dst_port);
		}
		break;
		}

		asio::ip::address bnd_addr = sock.local_endpoint(ec).address();
		std::uint16_t     bnd_port = sock.local_endpoint(ec).port();

		//         o  REP    Reply field:
		//             o  X'00' succeeded
		//             o  X'01' general SOCKS server failure
		//             o  X'02' connection not allowed by ruleset
		//             o  X'03' Network unreachable
		//             o  X'04' Host unreachable
		//             o  X'05' Connection refused
		//             o  X'06' TTL expired
		//             o  X'07' Command not supported
		//             o  X'08' Address type not supported
		//             o  X'09' to X'FF' unassigned
			
		std::uint8_t urep = std::uint8_t(0x00);

		if (dst_addr.empty() || dst_port == 0)
		{
			urep = std::uint8_t(socks5::connect_result::host_unreachable);
			ec = socks5::make_error_code(socks5::error::host_unreachable);
		}
		else if (cmd == socks5::command::connect)
		{
			using connect_socket_t = typename std::remove_cvref_t<AuthConfig>::connect_bound_socket_type;

			asio::ip::tcp::resolver resolver(asio::detail::get_lowest_executor(sock));
			asio::ip::tcp::resolver::results_type eps{};
			auto er = co_await asio::resolve(
				resolver, dst_addr, dst_port, eps, asio::ip::resolver_base::flags());
			if (er)
			{
				urep = std::uint8_t(socks5::connect_result::host_unreachable);
				ec = er;
			}
			else
			{
				connect_socket_t bnd_socket(asio::detail::get_lowest_executor(sock));
				auto [ed, ep] = co_await asio::async_connect(
					bnd_socket, eps, asio::default_tcp_connect_condition{}, asio::use_awaitable_executor(sock));

				if (!ed)
				{
					handsk_info.bound_socket = std::move(bnd_socket);
				}

				if (!ed)
					urep = std::uint8_t(0x00);
				else if (ed == asio::error::network_unreachable)
					urep = std::uint8_t(socks5::connect_result::network_unreachable);
				else if (ed == asio::error::host_unreachable || ed == asio::error::host_not_found)
					urep = std::uint8_t(socks5::connect_result::host_unreachable);
				else if (ed == asio::error::connection_refused)
					urep = std::uint8_t(socks5::connect_result::connection_refused);
				else
					urep = std::uint8_t(socks5::connect_result::general_socks_server_failure);

				ec = ed;
			}
		}
		else if (cmd == socks5::command::udp_associate)
		{
			asio::ip::udp bnd_protocol = asio::ip::udp::v4();

			// if the dest protocol is ipv4, bind local ipv4 protocol
			if /**/ (atyp == socks5::address_type::ipv4)
			{
				bnd_protocol = asio::ip::udp::v4();
			}
			// if the dest protocol is ipv6, bind local ipv6 protocol
			else if (atyp == socks5::address_type::ipv6)
			{
				bnd_protocol = asio::ip::udp::v6();
			}
			// if the dest id domain, bind local protocol as the same with the domain
			else if (atyp == socks5::address_type::domain)
			{
				asio::ip::udp::resolver resolver(asio::detail::get_lowest_executor(sock));
				asio::ip::udp::resolver::results_type eps{};
				auto er = co_await asio::resolve(
					resolver, dst_addr, dst_port, eps, asio::ip::resolver_base::flags());
				if (!er)
				{
					if ((*eps).endpoint().address().is_v6())
						bnd_protocol = asio::ip::udp::v6();
					else
						bnd_protocol = asio::ip::udp::v4();
				}
				// if resolve domain failed, bind local protocol as the same with the tcp connection
				else
				{
					if (sock.local_endpoint(ec).address().is_v6())
						bnd_protocol = asio::ip::udp::v6();
					else
						bnd_protocol = asio::ip::udp::v4();
				}
			}

			using udpass_socket_t = typename std::remove_cvref_t<AuthConfig>::udp_associate_bound_socket_type;

			try
			{
				// port equal to 0 is means use a random port.
				udpass_socket_t bnd_socket(asio::detail::get_lowest_executor(sock),
					asio::ip::udp::endpoint(bnd_protocol, 0));
				bnd_port = bnd_socket.local_endpoint().port();
				handsk_info.bound_socket = std::move(bnd_socket);
			}
			catch (const asio::system_error& e)
			{
				ec = e.code();
			}

			if (!ec)
				urep = std::uint8_t(0x00);
			else
				urep = std::uint8_t(socks5::connect_result::general_socks_server_failure);
		}
		else/* if (cmd == socks5::command::bind)*/
		{
			urep = std::uint8_t(socks5::connect_result::command_not_supported);
			ec = socks5::make_error_code(socks5::error::command_not_supported);
		}

		strbuf.consume(strbuf.size());

		// the address field contains a fully-qualified domain name.  The first
		// octet of the address field contains the number of octets of name that
		// follow, there is no terminating NUL octet.
		buffer = strbuf.prepare(1 + 1 + 1 + 1 + (std::max)(16, int(dst_addr.size() + 1)) + 2);
		p      = static_cast<char*>(buffer.data());

		write(p, std::uint8_t(0x05)); // VER 5.
		write(p, std::uint8_t(urep)); // REP 
		write(p, std::uint8_t(0x00)); // RSV.

		if (bnd_addr.is_v4())
		{
			write(p, std::to_underlying(socks5::address_type::ipv4)); // ATYP 

			// real length
			bytes = 1 + 1 + 1 + 1 + 4 + 2;

			write(p, bnd_addr.to_v4().to_uint());
		}
		else
		{
			write(p, std::to_underlying(socks5::address_type::ipv6)); // ATYP 

			// real length
			bytes = 1 + 1 + 1 + 1 + 16 + 2;

			auto addr_bytes = bnd_addr.to_v6().to_bytes();
			std::copy(addr_bytes.begin(), addr_bytes.end(), p);
			p += 16;
		}

		// port
		write(p, bnd_port);

		strbuf.commit(bytes);

		auto [ef, nf] = co_await asio::async_write(
			sock, strbuf, asio::transfer_exactly(bytes), asio::use_awaitable_executor(sock));
		co_return ef ? ef : ec;
	}
}

#ifdef ASIO_STANDALONE
namespace asio::socks5::detail
#else
namespace boost::asio::socks5::detail
#endif
{
	struct async_accept_op
	{
		auto operator()(auto state, auto sock_ref, auto auth_cfg_ref, auto handsk_info_ref) -> void
		{
			auto& sock = sock_ref.get();
			auto& auth_cfg = auth_cfg_ref.get();
			auto& handsk_info = handsk_info_ref.get();

			co_await asio::dispatch(asio::use_deferred_executor(sock));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			auto [e1] = co_await asio::async_call_coroutine(asio::detail::get_lowest_executor(sock),
				socks5::accept(sock, auth_cfg, handsk_info), asio::use_deferred_executor(sock));

			co_return{ e1 };
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
	 * @brief Perform the socks5 handshake asynchronously in the server role.
	 * @param socket - The read/write stream object reference.
	 * @param auth_cfg - The socks5 auth option reference.
	 * @param token - The completion handler to invoke when the operation completes.
	 *	  The equivalent function signature of the handler must be:
	 *    @code
	 *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		typename AuthConfig,
		typename AcceptToken = asio::default_token_type<AsyncStream>>
	requires std::derived_from<std::remove_cvref_t<AuthConfig>, socks5::auth_config>
	inline auto async_accept(
		AsyncStream& sock, AuthConfig& auth_cfg, socks5::handshake_info& handsk_info,
		AcceptToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<AcceptToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_accept_op{}, sock),
			token, std::ref(sock), std::ref(auth_cfg), std::ref(handsk_info));
	}
}
