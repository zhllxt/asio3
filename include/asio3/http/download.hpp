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

#include <asio3/core/asio.hpp>
#include <asio3/core/beast.hpp>
#include <asio3/core/timer.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/resolve.hpp>
#include <asio3/core/defer.hpp>
#include <asio3/core/root_certificates.hpp>

#if defined(ASIO3_ENABLE_SSL) || defined(ASIO3_USE_SSL)
#include <asio3/tcp/sslutil.hpp>
#endif

#include <asio3/http/util.hpp>
#include <asio3/http/make.hpp>
#include <asio3/http/url.hpp>
#include <asio3/http/read.hpp>
#include <asio3/http/write.hpp>

#include <asio3/proxy/handshake.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
	struct download_option
	{
	#if defined(ASIO3_ENABLE_SSL) || defined(ASIO3_USE_SSL)
		std::optional<std::reference_wrapper<asio::ssl::context>> sslctx;
	#endif
		std::string url;
		std::string data;
		std::map<std::string, std::string> headers;
		http::verb method = http::verb::get;
		std::function<bool(http::response<http::string_body>&)> on_head;
		std::function<bool(std::string_view)> on_chunk;
		std::optional<asio::stream_file> saved_file;
		std::optional<std::filesystem::path> saved_filepath;
		std::optional<socks5::option> socks5_option;
	};
}

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http::detail
#else
namespace boost::beast::http::detail
#endif
{
	struct async_download_op
	{
		download_option opt;

		auto operator()(auto state, auto ex_ref) -> void
		{
			const auto& ex = ex_ref.get();

			co_await asio::dispatch(ex, asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			if (!opt.saved_file.has_value() && !opt.saved_filepath.has_value())
				co_return{ asio::error::invalid_argument };

			asio::error_code ec{};

			http::url url;
			ec = url.reset(http::url_encode(opt.url));
			if (ec)
				co_return{ ec };

			asio::ip::tcp::socket sock{ ex };

			std::defer close_sock = [&sock]() mutable
			{
				asio::error_code ec{};
				sock.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
				sock.close(ec);
			};

			asio::ip::tcp::resolver resolver(ex);

			std::string addr{ url.get_host() };
			std::string port{ url.get_port() };

			if (opt.socks5_option.has_value())
			{
				socks5::option& s5opt = opt.socks5_option.value();
				addr = s5opt.proxy_address;
				port = std::to_string(s5opt.proxy_port);
			}

			// A successful resolve operation is guaranteed to pass a non-empty range to the handler.
			auto [e1, eps] = co_await asio::async_resolve(
				resolver, std::move(addr), std::move(port),
				asio::ip::resolver_base::flags(), asio::use_nothrow_deferred);
			if (e1)
				co_return{ e1 };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted };

			auto [e2, ep] = co_await asio::async_connect(
				sock, eps, asio::default_tcp_connect_condition{}, asio::use_nothrow_deferred);
			if (e2)
				co_return{ e2 };

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted };

			if (opt.socks5_option.has_value())
			{
				socks5::option& s5opt = opt.socks5_option.value();

				if (s5opt.method.empty())
					s5opt.method.emplace_back(socks5::auth_method::anonymous);
				if (s5opt.dest_address.empty())
					s5opt.dest_address = url.get_host();
				if (s5opt.dest_port == 0)
					s5opt.dest_port = static_cast<std::uint16_t>(std::stoul(std::string(url.get_port())));
				if (std::to_underlying(s5opt.cmd) == 0)
					s5opt.cmd = socks5::command::connect;

				auto [e3] = co_await socks5::async_handshake(sock, s5opt, asio::use_nothrow_deferred);
				if (e3)
					co_return{ e3 };
			}

			if (!!state.cancelled())
				co_return{ asio::error::operation_aborted };

			http::request<http::string_body> req{};
			req.method(opt.method);
			req.version(11);
			req.target(url.get_target());
			for (auto& [field_name, field_value] : opt.headers)
			{
				req.set(field_name, field_value);
			}

			req.body() = opt.data;
			try
			{
				req.prepare_payload();
			}
			catch (const std::exception&)
			{
				assert(false);
			}

			// Some sites must set the http::field::host
			if (req.find(http::field::host) == req.end())
			{
				std::string strhost{ url.get_host() };
				std::string strport{ url.get_port() };
				if (strport != "80" && strport != "443")
				{
					strhost += ":";
					strhost += strport;
				}
				req.set(http::field::host, strhost);
			}
			// Some sites must set the http::field::user_agent
			if (req.find(http::field::user_agent) == req.end())
			{
				req.set(http::field::user_agent,
					"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36");
			}

			if (!opt.saved_file.has_value())
			{
				asio::stream_file file(ex);
				file.open(opt.saved_filepath.value().string(),
					asio::file_base::write_only | asio::file_base::create | asio::file_base::truncate, ec);
				if (ec)
					co_return{ ec };
				opt.saved_file = std::move(file);
			}

			if (!opt.on_head)
				opt.on_head = [](http::response<http::string_body>&) { return true; };
			if (!opt.on_chunk)
				opt.on_chunk = [](std::string_view) { return true; };

			beast::flat_buffer buffer{};
			http::response_parser<http::string_body> parser;

			parser.body_limit((std::numeric_limits<std::size_t>::max)());

		#if defined(ASIO3_ENABLE_SSL) || defined(ASIO3_USE_SSL)
			// https://en.cppreference.com/w/cpp/memory/unique_ptr
			std::unique_ptr<asio::ssl::context, void(*)(asio::ssl::context*)> psslctx
			{
				nullptr,
				[](asio::ssl::context*) {}
			};
			if (opt.sslctx.has_value())
			{
				std::unique_ptr<asio::ssl::context, void(*)(asio::ssl::context*)> psslctx2
				{
					std::addressof(opt.sslctx.value().get()),
					[](asio::ssl::context*) {}
				};
				psslctx = std::move(psslctx2);
			}
			else
			{
				std::unique_ptr<asio::ssl::context, void(*)(asio::ssl::context*)> psslctx2
				{
					new asio::ssl::context(asio::ssl::context::tlsv12_client),
					[](asio::ssl::context* p) { delete p; }
				};
				asio::load_root_certificates(*psslctx2);
				//psslctx2->set_verify_mode(asio::ssl::verify_peer);
				psslctx = std::move(psslctx2);
			}

			std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket&>> stream;
			if (asio::iequals(url.get_schema(), "https"))
			{
				stream = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket&>>(sock, *psslctx);

				// Set SNI Hostname (many hosts need this to handshake successfully)
				// https://github.com/djarek/certify
				if (auto it = req.find(http::field::host); it != req.end())
				{
					std::string hostname(it->value());
					SSL_set_tlsext_host_name(stream->native_handle(), hostname.data());
				}

				auto [e3] = co_await stream->async_handshake(
					asio::ssl::stream_base::handshake_type::client, asio::use_nothrow_deferred);
				if (e3)
					co_return{ e3 };
			}

			if (stream)
			{
				auto [e4, n4] = co_await http::async_write(*stream, req, asio::use_nothrow_deferred);
				if (e4)
				{
					co_await stream->async_shutdown(asio::use_nothrow_deferred);
					co_return{ e4 };
				}

				if (!!state.cancelled())
				{
					co_await stream->async_shutdown(asio::use_nothrow_deferred);
					co_return{ asio::error::operation_aborted };
				}

				auto [e5, n5] = co_await http::async_read_header(
					*stream, buffer, parser, asio::use_nothrow_deferred);
				if (e5)
				{
					co_await stream->async_shutdown(asio::use_nothrow_deferred);
					co_return{ e5 };
				}

				if (!opt.on_head(parser.get()))
				{
					co_await stream->async_shutdown(asio::use_nothrow_deferred);
					co_return{ asio::error::operation_aborted };
				}

				auto [e6, n6] = co_await http::async_recv_file(
					*stream, opt.saved_file.value(), buffer, parser.get(), opt.on_chunk, asio::use_nothrow_deferred);
				if (e6)
				{
					co_await stream->async_shutdown(asio::use_nothrow_deferred);
					co_return{ e6 };
				}

				co_await stream->async_shutdown(asio::use_nothrow_deferred);
			}
			else
		#endif
			{
				auto [e4, n4] = co_await http::async_write(sock, req, asio::use_nothrow_deferred);
				if (e4)
					co_return{ e4 };

				if (!!state.cancelled())
					co_return{ asio::error::operation_aborted };

				auto [e5, n5] = co_await http::async_read_header(
					sock, buffer, parser, asio::use_nothrow_deferred);
				if (e5)
					co_return{ e5 };

				if (opt.on_head)
					opt.on_head(parser.get());

				auto [e6, n6] = co_await http::async_recv_file(
					sock, opt.saved_file.value(), buffer, parser.get(), opt.on_chunk, asio::use_nothrow_deferred);
				if (e6)
					co_return{ e6 };
			}

			co_return{ error_code{} };
        }
	};
}

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
/**
 * @brief Start an asynchronous http download.
 * @param opt - The request options.
 * @param token - The completion handler to invoke when the operation completes.
 *	  The equivalent function signature of the handler must be:
 *    @code
 *    void handler(const asio::error_code& ec);
 */
template<typename SendToken = asio::default_token_type<asio::tcp_socket>>
inline auto async_download(
	const auto& executor,
	download_option opt,
	SendToken&& token = asio::default_token_type<asio::tcp_socket>())
{
	return asio::async_initiate<SendToken, void(asio::error_code)>(
		asio::experimental::co_composed<void(asio::error_code)>(
			detail::async_download_op{ std::move(opt) }, executor),
		token,
		std::cref(executor));
}

}

#include <asio3/core/detail/pop_options.hpp>
