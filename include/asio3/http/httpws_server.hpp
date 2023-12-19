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

#include <asio3/http/http_server.hpp>
#include <asio3/http/ws_session.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	template<
		typename HttpSessionT = http_session,
		typename WsSessionT = ws_session,
		typename RouterT = http::basic_router<
			typename HttpSessionT::request_type, typename HttpSessionT::response_type>
	>
	class basic_httpws_server : public basic_http_server<HttpSessionT, RouterT>
	{
	public:
		using super = basic_http_server<HttpSessionT, RouterT>;
		using http_session_type = HttpSessionT;
		using ws_session_type = WsSessionT;

	public:
		/**
		 * @brief constructor
		 */
		explicit basic_httpws_server(const auto& ex)
			: super(ex)
			, http_session_map(super::session_map)
			, ws_session_map(ex)
		{
		}

		basic_httpws_server(basic_httpws_server&&) noexcept = default;
		basic_httpws_server& operator=(basic_httpws_server&&) noexcept = default;

		/**
		 * @brief destructor
		 */
		~basic_httpws_server()
		{
		}

		/**
		 * @brief Asynchronously stop the server.
		 */
		template<typename StopToken = asio::default_token_type<asio::tcp_acceptor>>
		inline auto async_stop(
			StopToken&& token = asio::default_token_type<asio::tcp_acceptor>())
		{
			return asio::async_initiate<StopToken, void(error_code)>(
				experimental::co_composed<void(error_code)>(
					[](auto state, auto self_ref) -> void
					{
						auto& self = self_ref.get();

						co_await asio::dispatch(self.get_executor(), use_nothrow_deferred);

						co_await self.base().async_stop(use_nothrow_deferred);

						co_await self.http_session_map.async_disconnect_all(use_nothrow_deferred);
						co_await self.ws_session_map.async_disconnect_all(use_nothrow_deferred);

						co_return error_code{};
					}, this->acceptor), token, std::ref(*this));
		}

	public:
		session_map<http_session_type>& http_session_map;

		session_map<ws_session_type>    ws_session_map;

	protected:
		using super::session_map;
	};

	using httpws_server = basic_httpws_server<http_session, ws_session>;
}
