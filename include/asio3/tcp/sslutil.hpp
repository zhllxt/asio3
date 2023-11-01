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

#if defined(ASIO3_ENABLE_SSL)
namespace asio
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
	inline error_code set_cert_buffer(
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
	inline error_code set_cert_file(
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
}
#endif
