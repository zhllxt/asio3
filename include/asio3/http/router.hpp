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

#include <asio3/core/detail/push_options.hpp>

#include <cstdint>
#include <memory>
#include <chrono>
#include <functional>
#include <atomic>
#include <string>
#include <string_view>
#include <queue>
#include <any>
#include <future>
#include <tuple>
#include <map>
#include <unordered_map>
#include <type_traits>
#include <filesystem>

#include <asio3/core/asio.hpp>
#include <asio3/core/beast.hpp>

#include <asio3/core/function_traits.hpp>
#include <asio3/core/stdutil.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/strutil.hpp>

#include <asio3/http/util.hpp>
#include <asio3/http/cache.hpp>
#include <asio3/http/make.hpp>
#include <asio3/http/core.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
	template<typename RequestT, typename ResponseT, typename... Ts>
	class basic_router
	{
	protected:
		template<class T, class R, class... Args>
		struct has_member_before : std::false_type {};

		template<class T, class... Args>
		struct has_member_before<T, decltype(std::declval<std::decay_t<T>>().
			before((std::declval<Args>())...)), Args...> : std::true_type {};

		template<class T, class R, class... Args>
		struct has_member_after : std::false_type {};

		template<class T, class... Args>
		struct has_member_after<T, decltype(std::declval<std::decay_t<T>>().
			after((std::declval<Args>())...)), Args...> : std::true_type {};

		template< typename T>
		struct filter_helper
		{
			static constexpr auto func()
			{
				return std::tuple<>();
			}

			template< class... Args >
			static constexpr auto func(T&&, Args&&...args)
			{
				return filter_helper::func(std::forward<Args>(args)...);
			}

			template< class... Args >
			static constexpr auto func(const T&, Args&&...args)
			{
				return filter_helper::func(std::forward<Args>(args)...);
			}

			template< class X, class... Args >
			static constexpr auto func(X&& x, Args&&...args)
			{
				return std::tuple_cat(std::make_tuple(std::forward<X>(x)),
					filter_helper::func(std::forward<Args>(args)...));
			}
		};

		template<typename T, typename... Args>
		inline auto filter_cache(Args&&... args)
		{
			return filter_helper<T>::func(std::forward<Args>(args)...);
		}

		struct dummy {};

	public:
		using self  = basic_router<RequestT, ResponseT, Ts...>;
		using request_type = RequestT;
		using response_type = ResponseT;
		using return_type = asio::awaitable<bool>;
		using function_type = std::function<return_type(RequestT&, ResponseT&, Ts...)>;
		using cache_type = cache;

		/**
		 * @brief constructor
		 */
		basic_router()
		{
			this->not_found_router_ = std::make_shared<function_type>(
			[](RequestT& req, ResponseT& rep, Ts...) mutable -> return_type
			{
				std::string desc;
				desc.reserve(64);
				desc += "The resource for ";
				desc += req.method_string();
				desc += " \"";
				desc += http::url_decode(req.target());
				desc += "\" was not found";

				rep = http::make_error_page_response(
					http::status::not_found, std::move(desc), "text/html", req.version());

				co_return true;
			});
		}

		/**
		 * @brief destructor
		 */
		~basic_router() = default;

	protected:
		template<http::verb... M>
		inline decltype(auto) _make_uris(std::string name)
		{
			std::size_t index = 0;
			std::array<std::string, sizeof...(M)> uris;
			//while (!name.empty() && name.back() == '*')
			//	name.erase(std::prev(name.end()));
			while (name.size() > static_cast<std::string::size_type>(1) && name.back() == '/')
				name.erase(std::prev(name.end()));
			assert(!name.empty());
			if (!name.empty())
			{
				((uris[index++] = std::string{ this->_to_char(M) } +name), ...);
			}
			return uris;
		}

		template<http::verb... M, class F, class C, class... AOP>
		inline void _add_impl(std::string name, F f, C* c, AOP&&... aop)
		{
			constexpr bool CacheFlag = asio::has_type<
				http::enable_cache_t, std::tuple<std::decay_t<AOP>...>>::value;

			auto tp = filter_cache<http::enable_cache_t>(std::forward<AOP>(aop)...);
			using Tup = decltype(tp);

			static_assert(!asio::has_type<http::enable_cache_t, Tup>::value);

			std::shared_ptr<function_type> op = std::make_shared<function_type>(
				std::bind_front(&self::template _proxy<CacheFlag, F, C, Tup>, this,
					std::move(f), c, std::move(tp)));

			this->_bind_uris(name, std::move(op), this->_make_uris<M...>(name));
		}

		template<http::verb... M, class F, class... AOP>
		inline void _do_add(std::string name, F f, AOP&&... aop)
		{
			this->_add_impl<M...>(std::move(name),
				std::move(f), static_cast<dummy*>(nullptr), std::forward<AOP>(aop)...);
		}

		template<http::verb... M, class F, class C, class... AOP>
		requires std::same_as<C, typename asio::function_traits<F>::class_type>
		inline void _do_add(std::string name, F f, C* c, AOP&&... aop)
		{
			this->_add_impl<M...>(std::move(name),
				std::move(f), c, std::forward<AOP>(aop)...);
		}

		template<http::verb... M, class F, class C, class... AOP>
		requires std::same_as<C, typename asio::function_traits<F>::class_type>
		inline void _do_add(std::string name, F f, C& c, AOP&&... aop)
		{
			this->_add_impl<M...>(std::move(name),
				std::move(f), std::addressof(c), std::forward<AOP>(aop)...);
		}

		template<class URIS>
		inline void _bind_uris(std::string& name, std::shared_ptr<function_type> op, URIS uris)
		{
			for (auto& uri : uris)
			{
				if (uri.empty())
					continue;

				if (name.back() == '*')
				{
					assert(this->wildcard_routers_.find(uri) == this->wildcard_routers_.end());
					this->wildcard_routers_[std::move(uri)] = op;
				}
				else
				{
					assert(this->strictly_routers_.find(uri) == this->strictly_routers_.end());
					this->strictly_routers_[std::move(uri)] = op;
				}
			}
		}

		template<class F, class C, class... TS>
		inline return_type _call_route_function(F& f, C* c, RequestT& req, ResponseT& rep, TS&&... ts)
		{
			if constexpr (std::same_as<std::decay_t<C>, dummy>)
			{
				co_return co_await f(req, rep, std::forward<TS>(ts)...);
			}
			else
			{
				co_return c ? (co_await (c->*f)(req, rep, std::forward<TS>(ts)...)) : false;
			}
		}

		template<bool CacheFlag, class F, class C, class Tup>
		requires (CacheFlag == false)
		inline return_type _proxy(F& f, C* c, Tup& aops, RequestT& req, ResponseT& rep, Ts... ts)
		{
			if (!(co_await _call_aop_before(aops, req, rep, ts...)))
				co_return false;

			if (!(co_await _call_route_function(f, c, req, rep, ts...)))
				co_return false;

			if (!(co_await _call_aop_after(aops, req, rep, ts...)))
				co_return false;

			co_return true;
		}

		template<bool CacheFlag, class F, class C, class Tup>
		requires (CacheFlag == true)
		inline return_type _proxy(F& f, C* c, Tup& aops, RequestT& req, ResponseT& rep, Ts... ts)
		{
			using pcache_node = decltype(this->cache_.find(""));

			if (http::is_cache_enabled(req))
			{
				pcache_node pcn = this->cache_.find(req.target());
				if (!pcn)
				{
					if (!(co_await _call_aop_before(aops, req, rep, ts...)))
						co_return false;

					if (!(co_await _call_route_function(f, c, req, rep, ts...)))
						co_return false;

					if (!(co_await _call_aop_after(aops, req, rep, ts...)))
						co_return false;

					if (this->cache_.full())
					{
						this->cache_.shrink_to_fit();
					}

					if (!this->cache_.full() && rep.get_response_header().result() == http::status::ok)
					{
						if (auto res = rep.to_string_body_response(); res.has_value())
						{
							this->cache_.add(req.target(), std::move(res.value()));
						}
					}

					co_return true;
				}
				else
				{
					if (!(co_await _call_aop_before(aops, req, rep, ts...)))
						co_return false;

					if (!(co_await _call_aop_after(aops, req, rep, ts...)))
						co_return false;

					pcn->update_alive_time();

					rep = std::ref(pcn->msg);

					co_return true;
				}
			}
			else
			{
				if (!(co_await _call_aop_before(aops, req, rep, ts...)))
					co_return false;

				if (!(co_await _call_route_function(f, c, req, rep, ts...)))
					co_return false;

				if (!(co_await _call_aop_after(aops, req, rep, ts...)))
					co_return false;

				co_return true;
			}
		}

		template<class F, class... TS>
		inline return_type _not_found_proxy(F& f, RequestT& req, ResponseT& rep, TS&&... ts)
		{
			co_return co_await f(req, rep, std::forward<TS>(ts)...);
		}

		template<class F, class C, class... TS>
		inline return_type _not_found_proxy(F& f, C* c, RequestT& req, ResponseT& rep, TS&&... ts)
		{
			co_return c ? (co_await (c->*f)(req, rep, std::forward<TS>(ts)...)) : false;
		}

		template<class F>
		inline void _add_not_found_impl(F f)
		{
			this->not_found_router_ = std::make_shared<function_type>(
				std::bind_front(&self::template _not_found_proxy<F>, this, std::move(f)));
		}

		template<class F, class C>
		inline void _add_not_found_impl(F f, C* c)
		{
			this->not_found_router_ = std::make_shared<function_type>(
				std::bind(&self::template _not_found_proxy<F, C>, this, std::move(f), c));
		}

		template<class F, class C>
		inline void _add_not_found_impl(F f, C& c)
		{
			this->_add_not_found_impl(std::move(f), std::addressof(c));
		}

		template <typename... Args, typename F, std::size_t... I>
		inline asio::awaitable<void> _for_each_tuple(std::tuple<Args...>& t, const F& f, std::index_sequence<I...>)
		{
			((co_await f(std::get<I>(t))), ...);
		}

		template<class Tup>
		inline return_type _do_call_aop_before(Tup& aops, RequestT& req, ResponseT& rep, Ts... ts)
		{
			asio::ignore_unused(aops, req, rep, ts...);

			bool continued = true;
			co_await _for_each_tuple(aops, [&](auto& aop) -> asio::awaitable<void>
			{
				asio::ignore_unused(req, rep, ts...);

				if (!continued)
					co_return;

				if constexpr (has_member_before<
					decltype(aop), asio::awaitable<bool>, RequestT&, ResponseT&>::value)
				{
					continued = co_await aop.before(req, rep);
				}
				else if constexpr (has_member_before<
					decltype(aop), asio::awaitable<bool>, RequestT&, ResponseT&, Ts...>::value)
				{
					continued = co_await aop.before(req, rep, ts...);
				}
				else
				{
					// note: You have set a AOP, but the function signature of the AOP is incorrect
				}
			}, std::make_index_sequence<std::tuple_size_v<Tup>>{});

			co_return continued;
		}

		template<class Tup, class... TS>
		inline return_type _call_aop_before(Tup& aops, RequestT& req, ResponseT& rep, TS&&... ts)
		{
			asio::ignore_unused(aops, req, rep, ts...);

			if constexpr (!std::tuple_size_v<Tup>)
			{
				co_return true;
			}
			else
			{
				co_return co_await this->_do_call_aop_before(aops, req, rep, std::forward<TS>(ts)...);
			}
		}

		template<class Tup>
		inline return_type _do_call_aop_after(Tup& aops, RequestT& req, ResponseT& rep, Ts... ts)
		{
			asio::ignore_unused(aops, req, rep, ts...);

			bool continued = true;
			co_await _for_each_tuple(aops, [&](auto& aop) -> asio::awaitable<void>
			{
				asio::ignore_unused(req, rep, ts...);

				if (!continued)
					co_return;

				if constexpr (has_member_after<
					decltype(aop), asio::awaitable<bool>, RequestT&, ResponseT&>::value)
				{
					continued = co_await aop.after(req, rep);
				}
				else if constexpr (has_member_after<
					decltype(aop), asio::awaitable<bool>, RequestT&, ResponseT&, Ts...>::value)
				{
					continued = co_await aop.after(req, rep, ts...);
				}
				else
				{
					// note: You have set a AOP, but the function signature of the AOP is incorrect
				}
			}, std::make_index_sequence<std::tuple_size_v<Tup>>{});

			co_return continued;
		}

		template<class Tup, class... TS>
		inline return_type _call_aop_after(Tup& aops, RequestT& req, ResponseT& rep, TS&&... ts)
		{
			asio::ignore_unused(aops, req, rep, ts...);

			if constexpr (!std::tuple_size_v<Tup>)
			{
				co_return true;
			}
			else
			{
				co_return co_await this->_do_call_aop_after(aops, req, rep, std::forward<TS>(ts)...);
			}
		}

		inline std::string _make_uri(std::string_view root, std::string_view path)
		{
			std::string uri;

			if (http::has_undecode_char(path, 1))
			{
				std::string decoded = http::url_decode(path);

				path = decoded;

				while (path.size() > static_cast<std::string_view::size_type>(1) && path.back() == '/')
				{
					path.remove_suffix(1);
				}

				uri.reserve(root.size() + path.size());
				uri += root;
				uri += path;
			}
			else
			{
				while (path.size() > static_cast<std::string_view::size_type>(1) && path.back() == '/')
				{
					path.remove_suffix(1);
				}

				uri.reserve(root.size() + path.size());
				uri += root;
				uri += path;
			}

			return uri;
		}

		inline constexpr std::string_view _to_char(http::verb method) noexcept
		{
			using namespace std::literals;
			// Use a special name to avoid conflicts with global variable names
			constexpr std::string_view _hrmchars =
				"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;
			return _hrmchars.substr(std::to_underlying(method), 1);
		}

	public:
		/**
		 * @brief bind a function for http router
		 * @param name - uri name in string format.
		 * @param fun - Function object.
		 * @param aops - A pointer or reference to a aop object list.
		 * if fun is member function, the first aops param must the class object's pointer or reference.
		 */
		template<http::verb... M, class F, class ...Aops>
		self& add(std::string name, F&& fun, Aops&&... aops)
		{
			asio::trim_both(name);

			if (name == "*")
				name = "/*";

			if (name.empty())
			{
				assert(false);
				return (*this);
			}

			if constexpr (sizeof...(M) == std::size_t(0))
			{
				this->_do_add<http::verb::get, http::verb::post>(
					std::move(name), std::forward<F>(fun), std::forward<Aops>(aops)...);
			}
			else
			{
				this->_do_add<M...>(
					std::move(name), std::forward<F>(fun), std::forward<Aops>(aops)...);
			}

			return (*this);
		}

		/**
		 * @brief set the 404 not found router function
		 */
		template<class F, class ...C>
		inline self& add_not_found(F&& f, C&&... obj)
		{
			this->_add_not_found_impl(std::forward<F>(f), std::forward<C>(obj)...);
			return (*this);
		}

		/**
		 * @brief set the 404 not found router function
		 */
		template<class F, class ...C>
		inline self& add_404(F&& f, C&&... obj)
		{
			this->_add_not_found_impl(std::forward<F>(f), std::forward<C>(obj)...);
			return (*this);
		}

		inline std::shared_ptr<function_type>& find(RequestT& req)
		{
			std::string_view path = req.target();

			if (auto pos = path.find('?'); pos != std::string_view::npos)
			{
				path = path.substr(0, pos);
			}

			std::string uri = this->_make_uri(this->_to_char(req.method()), path);

			{
				auto it = this->strictly_routers_.find(uri);
				if (it != this->strictly_routers_.end())
				{
					return (it->second);
				}
			}

			// Find the best match url from tail to head
			if (!wildcard_routers_.empty())
			{
				for (auto it = this->wildcard_routers_.rbegin(); it != this->wildcard_routers_.rend(); ++it)
				{
					auto& k = it->first;
					assert(k.size() >= std::size_t(3));
					if (!uri.empty() && !k.empty() && uri.front() == k.front() && uri.size() >= (k.size() - 2)
						&& uri[k.size() - 3] == k[k.size() - 3] && http::url_match(k, uri))
					{
						return (it->second);
					}
				}
			}

			return this->dummy_router_;
		}

		template<class... TS>
		inline return_type route(RequestT& req, ResponseT& rep, TS&&... ts)
		{
			std::shared_ptr<function_type>& router_ptr = this->find(req);

			if (router_ptr)
			{
				co_return co_await (*router_ptr)(req, rep, std::forward<TS>(ts)...);
			}

			if (this->not_found_router_ && (*(this->not_found_router_)))
			{
				co_return co_await (*(this->not_found_router_))(req, rep, std::forward<TS>(ts)...);
			}

			co_return false;
		}

	protected:
		std::unordered_map<std::string, std::shared_ptr<function_type>> strictly_routers_;

		std::          map<std::string, std::shared_ptr<function_type>> wildcard_routers_;

		std::shared_ptr<function_type>                                  not_found_router_;

		inline static std::shared_ptr<function_type>                    dummy_router_;

		cache_type                                                      cache_;
	};
}

#include <asio3/core/detail/pop_options.hpp>
