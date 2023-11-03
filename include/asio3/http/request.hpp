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

#include <asio3/http/detail/http_util.hpp>
#include <asio3/http/detail/http_url.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
namespace detail
{
	template<class Body, class Fields = http::fields>
	class http_request_impl_t : public http::message<true, Body, Fields>
	{
	public:
		using self  = http_request_impl_t<Body, Fields>;
		using super = http::message<true, Body, Fields>;
		using header_type = typename super::header_type;
		using body_type   = typename super::body_type;

	public:
		/**
		 * @brief constructor
		 */
		template<typename... Args>
		explicit http_request_impl_t(Args&&... args)
			: super(std::forward<Args>(args)...)
		{
		}

		http_request_impl_t(const http_request_impl_t& o) = default;
		http_request_impl_t(http_request_impl_t&& o) = default;
		self& operator=(const http_request_impl_t& o) = default;
		self& operator=(http_request_impl_t&& o) = default;

		http_request_impl_t(const http::message<true, Body, Fields>& req)
			: super()
		{
			this->base() = req;
		}

		http_request_impl_t(http::message<true, Body, Fields>&& req)
			: super()
		{
			this->base() = std::move(req);
		}

		self& operator=(const http::message<true, Body, Fields>& req)
		{
			this->base() = req;
			return *this;
		}

		self& operator=(http::message<true, Body, Fields>&& req)
		{
			this->base() = std::move(req);
			return *this;
		}

		/**
		 * @brief destructor
		 */
		~http_request_impl_t()
		{
		}

		/// Returns the base portion of the message
		inline super const& base() const noexcept
		{
			return *this;
		}

		/// Returns the base portion of the message
		inline super& base() noexcept
		{
			return *this;
		}

		inline void reset()
		{
			static_cast<super&>(*this) = {};
		}

	public:
		/**
		 * @brief Returns `true` if this HTTP request is a WebSocket Upgrade.
		 */
		inline bool is_upgrade() const noexcept { return websocket::is_upgrade(*this); }

		/**
		 * @brief Gets the content of the "schema" section, maybe empty
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 */
		inline std::string_view get_schema() const noexcept
		{
			return this->url_.schema();
		}

		/**
		 * @brief Gets the content of the "schema" section, maybe empty
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 * same as get_schema
		 */
		inline std::string_view schema() const noexcept
		{
			return this->get_schema();
		}

		/**
		 * @brief Gets the content of the "host" section, maybe empty
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 */
		inline std::string_view get_host() const noexcept
		{
			return this->url_.host();
		}

		/**
		 * @brief Gets the content of the "host" section, maybe empty
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 * same as get_host
		 */
		inline std::string_view host() const noexcept
		{
			return this->get_host();
		}

		/**
		 * @brief Gets the content of the "port" section, maybe empty
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 */
		inline std::string_view get_port() const noexcept
		{
			return this->url_.port();
		}

		/**
		 * @brief Gets the content of the "port" section, maybe empty
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 * same as get_port
		 */
		inline std::string_view port() const noexcept
		{
			return this->get_port();
		}

		/**
		 * @brief Gets the content of the "path" section of the target
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 */
		inline std::string_view get_path() const
		{
			if (!this->url_.string().empty())
				return this->url_.path();

			if (!(this->url_.parser().field_set & (1 << (int)http::parses::url_fields::UF_PATH)))
				return std::string_view{ "/" };

			std::string_view target = this->target();

			return std::string_view{ &target[
				this->url_.parser().field_data[(int)http::parses::url_fields::UF_PATH].off],
				this->url_.parser().field_data[(int)http::parses::url_fields::UF_PATH].len };
		}

		/**
		 * @brief Gets the content of the "path" section of the target
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 * same as get_path
		 */
		inline std::string_view path() const
		{
			return this->get_path();
		}

		/**
		 * @brief Gets the content of the "query" section of the target
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 */
		inline std::string_view get_query() const
		{
			if (!this->url_.string().empty())
				return this->url_.query();

			if (!(this->url_.parser().field_set & (1 << (int)http::parses::url_fields::UF_QUERY)))
				return std::string_view{};

			std::string_view target = this->target();

			return std::string_view{ &target[
				this->url_.parser().field_data[(int)http::parses::url_fields::UF_QUERY].off],
				this->url_.parser().field_data[(int)http::parses::url_fields::UF_QUERY].len };
		}

		/**
		 * @brief Gets the content of the "query" section of the target
		 * <scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>#<fragment>
		 * same as get_query
		 */
		inline std::string_view query() const
		{
			return this->get_query();
		}

		/**
		 * @brief Returns `true` if this HTTP request's Content-Type is "multipart/form-data";
		 */
		inline bool has_multipart() const noexcept
		{
			return http::has_multipart(*this);
		}

		/**
		 * @brief Get the "multipart/form-data" body content.
		 */
		inline decltype(auto) get_multipart()
		{
			return http::multipart(*this);
		}

		/**
		 * @brief Get the "multipart/form-data" body content. same as get_multipart
		 */
		inline decltype(auto) multipart()
		{
			return this->get_multipart();
		}

		/**
		 * @brief Get the url object reference. same as get_url
		 */
		inline http::url&    url() { return this->url_; }

		/**
		 * @brief Get the url object reference. same as get_url
		 */
		inline http::url const&    url() const { return this->url_; }

		/**
		 * @brief Get the url object reference.
		 */
		inline http::url& get_url() { return this->url_; }

		/**
		 * @brief Get the url object reference.
		 */
		inline http::url const& get_url() const { return this->url_; }

	protected:
		http::url                             url_{ "" };
		websocket::frame                      ws_frame_type_ = websocket::frame::unknown;
		std::string_view                      ws_frame_data_;
	};

	struct http_request_convert_helper
	{
		template<class Body, class Fields>
		void from(http::request<Body, Fields>) {}

		template<class Body, class Fields>
		void from(detail::http_request_impl_t<Body, Fields>) {}
	};

	template<typename T, typename = void>
	struct can_convert_to_http_request : std::false_type {};

	template<typename T>
	struct can_convert_to_http_request<T, std::void_t<decltype(std::declval<
		http_request_convert_helper>().from(std::declval<T>()))>> : std::true_type {};

	template<class T>
	inline constexpr bool can_convert_to_http_request_v =
		can_convert_to_http_request<std::remove_cvref_t<T>>::value;
}

using web_request = detail::http_request_impl_t<http::string_body>;
}

#include <asio3/core/detail/pop_options.hpp>
