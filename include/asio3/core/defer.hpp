/*
 * Copyright (c) 2017-2023 zhllxt
 *
 * author   : zhllxt
 * email    : 37792738@qq.com
 * 
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 * refrenced from https://github.com/loveyacper/ananas
 */

#pragma once

#include <algorithm>

namespace std
{
	template<typename Function>
	class defer
	{
	public:
		// movable
		defer(defer&& rhs) noexcept : fn_(std::move(rhs.fn_)), valid_(rhs.valid_)
		{
			rhs.valid_ = false;
		}
		defer& operator=(defer&&) noexcept = delete;

		// non copyable
		defer(const defer&) = delete;
		void operator=(const defer&) = delete;

		template <typename Func>
		defer(Func&& fun) noexcept : fn_(std::forward<Func>(fun)), valid_(true)
		{
		}

		~defer() noexcept
		{
			if (valid_)
			{
				fn_();
			}
		}

	protected:
		Function fn_;
		bool valid_ = false;
	};

	template<typename Function>
	defer(Function) -> defer<Function>;

#ifndef ASIO3_CONCAT
#define ASIO3_CONCAT(a, b) a##b
#endif
#define ASIO3_MAKE_DEFER(line) ::asio::defer ASIO3_CONCAT(_asio3_defer_, line) = [&]()
#define ASIO3_DEFER ASIO3_MAKE_DEFER(__LINE__)

}
