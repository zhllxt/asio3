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

#include <asio3/core/function_traits.hpp>
#include <asio3/core/defer.hpp>

#include <asio3/core/function_traits.hpp>
#include <asio3/core/strutil.hpp>
#include <asio3/core/stdutil.hpp>
#include <asio3/core/netutil.hpp>
#include <asio3/core/match_condition.hpp>

#include <asio3/rpc/serialization.hpp>
#include <asio3/rpc/message.hpp>

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	struct request_option
	{
		std::chrono::steady_clock::duration timeout = asio::http_request_timeout;
		bool requires_response = true;
	};

	template<typename SerializerT, typename DeserializerT>
	class basic_async_caller
	{
	public:
		using serializer_type   = SerializerT;
		using deserializer_type = DeserializerT;

	protected:
		using lock_type = asio::experimental::channel<
			void(asio::error_code, serializer_type*, deserializer_type*, std::string_view)>;

		template<typename ReturnT, typename Self, typename ...Args>
		requires (std::is_void_v<ReturnT>)
		asio::awaitable<std::tuple<asio::error_code>> async_call_impl(
			this Self&& self, request_option opt, std::string name, Args&&... args)
		{
			auto id = opt.requires_response ? self.id_generator.next() : self.id_generator.zero();
			auto req = rpc::request<Args...>{ id, std::move(name), std::forward<Args>(args)... };

			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_awaitable);

			if (opt.requires_response)
			{
				std::defer auto_remove_from_map = [&self, id]() mutable
				{
					self.pending_requests.erase(id);
				};

				bool notifyed = false;
				asio::error_code ec{};
				asio::timer timer(self.get_executor());

				std::function<void()> callback = [&self, &notifyed, &ec, &timer]() mutable
				{
					asio::cancel_timer(timer);

					notifyed = true;

				#if !defined(ASIO_NO_EXCEPTIONS) && !defined(BOOST_ASIO_NO_EXCEPTIONS)
					try
					{
				#endif
						self.deserializer >> ec;
				#if !defined(ASIO_NO_EXCEPTIONS) && !defined(BOOST_ASIO_NO_EXCEPTIONS)
					}
					catch (cereal::exception const&)
					{
						ec = rpc::make_error_code(rpc::error::parse_error);
					}
				#endif
				};

				self.pending_requests.emplace(id, std::move(callback));

				auto& data = (self.serializer.reset() << req).str();
				std::string head = asio::length_payload_match_condition::generate_length(asio::buffer(data));
				std::array<asio::const_buffer, 2> buffers
				{
					asio::buffer(head),
					asio::buffer(data),
				};
				auto [e1, n1] = co_await self.async_send(buffers, asio::use_nothrow_awaitable);
				if (e1)
					co_return std::tuple{ e1 };

				timer.expires_after(opt.timeout);
				auto [e0] = co_await timer.async_wait(asio::use_nothrow_awaitable);

				if (notifyed)
					co_return std::tuple{ ec };
				else
					co_return std::tuple{ rpc::make_error_code(rpc::error::timed_out) };
			}
			else
			{
				auto& data = (self.serializer.reset() << req).str();
				std::string head = asio::length_payload_match_condition::generate_length(asio::buffer(data));
				std::array<asio::const_buffer, 2> buffers
				{
					asio::buffer(head),
					asio::buffer(data),
				};
				auto [e1, n1] = co_await self.async_send(buffers, asio::use_nothrow_awaitable);
				co_return std::tuple{ e1 };
			}
		}

		template<typename ReturnT, typename Self, typename ...Args>
		requires (!std::is_void_v<ReturnT>)
		asio::awaitable<std::tuple<asio::error_code, ReturnT>> async_call_impl(
			this Self&& self, request_option opt, std::string name, Args&&... args)
		{
			ReturnT result{};

			auto id = opt.requires_response ? self.id_generator.next() : self.id_generator.zero();
			auto req = rpc::request<Args...>{ id, std::move(name), std::forward<Args>(args)... };

			co_await asio::dispatch(self.get_executor(), asio::use_nothrow_awaitable);

			if (opt.requires_response)
			{
				std::defer auto_remove_from_map = [&self, id]() mutable
				{
					self.pending_requests.erase(id);
				};

				bool notifyed = false;
				asio::error_code ec{};
				asio::timer timer(self.get_executor());

				std::function<void()> callback = [&self, &notifyed, &ec, &timer, &result]() mutable
				{
					asio::cancel_timer(timer);

					notifyed = true;

				#if !defined(ASIO_NO_EXCEPTIONS) && !defined(BOOST_ASIO_NO_EXCEPTIONS)
					try
					{
				#endif
						self.deserializer >> ec;
						if (!ec)
							self.deserializer >> result;
				#if !defined(ASIO_NO_EXCEPTIONS) && !defined(BOOST_ASIO_NO_EXCEPTIONS)
					}
					catch (cereal::exception const&)
					{
						ec = rpc::make_error_code(rpc::error::parse_error);
					}
				#endif
				};

				self.pending_requests.emplace(id, std::move(callback));

				auto& data = (self.serializer.reset() << req).str();
				std::string head = asio::length_payload_match_condition::generate_length(asio::buffer(data));
				std::array<asio::const_buffer, 2> buffers
				{
					asio::buffer(head),
					asio::buffer(data),
				};
				auto [e1, n1] = co_await self.async_send(buffers, asio::use_nothrow_awaitable);
				if (e1)
					co_return std::tuple{ e1, std::move(result) };

				timer.expires_after(opt.timeout);
				auto [e0] = co_await timer.async_wait(asio::use_nothrow_awaitable);

				if (notifyed)
					co_return std::tuple{ ec, std::move(result) };
				else
					co_return std::tuple{ rpc::make_error_code(rpc::error::timed_out), std::move(result) };
			}
			else
			{
				auto& data = (self.serializer.reset() << req).str();
				std::string head = asio::length_payload_match_condition::generate_length(asio::buffer(data));
				std::array<asio::const_buffer, 2> buffers
				{
					asio::buffer(head),
					asio::buffer(data),
				};
				auto [e1, n1] = co_await self.async_send(buffers, asio::use_nothrow_awaitable);
				co_return std::tuple{ e1, std::move(result) };
			}
		}

		template<typename derive_t>
		class caller_bridge
		{
			friend class basic_async_caller;
		protected:
			caller_bridge(derive_t& d) noexcept : derive(d)
			{
			}

			caller_bridge(const caller_bridge&) = delete;
			caller_bridge& operator=(const caller_bridge&) = delete;
			caller_bridge(caller_bridge&&) noexcept = default;
			caller_bridge& operator=(caller_bridge&&) noexcept = default;

		public:
			/**
			 * @brief Set the timeout of this rpc call, only valid for this once call.
			 */
			inline caller_bridge& set_request_option(request_option o) noexcept
			{
				opt = std::move(o);
				return *this;
			}

			template<typename ReturnT, typename ...Args>
			requires (std::is_void_v<ReturnT>)
			asio::awaitable<std::tuple<asio::error_code>> async_call(std::string name, Args&&... args)
			{
				co_return co_await derive.async_call_impl<ReturnT>(
					std::move(opt), std::move(name), std::forward<Args>(args)...);
			}

			template<typename ReturnT, typename ...Args>
			requires (!std::is_void_v<ReturnT>)
			asio::awaitable<std::tuple<asio::error_code, ReturnT>> async_call(std::string name, Args&&... args)
			{
				co_return co_await derive.async_call_impl<ReturnT>(
					std::move(opt), std::move(name), std::forward<Args>(args)...);
			}

		protected:
			derive_t       & derive;
			request_option   opt{};
		};

	public:
		/**
		 * @brief asynchronous call a rpc function, the return value of the rpc function is void
		 */
		template<typename ReturnT, typename Self, typename ...Args>
		requires (std::is_void_v<ReturnT>)
		inline asio::awaitable<std::tuple<asio::error_code>> async_call(
			this Self&& self, std::string name, Args&&... args)
		{
			caller_bridge<std::remove_cvref_t<Self>> caller{ self };
			co_return co_await caller.async_call<ReturnT>(std::move(name), std::forward<Args>(args)...);
		}

		/**
		 * @brief asynchronous call a rpc function, the return value of the rpc function is not void
		 */
		template<typename ReturnT, typename Self, typename ...Args>
		requires (!std::is_void_v<ReturnT>)
		inline asio::awaitable<std::tuple<asio::error_code, ReturnT>> async_call(
			this Self&& self, std::string name, Args&&... args)
		{
			caller_bridge<std::remove_cvref_t<Self>> caller{ self };
			co_return co_await caller.async_call<ReturnT>(std::move(name), std::forward<Args>(args)...);
		}

		/**
		 * @brief Set the timeout or other options of this rpc call, only valid for this once call.
		 */
		template<typename Self>
		inline caller_bridge<std::remove_cvref_t<Self>> set_request_option(
			this Self&& self, request_option opt)
		{
			caller_bridge<std::remove_cvref_t<Self>> caller{ self };
			caller.set_request_option(std::move(opt));
			return caller;
		}

		/**
		 * @brief 
		 */
		template<typename Self>
		inline asio::awaitable<asio::error_code> async_notify(this Self&& self,
			serializer_type& sr, deserializer_type& dr, rpc::header head, auto&& data)
		{
			if (auto it = self.pending_requests.find(head.id); it != self.pending_requests.end())
			{
				std::function<void()>& cb = it->second;
				cb();
				co_return rpc::make_error_code(rpc::error::success);
			}
			co_return rpc::make_error_code(rpc::error::invalid_request);
		}

	public:
		std::map<rpc::header::id_type, std::function<void()>> pending_requests;
	};

	using async_caller = basic_async_caller<rpc::serializer, rpc::deserializer>;
}
