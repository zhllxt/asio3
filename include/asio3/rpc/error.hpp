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

#include <asio3/core/asio.hpp>

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	/**
	 */
	enum class error
	{
		// The operation completed successfully. 
		success = 0,

		// The operation timed out.
		timed_out = -32099,

		// Parse error
		parse_error = -32700,

		// Invalid Request
		invalid_request = -32600,

		// Method not found
		method_not_found = -32601,

		// Invalid params
		invalid_params = -32602,

		// Internal error
		internal_error = -32603,

		// Server error
		server_error = -32000,
	};

	enum class condition
	{
		// The operation completed successfully. 
		success = 0,

		// The operation timed out.
		timed_out = -32099,

		// Parse error
		parse_error = -32700,

		// Invalid Request
		invalid_request = -32600,

		// Method not found
		method_not_found = -32601,

		// Invalid params
		invalid_params = -32602,

		// Internal error
		internal_error = -32603,

		// Server error
		server_error = -32000,
	};

	/// The type of error category used by the library
	using error_category  = asio::error_category;

	/// The type of error condition used by the library
	using error_condition = asio::error_condition;

	class rpc_error_category : public error_category
	{
	public:
		const char* name() const noexcept override
		{
			return "asio.rpc";
		}

		inline std::string message(int ev) const override
		{
			switch (static_cast<error>(ev))
			{
			case error::success           : return "The operation completed successfully.";
			case error::timed_out         : return "The operation timed out.";
			case error::parse_error       : return "Invalid data was received.";
			case error::invalid_request   : return "The data sent is not a valid Request object.";
			case error::method_not_found  : return "The method does not exist / is not available.";
			case error::invalid_params    : return "Invalid method parameter(s).";
			case error::internal_error    : return "Internal error.";
			case error::server_error      : return "Server error.";
			default                       : return "Unknown error";
			}
		}

		inline error_condition default_error_condition(int ev) const noexcept override
		{
			return error_condition{ ev, *this };
		}
	};

	inline const rpc_error_category& rpc_category() noexcept
	{
		static rpc_error_category const cat{};
		return cat;
	}

	inline asio::error_code make_error_code(error e)
	{
		return asio::error_code{ static_cast<std::underlying_type<error>::type>(e), rpc_category() };
	}

	inline error_condition make_error_condition(condition c)
	{
		return error_condition{ static_cast<std::underlying_type<condition>::type>(c), rpc_category() };
	}

	template<typename = void>
	inline constexpr std::string_view to_string(error e)
	{
		using namespace std::string_view_literals;
		switch (e)
		{
		case error::success           : return "The operation completed successfully.";
		case error::timed_out         : return "The operation timed out.";
		case error::parse_error       : return "Invalid data was received.";
		case error::invalid_request   : return "The data sent is not a valid Request object.";
		case error::method_not_found  : return "The method does not exist / is not available.";
		case error::invalid_params    : return "Invalid method parameter(s).";
		case error::internal_error    : return "Internal error.";
		case error::server_error      : return "Server error.";
		default                       : return "Unknown error";
		}
		return "Unknown error";
	};
}

#ifdef ASIO_STANDALONE
namespace rpc = ::asio::rpc;
#else
namespace rpc = boost::asio::rpc;
#endif

namespace std
{
	template<>
	struct is_error_code_enum<::rpc::error>
	{
		static bool const value = true;
	};
	template<>
	struct is_error_condition_enum<::rpc::condition>
	{
		static bool const value = true;
	};
}
