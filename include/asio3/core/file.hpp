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

#include <concepts>
#include <filesystem>

#include <asio3/core/asio.hpp>
#include <asio3/core/strutil.hpp>

#ifdef ASIO_STANDALONE
namespace asio
#else
namespace boost::asio
#endif
{
	using nothrow_stream_file = as_tuple_t<use_awaitable_t<>>::as_default_on_t<asio::stream_file>;
}

#ifdef ASIO_STANDALONE
namespace asio::detail
#else
namespace boost::asio::detail
#endif
{
	struct async_read_file_content_op
	{
		::std::string filepath;

		auto operator()(auto state, auto&& executor) -> void
		{
			auto ex = ::std::forward_like<decltype(executor)>(executor);

			co_await asio::dispatch(asio::use_deferred_executor(ex));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			::std::string buffer;

			asio::stream_file file(ex);

			error_code ec{};

			auto fsize = ::std::filesystem::file_size(filepath, ec);
			if (!ec)
			{
				buffer.reserve(::std::size_t(fsize));
			}

			file.open(filepath, asio::stream_file::read_only, ec);

			if (ec)
			{
				co_return{ ec, ::std::move(file), ::std::move(buffer) };
			}

			auto [e1, n1] = co_await asio::async_read(
				file, asio::dynamic_buffer(buffer), asio::transfer_all(), asio::use_deferred_executor(file));

			if (e1 == asio::error::eof)
			{
				e1 = {};
			}

			co_return{ e1, ::std::move(file), ::std::move(buffer) };
		}
	};

	struct async_write_file_op
	{
		::std::string filepath;

		auto operator()(auto state, auto&& executor, auto&& buffers, asio::file_base::flags open_flags) -> void
		{
			auto ex = ::std::forward_like<decltype(executor)>(executor);
			auto bs = ::std::forward_like<decltype(buffers)>(buffers);

			co_await asio::dispatch(asio::use_deferred_executor(ex));

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::stream_file file(ex);

			error_code ec{};

			file.open(filepath, open_flags, ec);

			if (ec)
			{
				co_return{ ec, ::std::move(file), 0 };
			}

			if (open_flags & file_base::append)
			{
				file.seek(0, file_base::seek_end, ec);

				if (ec)
				{
					co_return{ ec, ::std::move(file), 0 };
				}
			}

			auto [e1, n1] = co_await asio::async_write(file, bs, asio::use_deferred_executor(file));

			co_return{ e1, ::std::move(file), n1 };
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
	 * @brief Start an asynchronous operation to read a file.
	 * @param ex - executor
	 * @param filepath - The full file path.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::stream_file file, ::std::string content);
	 */
	template<
		typename Executor,
		typename ReadToken = asio::default_token_type<asio::nothrow_stream_file>>
	inline auto async_read_file_content(
		Executor&& ex,
		is_string auto&& filepath,
		ReadToken&& token = asio::default_token_type<asio::nothrow_stream_file>())
	{
		return async_initiate<ReadToken, void(asio::error_code, asio::stream_file, ::std::string)>(
			experimental::co_composed<void(asio::error_code, asio::stream_file, ::std::string)>(
				detail::async_read_file_content_op{
					asio::to_string(::std::forward_like<decltype(filepath)>(filepath)) }, ex),
			token, ex);
	}

	/**
	 * @brief Start an asynchronous operation to read a file.
	 * @param filepath - The full file path.
	 */
	template<typename = void>
	asio::awaitable<::std::tuple<asio::error_code, asio::stream_file, ::std::string>>
		async_read_file_content(is_string auto&& filepath)
	{
		auto ex = co_await asio::this_coro::executor;
		co_return co_await async_read_file_content(ex,
			::std::forward_like<decltype(filepath)>(filepath), asio::use_awaitable_executor(ex));
	}

	/**
	 * @brief Start an asynchronous operation to write file.
	 * @param ex - executor
	 * @param filepath - The full file path.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::stream_file file, ::std::size_t bytes_writen);
	 */
	template<
		typename Executor,
		typename ConstBufferSequence,
		typename ReadToken = asio::default_token_type<asio::nothrow_stream_file>>
	inline auto async_write_file(
		Executor&& ex,
		is_string auto&& filepath,
		const ConstBufferSequence& buffers,
		ReadToken&& token = asio::default_token_type<asio::nothrow_stream_file>())
	{
		return async_initiate<ReadToken, void(asio::error_code, asio::stream_file, ::std::size_t)>(
			experimental::co_composed<void(asio::error_code, asio::stream_file, ::std::size_t)>(
				detail::async_write_file_op{
					asio::to_string(::std::forward_like<decltype(filepath)>(filepath)) }, ex),
			token, ex, buffers,
			stream_file::write_only | stream_file::create | stream_file::truncate);
	}

	/**
	 * @brief Start an asynchronous operation to write file.
	 * @param ex - executor
	 * @param filepath - The full file path.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::stream_file file, ::std::size_t bytes_writen);
	 */
	template<
		typename Executor,
		typename ConstBufferSequence,
		typename ReadToken = asio::default_token_type<asio::nothrow_stream_file>>
	inline auto async_write_file(
		Executor&& ex,
		is_string auto&& filepath,
		const ConstBufferSequence& buffers,
		file_base::flags open_flags,
		ReadToken&& token = asio::default_token_type<asio::nothrow_stream_file>())
	{
		return async_initiate<ReadToken, void(asio::error_code, asio::stream_file, ::std::size_t)>(
			experimental::co_composed<void(asio::error_code, asio::stream_file, ::std::size_t)>(
				detail::async_write_file_op{
					asio::to_string(::std::forward_like<decltype(filepath)>(filepath)) }, ex),
			token, ex, buffers, open_flags);
	}

	/**
	 * @brief Start an asynchronous operation to write file.
	 * @param filepath - The full file path.
	 */
	template<typename ConstBufferSequence>
	asio::awaitable<::std::tuple<asio::error_code, asio::stream_file, ::std::size_t>> async_write_file(
		is_string auto&& filepath, const ConstBufferSequence& buffers,
		file_base::flags open_flags = stream_file::write_only | stream_file::create | stream_file::truncate)
	{
		auto ex = co_await asio::this_coro::executor;
		co_return co_await async_write_file(ex,
			::std::forward_like<decltype(filepath)>(filepath), buffers, open_flags,
			asio::use_awaitable_executor(ex));
	}
}
