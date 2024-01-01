//
// Copyright (c) 2022 Seth Heeren (sgheeren at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//
#pragma once

#include <asio3/config.hpp>

#include <expected>

#ifdef ASIO3_HEADER_ONLY
#include <asio3/bho/beast/core/span.hpp>
#include <asio3/bho/beast/http/message.hpp>
#include <asio3/bho/beast/http/serializer.hpp>
#include <asio3/bho/beast/http/string_body.hpp>
#else
#include <boost/beast/core/span.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/string_body.hpp>
#endif
#include <memory>

#ifdef ASIO3_HEADER_ONLY
namespace bho
#else
namespace boost
#endif
{
namespace beast {
namespace http {

/** Type-erased buffers generator for @ref http::message
   
    Implements the BuffersGenerator concept for any concrete instance of the
    @ref http::message template.
   
    @ref http::advanced_message_generator takes ownership of a message on construction,
    erasing the concrete type from the interface.
   
    This makes it practical for use in server applications to implement request
    handling:
   
    @code
    template <class Body, class Fields>
    http::advanced_message_generator handle_request(
        string_view doc_root,
        http::request<Body, Fields>&& request);
    @endcode
   
    The @ref beast::write and @ref beast::async_write operations are provided
    for BuffersGenerator. The @ref http::message::keep_alive property is made
    available for use after writing the message.
*/
class advanced_message_generator
{
public:
    using const_buffers_type = span<net::const_buffer>;

    template<typename = void>
    advanced_message_generator();

    template <bool isRequest, class Body, class Fields>
    advanced_message_generator(std::reference_wrapper<http::message<isRequest, Body, Fields>>);

    template <bool isRequest, class Body, class Fields>
    advanced_message_generator(http::message<isRequest, Body, Fields>&&);

    /// `BuffersGenerator`
    bool is_done() const {
        return impl_->is_done();
    }

    /// `BuffersGenerator`
    const_buffers_type
    prepare(error_code& ec)
    {
        return impl_->prepare(ec);
    }

    /// `BuffersGenerator`
    void
    consume(std::size_t n)
    {
        impl_->consume(n);
    }

    /// Returns the result of `m.keep_alive()` on the underlying message
    bool
    keep_alive() const noexcept
    {
        return impl_->keep_alive();
    }

    /// Returns the message header reference of the underlying message
    http::response_header<>&
    get_response_header() noexcept
    {
        return impl_->get_response_header();
    }

    std::expected<http::response<http::string_body>, error_code> to_string_body_response()
    {
        return impl_->to_string_body_response();
    }

private:
    struct impl_base
    {
        virtual ~impl_base() = default;
        virtual bool is_done() = 0;
        virtual const_buffers_type prepare(error_code& ec) = 0;
        virtual void consume(std::size_t n) = 0;
        virtual bool keep_alive() const noexcept = 0;
        virtual http::response_header<>& get_response_header() noexcept = 0;
        virtual std::expected<http::response<http::string_body>, error_code> to_string_body_response() = 0;
    };

    std::unique_ptr<impl_base> impl_;

    template <bool isRequest, class Body, class Fields>
    struct generator_impl;

    template <bool isRequest, class Body, class Fields>
    struct ref_generator_impl;
};

} // namespace http
} // namespace beast
} // namespace bho

#include <asio3/http/impl/advanced_message_generator.hpp>
