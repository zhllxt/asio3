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

#include <cctype>
#include <sstream>

#include <memory>
#include <expected>

#include <asio3/core/asio.hpp>
#include <asio3/core/beast.hpp>

#include <asio3/core/netutil.hpp>
#include <asio3/core/strutil.hpp>

#include <asio3/http/detail/http_parser.h>
#include <asio3/http/detail/mime_types.hpp>
#include <asio3/http/multipart.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
	namespace
	{
		// Percent-encoding : https://en.wikipedia.org/wiki/Percent-encoding

		// ---- RFC 3986 section 2.2 Reserved Characters (January 2005)
		// !#$&'()*+,/:;=?@[]
		// 
		// ---- RFC 3986 section 2.3 Unreserved Characters (January 2005)
		// ABCDEFGHIJKLMNOPQRSTUVWXYZ
		// abcdefghijklmnopqrstuvwxyz
		// 0123456789-_.~
		// 
		// 
		// http://www.baidu.com/query?key=x!#$&'()*+,/:;=?@[ ]-_.~%^{}\"|<>`\\y
		// 
		// C# System.Web.HttpUtility.UrlEncode
		// http%3a%2f%2fwww.baidu.com%2fquery%3fkey%3dx!%23%24%26%27()*%2b%2c%2f%3a%3b%3d%3f%40%5b+%5d-_.%7e%25%5e%7b%7d%22%7c%3c%3e%60%5cy
		// 
		// java.net.URLEncoder.encode
		// http%3A%2F%2Fwww.baidu.com%2Fquery%3Fkey%3Dx%21%23%24%26%27%28%29*%2B%2C%2F%3A%3B%3D%3F%40%5B+%5D-_.%7E%25%5E%7B%7D%5C%22%7C%3C%3E%60%5C%5Cy
		// 
		// postman
		// 
		// 
		// asp
		// http://127.0.0.1/index.asp?id=x!#$&name='()*+,/:;=?@[ ]-_.~%^{}\"|<>`\\y
		// http://127.0.0.1/index.asp?id=x%21%23%24&name=%27%28%29*%2B%2C%2F%3A%3B=?%40%5B%20%5D-_.%7E%25%5E%7B%7D%22%7C%3C%3E%60%5Cy
		// the character    &=?    can't be encoded, otherwise the result of queryString is wrong.
		// <%
		//   id=request.queryString("id")
		//   response.write "id=" & id
		//   response.write "</br>"
		//   name=request.queryString("name")
		//   response.write "name=" & name
		// %>
		// 
		// 
		// http%+y%2f%2fwww.baidu.com%2fquery%3fkey%3dx!%23%24%26%27()*%2b%2c%2f%3a%3b%3d%3f%40%5b+%5d-_.%7e%25%5e%7b%7d%22%7c%3c%3e%60%+5
		// 
		// C# System.Web.HttpUtility.UrlDecode
		// http% y//www.baidu.com/query?key=x!#$&'()*+,/:;=?@[ ]-_.~%^{}"|<>`% 5
		//

		static constexpr char unreserved_char[] = {
			//0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
			  0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 1, // 2
			  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, // 3
			  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 4
			  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 5
			  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 6
			  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 7
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // C
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // D
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // E
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // F
		};

		template<
			class CharT = char,
			class Traits = std::char_traits<CharT>,
			class Allocator = std::allocator<CharT>
		>
		std::basic_string<CharT, Traits, Allocator> url_decode(std::string_view url)
		{
			using size_type   = typename std::string_view::size_type;
			using value_type  = typename std::string_view::value_type;
			using rvalue_type = typename std::basic_string<CharT, Traits, Allocator>::value_type;

			std::basic_string<CharT, Traits, Allocator> r;
			r.reserve(url.size());

			if (url.empty())
				return r;

			for (size_type i = 0; i < url.size(); ++i)
			{
				value_type c = url[i];

				if (c == '%')
				{
					if (i + 3 <= url.size())
					{
						value_type h = url[i + 1];
						value_type l = url[i + 2];

						bool f1 = false, f2 = false;

						if /**/ (h >= '0' && h <= '9') { f1 = true; h = value_type(h - '0'     ); }
						else if (h >= 'a' && h <= 'f') { f1 = true; h = value_type(h - 'a' + 10); }
						else if (h >= 'A' && h <= 'F') { f1 = true; h = value_type(h - 'A' + 10); }

						if /**/ (l >= '0' && l <= '9') { f2 = true; l = value_type(l - '0'     ); }
						else if (l >= 'a' && l <= 'f') { f2 = true; l = value_type(l - 'a' + 10); }
						else if (l >= 'A' && l <= 'F') { f2 = true; l = value_type(l - 'A' + 10); }

						if (f1 && f2)
						{
							r += static_cast<rvalue_type>(h * 16 + l);
							i += 2;
						}
						else
						{
							r += static_cast<rvalue_type>(c);
						}
					}
					else
					{
						r += static_cast<rvalue_type>(c);
					}
				}
				else if (c == '+')
				{
					r += static_cast<rvalue_type>(' ');
				}
				else
				{
					r += static_cast<rvalue_type>(c);
				}
			}
			return r;
		}

		template<
			class CharT = char,
			class Traits = std::char_traits<CharT>,
			class Allocator = std::allocator<CharT>
		>
		std::basic_string<CharT, Traits, Allocator> url_encode(std::string_view url, std::size_t offset = 0)
		{
			using size_type   [[maybe_unused]] = typename std::string_view::size_type;
			using value_type  [[maybe_unused]] = typename std::string_view::value_type;
			using rvalue_type [[maybe_unused]] = typename std::basic_string<CharT, Traits, Allocator>::value_type;

			std::basic_string<CharT, Traits, Allocator> r;
			r.reserve(url.size() * 2);

			if (url.empty())
				return r;

			size_type i = 0;

			if (offset == 0)
			{
				http::parses::http_parser_url u;
				if (0 == http::parses::http_parser_parse_url(url.data(), url.size(), 0, std::addressof(u)))
				{
					if /**/ (u.field_set & (1 << (int)http::parses::url_fields::UF_PATH))
					{
						i = u.field_data[(int)http::parses::url_fields::UF_PATH].off + 1;
					}
					else if (u.field_set & (1 << (int)http::parses::url_fields::UF_PORT))
					{
						i = u.field_data[(int)http::parses::url_fields::UF_PORT].off +
							u.field_data[(int)http::parses::url_fields::UF_PORT].len;
					}
					else if (u.field_set & (1 << (int)http::parses::url_fields::UF_HOST))
					{
						i = u.field_data[(int)http::parses::url_fields::UF_HOST].off +
							u.field_data[(int)http::parses::url_fields::UF_HOST].len;
					}

					if constexpr (std::is_same_v<CharT, char>)
					{
						r += std::string_view{ url.data(), i };
					}
					else
					{
						for (size_type n = 0; n < i; ++n)
						{
							r += static_cast<rvalue_type>(url[n]);
						}
					}
				}
			}
			else
			{
				r += url.substr(0, offset);
				i = offset;
			}

			for (; i < url.size(); ++i)
			{
				unsigned char c = static_cast<unsigned char>(url[i]);

				if (/*std::isalnum(c) || */unreserved_char[c])
				{
					r += static_cast<rvalue_type>(c);
				}
				else
				{
					r += static_cast<rvalue_type>('%');
					rvalue_type h = rvalue_type(c >> 4);
					r += h > rvalue_type(9) ? rvalue_type(h + 55) : rvalue_type(h + 48);
					rvalue_type l = rvalue_type(c % 16);
					r += l > rvalue_type(9) ? rvalue_type(l + 55) : rvalue_type(l + 48);
				}
			}
			return r;
		}

		template<typename = void>
		bool has_unencode_char(std::string_view url, std::size_t offset = 0) noexcept
		{
			using size_type   = typename std::string_view::size_type;
			using value_type  = typename std::string_view::value_type;

			if (url.empty())
				return false;

			size_type i = 0;

			if (offset == 0)
			{
				http::parses::http_parser_url u;
				if (0 == http::parses::http_parser_parse_url(url.data(), url.size(), 0, std::addressof(u)))
				{
					if /**/ (u.field_set & (1 << (int)http::parses::url_fields::UF_PATH))
					{
						i = u.field_data[(int)http::parses::url_fields::UF_PATH].off + 1;
					}
					else if (u.field_set & (1 << (int)http::parses::url_fields::UF_PORT))
					{
						i = u.field_data[(int)http::parses::url_fields::UF_PORT].off +
							u.field_data[(int)http::parses::url_fields::UF_PORT].len;
					}
					else if (u.field_set & (1 << (int)http::parses::url_fields::UF_HOST))
					{
						i = u.field_data[(int)http::parses::url_fields::UF_HOST].off +
							u.field_data[(int)http::parses::url_fields::UF_HOST].len;
					}
				}
			}
			else
			{
				i = offset;
			}

			for (; i < url.size(); ++i)
			{
				unsigned char c = static_cast<unsigned char>(url[i]);

				if (c == static_cast<unsigned char>('%'))
				{
					if (i + 3 <= url.size())
					{
						value_type h = url[i + 1];
						value_type l = url[i + 2];

						if /**/ (h >= '0' && h <= '9') {}
						else if (h >= 'a' && h <= 'f') {}
						else if (h >= 'A' && h <= 'F') {}
						else { return true; }

						if /**/ (l >= '0' && l <= '9') {}
						else if (l >= 'a' && l <= 'f') {}
						else if (l >= 'A' && l <= 'F') {}
						else { return true; }

						i += 2;
					}
					else
					{
						return true;
					}
				}
				else if (!unreserved_char[c])
				{
					return true;
				}
			}
			return false;
		}

		template<typename = void>
		bool has_undecode_char(std::string_view url, std::size_t offset = 0) noexcept
		{
			using size_type   = typename std::string_view::size_type;
			using value_type  = typename std::string_view::value_type;

			if (url.empty())
				return false;

			for (size_type i = offset; i < url.size(); ++i)
			{
				value_type c = url[i];

				if (c == '%')
				{
					if (i + 3 <= url.size())
					{
						value_type h = url[i + 1];
						value_type l = url[i + 2];

						bool f1 = false, f2 = false;

						if /**/ (h >= '0' && h <= '9') { f1 = true; }
						else if (h >= 'a' && h <= 'f') { f1 = true; }
						else if (h >= 'A' && h <= 'F') { f1 = true; }

						if /**/ (l >= '0' && l <= '9') { f2 = true; }
						else if (l >= 'a' && l <= 'f') { f2 = true; }
						else if (l >= 'A' && l <= 'F') { f2 = true; }

						if (f1 && f2)
						{
							return true;
						}
					}
				}
				else if (c == '+')
				{
					return true;
				}
			}

			return false;
		}

		template<typename = void>
		std::string_view url_to_host(std::string_view url)
		{
			if (url.empty())
				return std::string_view{};

			http::parses::http_parser_url u;
			if (0 != http::parses::http_parser_parse_url(url.data(), url.size(), 0, std::addressof(u)))
				return std::string_view{};

			if (!(u.field_set & (1 << (int)http::parses::url_fields::UF_HOST)))
				return std::string_view{};

			return std::string_view{ &url[
				u.field_data[(int)http::parses::url_fields::UF_HOST].off],
				u.field_data[(int)http::parses::url_fields::UF_HOST].len };
		}

		template<typename = void>
		std::string_view url_to_port(std::string_view url)
		{
			if (url.empty())
				return std::string_view{};

			http::parses::http_parser_url u;
			if (0 != http::parses::http_parser_parse_url(url.data(), url.size(), 0, std::addressof(u)))
				return std::string_view{};

			if (u.field_set & (1 << (int)http::parses::url_fields::UF_PORT))
				return std::string_view{ &url[
					u.field_data[(int)http::parses::url_fields::UF_PORT].off],
					u.field_data[(int)http::parses::url_fields::UF_PORT].len };

			if (u.field_set & (1 << (int)http::parses::url_fields::UF_SCHEMA))
			{
				std::string_view schema(&url[
					u.field_data[(int)http::parses::url_fields::UF_SCHEMA].off],
					u.field_data[(int)http::parses::url_fields::UF_SCHEMA].len);
				if (asio::iequals(schema, "http"))
					return std::string_view{ "80" };
				if (asio::iequals(schema, "https"))
					return std::string_view{ "443" };
			}

			return std::string_view{ "80" };
		}

		template<typename = void>
		std::string_view url_to_path(std::string_view url)
		{
			if (url.empty())
				return std::string_view{};

			http::parses::http_parser_url u;
			if (0 != http::parses::http_parser_parse_url(url.data(), url.size(), 0, std::addressof(u)))
				return std::string_view{};

			if (!(u.field_set & (1 << (int)http::parses::url_fields::UF_PATH)))
				return std::string_view{ "/" };

			return std::string_view{ &url[
				u.field_data[(int)http::parses::url_fields::UF_PATH].off],
				u.field_data[(int)http::parses::url_fields::UF_PATH].len };
		}

		template<typename = void>
		std::string_view url_to_query(std::string_view url)
		{
			if (url.empty())
				return std::string_view{};

			http::parses::http_parser_url u;
			if (0 != http::parses::http_parser_parse_url(url.data(), url.size(), 0, std::addressof(u)))
				return std::string_view{};

			if (!(u.field_set & (1 << (int)http::parses::url_fields::UF_QUERY)))
				return std::string_view{};

			return std::string_view{ &url[
				u.field_data[(int)http::parses::url_fields::UF_QUERY].off],
				u.field_data[(int)http::parses::url_fields::UF_QUERY].len };
		}

		template<typename = void>
		inline bool url_match(std::string_view pattern, std::string_view url)
		{
			if (pattern == "*" || pattern == "/*")
				return true;

			if (url.empty())
				return false;

			std::vector<std::string_view> fragments = asio::split(pattern, "*");
			std::size_t index = 0;
			while (!url.empty())
			{
				if (index == fragments.size())
					return (pattern.back() == '*');
				std::string_view fragment = fragments[index++];
				if (fragment.empty())
					continue;
				while (fragment.size() > static_cast<std::string_view::size_type>(1) && fragment.back() == '/')
				{
					fragment.remove_suffix(1);
				}
				std::size_t pos = url.find(fragment);
				if (pos == std::string_view::npos)
					return false;
				url = url.substr(pos + fragment.size());
			}
			return true;
		}

		template<class StringT = std::string_view>
		inline std::string make_error_page(http::status result, StringT&& desc = std::string_view{})
		{
			std::string_view reason = http::obsolete_reason(result);
			std::string_view descrb = asio::to_string_view(desc);
			std::string content;
			if (descrb.empty())
				content.reserve(reason.size() * 2 + 67);
			else
				content.reserve(reason.size() * 2 + 67 + descrb.size() + 21);
			content += "<html><head><title>";
			content += reason;
			content += "</title></head><body><h1>";
			content += std::to_string(std::to_underlying(result));
			content += " ";
			content += reason;
			content += "</h1>";
			if (!descrb.empty())
			{
				content += "<p>Description : ";
				content += descrb;
				content += "</p>";
			}
			content += "</body></html>";
			return content;
		}

		template<class StringT = std::string_view>
		inline std::string error_page(http::status result, StringT&& desc = std::string_view{})
		{
			return make_error_page(result, std::forward<StringT>(desc));
		}

		/**
		 * @brief Returns `true` if the HTTP message's Content-Type is "multipart/form-data";
		 */
		template<class HttpMessage>
		inline bool has_multipart(const HttpMessage& msg) noexcept
		{
			return (asio::ifind(msg[http::field::content_type], "multipart/form-data") != std::string_view::npos);
		}

		/**
		 * @brief Get the "multipart/form-data" body content.
		 */
		template<class HttpMessage, class String = std::string>
		inline basic_multipart_fields<String> get_multipart(const HttpMessage& msg)
		{
			return multipart_parser_execute(msg);
		}

		/**
		 * @brief Get the "multipart/form-data" body content. same as get_multipart
		 */
		template<class HttpMessage, class String = std::string>
		inline basic_multipart_fields<String> multipart(const HttpMessage& msg)
		{
			return get_multipart(msg);
		}
	}

	// /boost_1_80_0/libs/beast/example/doc/http_examples.hpp
	template<bool isRequest, class SyncReadStream, class DynamicBuffer, class HeaderCallback, class BodyCallback>
	error_code read_large_body(SyncReadStream& stream, DynamicBuffer& buffer, HeaderCallback&& cbh, BodyCallback&& cbb)
	{
		error_code ec{};

		// Declare the parser with an empty body since
		// we plan on capturing the chunks ourselves.
		http::parser<isRequest, http::string_body> hp;

		// First read the complete header
		http::read_header(stream, buffer, hp, ec);
		if (ec)
			return ec;

		cbh(hp.get());

		// should we check the http reponse status?
		// should we handle the http range? 301 302 ?
		//if (hp.get().result() != http::status::ok)
		//{
		//	ec = http::error::bad_status;
		//	return ec;
		//}

		// if the http response has no body, returned with error.
		if (hp.is_done())
		{
			ec = http::error::end_of_stream;
			return ec;
		}

		http::parser<isRequest, http::buffer_body> p(std::move(hp));

		if (p.get().chunked())
		{
			// This container will hold the extensions for each chunk
			http::chunk_extensions ce;

			// This string will hold the body of each chunk
			//std::string chunk;

			// Declare our chunk header callback  This is invoked
			// after each chunk header and also after the last chunk.
			auto header_cb = [&](
				std::uint64_t size,          // Size of the chunk, or zero for the last chunk
				std::string_view extensions, // The raw chunk-extensions string. Already validated.
				error_code& ev)              // We can set this to indicate an error
			{
				// Parse the chunk extensions so we can access them easily
				ce.parse(extensions, ev);
				if (ev)
					return;

				// See if the chunk is too big
				if (size > (std::numeric_limits<std::size_t>::max)())
				{
					ev = http::error::body_limit;
					return;
				}

				// Make sure we have enough storage, and
				// reset the container for the upcoming chunk
				//chunk.reserve(static_cast<std::size_t>(size));
				//chunk.clear();
			};

			// Set the callback. The function requires a non-const reference so we
			// use a local variable, since temporaries can only bind to const refs.
			p.on_chunk_header(header_cb);

			// Declare the chunk body callback. This is called one or
			// more times for each piece of a chunk body.
			auto body_cb = [&](
				std::uint64_t remain,   // The number of bytes left in this chunk
				std::string_view body,  // A buffer holding chunk body data
				error_code& ev)         // We can set this to indicate an error
			{
				// If this is the last piece of the chunk body,
				// set the error so that the call to `read` returns
				// and we can process the chunk.
				if (remain == body.size())
					ev = http::error::end_of_chunk;

				// Append this piece to our container
				//chunk.append(body.data(), body.size());
				cbb(body);

				// The return value informs the parser of how much of the body we
				// consumed. We will indicate that we consumed everything passed in.
				return body.size();
			};
			p.on_chunk_body(body_cb);

			while (!p.is_done())
			{
				// Read as much as we can. When we reach the end of the chunk, the chunk
				// body callback will make the read return with the end_of_chunk error.
				http::read(stream, buffer, p, ec);
				if (!ec)
					continue;
				else if (ec != http::error::end_of_chunk)
					return ec;
				else
					ec = {};
			}
		}
		else
		{
			std::array<char, 512> buf;

			while (!p.is_done())
			{
				p.get().body().data = buf.data();
				p.get().body().size = buf.size();

				std::size_t bytes_read = http::read(stream, buffer, p, ec);

				if (ec == http::error::need_buffer)
					ec = {};
				if (ec)
					return ec;

				assert(bytes_read == buf.size() - p.get().body().size);

				cbb(std::string_view(buf.data(), bytes_read));
			}
		}

		return ec;
	}

	template<bool isRequest, class Body, class Fields>
	inline error_code try_prepare_payload(http::message<isRequest, Body, Fields>& msg)
	{
		try
		{
			msg.prepare_payload();
		}
		catch (const std::exception&)
		{
			return asio::error::invalid_argument;
		}
		return {};
	}

	template<typename T>
	concept is_http_message = requires(T& m)
	{
		typename T::header_type;
		typename T::body_type;
		requires std::same_as<T, http::message<
			T::header_type::is_request::value,
				typename T::body_type,
				typename T::header_type::fields_type>>;
	};

	/**
	 * @brief Respond to http request with plain text content
	 * @param content - the response body, it's usually a simple string,
	 * and the content-type is "text/plain" by default.
	 */
	inline std::expected<http::response<http::string_body>, error_code> make_text_response(
		asio::is_string auto&& content, http::status result = http::status::ok,
		std::string_view mimetype = "text/plain", unsigned version = 11)
	{
		http::response<http::string_body> rep{};

		rep.set(http::field::server, BEAST_VERSION_STRING);
		rep.set(http::field::content_type, mimetype.empty() ? "text/plain" : mimetype);

		rep.result(result);
		rep.version(version < 10 ? 11 : version);

		rep.body() = asio::to_string(std::forward_like<decltype(content)>(content));

		error_code ec = http::try_prepare_payload(*this);
		if (ec)
			return std::unexpected(ec);

		return rep;
	}

	/**
	 * @brief Respond to http request with json content
	 */
	inline std::expected<http::response<http::string_body>, error_code> make_json_response(
		asio::is_string auto&& content, http::status result = http::status::ok,
		std::string_view mimetype = "application/json", unsigned version = 11)
	{
		return make_text_response(std::forward_like<decltype(content)>(content), result,
			mimetype.empty() ? "application/json" : mimetype, version);
	}

	/**
	 * @brief Respond to http request with html content
	 * @param content - the response body, may be a plain text string, or a stardand
	 * <html>...</html> string, it's just that the content-type is "text/html" by default.
	 */
	inline std::expected<http::response<http::string_body>, error_code> make_html_response(
		asio::is_string auto&& content, http::status result = http::status::ok,
		std::string_view mimetype = "text/html", unsigned version = 11)
	{
		return make_text_response(std::forward_like<decltype(content)>(content), result,
			mimetype.empty() ? "text/html" : mimetype, version);
	}

	/**
	 * @brief Respond to http request with pre-prepared error page content
	 * Generated a standard html error page automatically use the status coe 'result',
	 * like <html>...</html>, and the content-type is "text/html" by default.
	 */
	template<class StringT = std::string_view>
	inline std::expected<http::response<http::string_body>, error_code> make_error_page_response(
		http::status result, StringT&& desc = std::string_view{},
		std::string_view mimetype = "text/html", unsigned version = 11)
	{
		return make_text_response(http::error_page(result, std::forward<StringT>(desc)), result,
			mimetype.empty() ? "text/html" : mimetype, version);
	}

	/**
	 * @brief Respond to http request with local file
	 */
	inline std::expected<http::response<http::file_body>, error_code> make_file_response(
		std::filesystem::path filepath,
		http::status result = http::status::ok, unsigned version = 11)
	{
		// Attempt to open the file
		beast::error_code ec;
		http::file_body::value_type body;
		body.open(filepath.string().c_str(), beast::file_mode::scan, ec);

		// Handle the case where the file doesn't exist
		if (ec == beast::errc::no_such_file_or_directory)
			return std::unexpected(ec);

		// Handle an unknown error
		if (ec)
			return std::unexpected(ec);

		// Cache the size since we need it after the move
		auto const size = body.size();

		// Respond to GET request
		http::response<http::file_body> res{
			std::piecewise_construct,
			std::make_tuple(std::move(body)),
			std::make_tuple(http::status::ok, version) };
		res.set(http::field::server, BEAST_VERSION_STRING);
		res.set(http::field::content_type, http::extension_to_mimetype(filepath.extension().string()));
		res.content_length(size);

		return res;
	}

	/**
	 * @brief Respond to http request with local file
	 */
	inline std::expected<http::response<http::file_body>, error_code> make_file_response(
		std::filesystem::path root_path,
		std::filesystem::path file_path,
		http::status result = http::status::ok, unsigned version = 11)
	{
		// if you want to build a absolute path by youself and passed it to fill_file function,
		// call set_root_directory("") first, then passed you absolute path to fill_file is ok.

		// Build the path to the requested file
		std::filesystem::path filepath;

		if (root_path.empty())
		{
			filepath = std::move(file_path);
			filepath.make_preferred();
		}
		else
		{
			filepath = root_path;
			filepath.make_preferred();
			filepath /= file_path.make_preferred().relative_path();
		}

		return make_file_response(std::move(filepath), result, version);
	}
}

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::websocket
#else
namespace boost::beast::websocket
#endif
{
	enum class frame : std::uint8_t
	{
		/// 
		unknown,

		/// A message frame was received
		message,

		/// A ping frame was received
		ping,

		/// A pong frame was received
		pong,

		/// http is upgrade to websocket
		open,

		/// A close frame was received
		close
	};
}
