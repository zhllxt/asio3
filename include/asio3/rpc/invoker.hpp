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

#include <cstdint>
#include <functional>
#include <string>
#include <tuple>
#include <unordered_map>

#include <asio3/core/function_traits.hpp>
#include <asio3/core/strutil.hpp>
#include <asio3/core/stdutil.hpp>
#include <asio3/core/netutil.hpp>

#include <asio3/rpc/serialization.hpp>
#include <asio3/rpc/message.hpp>

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	template<typename SerializerT, typename DeserializerT, typename... Ts>
	class basic_invoker
	{
	protected:
		struct dummy {};

	public:
		using self = basic_invoker<SerializerT, DeserializerT, Ts...>;
		using serializer_type = SerializerT;
		using deserializer_type = DeserializerT;
		using function_type = std::function<
			asio::awaitable<std::string>(serializer_type&, deserializer_type&, rpc::header, Ts...)>;

		/**
		 * @brief constructor
		 */
		basic_invoker() = default;

		/**
		 * @brief destructor
		 */
		~basic_invoker() = default;

	protected:
		template<typename F>
		inline void _bind(std::string name, F f)
		{
			this->invokers_[std::move(name)] = std::make_shared<function_type>(
				std::bind_front(&self::template _proxy<F, dummy>, std::move(f), nullptr));
		}

		template<typename F, typename C>
		inline void _bind(std::string name, F f, C& c)
		{
			this->invokers_[std::move(name)] = std::make_shared<function_type>(
				std::bind_front(&self::template _proxy<F, C>, std::move(f), std::addressof(c)));
		}

		template<typename F, typename C>
		inline void _bind(std::string name, F f, C* c)
		{
			this->invokers_[std::move(name)] = std::make_shared<function_type>(
				std::bind_front(&self::template _proxy<F, C>, std::move(f), c));
		}

		template<typename F, typename C, class... TS>
		inline static asio::awaitable<std::string> _proxy(
			F& f, C* c, serializer_type& sr, deserializer_type& dr, rpc::header head, TS&&... ts)
		{
			using fun_traits_type = function_traits<F>;
			using fun_params_tuple = typename fun_traits_type::pod_tuple_type;

			fun_params_tuple tp;
			asio::for_each_tuple(tp, [&dr](auto& elem) mutable
			{
				dr >> elem;
			});

			co_return co_await _invoke(f, c, sr, dr,
				std::move(head), std::move(tp), std::move(ts)...);
		}

		template<typename F, typename C, typename... Args, class... TS>
		inline static asio::awaitable<std::string> _invoke(
			F& f, C* c, serializer_type& sr, deserializer_type& dr,
			rpc::header head, std::tuple<Args...>&& tp, TS&&... ts)
		{
			using fun_traits_type = function_traits<F>;
			using fun_return_type = typename fun_traits_type::return_type;

			if constexpr (std::same_as<fun_return_type, asio::awaitable<void>>)
			{
				co_await _invoke_impl<fun_return_type>(f, c,
					std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), std::forward<TS>(ts)...);

				sr.reset();
				sr << head;
				sr << rpc::make_error_code(rpc::error::success);

				co_return sr.str();
			}
			else
			{
				auto r = co_await _invoke_impl<fun_return_type>(f, c,
					std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), std::forward<TS>(ts)...);

				sr.reset();
				sr << head;
				sr << rpc::make_error_code(rpc::error::success);
				sr << r;

				co_return sr.str();
			}
		}

		template<typename R, typename F, typename C, std::size_t... I, typename... Args, class... TS>
		inline static R _invoke_impl(F& f, C* c, std::index_sequence<I...>, std::tuple<Args...>&& tp, TS&&... ts)
		{
			asio::ignore_unused(c);

			if constexpr (std::same_as<asio::remove_cvref_t<C>, dummy>)
				co_return co_await f(std::get<I>(std::move(tp))..., std::forward<TS>(ts)...);
			else
				co_return co_await(c->*f)(std::get<I>(std::move(tp))..., std::forward<TS>(ts)...);
		}

	public:
		/**
		 * @brief bind a rpc function
		 * @param name - Function name in string format.
		 * @param fun - Function object.
		 * @param obj - A pointer or reference to a class object, this parameter can be none.
		 * if fun is nonmember function, the obj param must be none, otherwise the obj must be the
		 * the class object's pointer or reference.
		 */
		template<typename F, typename ...C>
		inline self& bind(std::string name, F&& fun, C&&... obj)
		{
			asio::trim_both(name);

			assert(!name.empty());
			if (name.empty())
				return (*this);

		#if !defined(NDEBUG)
			assert(this->invokers_.find(name) == this->invokers_.end());
		#endif

			this->_bind(std::move(name), std::forward<F>(fun), std::forward<C>(obj)...);

			return (*this);
		}

		/**
		 * @brief unbind a rpc function
		 */
		inline self& unbind(std::string const& name)
		{
			this->invokers_.erase(name);

			return (*this);
		}

		/**
		 * @brief find binded rpc function by name
		 */
		inline std::shared_ptr<function_type> find(std::string const& name)
		{
			if (auto it = this->invokers_.find(name); it != this->invokers_.end())
				return it->second;

			return nullptr;
		}

		/**
		 * @brief invoke the binded rpc function
		 */
		template<typename... TS>
		inline asio::awaitable<std::tuple<asio::error_code, std::string>> invoke(
			auto&& sr, auto&& dr, rpc::header head, auto&& data, TS&&... ts)
		{
			head.type = rpc::response_mark;
		#if !defined(ASIO_NO_EXCEPTIONS) && !defined(BOOST_ASIO_NO_EXCEPTIONS)
			try
			{
		#endif
				if (auto it = this->invokers_.find(head.method); it != this->invokers_.end())
				{
					std::string resp = co_await(*(it->second))(sr, dr, std::move(head), std::forward<TS>(ts)...);

					co_return std::tuple{ asio::error_code{}, std::move(resp) };
				}
				else
				{
					sr.reset();
					sr << head;
					sr << rpc::make_error_code(rpc::error::method_not_found);

					co_return std::tuple{ rpc::make_error_code(rpc::error::method_not_found), sr.str() };
				}
		#if !defined(ASIO_NO_EXCEPTIONS) && !defined(BOOST_ASIO_NO_EXCEPTIONS)
			}
			catch (const cereal::exception&)
			{
				sr.reset();
				sr << head;
				sr << rpc::make_error_code(rpc::error::parse_error);

				co_return std::tuple{ rpc::make_error_code(rpc::error::parse_error), sr.str() };
			}
			catch (const std::exception&)
			{
				sr.reset();
				sr << head;
				sr << rpc::make_error_code(rpc::error::internal_error);

				co_return std::tuple{ rpc::make_error_code(rpc::error::internal_error), sr.str() };
			}
		#endif
		}

	protected:
		std::unordered_map<std::string, std::shared_ptr<function_type>> invokers_;
	};

	using invoker = basic_invoker<rpc::serializer, rpc::deserializer>;
}
