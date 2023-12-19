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

#include <cassert>

#include <asio3/proxy/core.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/fixed_capacity_vector.hpp>

#ifdef ASIO_STANDALONE
namespace asio::socks5
#else
namespace boost::asio::socks5
#endif
{
namespace
{
template<std::integral I>
auto make_udp_header(const asio::ip::address& dest_addr, std::uint16_t dest_port, I datalen = 0)
{
	using value_type = std::uint8_t;

	std::experimental::fixed_capacity_vector<std::uint8_t, 4 + 16 + 2> head;

	if (datalen == 0)
	{
		head.push_back(static_cast<value_type>(0x00)); // RSV 
		head.push_back(static_cast<value_type>(0x00)); // RSV 
	}
	else
	{
		std::uint16_t udatalen = asio::host_to_network(std::uint16_t(datalen));
		std::uint8_t* pdatalen = reinterpret_cast<std::uint8_t*>(std::addressof(udatalen));
		head.push_back(static_cast<value_type>(pdatalen[0])); // RSV 
		head.push_back(static_cast<value_type>(pdatalen[1])); // RSV 
	}

	head.push_back(static_cast<value_type>(0x00)); // FRAG 

	if (dest_addr.is_v4()) // IP V4 address: X'01'
	{
		head.push_back(static_cast<value_type>(socks5::address_type::ipv4));
		asio::ip::address_v4::bytes_type bytes = dest_addr.to_v4().to_bytes();
		head.insert(head.cend(), bytes.begin(), bytes.end());
	}
	else if (dest_addr.is_v6()) // IP V6 address: X'04'
	{
		head.push_back(static_cast<value_type>(socks5::address_type::ipv6));
		asio::ip::address_v6::bytes_type bytes = dest_addr.to_v6().to_bytes();
		head.insert(head.cend(), bytes.begin(), bytes.end());
	}
	else
	{
		assert(false);

		head.push_back(static_cast<value_type>(socks5::address_type::unknown));
		asio::ip::address_v4::bytes_type bytes{};
		head.insert(head.cend(), bytes.begin(), bytes.end());
	}

	std::uint16_t uport = asio::host_to_network(dest_port);
	std::uint8_t* pport = reinterpret_cast<std::uint8_t*>(std::addressof(uport));
	head.push_back(static_cast<value_type>(pport[0]));
	head.push_back(static_cast<value_type>(pport[1]));

	return head;
}

template<std::integral I>
auto make_udp_header(const asio::ip::udp::endpoint& dest_endpoint, I datalen = 0)
{
	return make_udp_header(dest_endpoint.address(), dest_endpoint.port(), datalen);
}

template<std::integral I>
auto make_udp_header(const asio::ip::tcp::endpoint& dest_endpoint, I datalen = 0)
{
	return make_udp_header(dest_endpoint.address(), dest_endpoint.port(), datalen);
}

template<std::integral I>
std::string make_udp_header(std::string domain, std::uint16_t dest_port, I datalen = 0)
{
	using value_type = typename std::string::value_type;

	if (domain.size() > (std::numeric_limits<std::uint8_t>::max)())
	{
		assert(false);
		domain.resize((std::numeric_limits<std::uint8_t>::max)());
	}

	std::string& head = domain;

	head.reserve(4 + 1 + head.size() + 2);

	std::experimental::fixed_capacity_vector<value_type, 4 + 1> tmp;

	if (datalen == 0)
	{
		tmp.push_back(static_cast<value_type>(0x00)); // RSV 
		tmp.push_back(static_cast<value_type>(0x00)); // RSV 
	}
	else
	{
		std::uint16_t udatalen = asio::host_to_network(std::uint16_t(datalen));
		std::uint8_t* pdatalen = reinterpret_cast<std::uint8_t*>(std::addressof(udatalen));
		tmp.push_back(static_cast<value_type>(pdatalen[0])); // RSV 
		tmp.push_back(static_cast<value_type>(pdatalen[1])); // RSV 
	}

	tmp.push_back(static_cast<value_type>(0x00)); // FRAG 

	// DOMAINNAME: X'03'
	tmp.push_back(static_cast<value_type>(socks5::address_type::domain));
	tmp.push_back(static_cast<value_type>(domain.size()));

	head.insert(head.cbegin(), tmp.begin(), tmp.end());

	std::uint16_t uport = asio::host_to_network(dest_port);
	std::uint8_t* pport = reinterpret_cast<std::uint8_t*>(std::addressof(uport));
	head.push_back(static_cast<value_type>(pport[0]));
	head.push_back(static_cast<value_type>(pport[1]));

	return std::move(domain);
}

template<class T>
inline void insert_udp_header(
	T& container, const asio::ip::address& dest_addr, std::uint16_t dest_port, bool rsv_as_datalen = false)
{
	static_assert(sizeof(typename T::value_type) == std::size_t(1));

	auto head = make_udp_header(dest_addr, dest_port, rsv_as_datalen ? container.size() : 0);

	if /**/ (dest_addr.is_v4()) // IP V4 address: X'01'
		container.reserve(container.size() + 4 + 4 + 2);
	else if (dest_addr.is_v6()) // IP V6 address: X'04'
		container.reserve(container.size() + 4 + 16 + 2);
	else
		container.reserve(container.size() + 4 + 4 + 2);

	container.insert(container.cbegin(), head.begin(), head.end());
}

template<class T>
inline void insert_udp_header(
	T& container, const asio::ip::udp::endpoint& dest_endpoint, bool rsv_as_datalen = false)
{
	insert_udp_header(container, dest_endpoint.address(), dest_endpoint.port(), rsv_as_datalen);
}

template<class T>
inline void insert_udp_header(
	T& container, const asio::ip::tcp::endpoint& dest_endpoint, bool rsv_as_datalen = false)
{
	insert_udp_header(container, dest_endpoint.address(), dest_endpoint.port(), rsv_as_datalen);
}

template<class T>
inline void insert_udp_header(
	T& container, std::string dest_address, std::uint16_t dest_port, bool rsv_as_datalen = false)
{
	using value_type = typename T::value_type;

	static_assert(sizeof(value_type) == std::size_t(1));

	asio::error_code ec{};

	asio::ip::address dest_addr = asio::ip::address::from_string(dest_address, ec);
	
	// ec has no error, it must be ipv4 or ipv6
	if (!ec)
	{
		insert_udp_header(container, dest_addr, dest_port, rsv_as_datalen);
		return;
	}

	// ec has error, it is domain.
	std::string head = make_udp_header(std::move(dest_address), dest_port, rsv_as_datalen ? container.size() : 0);

	container.reserve(container.size() + head.size());

	container.insert(container.cbegin(), head.begin(), head.end());
}
}
}
