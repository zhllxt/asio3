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
#include <asio3/core/asio_buffer_specialization.hpp>
#include <asio3/tcp/core.hpp>

namespace asio
{
template <typename AsyncWriteStream, typename ConstBufferSequence>
auto async_write(AsyncWriteStream& s, const ConstBufferSequence& buffers, detail::transfer_all_t completion_condition)
{
	return async_write(s, buffers, std::move(completion_condition),
		::asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename Allocator>
auto async_write(AsyncWriteStream& s, basic_streambuf<Allocator>& b, detail::transfer_all_t completion_condition)
{
	return async_write(s, b, std::move(completion_condition),
		::asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename ConstBufferSequence>
auto async_write(AsyncWriteStream& s, const ConstBufferSequence& buffers, detail::transfer_at_least_t completion_condition)
{
	return async_write(s, buffers, std::move(completion_condition),
		::asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename Allocator>
auto async_write(AsyncWriteStream& s, basic_streambuf<Allocator>& b, detail::transfer_at_least_t completion_condition)
{
	return async_write(s, b, std::move(completion_condition),
		::asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename ConstBufferSequence>
auto async_write(AsyncWriteStream& s, const ConstBufferSequence& buffers, detail::transfer_exactly_t completion_condition)
{
	return async_write(s, buffers, std::move(completion_condition),
		::asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}

template <typename AsyncWriteStream, typename Allocator>
auto async_write(AsyncWriteStream& s, basic_streambuf<Allocator>& b, detail::transfer_exactly_t completion_condition)
{
	return async_write(s, b, std::move(completion_condition),
		::asio::default_completion_token<typename AsyncWriteStream::executor_type>::type());
}
}
