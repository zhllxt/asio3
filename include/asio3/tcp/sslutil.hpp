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

#include <asio3/core/netutil.hpp>
#include <asio3/core/with_lock.hpp>

#if defined(ASIO3_ENABLE_SSL)
#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct ssl_async_handshake_op
	{
		auto operator()(auto state, auto stream_ref,
			ssl::stream_base::handshake_type handsk_type,
			std::chrono::steady_clock::duration handsk_timeout) -> void
		{
			auto& ssl_stream = stream_ref.get();

			co_await asio::dispatch(asio::detail::get_lowest_executor(ssl_stream), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			[[maybe_unused]] detail::call_func_when_timeout wt(
				asio::detail::get_lowest_executor(ssl_stream), handsk_timeout,
				[&ssl_stream]() mutable
				{
					error_code ec{};
					ssl_stream.next_layer().close(ec);
				});

			auto [e1] = co_await ssl_stream.async_handshake(handsk_type, asio::use_nothrow_deferred);

			co_return e1;
		}
	};

	struct ssl_async_shutdown_op
	{
		auto operator()(auto state, auto stream_ref,
			std::chrono::steady_clock::duration shutdown_timeout) -> void
		{
			auto& ssl_stream = stream_ref.get();

			co_await asio::dispatch(asio::detail::get_lowest_executor(ssl_stream), asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			co_await asio::async_lock(ssl_stream, asio::use_nothrow_deferred);

			[[maybe_unused]] asio::defer_unlock defered_unlock{ ssl_stream };

			[[maybe_unused]] detail::call_func_when_timeout wt(
				asio::detail::get_lowest_executor(ssl_stream), shutdown_timeout,
				[&ssl_stream]() mutable
				{
					error_code ec{};
					ssl_stream.next_layer().close(ec);
				});

			auto [e1] = co_await ssl_stream.async_shutdown(asio::use_nothrow_deferred);

			co_return e1;
		}
	};
}

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	/**
	 *
	 * >> openssl create your certificates and sign them
	 * ------------------------------------------------------------------------------------------------
	 * // 1. Generate Server private key
	 * openssl genrsa -des3 -out server.key 1024
	 * // 2. Generate Server Certificate Signing Request(CSR)
	 * openssl req -new -key server.key -out server.csr -config openssl.cnf
	 * // 3. Generate Client private key
	 * openssl genrsa -des3 -out client.key 1024
	 * // 4. Generate Client Certificate Signing Request(CSR)
	 * openssl req -new -key client.key -out client.csr -config openssl.cnf
	 * // 5. Generate CA private key
	 * openssl genrsa -des3 -out ca.key 2048
	 * // 6. Generate CA Certificate file
	 * openssl req -new -x509 -key ca.key -out ca.crt -days 3650 -config openssl.cnf
	 * // 7. Generate Server Certificate file
	 * openssl ca -in server.csr -out server.crt -cert ca.crt -keyfile ca.key -config openssl.cnf
	 * // 8. Generate Client Certificate file
	 * openssl ca -in client.csr -out client.crt -cert ca.crt -keyfile ca.key -config openssl.cnf
	 * // 9. Generate dhparam file
	 * openssl dhparam -out dh1024.pem 1024
	 *
	 */

	 /**
	  * server set_verify_mode :
	  *   "verify_peer", ca_cert_buffer can be empty.
	  *      Whether the client has a certificate or not is ok.
	  *   "verify_fail_if_no_peer_cert", ca_cert_buffer can be empty.
	  *      Whether the client has a certificate or not is ok.
	  *   "verify_peer | verify_fail_if_no_peer_cert", ca_cert_buffer cannot be empty.
	  *      Client must use certificate, otherwise handshake will be failed.
	  * client set_verify_mode :
	  *   "verify_peer", ca_cert_buffer cannot be empty.
	  *   "verify_none", ca_cert_buffer can be empty.
	  * private_cert_buffer,private_key_buffer,private_password always cannot be empty.
	  */
	template<typename = void>
	inline error_code load_cert_from_string(
		asio::ssl::context& ssl_context,
		asio::ssl::verify_mode verify_mode,
		std::string_view ca_cert_buffer,
		std::string_view private_cert_buffer,
		std::string_view private_key_buffer,
		std::string_view private_password,
		std::string_view dh_buffer = {}
	) noexcept
	{
		error_code ec{};

		do
		{
			ssl_context.set_verify_mode(verify_mode, ec);
			if (ec)
				break;

			ssl_context.set_password_callback([password = std::string{ private_password }]
			(std::size_t max_length, asio::ssl::context_base::password_purpose purpose)->std::string
			{
				asio::ignore_unused(max_length, purpose);

				return password;
			}, ec);
			if (ec)
				break;

			assert(!private_cert_buffer.empty() && !private_key_buffer.empty());

			ssl_context.use_certificate(asio::buffer(private_cert_buffer), asio::ssl::context::pem, ec);
			if (ec)
				break;

			ssl_context.use_private_key(asio::buffer(private_key_buffer), asio::ssl::context::pem, ec);
			if (ec)
				break;

			if (!ca_cert_buffer.empty())
			{
				ssl_context.add_certificate_authority(asio::buffer(ca_cert_buffer), ec);
				if (ec)
					break;
			}

			// BIO_new_mem_buf -> SSL_CTX_set_tmp_dh
			if (!dh_buffer.empty())
			{
				ssl_context.use_tmp_dh(asio::buffer(dh_buffer), ec);
				if (ec)
					break;
			}
		} while (false);

		return ec;
	}

	/**
	 * server set_verify_mode :
	 *   "verify_peer", ca_cert_buffer can be empty.
	 *      Whether the client has a certificate or not is ok.
	 *   "verify_fail_if_no_peer_cert", ca_cert_buffer can be empty.
	 *      Whether the client has a certificate or not is ok.
	 *   "verify_peer | verify_fail_if_no_peer_cert", ca_cert_buffer cannot be empty.
	 *      Client must use certificate, otherwise handshake will be failed.
	 * client set_verify_mode :
	 *   "verify_peer", ca_cert_buffer cannot be empty.
	 *   "verify_none", ca_cert_buffer can be empty.
	 * private_cert_buffer,private_key_buffer,private_password always cannot be empty.
	 */
	template<typename = void>
	inline error_code load_cert_from_file(
		asio::ssl::context& ssl_context,
		asio::ssl::verify_mode verify_mode,
		const std::string& ca_cert_file,
		const std::string& private_cert_file,
		const std::string& private_key_file,
		const std::string& private_password,
		const std::string& dh_file = {}
	) noexcept
	{
		error_code ec{};

		do
		{
			ssl_context.set_verify_mode(verify_mode, ec);
			if (ec)
				break;

			ssl_context.set_password_callback([password = private_password]
			(std::size_t max_length, asio::ssl::context_base::password_purpose purpose)->std::string
			{
				asio::ignore_unused(max_length, purpose);

				return password;
			}, ec);
			if (ec)
				break;

			assert(!private_cert_file.empty() && !private_key_file.empty());

			ssl_context.use_certificate_chain_file(private_cert_file, ec);
			if (ec)
				break;

			ssl_context.use_private_key_file(private_key_file, asio::ssl::context::pem, ec);
			if (ec)
				break;

			if (!ca_cert_file.empty())
			{
				ssl_context.load_verify_file(ca_cert_file, ec);
				if (ec)
					break;
			}

			// BIO_new_file -> SSL_CTX_set_tmp_dh
			if (!dh_file.empty())
			{
				ssl_context.use_tmp_dh_file(dh_file, ec);
				if (ec)
					break;
			}
		} while (false);

		return ec;
	}

	/// Start an asynchronous SSL handshake.
	/**
	 * This function is used to asynchronously perform an SSL handshake on the
	 * stream. It is an initiating function for an @ref asynchronous_operation,
	 * and always returns immediately.
	 *
	 * @param type The type of handshaking to be performed, i.e. as a client or as
	 * a server.
	 *
	 * @param token The @ref completion_token that will be used to produce a
	 * completion handler, which will be called when the handshake completes.
	 * Potential completion tokens include @ref use_future, @ref use_awaitable,
	 * @ref yield_context, or a function object with the correct completion
	 * signature. The function signature of the completion handler must be:
	 * @code void handler(
	 *   const asio::error_code& error // Result of operation.
	 * ); @endcode
	 * Regardless of whether the asynchronous operation completes immediately or
	 * not, the completion handler will not be invoked from within this function.
	 * On immediate completion, invocation of the handler will be performed in a
	 * manner equivalent to using asio::post().
	 *
	 * @par Completion Signature
	 * @code void(asio::error_code) @endcode
	 *
	 * @par Per-Operation Cancellation
	 * This asynchronous operation supports cancellation for the following
	 * asio::cancellation_type values:
	 *
	 * @li @c cancellation_type::terminal
	 *
	 * @li @c cancellation_type::partial
	 *
	 * if they are also supported by the @c Stream type's @c async_read_some and
	 * @c async_write_some operations.
	 */
	template <
		typename SslStream,
		typename HandshakeToken = asio::default_token_type<SslStream>>
	inline auto async_handshake(
		SslStream& ssl_stream,
		ssl::stream_base::handshake_type handsk_type,
		HandshakeToken&& token = asio::default_token_type<SslStream>())
	{
		return async_initiate<HandshakeToken, void(asio::error_code)>(
			experimental::co_composed<void(asio::error_code)>(
				detail::ssl_async_handshake_op{}, ssl_stream),
			token,
			std::ref(ssl_stream), handsk_type,
			asio::ssl_handshake_timeout);
	}

	template <
		typename SslStream,
		typename HandshakeToken = asio::default_token_type<SslStream>>
	inline auto async_handshake(
		SslStream& ssl_stream,
		ssl::stream_base::handshake_type handsk_type,
		std::chrono::steady_clock::duration handsk_timeout,
		HandshakeToken&& token = asio::default_token_type<SslStream>())
	{
		return async_initiate<HandshakeToken, void(asio::error_code)>(
			experimental::co_composed<void(asio::error_code)>(
				detail::ssl_async_handshake_op{}, ssl_stream),
			token,
			std::ref(ssl_stream), handsk_type,
			handsk_timeout);
	}

	/// Asynchronously shut down SSL on the stream.
	/**
	 * This function is used to asynchronously shut down SSL on the stream. It is
	 * an initiating function for an @ref asynchronous_operation, and always
	 * returns immediately.
	 *
	 * @param token The @ref completion_token that will be used to produce a
	 * completion handler, which will be called when the shutdown completes.
	 * Potential completion tokens include @ref use_future, @ref use_awaitable,
	 * @ref yield_context, or a function object with the correct completion
	 * signature. The function signature of the completion handler must be:
	 * @code void handler(
	 *   const asio::error_code& error // Result of operation.
	 * ); @endcode
	 * Regardless of whether the asynchronous operation completes immediately or
	 * not, the completion handler will not be invoked from within this function.
	 * On immediate completion, invocation of the handler will be performed in a
	 * manner equivalent to using asio::post().
	 *
	 * @par Completion Signature
	 * @code void(asio::error_code) @endcode
	 *
	 * @par Per-Operation Cancellation
	 * This asynchronous operation supports cancellation for the following
	 * asio::cancellation_type values:
	 *
	 * @li @c cancellation_type::terminal
	 *
	 * @li @c cancellation_type::partial
	 *
	 * if they are also supported by the @c Stream type's @c async_read_some and
	 * @c async_write_some operations.
	 */
	template <
		typename SslStream,
		typename ShutdownToken = asio::default_token_type<SslStream>>
	inline auto async_shutdown(
		SslStream& ssl_stream,
		ShutdownToken&& token = asio::default_token_type<SslStream>())
	{
		return async_initiate<ShutdownToken, void(asio::error_code)>(
			experimental::co_composed<void(asio::error_code)>(
				detail::ssl_async_shutdown_op{}, ssl_stream),
			token,
			std::ref(ssl_stream),
			asio::ssl_shutdown_timeout);
	}

	template <
		typename SslStream,
		typename ShutdownToken = asio::default_token_type<SslStream>>
	inline auto async_shutdown(
		SslStream& ssl_stream,
		std::chrono::steady_clock::duration shutdown_timeout,
		ShutdownToken&& token = asio::default_token_type<SslStream>())
	{
		return async_initiate<ShutdownToken, void(asio::error_code)>(
			experimental::co_composed<void(asio::error_code)>(
				detail::ssl_async_shutdown_op{}, ssl_stream),
			token,
			std::ref(ssl_stream),
			shutdown_timeout);
	}
}
#endif
