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

#include <string>
#include <string_view>
#include <tuple>

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	namespace detail
	{
		template<class T>
		struct parameter_traits_t
		{
			template<class T>
			struct value_t
			{
				using type = T;
			};
			template<class... Ts>
			struct value_t<std::basic_string_view<Ts...>>
			{
				using type = typename std::basic_string_view<Ts...>::value_type;
			};

			// if the parameters of rpc calling is raw pointer like char* , must convert it to std::string
			// if the parameters of rpc calling is std::string_view , must convert it to std::string
			// if the parameters of rpc calling is reference like std::string& , must remove it's 
			//   reference to std::string
			using ncvr_type = std::remove_cvref_t<T>;
			using char_type = std::remove_cvref_t<
				std::remove_all_extents_t<std::remove_pointer_t<ncvr_type>>>;
			using type = std::conditional_t<
				asio::is_char_pointer<ncvr_type> || asio::is_char_array<ncvr_type>
				, std::basic_string<char_type>
				, std::conditional_t<asio::is_template_instance_of<std::basic_string_view, ncvr_type>
				, std::basic_string<typename value_t<ncvr_type>::type>
				, ncvr_type>>;
		};
	}

	/*
	 * request  : message type + request id + method name + parameters value...
	 * response : message type + request id + method name + error code + result value
	 *
	 * message type : q - request, p - response
	 *
	 * if result type is void, then result type will wrapped to std::int8_t
	 */

	static constexpr char request_mark  = 'q';
	static constexpr char response_mark = 'p';

	struct header
	{
		using id_type = std::uint64_t;

		header() = default;
		header(char type, id_type id, std::string method)
			: type(type), id(id), method(std::move(method))
		{
		}

		template <class Archive>
		void save(Archive& ar) const
		{
			ar(type, id, method);
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar(type, id, method);
		}

		inline constexpr bool is_request()  const noexcept { return type == request_mark; }
		inline constexpr bool is_response() const noexcept { return type == response_mark; }

		char           type = '\0';
		id_type        id = 0;
		std::string    method;
	};

	template<bool isRequest, typename ...Args>
	struct message;

	template<typename ...Args>
	struct message<true, Args...> : public header
	{
		// can't use parameters(std::forward_as_tuple(std::forward<Args>(args)...))
		// if use parameters(std::forward_as_tuple(std::forward<Args>(args)...)),
		// when the args is nlohmann::json and under gcc 9.4.0, the json::object
		// maybe changed to json::array, like this:
		// {"method":"hello","age":10} will changed to [{"method":"hello","age":10}]
		// i don't why?
		message(id_type id, std::string method, Args&&... args)
			: header(request_mark, id, std::move(method)), parameters(std::forward<Args>(args)...)
		{
		}

		template <class Archive>
		void save(Archive & ar) const
		{
			ar(cereal::base_class<header>(this));

			asio::for_each_tuple(parameters, [&ar](const auto& elem) mutable
			{
				ar << elem;
			});
		}

		template <class Archive>
		void load(Archive & ar)
		{
			ar(cereal::base_class<header>(this));

			asio::for_each_tuple(parameters, [&ar](auto& elem) mutable
			{
				ar >> elem;
			});
		}

		std::tuple<typename detail::parameter_traits_t<Args>::type...> parameters;
	};

	template<typename ...Args>
	message(header::id_type, std::string, Args...) -> message<true, Args...>;

	template<typename T>
	struct message<false, T> : public header
	{
		template <class Archive>
		void save(Archive& ar) const
		{
			ar << cereal::base_class<header>(this);
			ar << ec.value();
			ar << result;
		}

		template <class Archive>
		void load(Archive& ar)
		{
			ar >> cereal::base_class<header>(this);
			ar >> ec.value();
			ar >> result;
		}

		error_code ec{};
		T          result{};
	};

	template<typename ...Args>
	using request = message<true, Args...>;

	template<typename T>
	using response = message<false, T>;
}
