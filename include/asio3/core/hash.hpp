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

#include <functional>

#include <asio3/core/stdconcepts.hpp>
#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	/**
	 * BKDR Hash Function
	 */
	template<typename = void>
	inline std::size_t bkdr_hash(const unsigned char * const p, std::size_t size) noexcept
	{
		std::size_t v = 0;
		for (std::size_t i = 0; i < size; ++i)
		{
			v = v * 131 + static_cast<std::size_t>(p[i]);
		}
		return v;
	}

	/**
	 * Fnv1a Hash Function
	 * Reference from Visual c++ implementation, see vc++ std::hash
	 */
	template<typename T>
	inline T fnv1a_hash(const unsigned char * const p, const T size) noexcept
	{
		static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Must be 32 or 64 digits");
		T v;
		if constexpr (sizeof(T) == 4)
			v = 2166136261u;
		else
			v = 14695981039346656037ull;
		for (T i = 0; i < size; ++i)
		{
			v ^= static_cast<T>(p[i]);
			if constexpr (sizeof(T) == 4)
				v *= 16777619u;
			else
				v *= 1099511628211ull;
		}
		return (v);
	}

	template<typename T>
	inline T fnv1a_hash(T v, const unsigned char * const p, const T size) noexcept
	{
		static_assert(sizeof(T) == 4 || sizeof(T) == 8, "Must be 32 or 64 digits");
		for (T i = 0; i < size; ++i)
		{
			v ^= static_cast<T>(p[i]);
			if constexpr (sizeof(T) == 4)
				v *= 16777619u;
			else
				v *= 1099511628211ull;
		}
		return (v);
	}
}

// custom specialization of std::hash can be injected in namespace std
// see : struct hash<asio::ip::basic_endpoint<InternetProtocol>> in /asio/ip/basic_endpoint.hpp
#if !defined(ASIO_HAS_STD_HASH)
namespace std
{
	template <>
	struct hash<asio::ip::address_v4>
	{
		std::size_t operator()(const asio::ip::address_v4& addr) const noexcept
		{
			return std::hash<unsigned int>()(addr.to_uint());
		}
	};

	template <>
	struct hash<asio::ip::address_v6>
	{
		std::size_t operator()(const asio::ip::address_v6& addr) const noexcept
		{
			const asio::ip::address_v6::bytes_type bytes = addr.to_bytes();
			std::size_t result = static_cast<std::size_t>(addr.scope_id());
			combine_4_bytes(result, &bytes[0]);
			combine_4_bytes(result, &bytes[4]);
			combine_4_bytes(result, &bytes[8]);
			combine_4_bytes(result, &bytes[12]);
			return result;
		}

	private:
		static void combine_4_bytes(std::size_t& seed, const unsigned char* bytes)
		{
			const std::size_t bytes_hash =
				(static_cast<std::size_t>(bytes[0]) << 24) |
				(static_cast<std::size_t>(bytes[1]) << 16) |
				(static_cast<std::size_t>(bytes[2]) << 8) |
				(static_cast<std::size_t>(bytes[3]));
			seed ^= bytes_hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
	};

	template <>
	struct hash<asio::ip::address>
	{
		std::size_t operator()(const asio::ip::address& addr) const noexcept
		{
			return addr.is_v4()
				? std::hash<asio::ip::address_v4>()(addr.to_v4())
				: std::hash<asio::ip::address_v6>()(addr.to_v6());
		}
	};

	template <typename InternetProtocol>
	struct hash<asio::ip::basic_endpoint<InternetProtocol>>
	{
		std::size_t operator()(const asio::ip::basic_endpoint<InternetProtocol>& ep) const noexcept
		{
			std::size_t hash1 = std::hash<asio::ip::address>()(ep.address());
			std::size_t hash2 = std::hash<unsigned short>()(ep.port());
			return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
		}
	};
}
#endif
