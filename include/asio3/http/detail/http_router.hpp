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

#include <asio3/http/detail/http_util.hpp>
#include <asio3/http/detail/http_cache.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace bho::beast::http
#else
namespace boost::beast::http
#endif
{
	class http_router
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
		/////////////////////////////////////////////////////////////////////////////

	public:
		using opret = http::message_generator;
		using opfun = std::function<opret(http::request<http::string_body>&)>;

		/**
		 * @brief constructor
		 */
		http_router()
		{
			this->not_found_router_ = std::make_shared<opfun>(
			[](http::request<http::string_body>& req) mutable
			{
				std::string desc;
				desc.reserve(64);
				desc += "The resource for ";
				desc += req.method_string();
				desc += " \"";
				desc += http::url_decode(req.target());
				desc += "\" was not found";

				rep.fill_page(http::status::not_found, std::move(desc), {}, req.version());

				return std::addressof(rep.base());
			});

		#if defined(_DEBUG) || defined(DEBUG)
			// used to test ThreadSafetyAnalysis
			asio::ignore_unused(this->http_cache_.empty());
			asio::ignore_unused(this->http_cache_.get_cache_count());
		#endif
		}

		/**
		 * @brief destructor
		 */
		~http_router() = default;

		/**
		 * @brief bind a function for http router
		 * @param name - uri name in string format.
		 * @param fun - Function object.
		 * @param caop - A pointer or reference to a class object, and aop object list.
		 * if fun is member function, the first caop param must the class object's pointer or reference.
		 */
		template<http::verb... M, class F, class ...CAOP>
		typename std::enable_if_t<!std::is_base_of_v<websocket::detail::listener_tag,
			detail::remove_cvref_t<F>>, self&>
		inline bind(std::string name, F&& fun, CAOP&&... caop)
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
				this->_bind<http::verb::get, http::verb::post>(
					std::move(name), std::forward<F>(fun), std::forward<CAOP>(caop)...);
			}
			else
			{
				this->_bind<M...>(
					std::move(name), std::forward<F>(fun), std::forward<CAOP>(caop)...);
			}

			return (*this);
		}

		/**
		 * @brief bind a function for websocket router
		 * @param name - uri name in string format.
		 * @param listener - callback listener.
		 * @param aop - aop object list.
		 */
		template<class ...AOP>
		inline self& bind(std::string name, websocket::listener<caller_t> listener, AOP&&... aop)
		{
			asio::trim_both(name);

			assert(!name.empty());

			if (!name.empty())
				this->_bind(std::move(name), std::move(listener), std::forward<AOP>(aop)...);

			return (*this);
		}

		/**
		 * @brief set the 404 not found router function
		 */
		template<class F, class ...C>
		inline self& bind_not_found(F&& f, C&&... obj)
		{
			this->_bind_not_found(std::forward<F>(f), std::forward<C>(obj)...);
			return (*this);
		}

		/**
		 * @brief set the 404 not found router function
		 */
		template<class F, class ...C>
		inline self& bind_404(F&& f, C&&... obj)
		{
			this->_bind_not_found(std::forward<F>(f), std::forward<C>(obj)...);
			return (*this);
		}

	protected:
		inline self& _router() noexcept { return (*this); }

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

		template<http::verb... M, class F, class... AOP>
		typename std::enable_if_t<!std::is_base_of_v<websocket::detail::listener_tag,
			detail::remove_cvref_t<F>>, void>
		inline _bind(std::string name, F f, AOP&&... aop)
		{
			constexpr bool CacheFlag = has_type<http::enable_cache_t, std::tuple<std::decay_t<AOP>...>>::value;

			auto tp = filter_cache<http::enable_cache_t>(std::forward<AOP>(aop)...);
			using Tup = decltype(tp);

		#if defined(ASIO2_ENABLE_LOG)
			static_assert(!has_type<http::enable_cache_t, Tup>::value);
		#endif

			std::shared_ptr<opfun> op = std::make_shared<opfun>(
				std::bind(&self::template _proxy<CacheFlag, F, Tup>, this,
					std::move(f), std::move(tp),
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

			this->_bind_uris(name, std::move(op), this->_make_uris<M...>(name));
		}

		template<http::verb... M, class F, class C, class... AOP>
		typename std::enable_if_t<!std::is_base_of_v<websocket::detail::listener_tag,
			detail::remove_cvref_t<F>> && std::is_same_v<C, typename function_traits<F>::class_type>, void>
		inline _bind(std::string name, F f, C* c, AOP&&... aop)
		{
			constexpr bool CacheFlag = has_type<http::enable_cache_t, std::tuple<std::decay_t<AOP>...>>::value;

			auto tp = filter_cache<http::enable_cache_t>(std::forward<AOP>(aop)...);
			using Tup = decltype(tp);

		#if defined(ASIO2_ENABLE_LOG)
			static_assert(!has_type<http::enable_cache_t, Tup>::value);
		#endif

			std::shared_ptr<opfun> op = std::make_shared<opfun>(
				std::bind(&self::template _proxy<CacheFlag, F, C, Tup>, this,
					std::move(f), c, std::move(tp),
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

			this->_bind_uris(name, std::move(op), this->_make_uris<M...>(name));
		}

		template<http::verb... M, class F, class C, class... AOP>
		typename std::enable_if_t<!std::is_base_of_v<websocket::detail::listener_tag,
			detail::remove_cvref_t<F>> && std::is_same_v<C, typename function_traits<F>::class_type>, void>
		inline _bind(std::string name, F f, C& c, AOP&&... aop)
		{
			this->_bind<M...>(std::move(name), std::move(f), std::addressof(c), std::forward<AOP>(aop)...);
		}

		template<class... AOP>
		inline void _bind(std::string name, websocket::listener<caller_t> listener, AOP&&... aop)
		{
			auto tp = filter_cache<http::enable_cache_t>(std::forward<AOP>(aop)...);
			using Tup = decltype(tp);

		#if defined(ASIO2_ENABLE_LOG)
			static_assert(!has_type<http::enable_cache_t, Tup>::value);
		#endif

			std::shared_ptr<opfun> op = std::make_shared<opfun>(
				std::bind(&self::template _proxy<Tup>, this,
					std::move(listener), std::move(tp),
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

			// https://datatracker.ietf.org/doc/rfc6455/

			// Once a connection to the server has been established (including a
			// connection via a proxy or over a TLS-encrypted tunnel), the client
			// MUST send an opening handshake to the server.  The handshake consists
			// of an HTTP Upgrade request, along with a list of required and
			// optional header fields.  The requirements for this handshake are as
			// follows.

			// 2.   The method of the request MUST be GET, and the HTTP version MUST
			//      be at least 1.1.

			this->_bind_uris(name, std::move(op), std::array<std::string, 1>{std::string{ "Z" } +name});
		}

		template<class URIS>
		inline void _bind_uris(std::string& name, std::shared_ptr<opfun> op, URIS uris)
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

		//template<http::verb... M, class C, class R, class...Ps, class... AOP>
		//inline void _bind(std::string name, R(C::*memfun)(Ps...), C* c, AOP&&... aop)
		//{
		//}

		template<bool CacheFlag, class F, class Tup>
		typename std::enable_if_t<!CacheFlag, opret>
		inline _proxy(F& f, Tup& aops, 
			std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			using fun_traits_type = function_traits<F>;
			using arg0_type = typename std::remove_cv_t<std::remove_reference_t<
				typename fun_traits_type::template args<0>::type>>;

			if (!_call_aop_before(aops, caller, req, rep))
				return std::addressof(rep.base());

			if constexpr (std::is_same_v<std::shared_ptr<caller_t>, arg0_type>)
			{
				f(caller, req, rep);
			}
			else
			{
				f(req, rep);
			}

			_call_aop_after(aops, caller, req, rep);

			return std::addressof(rep.base());
		}

		template<bool CacheFlag, class F, class C, class Tup>
		typename std::enable_if_t<!CacheFlag, opret>
		inline _proxy(F& f, C* c, Tup& aops,
			std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			using fun_traits_type = function_traits<F>;
			using arg0_type = typename std::remove_cv_t<std::remove_reference_t<
				typename fun_traits_type::template args<0>::type>>;

			if (!_call_aop_before(aops, caller, req, rep))
				return std::addressof(rep.base());

			if constexpr (std::is_same_v<std::shared_ptr<caller_t>, arg0_type>)
			{
				if (c) (c->*f)(caller, req, rep);
			}
			else
			{
				if (c) (c->*f)(req, rep);
			}

			_call_aop_after(aops, caller, req, rep);

			return std::addressof(rep.base());
		}

		template<class Tup>
		inline opret _proxy(websocket::listener<caller_t>& listener, Tup& aops,
			std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			assert(req.ws_frame_type_ != websocket::frame::unknown);

			if (req.ws_frame_type_ == websocket::frame::open)
			{
				if (!_call_aop_before(aops, caller, req, rep))
					return nullptr;
			}

			if (req.ws_frame_type_ != websocket::frame::unknown)
			{
				listener(caller, req, rep);
			}

			if (req.ws_frame_type_ == websocket::frame::open)
			{
				if (!_call_aop_after(aops, caller, req, rep))
					return nullptr;
			}

			return std::addressof(rep.base());
		}

		template<bool CacheFlag, class F, class Tup>
		typename std::enable_if_t<CacheFlag, opret>
		inline _proxy(F& f, Tup& aops, 
			std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			using fun_traits_type = function_traits<F>;
			using arg0_type = typename std::remove_cv_t<std::remove_reference_t<
				typename fun_traits_type::template args<0>::type>>;
			using pcache_node = decltype(this->http_cache_.find(""));

			if (http::is_cache_enabled(req.base()))
			{
				pcache_node pcn = this->http_cache_.find(req.target());
				if (!pcn)
				{
					if (!_call_aop_before(aops, caller, req, rep))
						return std::addressof(rep.base());

					if constexpr (std::is_same_v<std::shared_ptr<caller_t>, arg0_type>)
					{
						f(caller, req, rep);
					}
					else
					{
						f(req, rep);
					}

					if (_call_aop_after(aops, caller, req, rep) && rep.result() == http::status::ok)
					{
						http::web_response::super msg{ std::move(rep.base()) };
						if (msg.body().to_text())
						{
							this->http_cache_.shrink_to_fit();
							pcn = this->http_cache_.emplace(req.target(), std::move(msg));
							return std::addressof(pcn->msg);
						}
						else
						{
							rep.base() = std::move(msg);
						}
					}

					return std::addressof(rep.base());
				}
				else
				{
					if (!_call_aop_before(aops, caller, req, rep))
						return std::addressof(rep.base());

					if (!_call_aop_after(aops, caller, req, rep))
						return std::addressof(rep.base());

					assert(!pcn->msg.body().is_file());
					pcn->update_alive_time();
					return std::addressof(pcn->msg);
				}
			}
			else
			{
				if (!_call_aop_before(aops, caller, req, rep))
					return std::addressof(rep.base());

				if constexpr (std::is_same_v<std::shared_ptr<caller_t>, arg0_type>)
				{
					f(caller, req, rep);
				}
				else
				{
					f(req, rep);
				}

				_call_aop_after(aops, caller, req, rep);

				return std::addressof(rep.base());
			}
		}

		template<bool CacheFlag, class F, class C, class Tup>
		typename std::enable_if_t<CacheFlag, opret>
		inline _proxy(F& f, C* c, Tup& aops,
			std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			using fun_traits_type = function_traits<F>;
			using arg0_type = typename std::remove_cv_t<std::remove_reference_t<
				typename fun_traits_type::template args<0>::type>>;
			using pcache_node = decltype(this->http_cache_.find(""));

			if (http::is_cache_enabled(req.base()))
			{
				pcache_node pcn = this->http_cache_.find(req.target());
				if (!pcn)
				{
					if (!_call_aop_before(aops, caller, req, rep))
						return std::addressof(rep.base());

					if constexpr (std::is_same_v<std::shared_ptr<caller_t>, arg0_type>)
					{
						if (c) (c->*f)(caller, req, rep);
					}
					else
					{
						if (c) (c->*f)(req, rep);
					}

					if (_call_aop_after(aops, caller, req, rep) && rep.result() == http::status::ok)
					{
						http::web_response::super msg{ std::move(rep.base()) };
						if (msg.body().to_text())
						{
							this->http_cache_.shrink_to_fit();
							pcn = this->http_cache_.emplace(req.target(), std::move(msg));
							return std::addressof(pcn->msg);
						}
						else
						{
							rep.base() = std::move(msg);
						}
					}

					return std::addressof(rep.base());
				}
				else
				{
					if (!_call_aop_before(aops, caller, req, rep))
						return std::addressof(rep.base());

					if (!_call_aop_after(aops, caller, req, rep))
						return std::addressof(rep.base());

					assert(!pcn->msg.body().is_file());
					pcn->update_alive_time();
					return std::addressof(pcn->msg);
				}
			}
			else
			{
				if (!_call_aop_before(aops, caller, req, rep))
					return std::addressof(rep.base());

				if constexpr (std::is_same_v<std::shared_ptr<caller_t>, arg0_type>)
				{
					if (c) (c->*f)(caller, req, rep);
				}
				else
				{
					if (c) (c->*f)(req, rep);
				}

				_call_aop_after(aops, caller, req, rep);

				return std::addressof(rep.base());
			}
		}

		template<class F>
		inline void _bind_not_found(F f)
		{
			this->not_found_router_ = std::make_shared<opfun>(
				std::bind(&self::template _not_found_proxy<F>, this, std::move(f),
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		}

		template<class F, class C>
		inline void _bind_not_found(F f, C* c)
		{
			this->not_found_router_ = std::make_shared<opfun>(
				std::bind(&self::template _not_found_proxy<F, C>, this, std::move(f), c,
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		}

		template<class F, class C>
		inline void _bind_not_found(F f, C& c)
		{
			this->_bind_not_found(std::move(f), std::addressof(c));
		}

		template<class F>
		inline opret _not_found_proxy(F& f, std::shared_ptr<caller_t>& caller,
			http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(caller);

			if constexpr (detail::is_template_callable_v<F, std::shared_ptr<caller_t>&,
				http::web_request&, http::web_response&>)
			{
				f(caller, req, rep);
			}
			else
			{
				f(req, rep);
			}

			return std::addressof(rep.base());
		}

		template<class F, class C>
		inline opret _not_found_proxy(F& f, C* c, std::shared_ptr<caller_t>& caller,
			http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(caller);

			if constexpr (detail::is_template_callable_v<F, std::shared_ptr<caller_t>&,
				http::web_request&, http::web_response&>)
			{
				if (c) (c->*f)(caller, req, rep);
			}
			else
			{
				if (c) (c->*f)(req, rep);
			}

			return std::addressof(rep.base());
		}

		template <typename... Args, typename F, std::size_t... I>
		inline void _for_each_tuple(std::tuple<Args...>& t, const F& f, std::index_sequence<I...>)
		{
			(f(std::get<I>(t)), ...);
		}

		template<class Tup>
		inline bool _do_call_aop_before(
			Tup& aops, std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(aops, caller, req, rep);

			//asio2::detail::get_current_object<std::shared_ptr<caller_t>>() = caller;

			bool continued = true;
			_for_each_tuple(aops, [&continued, &caller, &req, &rep](auto& aop)
			{
				asio::ignore_unused(caller, req, rep);

				if (!continued)
					return;

				if constexpr (has_member_before<decltype(aop), bool,
					std::shared_ptr<caller_t>&, http::web_request&, http::web_response&>::value)
				{
					continued = aop.before(caller, req, rep);
				}
				else if constexpr (has_member_before<decltype(aop), bool,
					http::web_request&, http::web_response&>::value)
				{
					continued = aop.before(req, rep);
				}
				else
				{
					std::ignore = true;
				}
			}, std::make_index_sequence<std::tuple_size_v<Tup>>{});

			return continued;
		}

		template<class Tup>
		inline bool _call_aop_before(
			Tup& aops, std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(aops, caller, req, rep);

			if constexpr (!std::tuple_size_v<Tup>)
			{
				return true;
			}
			else
			{
				return this->_do_call_aop_before(aops, caller, req, rep);
			}
		}

		template<class Tup>
		inline bool _do_call_aop_after(
			Tup& aops, std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(aops, caller, req, rep);

			//asio2::detail::get_current_object<std::shared_ptr<caller_t>>() = caller;

			bool continued = true;
			_for_each_tuple(aops, [&continued, &caller, &req, &rep](auto& aop)
			{
				asio::ignore_unused(caller, req, rep);

				if (!continued)
					return;

				if constexpr (has_member_after<decltype(aop), bool,
					std::shared_ptr<caller_t>&, http::web_request&, http::web_response&>::value)
				{
					continued = aop.after(caller, req, rep);
				}
				else if constexpr (has_member_after<decltype(aop), bool,
					http::web_request&, http::web_response&>::value)
				{
					continued = aop.after(req, rep);
				}
				else
				{
					std::ignore = true;
				}
			}, std::make_index_sequence<std::tuple_size_v<Tup>>{});

			return continued;
		}

		template<class Tup>
		inline bool _call_aop_after(
			Tup& aops, std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(aops, caller, req, rep);

			if constexpr (!std::tuple_size_v<Tup>)
			{
				return true;
			}
			else
			{
				if (rep.defer_guard_)
				{
					rep.defer_guard_->aop_after_cb_ = std::make_unique<std::function<void()>>(
					[this, &aops, caller, &req, &rep]() mutable
					{
						caller_t* p = caller.get();

						asio::dispatch(p->io_->context(), make_allocator(p->wallocator(),
						[this, &aops, caller = std::move(caller), &req, &rep]() mutable
						{
							this->_do_call_aop_after(aops, caller, req, rep);
						}));
					});
					return true;
				}
				else
				{
					return this->_do_call_aop_after(aops, caller, req, rep);
				}
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

		template<bool IsHttp>
		inline std::shared_ptr<opfun>& _find(http::web_request& req, http::web_response& rep)
		{
			asio::ignore_unused(rep);

			std::string uri;

			if constexpr (IsHttp)
			{
				uri = this->_make_uri(this->_to_char(req.method()), req.path());
			}
			else
			{
				uri = this->_make_uri(std::string_view{ "Z" }, req.path());
			}

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

		inline opret _route(std::shared_ptr<caller_t>& caller, http::web_request& req, http::web_response& rep)
		{
			if (caller->websocket_router_)
			{
				return (*(caller->websocket_router_))(caller, req, rep);
			}

			std::shared_ptr<opfun>& router_ptr = this->template _find<true>(req, rep);

			if (router_ptr)
			{
				return (*router_ptr)(caller, req, rep);
			}

			if (this->not_found_router_ && (*(this->not_found_router_)))
				(*(this->not_found_router_))(caller, req, rep);

			return std::addressof(rep.base());
		}

		inline constexpr std::string_view _to_char(http::verb method) noexcept
		{
			using namespace std::literals;
			// Use a special name to avoid conflicts with global variable names
			constexpr std::string_view _hrmchars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv;
			return _hrmchars.substr(detail::to_underlying(method), 1);
		}

	protected:
		std::unordered_map<std::string, std::shared_ptr<opfun>> strictly_routers_;

		std::          map<std::string, std::shared_ptr<opfun>> wildcard_routers_;

		std::shared_ptr<opfun>                                  not_found_router_;

		inline static std::shared_ptr<opfun>                    dummy_router_;

		detail::http_cache_t<caller_t, args_t>                  http_cache_;
	};
}

#include <asio3/core/detail/pop_options.hpp>
