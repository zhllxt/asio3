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

#include <asio3/core/detail/push_options.hpp>

#include <asio3/core/io_context_thread.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/resolve.hpp>

#include <asio3/icmp/detail/icmp_header.hpp>
#include <asio3/icmp/detail/ipv4_header.hpp>
#include <asio3/icmp/detail/ipv6_header.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	namespace detail
	{
		inline unsigned short icmp_decode(const unsigned char* buffer, int a, int b)
		{
			return (unsigned short)((buffer[a] << 8) + buffer[b]);
		}
	}

	struct ping_option
	{
		std::string                                 host;
		std::chrono::steady_clock::duration         timeout = asio::icmp_request_timeout;
		std::string                                 payload{ R"("Hello!" from Asio ping.)" };
		unsigned short                              identifier = 0;
		unsigned short                              sequence = 0;
		std::optional<std::reference_wrapper<asio::ip::icmp::socket>> socket;
	};

	struct icmp_response : public detail::icmp_header
	{
		alignas(std::max_align_t) unsigned char buffer[
			(std::max)(sizeof(detail::ipv4_header), sizeof(detail::ipv6_header))];

		std::chrono::steady_clock::duration lag{ std::chrono::steady_clock::duration(-1) };

		detail::icmp_header& base() noexcept { return static_cast<detail::icmp_header&>(*this); }

		inline unsigned char  version()         const { return (buffer[0] >> 4) & 0xF; }
		inline unsigned short identification()  const { return detail::icmp_decode(buffer, 4, 5); }
		inline unsigned char  next_header()     const { return buffer[6]; }
		inline unsigned char  hop_limit()       const { return buffer[7]; }
		inline unsigned short header_length()   const { return (unsigned short)((buffer[0] & 0xF) * 4); }
		inline unsigned char  type_of_service() const { return buffer[1]; }
		inline unsigned short total_length()    const { return detail::icmp_decode(buffer, 2, 3); }
		inline bool           dont_fragment()   const { return (buffer[6] & 0x40) != 0; }
		inline bool           more_fragments()  const { return (buffer[6] & 0x20) != 0; }
		inline unsigned short fragment_offset() const { return detail::icmp_decode(buffer, 6, 7) & 0x1FFF; }
		inline unsigned int   time_to_live()    const { return buffer[8]; }
		inline unsigned char  protocol()        const { return buffer[9]; }
		inline unsigned short header_checksum() const { return detail::icmp_decode(buffer, 10, 11); }

		inline bool is_v4() const noexcept { return version() == 4; }
		inline bool is_v6() const noexcept { return version() == 6; }

		inline detail::ipv4_header& to_v4() const noexcept { return *((detail::ipv4_header*)(buffer)); }
		inline detail::ipv6_header& to_v6() const noexcept { return *((detail::ipv6_header*)(buffer)); }

		inline asio::ip::address source_address() const
		{
			return is_v4() ?
				asio::ip::address(to_v4().source_address()) :
				asio::ip::address(to_v6().source_address());
		}

		inline bool is_timeout() const noexcept { return (this->lag.count() == -1); }

		inline auto milliseconds() const noexcept
		{
			return this->lag.count() == -1 ? -1 :
				std::chrono::duration_cast<std::chrono::milliseconds>(this->lag).count();
		}
	};

	asio::awaitable<std::tuple<asio::error_code, icmp_response>> co_ping(ping_option opt)
	{
		const auto& ex = co_await asio::this_coro::executor;

		co_await asio::dispatch(ex, asio::use_nothrow_awaitable);

		asio::error_code ec{};
		icmp_response resp{};

		std::unique_ptr<asio::ip::icmp::socket, void(*)(asio::ip::icmp::socket*)> psock
		{
			nullptr,
			[](asio::ip::icmp::socket*) {}
		};
		if (opt.socket.has_value())
		{
			std::unique_ptr<asio::ip::icmp::socket, void(*)(asio::ip::icmp::socket*)> psock2
			{
				std::addressof(opt.socket.value().get()),
				[](asio::ip::icmp::socket*) {}
			};
			psock = std::move(psock2);
		}
		else
		{
			std::unique_ptr<asio::ip::icmp::socket, void(*)(asio::ip::icmp::socket*)> psock2
			{
				new asio::ip::icmp::socket(ex),
				[](asio::ip::icmp::socket* p) { delete p; }
			};
			psock = std::move(psock2);
		}

		asio::ip::icmp::socket& sock = *psock;

		asio::detail::call_func_when_timeout wt(ex, opt.timeout, [&sock]() mutable
		{
			asio::error_code ec{};
			sock.close(ec);
		});
		
		std::chrono::steady_clock::time_point time_sent = std::chrono::steady_clock::now();

		asio::ip::icmp::resolver resolver(ex);
		asio::ip::icmp::resolver::results_type eps{};

		// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
		auto e1 = co_await asio::resolve(
			resolver, opt.host, 0, eps, asio::ip::resolver_base::flags());
		if (e1)
			co_return std::tuple{ e1, std::move(resp) };

		asio::ip::icmp::endpoint dest = eps.begin()->endpoint();

		if (!sock.is_open())
		{
			sock.open(dest.protocol(), ec);
			if (ec)
			{
				sock.close(e1);
				co_return std::tuple{ ec, std::move(resp) };
			}
		}
		else
		{
			if (sock.local_endpoint(ec).protocol() != dest.protocol())
			{
				sock.close(ec);
				sock.open(dest.protocol(), ec);
				if (ec)
				{
					sock.close(e1);
					co_return std::tuple{ ec, std::move(resp) };
				}
			}
		}

		asio::streambuf request_buffer;
		asio::streambuf reply_buffer;
		std::ostream os(std::addressof(request_buffer));
		std::istream is(std::addressof(reply_buffer));
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();

		detail::icmp_header echo_request{};
		// Create an ICMP header for an echo request.
		echo_request.type(detail::icmp_header::echo_request);
		echo_request.code(0);
		echo_request.identifier(opt.identifier == 0 ?
			(unsigned short)(std::size_t(std::addressof(sock))) : opt.identifier);
		echo_request.sequence_number(opt.sequence == 0 ?
			(unsigned short)(ms % (std::numeric_limits<unsigned short>::max)()) : opt.sequence);
		detail::compute_checksum(echo_request, opt.payload.begin(), opt.payload.end());

		// Encode the request packet.
		os << echo_request << opt.payload;

		auto [e4, n4] = co_await sock.async_send_to(request_buffer.data(), dest, asio::use_nothrow_awaitable);
		if (e4)
		{
			sock.close(ec);
			co_return std::tuple{ e4, std::move(resp) };
		}

		// Discard any data already in the buffer.
		reply_buffer.consume(reply_buffer.size());

		std::size_t length;
		if (dest.address().is_v4())
		{
			length = sizeof(detail::ipv4_header) + sizeof(detail::icmp_header) + opt.payload.size();
		}
		else
		{
			length = sizeof(detail::ipv6_header) + sizeof(detail::icmp_header) + opt.payload.size();
		}

		// Wait for a reply. We prepare the buffer to receive up to 64KB.
		auto [e6, n6] = co_await sock.async_receive(reply_buffer.prepare(length), asio::use_nothrow_awaitable);
		if (e6)
		{
			sock.close(ec);
			co_return std::tuple{ e6, std::move(resp) };
		}

		// The actual number of bytes received is committed to the buffer so that we
		// can extract it using a std::istream object.
		reply_buffer.commit(n6);

		detail::icmp_header& icmp_hdr = resp.base();
		detail::ipv4_header& ipv4_hdr = resp.to_v4();
		detail::ipv6_header& ipv6_hdr = resp.to_v6();

		// Decode the reply packet.
		if (dest.address().is_v4())
		{
			is >> ipv4_hdr >> icmp_hdr;

			assert(ipv4_hdr.total_length() == n6);
		}
		else
		{
			is >> ipv6_hdr >> icmp_hdr;
		}

		// We can receive all ICMP packets received by the host, so we need to
		// filter out only the echo replies that match the our identifier and
		// expected sequence number.
		if (is && icmp_hdr.type() == detail::icmp_header::echo_reply
			&& icmp_hdr.identifier() == echo_request.identifier()
			&& icmp_hdr.sequence_number() == echo_request.sequence_number())
		{
			// Print out some information about the reply packet.
			resp.lag = std::chrono::steady_clock::now() - time_sent;
		}

		co_return std::tuple{ error_code{}, std::move(resp) };
    }

	asio::awaitable<std::tuple<asio::error_code, icmp_response>> co_ping(std::string host)
	{
		co_return co_await co_ping({ .host = std::move(host) });
	}
}

#include <asio3/core/detail/pop_options.hpp>
