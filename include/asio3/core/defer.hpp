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

#include <functional>

namespace asio
{
	class defer
	{
	public:
		defer() noexcept = default;

		// movable
		defer(defer&&) noexcept = default;
		defer& operator=(defer&&) noexcept = default;

		// non copyable
		defer(const defer&) = delete;
		void operator=(const defer&) = delete;

		template <typename Fun, typename... Args>
		requires std::invocable<Fun, Args...>
		defer(Fun&& fun, Args&&... args)
		{
			this->fn_ = std::bind(std::forward<Fun>(fun), std::forward<Args>(args)...);
		}

		template <typename Constructor, typename Destructor>
		requires (std::invocable<Constructor, void> && std::invocable<Destructor, void>)
		defer(Constructor&& ctor, Destructor&& dtor)
		{
			(ctor)();

			this->fn_ = std::forward<Destructor>(dtor);
		}

		~defer() noexcept
		{
			if (this->fn_) this->fn_();
		}

	protected:
		std::function<void()> fn_;
	};

#ifndef ASIO3_CONCAT
#define ASIO3_CONCAT(a, b) a##b
#endif
#define ASIO3_MAKE_DEFER(line) ::asio::defer ASIO3_CONCAT(_asio3_defer_, line) = [&]()
#define ASIO3_DEFER ASIO3_MAKE_DEFER(__LINE__)

}
