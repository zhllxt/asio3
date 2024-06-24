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
 * http proxy : https://blog.csdn.net/dolphin98629/article/details/54599850
 */

#pragma once

#include <cassert>

#include <asio3/core/error.hpp>
#include <asio3/core/netutil.hpp>

#include <asio3/proxy/core.hpp>
#include <asio3/proxy/error.hpp>

#include <asio3/tcp/connect.hpp>
#include <asio3/tcp/read.hpp>
#include <asio3/tcp/write.hpp>

#ifdef ASIO_STANDALONE
namespace asio::socks5::detail
#else
namespace boost::asio::socks5::detail
#endif
{
	struct async_handshake_op
	{
		auto operator()(auto state, auto sock_ref, auto sock5_opt_ref) -> void
		{
		#ifdef ASIO_STANDALONE
			using ::asio::read;
			using ::asio::write;
		#else
			using boost::asio::read;
			using boost::asio::write;
		#endif

			auto& sock = sock_ref.get();
			option& sock5_opt = sock5_opt_ref.get();

			co_await asio::dispatch(asio::detail::get_lowest_executor(sock), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::error_code ec{};

			std::string  & dst_addr = sock5_opt.dest_address;
			std::uint16_t& dst_port = sock5_opt.dest_port;
			std::string  & bnd_addr = sock5_opt.bound_address;
			std::uint16_t& bnd_port = sock5_opt.bound_port;

			asio::streambuf strbuf{};
			asio::mutable_buffer buffer{};
			std::size_t bytes{};
			char* p{};

			// The client connects to the server, and sends a version
			// identifier / method selection message :

			// +----+----------+----------+
			// |VER | NMETHODS | METHODS  |
			// +----+----------+----------+
			// | 1  |    1     | 1 to 255 |
			// +----+----------+----------+

			strbuf.consume(strbuf.size());

			bytes  = 1 + 1 + sock5_opt.method.size();
			buffer = strbuf.prepare(bytes);
			p      = static_cast<char*>(buffer.data());

			write(p, std::uint8_t(0x05));                      // SOCKS VERSION 5.
			write(p, std::uint8_t(sock5_opt.method.size()));   // NMETHODS
			for (auto m : sock5_opt.method)
			{
				write(p, std::uint8_t(std::to_underlying(m))); // METHODS
			}

			strbuf.commit(bytes);

			auto [e1, n1] = co_await asio::async_write(
				sock, strbuf, asio::transfer_exactly(bytes), use_nothrow_deferred);
			if (e1)
				co_return{ e1 };

			// The server selects from one of the methods given in METHODS, and 
			// sends a METHOD selection message :

			// +----+--------+
			// |VER | METHOD |
			// +----+--------+
			// | 1  |   1    |
			// +----+--------+

			strbuf.consume(strbuf.size());

			auto [e2, n2] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(1 + 1), use_nothrow_deferred);
			if (e2)
				co_return{ e2 };

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			if (std::uint8_t version = read<std::uint8_t>(p); version != std::uint8_t(0x05))
			{
				ec = socks5::make_error_code(socks5::error::unsupported_version);
				co_return{ ec };
			}

			// If the selected METHOD is X'FF', none of the methods listed by the
			// client are acceptable, and the client MUST close the connection.
			// 
			// The values currently defined for METHOD are:
			// 
			//         o  X'00' NO AUTHENTICATION REQUIRED
			//         o  X'01' GSSAPI
			//         o  X'02' USERNAME/PASSWORD
			//         o  X'03' to X'7F' IANA ASSIGNED
			//         o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
			//         o  X'FF' NO ACCEPTABLE METHODS

			// Once the method-dependent subnegotiation has completed, the client
			// sends the request details.  If the negotiated method includes
			// encapsulation for purposes of integrity checking and/or
			// confidentiality, these requests MUST be encapsulated in the method-
			// dependent encapsulation.
			// 
			// The SOCKS request is formed as follows:
			// 
			//     +----+-----+-------+------+----------+----------+
			//     |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
			//     +----+-----+-------+------+----------+----------+
			//     | 1  |  1  | X'00' |  1   | Variable |    2     |
			//     +----+-----+-------+------+----------+----------+
			// 
			//     Where:
			// 
			//         o  VER    protocol version: X'05'
			//         o  CMD
			//             o  CONNECT X'01'
			//             o  BIND X'02'
			//             o  UDP ASSOCIATE X'03'
			//         o  RSV    RESERVED
			//         o  ATYP   address type of following address
			//             o  IP V4 address: X'01'
			//             o  DOMAINNAME: X'03'
			//             o  IP V6 address: X'04'
			//         o  DST.ADDR       desired destination address
			//         o  DST.PORT desired destination port in network octet
			//             order
			// 
			// The SOCKS server will typically evaluate the request based on source
			// and destination addresses, and return one or more reply messages, as
			// appropriate for the request type.

			auth_method method = auth_method(read<std::uint8_t>(p));

			if /**/ (method == auth_method::anonymous)
			{
			}
			else if (method == auth_method::gssapi)
			{
				ec = socks5::make_error_code(socks5::error::unsupported_method);
				co_return{ ec };
			}
			else if (method == auth_method::password)
			{
				// Once the SOCKS V5 server has started, and the client has selected the
				// Username/Password Authentication protocol, the Username/Password
				// subnegotiation begins.  This begins with the client producing a
				// Username/Password request:
				// 
				//         +----+------+----------+------+----------+
				//         |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
				//         +----+------+----------+------+----------+
				//         | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
				//         +----+------+----------+------+----------+

				const std::string& username = sock5_opt.username;
				const std::string& password = sock5_opt.password;

				if (username.empty() || password.empty())
				{
					assert(false);
					ec = socks5::make_error_code(socks5::error::username_required);
					co_return{ ec };
				}

				strbuf.consume(strbuf.size());

				bytes  = 1 + 1 + username.size() + 1 + password.size();
				buffer = strbuf.prepare(bytes);
				p      = static_cast<char*>(buffer.data());

				// The VER field contains the current version of the subnegotiation,
				// which is X'01'. The ULEN field contains the length of the UNAME field
				// that follows. The UNAME field contains the username as known to the
				// source operating system. The PLEN field contains the length of the
				// PASSWD field that follows. The PASSWD field contains the password
				// association with the given UNAME.

				// VER
				write(p, std::uint8_t(0x01));

				// ULEN
				write(p, std::uint8_t(username.size()));

				// UNAME
				std::copy(username.begin(), username.end(), p);
				p += username.size();

				// PLEN
				write(p, std::uint8_t(password.size()));

				// PASSWD
				std::copy(password.begin(), password.end(), p);
				p += password.size();

				strbuf.commit(bytes);

				// send username and password to server
				auto [e3, n3] = co_await asio::async_write(
					sock, strbuf, asio::transfer_exactly(bytes), use_nothrow_deferred);
				if (e3)
					co_return{ e3 };

				// The server verifies the supplied UNAME and PASSWD, and sends the
				// following response:
				// 
				//                     +----+--------+
				//                     |VER | STATUS |
				//                     +----+--------+
				//                     | 1  |   1    |
				//                     +----+--------+
				// 
				// A STATUS field of X'00' indicates success. If the server returns a
				// `failure' (STATUS value other than X'00') status, it MUST close the
				// connection.

				// read reply
				strbuf.consume(strbuf.size());

				auto [e4, n4] = co_await asio::async_read(
					sock, strbuf, asio::transfer_exactly(1 + 1), use_nothrow_deferred);
				if (e4)
					co_return{ e4 };

				// parse reply
				p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

				if (std::uint8_t ver = read<std::uint8_t>(p); ver != std::uint8_t(0x01))
				{
					ec = socks5::make_error_code(socks5::error::unsupported_authentication_version);
					co_return{ ec };
				}

				if (std::uint8_t status = read<std::uint8_t>(p); status != std::uint8_t(0x00))
				{
					ec = socks5::make_error_code(socks5::error::authentication_failed);
					co_return{ ec };
				}
			}
			//else if (method == auth_method::iana)
			//{
			//	ec = socks5::make_error_code(socks5::error::unsupported_method);
			//	co_return{ ec };
			//}
			//else if (method == auth_method::reserved)
			//{
			//	ec = socks5::make_error_code(socks5::error::unsupported_method);
			//	co_return{ ec };
			//}
			else if (method == auth_method::noacceptable)
			{
				ec = socks5::make_error_code(socks5::error::no_acceptable_methods);
				co_return{ ec };
			}
			else
			{
				ec = socks5::make_error_code(socks5::error::no_acceptable_methods);
				co_return{ ec };
			}

			strbuf.consume(strbuf.size());

			// the address field contains a fully-qualified domain name.  The first
			// octet of the address field contains the number of octets of name that
			// follow, there is no terminating NUL octet.
			buffer = strbuf.prepare(1 + 1 + 1 + 1 + (std::max)(16, int(dst_addr.size() + 1)) + 2);
			p      = static_cast<char*>(buffer.data());

			write(p, std::uint8_t(0x05));                              // VER 5.
			write(p, std::uint8_t(std::to_underlying(sock5_opt.cmd))); // CMD CONNECT .
			write(p, std::uint8_t(0x00));                              // RSV.

			asio::ip::tcp::endpoint dst_endpoint{};
			// ATYP
			dst_endpoint.address(asio::ip::make_address(dst_addr, ec));
			if (ec)
			{
				assert(dst_addr.size() <= std::size_t(0xff));

				// real length
				bytes = 1 + 1 + 1 + 1 + 1 + dst_addr.size() + 2;

				// type is domain
				write(p, std::uint8_t(0x03));

				// domain size
				write(p, std::uint8_t(dst_addr.size()));

				// domain name 
				std::copy(dst_addr.begin(), dst_addr.end(), p);
				p += dst_addr.size();
			}
			else
			{
				if /**/ (dst_endpoint.address().is_v4())
				{
					// real length
					bytes = 1 + 1 + 1 + 1 + 4 + 2;

					// type is ipv4
					write(p, std::uint8_t(0x01));

					write(p, std::uint32_t(dst_endpoint.address().to_v4().to_uint()));
				}
				else if (dst_endpoint.address().is_v6())
				{
					// real length
					bytes = 1 + 1 + 1 + 1 + 16 + 2;

					// type is ipv6
					write(p, std::uint8_t(0x04));

					auto addr_bytes = dst_endpoint.address().to_v6().to_bytes();
					std::copy(addr_bytes.begin(), addr_bytes.end(), p);
					p += 16;
				}
				else
				{
					assert(false);
				}
			}

			// port
			write(p, dst_port);

			strbuf.commit(bytes);

			auto [e5, n5] = co_await asio::async_write(
				sock, strbuf, asio::transfer_exactly(bytes), use_nothrow_deferred);
			if (e5)
				co_return{ e5 };

			// The SOCKS request information is sent by the client as soon as it has
			// established a connection to the SOCKS server, and completed the
			// authentication negotiations.  The server evaluates the request, and
			// returns a reply formed as follows:
			// 
			//     +----+-----+-------+------+----------+----------+
			//     |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
			//     +----+-----+-------+------+----------+----------+
			//     | 1  |  1  | X'00' |  1   | Variable |    2     |
			//     +----+-----+-------+------+----------+----------+
			// 
			//     Where:
			// 
			//         o  VER    protocol version: X'05'
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
			//         o  RSV    RESERVED
			//         o  ATYP   address type of following address

			strbuf.consume(strbuf.size());

			// 1. read the first 5 bytes : VER REP RSV ATYP [LEN]
			auto [e6, n6] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(5), use_nothrow_deferred);
			if (e6)
				co_return{ e6 };

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			// VER
			if (std::uint8_t ver = read<std::uint8_t>(p); ver != std::uint8_t(0x05))
			{
				ec = socks5::make_error_code(socks5::error::unsupported_version);
				co_return{ ec };
			}

			// REP
			switch (read<std::uint8_t>(p))
			{
			case std::uint8_t(0x00): ec = {}													                   ; break;
			case std::uint8_t(0x01): ec = socks5::make_error_code(socks5::error::general_socks_server_failure     ); break;
			case std::uint8_t(0x02): ec = socks5::make_error_code(socks5::error::connection_not_allowed_by_ruleset); break;
			case std::uint8_t(0x03): ec = socks5::make_error_code(socks5::error::network_unreachable              ); break;
			case std::uint8_t(0x04): ec = socks5::make_error_code(socks5::error::host_unreachable                 ); break;
			case std::uint8_t(0x05): ec = socks5::make_error_code(socks5::error::connection_refused               ); break;
			case std::uint8_t(0x06): ec = socks5::make_error_code(socks5::error::ttl_expired                      ); break;
			case std::uint8_t(0x07): ec = socks5::make_error_code(socks5::error::command_not_supported            ); break;
			case std::uint8_t(0x08): ec = socks5::make_error_code(socks5::error::address_type_not_supported       ); break;
			case std::uint8_t(0x09): ec = socks5::make_error_code(socks5::error::unassigned                       ); break;
			default:                 ec = socks5::make_error_code(socks5::error::unassigned                       ); break;
			}

			if (ec)
				co_return{ ec };

			// skip RSV.
			read<std::uint8_t>(p);

			std::uint8_t atyp = read<std::uint8_t>(p); // ATYP
			std::uint8_t alen = read<std::uint8_t>(p); // [LEN]

			// ATYP
			switch (atyp)
			{
			case std::uint8_t(0x01): bytes = 4    + 2 - 1; break; // IP V4 address: X'01'
			case std::uint8_t(0x03): bytes = alen + 2 - 0; break; // DOMAINNAME: X'03'
			case std::uint8_t(0x04): bytes = 16   + 2 - 1; break; // IP V6 address: X'04'
			default:
			{
				ec = socks5::make_error_code(socks5::error::address_type_not_supported);
				co_return{ ec };
			}
			}

			strbuf.consume(strbuf.size());

			auto [e7, n7] = co_await asio::async_read(
				sock, strbuf, asio::transfer_exactly(bytes), use_nothrow_deferred);
			if (e7)
				co_return{ e7 };

			p = const_cast<char*>(static_cast<const char*>(strbuf.data().data()));

			switch (atyp)
			{
			case std::uint8_t(0x01): // IP V4 address: X'01'
			{
				asio::ip::address_v4::bytes_type addr{};
				addr[0] = alen;
				addr[1] = read<std::uint8_t>(p);
				addr[2] = read<std::uint8_t>(p);
				addr[3] = read<std::uint8_t>(p);
				bnd_addr = asio::ip::address_v4(addr).to_string(ec);
				bnd_port = read<std::uint16_t>(p);
			}
			break;
			case std::uint8_t(0x03): // DOMAINNAME: X'03'
			{
				std::string addr;
				addr.resize(alen);
				std::copy(p, p + alen, addr.data());
				p += alen;
				bnd_addr = std::move(addr);
				bnd_port = read<std::uint16_t>(p);
			}
			break;
			case std::uint8_t(0x04): // IP V6 address: X'04'
			{
				asio::ip::address_v6::bytes_type addr{};
				addr[0] = alen;
				for (int i = 1; i < 16; i++)
				{
					addr[i] = read<std::uint8_t>(p);
				}
				bnd_addr = asio::ip::address_v6(addr).to_string(ec);
				bnd_port = read<std::uint16_t>(p);
			}
			break;
			}

			ec = {};

			co_return{ ec };
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
	 * @brief Perform the socks5 handshake asynchronously in the client role.
	 * @param sock - The read/write stream object reference.
	 * @param sock5_opt - The socks5 option reference.
	 * @param token - The completion handler to invoke when the operation completes.
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec);
	 */
	template<
		typename AsyncStream,
		typename Socks5Option,
		typename HandshakeToken = asio::default_token_type<AsyncStream>>
	requires std::derived_from<std::remove_cvref_t<Socks5Option>, socks5::option>
	inline auto async_handshake(
		AsyncStream& sock, Socks5Option& sock5_opt,
		HandshakeToken&& token = asio::default_token_type<AsyncStream>())
	{
		return asio::async_initiate<HandshakeToken, void(asio::error_code)>(
			asio::experimental::co_composed<void(asio::error_code)>(
				detail::async_handshake_op{}, sock),
			token, std::ref(sock), std::ref(sock5_opt));
	}
}
