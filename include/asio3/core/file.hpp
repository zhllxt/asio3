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

namespace asio
{
	using nothrow_stream_file = as_tuple_t<deferred_t>::as_default_on_t<asio::stream_file>;
}

namespace asio::detail
{
	struct async_read_file_op
	{
		auto operator()(auto state, auto&& ex, std::string filepath) -> void
		{
			co_await asio::dispatch(ex, asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			std::string buffer;

			asio::stream_file file(ex);

			error_code ec{};

			auto fsize = std::filesystem::file_size(filepath, ec);
			if (!ec)
			{
				buffer.reserve(std::size_t(fsize));
			}

			file.open(filepath, asio::stream_file::read_only, ec);

			if (ec)
			{
				co_return{ ec, std::move(file), std::move(buffer) };
			}

			auto [e1, n1] = co_await asio::async_read(
				file, asio::dynamic_buffer(buffer), asio::use_nothrow_deferred);

			if (e1 == asio::error::eof)
			{
				e1 = {};
			}

			co_return{ e1, std::move(file), std::move(buffer) };
		}
	};

	struct async_write_file_op
	{
		auto operator()(auto state, auto&& ex,
			std::string filepath, auto buffers, file_base::flags open_flags) -> void
		{
			co_await asio::dispatch(ex, asio::use_nothrow_deferred);

			state.reset_cancellation_state(asio::enable_terminal_cancellation());

			asio::stream_file file(ex);

			error_code ec{};

			file.open(filepath, open_flags, ec);

			if (ec)
			{
				co_return{ ec, std::move(file), 0 };
			}

			if (open_flags & file_base::append)
			{
				file.seek(0, file_base::seek_end, ec);

				if (ec)
				{
					co_return{ ec, std::move(file), 0 };
				}
			}

			auto [e1, n1] = co_await asio::async_write(file, buffers, asio::use_nothrow_deferred);

			co_return{ e1, std::move(file), n1 };
		}
	};
}

namespace asio
{
	/**
	 * @brief Start an asynchronous operation to read a file.
	 * @param ex - executor
	 * @param filepath - The full file path.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::stream_file file, std::string content);
	 */
	template<typename Executor, typename String,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::stream_file, std::string)) ReadToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::nothrow_stream_file::executor_type)>
	requires std::constructible_from<std::string, String>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, asio::stream_file, std::string))
	async_read_file(
		Executor&& ex,
		String&& filepath,
		ReadToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::nothrow_stream_file::executor_type))
	{
		return async_initiate<ReadToken, void(asio::error_code, asio::stream_file, std::string)>(
			experimental::co_composed<void(asio::error_code, asio::stream_file, std::string)>(
				detail::async_read_file_op{}, ex),
			token, ex, asio::to_string(std::forward<String>(filepath)));
	}

	/**
	 * @brief Start an asynchronous operation to read a file.
	 * @param filepath - The full file path.
	 */
	template<typename String>
	requires std::constructible_from<std::string, String>
	asio::awaitable<std::tuple<asio::error_code, asio::stream_file, std::string>> async_read_file(
		String&& filepath)
	{
		co_return co_await async_read_file(co_await asio::this_coro::executor,
			std::forward<String>(filepath), asio::use_nothrow_awaitable);
	}

	/**
	 * @brief Start an asynchronous operation to write file.
	 * @param ex - executor
	 * @param filepath - The full file path.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::stream_file file, std::size_t bytes_writen);
	 */
	template<typename Executor, typename String, typename ConstBufferSequence,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::stream_file, std::size_t)) ReadToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::nothrow_stream_file::executor_type)>
	requires std::constructible_from<std::string, String>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, asio::stream_file, std::size_t))
	async_write_file(
		Executor&& ex,
		String&& filepath,
		const ConstBufferSequence& buffers,
		ReadToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::nothrow_stream_file::executor_type))
	{
		return async_initiate<ReadToken, void(asio::error_code, asio::stream_file, std::size_t)>(
			experimental::co_composed<void(asio::error_code, asio::stream_file, std::size_t)>(
				detail::async_write_file_op{}, ex),
			token, ex, asio::to_string(std::forward<String>(filepath)), buffers,
			stream_file::write_only | stream_file::create | stream_file::truncate);
	}

	/**
	 * @brief Start an asynchronous operation to write file.
	 * @param ex - executor
	 * @param filepath - The full file path.
	 * @param token - The completion handler to invoke when the operation completes. 
	 *	  The equivalent function signature of the handler must be:
     *    @code
     *    void handler(const asio::error_code& ec, asio::stream_file file, std::size_t bytes_writen);
	 */
	template<typename Executor, typename String, typename ConstBufferSequence,
		ASIO_COMPLETION_TOKEN_FOR(void(asio::error_code, asio::stream_file, std::size_t)) ReadToken
		ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(typename asio::nothrow_stream_file::executor_type)>
	requires std::constructible_from<std::string, String>
	ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(ReadToken, void(asio::error_code, asio::stream_file, std::size_t))
	async_write_file(
		Executor&& ex,
		String&& filepath,
		const ConstBufferSequence& buffers,
		file_base::flags open_flags,
		ReadToken&& token ASIO_DEFAULT_COMPLETION_TOKEN(typename asio::nothrow_stream_file::executor_type))
	{
		return async_initiate<ReadToken, void(asio::error_code, asio::stream_file, std::size_t)>(
			experimental::co_composed<void(asio::error_code, asio::stream_file, std::size_t)>(
				detail::async_write_file_op{}, ex),
			token, ex, asio::to_string(std::forward<String>(filepath)), buffers, open_flags);
	}

	/**
	 * @brief Start an asynchronous operation to write file.
	 * @param filepath - The full file path.
	 */
	template<typename String, typename ConstBufferSequence>
	requires std::constructible_from<std::string, String>
	asio::awaitable<std::tuple<asio::error_code, asio::stream_file, std::size_t>> async_write_file(
		String&& filepath, const ConstBufferSequence& buffers,
		file_base::flags open_flags = stream_file::write_only | stream_file::create | stream_file::truncate)
	{
		co_return co_await async_write_file(co_await asio::this_coro::executor,
			std::forward<String>(filepath), buffers, open_flags, asio::use_nothrow_awaitable);
	}
}
