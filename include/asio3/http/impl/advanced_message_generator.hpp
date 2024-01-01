//
// Copyright (c) 2022 Seth Heeren (sgheeren at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#pragma once

#include <asio3/http/advanced_message_generator.hpp>

#ifdef ASIO3_HEADER_ONLY
#include <asio3/bho/beast/core/buffers_generator.hpp>
#else
#include <boost/beast/core/buffers_generator.hpp>
#endif

#ifdef ASIO3_HEADER_ONLY
namespace bho
#else
namespace boost
#endif
{
namespace beast {
namespace http {

// The detail namespace means "not public"
namespace detail {

// This helper is needed for C++11.
// When invoked with a buffer sequence, writes the buffers `to the http::response<http::string_body>`.
template<class Serializer>
struct write_to_string_body_helper
{
    Serializer& sr_;
    http::response<http::string_body>& res_;

    write_to_string_body_helper(Serializer& sr, http::response<http::string_body>& res)
        : sr_(sr)
        , res_(res)
    {
    }

    // This function is called by the serializer
    template<class ConstBufferSequence>
    void
    operator()(error_code& ec, ConstBufferSequence const& buffers) const
    {
        // Error codes must be cleared on success
        ec = {};

        // Keep a running total of how much we wrote
        std::size_t bytes_transferred = 0;

        asio::const_buffer buffer{};

        // Loop over the buffer sequence
        if (auto it = asio::buffer_sequence_begin(buffers); it != asio::buffer_sequence_end(buffers))
        {
            // This is the next buffer in the sequence
            buffer = *it;

            // Adjust our running total
            bytes_transferred += asio::buffer_size(buffer);
        }

        // Inform the serializer of the amount we consumed
        sr_.consume(bytes_transferred);

        // Write it to the http::response<http::string_body>
		if (sr_.is_header_done())
            res_.body().append(reinterpret_cast<char const*>(buffer.data()), buffer.size());
    }
};

template <class BodyT, class FieldsT>
void reinit_response(http::response<BodyT, FieldsT>& m)
{
    if constexpr (std::same_as<std::decay_t<BodyT>, http::file_body>)
    {
        error_code ec{};
        m.body().seek(0, ec);
    }
}

template <class BodyT, class FieldsT>
std::expected<http::response<http::string_body>, error_code> to_string_body_response_impl(
    http::response<BodyT, FieldsT>& m)
{
    http::response<http::string_body> res{};
    static_cast<http::response<http::string_body>::header_type&>(res) = m;
	http::response_serializer<BodyT, FieldsT> sr{ m };

    // This lambda is used as the "visit" function
    detail::write_to_string_body_helper<decltype(sr)> lambda{ sr, res };
    error_code ec{};
    while (!sr.is_done())
    {
        // In C++14 we could use a generic lambda but since we want
        // to require only C++11, the lambda is written out by hand.
        // This function call retrieves the next serialized buffers.
        sr.next(ec, lambda);
        if (ec)
            break;
    }

    // if the response body is file_body, after serialize, we need seek the read pos to 0.
    // otherwise the next read for the file_body will failed.
    reinit_response(m);

    try
    {
        res.prepare_payload();
    }
    catch (const std::exception&)
    {
        return std::unexpected(asio::error::invalid_argument);
    }

    return res;
}

template <class BodyT, class FieldsT>
std::expected<http::response<http::string_body>, error_code> to_string_body_response_impl(http::request<BodyT, FieldsT>&)
{
    return std::unexpected(asio::error::invalid_argument);
}

template <class BodyT, class FieldsT>
http::response_header<>& get_response_header_impl(http::response<BodyT, FieldsT>& m) noexcept
{
    return static_cast<http::response_header<>&>(m);
}

template <class BodyT, class FieldsT>
http::response_header<>& get_response_header_impl(http::request<BodyT, FieldsT>&) noexcept
{
    static http::response_header<> s{};
    return s;
}

} // detail

template <bool isRequest, class Body, class Fields>
advanced_message_generator::advanced_message_generator(
    http::message<isRequest, Body, Fields>&& m)
    : impl_(std::make_unique<
            generator_impl<isRequest, Body, Fields>>(
          std::move(m)))
{
}

template <bool isRequest, class Body, class Fields>
struct advanced_message_generator::generator_impl final
    : advanced_message_generator::impl_base
{
    explicit generator_impl(
        http::message<isRequest, Body, Fields>&& m)
        : m_(std::move(m))
        , sr_(m_)
    {
    }

    bool
    is_done() override
    {
        return sr_.is_done();
    }

    const_buffers_type
    prepare(error_code& ec) override
    {
        sr_.next(ec, visit{*this});
        return current_;
    }

    void
    consume(std::size_t n) override
    {
        sr_.consume((std::min)(n, beast::buffer_bytes(current_)));
    }

    bool
    keep_alive() const noexcept override
    {
        return m_.keep_alive();
    }

    http::response_header<>&
    get_response_header() noexcept override
    {
        return detail::get_response_header_impl(m_);
    }

    std::expected<http::response<http::string_body>, error_code> to_string_body_response() override
    {
        return detail::to_string_body_response_impl(m_);
    }

private:
    static constexpr unsigned max_fixed_bufs = 12;

    http::message<isRequest, Body, Fields> m_;
    http::serializer<isRequest, Body, Fields> sr_;

    std::array<net::const_buffer, max_fixed_bufs> bs_;
    const_buffers_type current_ = bs_; // subspan

    struct visit
    {
        generator_impl& self_;

        template<class ConstBufferSequence>
        void
        operator()(error_code&, ConstBufferSequence const& buffers)
        {
            auto& s = self_.bs_;
            auto& cur = self_.current_;

            auto it = net::buffer_sequence_begin(buffers);

            std::size_t n =
                std::distance(it, net::buffer_sequence_end(buffers));

            n = (std::min)(s.size(), n);

            cur = { s.data(), n };
            std::copy_n(it, n, cur.begin());
        }
    };

};

template <bool isRequest, class Body, class Fields>
struct advanced_message_generator::ref_generator_impl final
    : advanced_message_generator::impl_base
{
    explicit ref_generator_impl(
        http::message<isRequest, Body, Fields>& m)
        : m_(m)
        , sr_(m_)
    {
    }

    bool
    is_done() override
    {
        return sr_.is_done();
    }

    const_buffers_type
    prepare(error_code& ec) override
    {
        sr_.next(ec, visit{*this});
        return current_;
    }

    void
    consume(std::size_t n) override
    {
        sr_.consume((std::min)(n, beast::buffer_bytes(current_)));
    }

    bool
    keep_alive() const noexcept override
    {
        return m_.keep_alive();
    }

    http::response_header<>&
    get_response_header() noexcept override
    {
        return detail::get_response_header_impl(m_);
    }

    std::expected<http::response<http::string_body>, error_code> to_string_body_response() override
    {
        return detail::to_string_body_response_impl(m_);
    }

private:
    static constexpr unsigned max_fixed_bufs = 12;

    http::message<isRequest, Body, Fields>& m_;
    http::serializer<isRequest, Body, Fields> sr_;

    std::array<net::const_buffer, max_fixed_bufs> bs_;
    const_buffers_type current_ = bs_; // subspan

    struct visit
    {
        ref_generator_impl& self_;

        template<class ConstBufferSequence>
        void
        operator()(error_code&, ConstBufferSequence const& buffers)
        {
            auto& s = self_.bs_;
            auto& cur = self_.current_;

            auto it = net::buffer_sequence_begin(buffers);

            std::size_t n =
                std::distance(it, net::buffer_sequence_end(buffers));

            n = (std::min)(s.size(), n);

            cur = { s.data(), n };
            std::copy_n(it, n, cur.begin());
        }
    };

};

template<typename>
advanced_message_generator::advanced_message_generator()
    : impl_(std::make_unique<
            generator_impl<false, http::string_body, http::fields>>(
          http::message<false, http::string_body, http::fields>(http::status::unknown, 11)))
{
}

template <bool isRequest, class Body, class Fields>
advanced_message_generator::advanced_message_generator(
    std::reference_wrapper<http::message<isRequest, Body, Fields>> m)
    : impl_(std::make_unique<
            ref_generator_impl<isRequest, Body, Fields>>(
          m.get()))
{
}

} // namespace http
} // namespace beast
} // namespace bho
