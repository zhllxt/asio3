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

#include <string_view>
#include <tuple>

#include <asio3/proxy/core.hpp>
#include <asio3/core/netutil.hpp>

#ifdef ASIO_STANDALONE
namespace asio::socks5
#else
namespace boost::asio::socks5
#endif
{
namespace
{
/**
 * @return tuple: error, endpoint, domain, real_data.
 */
std::tuple<int, asio::ip::udp::endpoint, std::string_view, std::string_view>
parse_udp_packet(std::string_view data, bool rsv_as_datalen = false)
{
	// RSV FRAG 
	if (data.size() < std::size_t(3))
		return { 1,{},{},{} };

	std::uint16_t data_size = asio::network_to_host(
		std::uint16_t(*(reinterpret_cast<const std::uint16_t*>(data.data()))));

	data.remove_prefix(3);

	// ATYP 
	if (data.size() < std::size_t(1))
		return { 2,{},{},{} };

	std::uint8_t atyp = std::uint8_t(data[0]);

	data.remove_prefix(1);

	asio::ip::udp::endpoint endpoint{};
	std::string_view domain;

	if /**/ (atyp == std::uint8_t(0x01)) // IP V4 address: X'01'
	{
		if (data.size() < std::size_t(4 + 2))
			return { 3,{},{},{} };

		asio::ip::address_v4::bytes_type addr4{};
		for (std::size_t i = 0; i < addr4.size(); i++)
		{
			addr4[i] = asio::ip::address_v4::bytes_type::value_type(data[i]);
		}
		endpoint.address(asio::ip::address_v4(addr4));

		data.remove_prefix(4);

		auto* p = data.data();

		std::uint16_t uport = asio::read<std::uint16_t>(p);
		endpoint.port(uport);

		data.remove_prefix(2);

		if (rsv_as_datalen && data.size() != data_size)
			return { 4,{},{},{} };
	}
	else if (atyp == std::uint8_t(0x04)) // IP V6 address: X'04'
	{
		if (data.size() < std::size_t(16 + 2))
			return { 5,{},{},{} };

		data.remove_prefix(16 + 2);

		asio::ip::address_v6::bytes_type addr6{};
		for (std::size_t i = 0; i < addr6.size(); i++)
		{
			addr6[i] = asio::ip::address_v6::bytes_type::value_type(data[i]);
		}
		endpoint.address(asio::ip::address_v6(addr6));

		data.remove_prefix(16);

		auto* p = data.data();

		std::uint16_t uport = asio::read<std::uint16_t>(p);
		endpoint.port(uport);

		data.remove_prefix(2);

		if (rsv_as_datalen && data.size() != data_size)
			return { 6,{},{},{} };
	}
	else if (atyp == std::uint8_t(0x03)) // DOMAINNAME: X'03'
	{
		if (data.size() < std::size_t(1))
			return { 7,{},{},{} };

		std::uint8_t domain_len = std::uint8_t(data[0]);
		if (domain_len == 0)
			return { 8,{},{},{} };

		data.remove_prefix(1);

		if (data.size() < std::size_t(domain_len + 2))
			return { 9,{},{},{} };

		domain = data.substr(0, domain_len);

		data.remove_prefix(domain_len);

		auto* p = data.data();

		std::uint16_t uport = asio::read<std::uint16_t>(p);
		endpoint.port(uport);

		data.remove_prefix(2);

		if (rsv_as_datalen && data.size() != data_size)
			return { 10,{},{},{} };
	}
	else
	{
		return { 11,{},{},{} };
	}

	return { 0, std::move(endpoint), domain, data };
}

std::tuple<int, asio::ip::udp::endpoint, std::string_view, std::string_view>
parse_udp_packet(asio::const_buffer buffer, bool rsv_as_datalen = false)
{
	return parse_udp_packet(asio::to_string_view(buffer), rsv_as_datalen);
}

std::tuple<int, asio::ip::udp::endpoint, std::string_view, std::string_view>
parse_udp_packet(asio::mutable_buffer buffer, bool rsv_as_datalen = false)
{
	return parse_udp_packet(asio::to_string_view(buffer), rsv_as_datalen);
}
}
}
