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

#if defined(__GNUC__) || defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#include <cstring>
#include <cctype>
#include <cstdarg>
#include <clocale>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <ctime>

#include <memory>
#include <string>
#include <locale>
#include <string_view>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <system_error>
#include <limits>
#include <algorithm>
#include <tuple>
#include <chrono>
#include <filesystem>
#include <concepts>

#if defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || \
	defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
#	if __has_include(<unistd.h>)
#		include <unistd.h>
#	endif
#	if __has_include(<sys/types.h>)
#		include <sys/types.h>
#	endif
#	if __has_include(<sys/stat.h>)
#		include <sys/stat.h>
#	endif
#	if __has_include(<dirent.h>)
#		include <dirent.h>
#	endif
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || \
	defined(_WINDOWS_) || defined(__WINDOWS__) || defined(__TOS_WIN__)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	if __has_include(<Windows.h>)
#		include <Windows.h>
#	endif
#	if __has_include(<tchar.h>)
#		include <tchar.h>
#	endif
#	if __has_include(<io.h>)
#		include <io.h>
#	endif
#	if __has_include(<direct.h>)
#		include <direct.h>
#	endif
#elif defined(__APPLE__) && defined(__MACH__)
#	if __has_include(<mach-o/dyld.h>)
#		include <mach-o/dyld.h>
#	endif
#endif

/*
 * mutex:
 * Linux platform needs to add -lpthread option in link libraries
 */

#include <asio3/config.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace asio::detail::iniutil
#else
namespace boost::asio::detail::iniutil
#endif
{
	template<class T>
	struct convert;

	template<>
	struct convert<bool>
	{
		/**
		 * @brief Returns `true` if two strings are equal, using a case-insensitive comparison.
		 */
		template<typename = void>
		inline static bool iequals(std::string_view lhs, std::string_view rhs) noexcept
		{
			auto n = lhs.size();
			if (rhs.size() != n)
				return false;
			auto p1 = lhs.data();
			auto p2 = rhs.data();
			char a, b;
			// fast loop
			while (n--)
			{
				a = *p1++;
				b = *p2++;
				if (a != b)
					goto slow;
			}
			return true;
		slow:
			do
			{
				if (std::tolower(a) != std::tolower(b))
					return false;
				a = *p1++;
				b = *p2++;
			} while (n--);
			return true;
		}

		template<
			class CharT,
			class Traits = std::char_traits<CharT>,
			class Allocator = std::allocator<CharT>
		>
		inline static bool stov(std::basic_string<CharT, Traits, Allocator>& val)
		{
			if (iequals(val, "true"))
				return true;
			if (iequals(val, "false"))
				return false;
			return (!(std::stoi(val) == 0));
		}
	};

	template<>
	struct convert<char>
	{
		template<class ...Args>
		inline static char stov(Args&&... args)
		{ return static_cast<char>(std::stoi(std::forward<Args>(args)...)); }
	};

	template<>
	struct convert<signed char>
	{
		template<class ...Args>
		inline static signed char stov(Args&&... args)
		{ return static_cast<signed char>(std::stoi(std::forward<Args>(args)...)); }
	};

	template<>
	struct convert<unsigned char>
	{
		template<class ...Args>
		inline static unsigned char stov(Args&&... args)
		{ return static_cast<unsigned char>(std::stoul(std::forward<Args>(args)...)); }
	};

	template<>
	struct convert<short>
	{
		template<class ...Args>
		inline static short stov(Args&&... args)
		{ return static_cast<short>(std::stoi(std::forward<Args>(args)...)); }
	};

	template<>
	struct convert<unsigned short>
	{
		template<class ...Args>
		inline static unsigned short stov(Args&&... args)
		{ return static_cast<unsigned short>(std::stoul(std::forward<Args>(args)...)); }
	};

	template<>
	struct convert<int>
	{
		template<class ...Args>
		inline static int stov(Args&&... args)
		{ return std::stoi(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<unsigned int>
	{
		template<class ...Args>
		inline static unsigned int stov(Args&&... args)
		{ return static_cast<unsigned int>(std::stoul(std::forward<Args>(args)...)); }
	};

	template<>
	struct convert<long>
	{
		template<class ...Args>
		inline static long stov(Args&&... args)
		{ return std::stol(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<unsigned long>
	{
		template<class ...Args>
		inline static unsigned long stov(Args&&... args)
		{ return std::stoul(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<long long>
	{
		template<class ...Args>
		inline static long long stov(Args&&... args)
		{ return std::stoll(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<unsigned long long>
	{
		template<class ...Args>
		inline static unsigned long long stov(Args&&... args)
		{ return std::stoull(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<float>
	{
		template<class ...Args>
		inline static float stov(Args&&... args)
		{ return std::stof(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<double>
	{
		template<class ...Args>
		inline static double stov(Args&&... args)
		{ return std::stod(std::forward<Args>(args)...); }
	};

	template<>
	struct convert<long double>
	{
		template<class ...Args>
		inline static long double stov(Args&&... args)
		{ return std::stold(std::forward<Args>(args)...); }
	};

	template<class CharT, class Traits, class Allocator>
	struct convert<std::basic_string<CharT, Traits, Allocator>>
	{
		template<class ...Args>
		inline static std::basic_string<CharT, Traits, Allocator> stov(Args&&... args)
		{ return std::basic_string<CharT, Traits, Allocator>(std::forward<Args>(args)...); }
	};

	template<class CharT, class Traits>
	struct convert<std::basic_string_view<CharT, Traits>>
	{
		template<class ...Args>
		inline static std::basic_string_view<CharT, Traits> stov(Args&&... args)
		{ return std::basic_string_view<CharT, Traits>(std::forward<Args>(args)...); }
	};

	template<class Rep, class Period>
	struct convert<std::chrono::duration<Rep, Period>>
	{
		// referenced from: C# TimeSpan
		// 30                 - 30 seconds
		// 00:00:00.0000036   - 36 milliseconds
		// 00:00:00           - 0 seconds
		// 2.10:36:45         - 2 days 10 hours 36 minutes 45 seconds
		// 2.00:00:00.0000036 - 
		template<class S>
		inline static std::chrono::duration<Rep, Period> stov(S&& s)
		{
			std::size_t n1 = s.find(':');

			if (n1 == std::string::npos)
				return std::chrono::seconds(std::stoll(s));

			int day = 0, hour = 0, min = 0, sec = 0, msec = 0;

			std::size_t m1 = s.find('.');

			if (m1 < n1)
			{
				day = std::stoi(s.substr(0, m1));
				s.erase(0, m1 + 1);
			}

			n1 = s.find(':');
			hour = std::stoi(s.substr(0, n1));
			s.erase(0, n1 + 1);

			n1 = s.find(':');
			min = std::stoi(s.substr(0, n1));
			s.erase(0, n1 + 1);

			n1 = s.find('.');
			sec = std::stoi(s.substr(0, n1));

			if (n1 != std::string::npos)
			{
				s.erase(0, n1 + 1);

				msec = std::stoi(s);
			}

			return
				std::chrono::hours(day * 24) +
				std::chrono::hours(hour) +
				std::chrono::minutes(min) +
				std::chrono::seconds(sec) +
				std::chrono::milliseconds(msec);
		}
	};

	template<class Clock, class Duration>
	struct convert<std::chrono::time_point<Clock, Duration>>
	{
		template<class S>
		inline static std::chrono::time_point<Clock, Duration> stov(S&& s)
		{
			std::stringstream ss;
			ss << std::forward<S>(s);

			std::tm t{};

			if (s.find('/') != std::string::npos)
				ss >> std::get_time(&t, "%m/%d/%Y %H:%M:%S");
			else
				ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");

			return Clock::from_time_t(std::mktime(&t));
		}
	};
}

#ifdef ASIO3_HEADER_ONLY
namespace asio
#else
namespace boost::asio
#endif
{
	// use namespace asio::detail::iniutil to avoid conflict with
	// asio::detail in file "asio3/core/strutil.hpp"
	// is_basic_string_view ...
	namespace detail::iniutil
	{
		enum class line_type : std::uint8_t
		{
			section = 1,
			kv,
			comment,
			other,
			eof,
		};

		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_fstream = std::is_same_v<U, std::basic_fstream<
			typename U::char_type, typename U::traits_type>>;

		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_ifstream = std::is_same_v<U, std::basic_ifstream<
			typename U::char_type, typename U::traits_type>>;

		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_ofstream = std::is_same_v<U, std::basic_ofstream<
			typename U::char_type, typename U::traits_type>>;

		template<typename T>
		concept is_file_stream = is_fstream<T> || is_ifstream<T> || is_ofstream<T>;

		// char,wchar_t...
		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_char =
			std::is_same_v<U, char    > ||
			std::is_same_v<U, wchar_t > ||
			std::is_same_v<U, char8_t > ||
			std::is_same_v<U, char16_t> ||
			std::is_same_v<U, char32_t>;

		// std::basic_string<...>
		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_basic_string = std::is_same_v<U, std::basic_string<
			typename U::value_type, typename U::traits_type, typename U::allocator_type>>;

		// std::basic_string_view<...>
		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_basic_string_view = std::is_same_v<U, std::basic_string_view<
			typename U::value_type, typename U::traits_type>>;

		// char*, const char*, not char**
		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_char_pointer =
			std::is_pointer_v<U> &&
			!std::is_pointer_v<std::remove_cvref_t<std::remove_pointer_t<U>>> &&
			detail::iniutil::is_char<std::remove_pointer_t<U>>;

		// char[]
		template<typename T, typename U = std::remove_cvref_t<T>>
		concept is_char_array =
			std::is_array_v<U> &&
			detail::iniutil::is_char<std::remove_all_extents_t<U>>;

		template<class R>
		struct return_type
		{
			template<class T, bool> struct string_view_traits { using type = T; };

			template<class T> struct string_view_traits<T, true>
			{
				using type = std::basic_string<typename std::remove_cv_t<std::remove_reference_t<R>>::value_type>;
			};

			using type = typename std::conditional_t<is_char_pointer<R> || is_char_array<R>,
				std::basic_string<std::remove_cv_t<std::remove_all_extents_t<
				std::remove_pointer_t<std::remove_cv_t<std::remove_reference_t<R>>>>>>,
				typename string_view_traits<R, is_basic_string_view<R>>::type>;
		};
	}

	namespace detail
	{
		template<class Stream>
		class basic_file_ini_impl : public Stream
		{
		public:
			template<class ...Args>
			basic_file_ini_impl(Args&&... args)
			{
				std::ios_base::openmode mode{};

				if constexpr /**/ (sizeof...(Args) == 0)
				{
				#if defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || \
					defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
					filepath_.resize(PATH_MAX);
					auto r = readlink("/proc/self/exe", (char *)filepath_.data(), PATH_MAX);
					std::ignore = r; // gcc 7 warning: ignoring return value of ... [-Wunused-result]
				#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || \
					defined(_WINDOWS_) || defined(__WINDOWS__) || defined(__TOS_WIN__)
					filepath_.resize(MAX_PATH);
					filepath_.resize(::GetModuleFileNameA(NULL, (LPSTR)filepath_.data(), MAX_PATH));
				#elif defined(__APPLE__) && defined(__MACH__)
					filepath_.resize(PATH_MAX);
					std::uint32_t bufsize = std::uint32_t(PATH_MAX);
					_NSGetExecutablePath(filepath_.data(), std::addressof(bufsize));
				#endif
					
					if (std::string::size_type pos = filepath_.find('\0'); pos != std::string::npos)
						filepath_.resize(pos);

				#if defined(_DEBUG) || defined(DEBUG)
					assert(!filepath_.empty());
				#endif

					std::filesystem::path path{ filepath_ };

					std::string name = path.filename().string();

					std::string ext = path.extension().string();

					name.resize(name.size() - ext.size());

					filepath_ = path.parent_path().append(name).string() + ".ini";
				}
				else if constexpr (sizeof...(Args) == 1)
				{
					filepath_ = std::move(std::get<0>(std::make_tuple(std::forward<Args>(args)...)));
				}
				else if constexpr (sizeof...(Args) == 2)
				{
					auto t = std::make_tuple(std::forward<Args>(args)...);

					filepath_ = std::move(std::get<0>(t));
					mode |= std::get<1>(t);
				}
				else
				{
					std::ignore = true;
				}

				std::error_code ec;

				// if file is not exists, create it
				if (bool b = std::filesystem::exists(filepath_, ec); !b && !ec)
				{
					Stream f(filepath_, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
				}

				if constexpr /**/ (detail::iniutil::is_fstream<Stream>)
				{
					mode |= std::ios_base::in | std::ios_base::out | std::ios_base::binary;
				}
				else if constexpr (detail::iniutil::is_ifstream<Stream>)
				{
					mode |= std::ios_base::in | std::ios_base::binary;
				}
				else if constexpr (detail::iniutil::is_ofstream<Stream>)
				{
					mode |= std::ios_base::out | std::ios_base::binary;
				}
				else
				{
					mode |= std::ios_base::in | std::ios_base::out | std::ios_base::binary;
				}

				Stream::open(filepath_, mode);
			}

			~basic_file_ini_impl()
			{
				using pos_type = typename Stream::pos_type;

				Stream::clear();
				Stream::seekg(0, std::ios::end);
				auto filesize = Stream::tellg();

				if (filesize)
				{
					pos_type spaces = pos_type(0);

					do
					{
						Stream::clear();
						Stream::seekg(filesize - spaces - pos_type(1));

						char c;
						if (!Stream::get(c))
							break;

						if (c == ' ' || c == '\0')
							spaces = spaces + pos_type(1);
						else
							break;

					} while (true);

					if (spaces)
					{
						std::error_code ec;
						std::filesystem::resize_file(filepath_, filesize - spaces, ec);
					}
				}
			}

			inline std::string filepath() { return filepath_; }

		protected:
			std::string                  filepath_;
		};

		template<class Stream>
		class basic_ini_impl : public Stream
		{
		public:
			template<class ...Args>
			basic_ini_impl(Args&&... args) : Stream(std::forward<Args>(args)...) {}
		};

		template<class... Ts>
		class basic_ini_impl<std::basic_fstream<Ts...>> : public basic_file_ini_impl<std::basic_fstream<Ts...>>
		{
		public:
			template<class ...Args>
			basic_ini_impl(Args&&... args) : basic_file_ini_impl<std::basic_fstream<Ts...>>(std::forward<Args>(args)...) {}
		};

		template<class... Ts>
		class basic_ini_impl<std::basic_ifstream<Ts...>> : public basic_file_ini_impl<std::basic_ifstream<Ts...>>
		{
		public:
			template<class ...Args>
			basic_ini_impl(Args&&... args) : basic_file_ini_impl<std::basic_ifstream<Ts...>>(std::forward<Args>(args)...) {}
		};

		template<class... Ts>
		class basic_ini_impl<std::basic_ofstream<Ts...>> : public basic_file_ini_impl<std::basic_ofstream<Ts...>>
		{
		public:
			template<class ...Args>
			basic_ini_impl(Args&&... args) : basic_file_ini_impl<std::basic_ofstream<Ts...>>(std::forward<Args>(args)...) {}
		};
	}

	/**
	 * basic_ini operator class
	 */
	template<class Stream>
	class basic_ini : public detail::basic_ini_impl<Stream>
	{
	public:
		using char_type = typename Stream::char_type;
		using pos_type  = typename Stream::pos_type;
		using size_type = typename std::basic_string<char_type>::size_type;

		template<class ...Args>
		basic_ini(Args&&... args) : detail::basic_ini_impl<Stream>(std::forward<Args>(args)...)
		{
			this->endl_ = { '\n' };
		#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || \
			defined(_WINDOWS_) || defined(__WINDOWS__) || defined(__TOS_WIN__)
			this->endl_ = { '\r','\n' };
		#elif defined(__APPLE__) && defined(__MACH__)
			// on the macos 9, the newline character is '\r'.
			// the last macos 9 version is 9.2.2 (20011205)
			//this->endl_ = { '\r' };
		#endif
		}

	public:
		/**
		 * get the value associated with a key in the specified section of an ini file.
		 * This function does not throw an exception.
		 * example : 
		 * asio::ini ini("config.ini");
		 * std::string   host = ini.get("main", "host", "127.0.0.1");
		 * std::uint16_t port = ini.get("main", "port", 8080);
		 * or : 
		 * std::string   host = ini.get<std::string  >("main", "host");
		 * std::uint16_t port = ini.get<std::uint16_t>("main", "port");
		 */
		template<class R, class Sec, class Key, class Traits = std::char_traits<char_type>,
			class Allocator = std::allocator<char_type>>
		inline typename detail::iniutil::return_type<R>::type
			get(const Sec& sec, const Key& key, R default_val = R())
		{
			using return_t = typename detail::iniutil::return_type<R>::type;

			try
			{
				std::basic_string<char_type, Traits, Allocator> val;

				bool flag = this->_get_impl(
					std::basic_string_view<char_type, Traits>(sec),
					std::basic_string_view<char_type, Traits>(key),
					val);

				if constexpr (detail::iniutil::is_char_pointer<R> || detail::iniutil::is_char_array<R>)
				{
					return (flag ? val : return_t{ default_val });
				}
				else if constexpr (detail::iniutil::is_basic_string_view<R>)
				{
					return (flag ? val : return_t{ default_val });
				}
				else
				{
					return (flag ? asio::detail::iniutil::convert<R>::stov(val) : default_val);
				}
			}
			catch (std::invalid_argument const&)
			{
			}
			catch (std::out_of_range const&)
			{
			}
			catch (std::exception const&)
			{
			}

			return return_t{ default_val };
		}

		/**
		 * set the value associated with a key in the specified section of an ini file.
		 * example :
		 * asio::ini ini("config.ini");
		 * ini.set("main", "host", "127.0.0.1");
		 * ini.set("main", "port", 8080);
		 */
		template<class Sec, class Key, class Val, class Traits = std::char_traits<char_type>>
		requires requires(Sec a, Key b, Val c)
		{
			std::basic_string_view<char_type, Traits>(a);
			std::basic_string_view<char_type, Traits>(b);
			std::basic_string_view<char_type, Traits>(c);
		}
		inline bool set(const Sec& sec, const Key& key, const Val& val)
		{
			return this->set(
				std::basic_string_view<char_type, Traits>(sec),
				std::basic_string_view<char_type, Traits>(key),
				std::basic_string_view<char_type, Traits>(val));
		}

		/**
		 * set the value associated with a key in the specified section of an ini file.
		 * example :
		 * asio::ini ini("config.ini");
		 * ini.set("main", "host", "127.0.0.1");
		 * ini.set("main", "port", 8080);
		 */
		template<class Sec, class Key, class Val, class Traits = std::char_traits<char_type>>
		requires requires(Sec a, Key b, Val c)
		{
			std::basic_string_view<char_type, Traits>(a);
			std::basic_string_view<char_type, Traits>(b);
			std::to_string(c);
		}
		inline bool set(const Sec& sec, const Key& key, Val val)
		{
			std::basic_string<char_type, Traits> v = std::to_string(val);
			return this->set(
				std::basic_string_view<char_type, Traits>(sec),
				std::basic_string_view<char_type, Traits>(key),
				std::basic_string_view<char_type, Traits>(v));
		}

		/**
		 * set the value associated with a key in the specified section of an ini file.
		 * example :
		 * asio::ini ini("config.ini");
		 * ini.set("main", "host", "127.0.0.1");
		 * ini.set("main", "port", 8080);
		 */
		template<class Traits = std::char_traits<char_type>, class Allocator = std::allocator<char_type>>
		bool set(
			std::basic_string_view<char_type, Traits> sec,
			std::basic_string_view<char_type, Traits> key,
			std::basic_string_view<char_type, Traits> val)
		{
			std::unique_lock guard(this->mutex_);

			Stream::clear();
			if (Stream::operator bool())
			{
				std::basic_string<char_type, Traits, Allocator> line;
				std::basic_string<char_type, Traits, Allocator> s, k, v;
				pos_type posg = 0;
				detail::iniutil::line_type ret;

				auto update_v = [&]() mutable -> bool
				{
					try
					{
						if (val != v)
						{
							Stream::clear();
							Stream::seekg(0, std::ios::end);
							auto filesize = Stream::tellg();

							std::basic_string<char_type, Traits, Allocator> content;
							auto pos = line.find_first_of('=');
							++pos;
							while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
								++pos;
							content += line.substr(0, pos);
							content += val;
							content += this->endl_;

							int pos_diff = int(line.size() + 1 - content.size());

							Stream::clear();
							Stream::seekg(posg + pos_type(line.size() + 1));

							int remain = int(filesize - (posg + pos_type(line.size() + 1)));
							if (remain > 0)
							{
								content.resize(size_type(content.size() + remain));
								Stream::read(content.data() + content.size() - remain, remain);
							}

							if (pos_diff > 0) content.append(pos_diff, ' ');

							while (!content.empty() &&
								(pos_type(content.size()) + posg > filesize) &&
								content.back() == ' ')
							{
								content.erase(content.size() - 1);
							}

							Stream::clear();
							Stream::seekp(posg);
							//*this << content;
							Stream::write(content.data(), content.size());
							Stream::flush();
						}
						return true;
					}
					catch (std::exception &) {}
					return false;
				};

				Stream::seekg(0, std::ios::beg);

				for (;;)
				{
					ret = this->_getline(line, s, k, v, posg);

					if (ret == detail::iniutil::line_type::eof)
						break;

					switch (ret)
					{
					case detail::iniutil::line_type::section:
						if (s == sec)
						{
							for (;;)
							{
								ret = this->_getline(line, s, k, v, posg);

								if (ret == detail::iniutil::line_type::section)
									break;
								if (ret == detail::iniutil::line_type::eof)
									break;

								if (ret == detail::iniutil::line_type::kv && k == key)
								{
									return update_v();
								}
							}

							// can't find the key, add a new key
							std::basic_string<char_type, Traits, Allocator> content;

							if (posg == pos_type(-1))
							{
								Stream::clear();
								Stream::seekg(0, std::ios::end);
								posg = Stream::tellg();
							}

							content += this->endl_;
							content += key;
							content += '=';
							content += val;
							content += this->endl_;

							Stream::clear();
							Stream::seekg(0, std::ios::end);
							auto filesize = Stream::tellg();

							Stream::clear();
							Stream::seekg(posg);

							int remain = int(filesize - posg);
							if (remain > 0)
							{
								content.resize(size_type(content.size() + remain));
								Stream::read(content.data() + content.size() - remain, remain);
							}

							while (!content.empty() &&
								(pos_type(content.size()) + posg > filesize) &&
								content.back() == ' ')
							{
								content.erase(content.size() - 1);
							}

							Stream::clear();
							Stream::seekp(posg);
							//*this << content;
							Stream::write(content.data(), content.size());
							Stream::flush();

							return true;
						}
						break;
					case detail::iniutil::line_type::kv:
						if (s == sec)
						{
							if (k == key)
							{
								return update_v();
							}
						}
						break;
					default:break;
					}
				}

				// can't find the sec and key, add a new sec and key.
				std::basic_string<char_type, Traits, Allocator> content;

				Stream::clear();
				Stream::seekg(0, std::ios::end);
				auto filesize = Stream::tellg();
				content.resize(size_type(filesize));

				Stream::clear();
				Stream::seekg(0, std::ios::beg);
				Stream::read(content.data(), content.size());

				if (!content.empty() && content.back() == '\n') content.erase(content.size() - 1);
				if (!content.empty() && content.back() == '\r') content.erase(content.size() - 1);

				std::basic_string<char_type, Traits, Allocator> buffer;

				if (!sec.empty())
				{
					buffer += '[';
					buffer += sec;
					buffer += ']';
					buffer += this->endl_;
				}

				buffer += key;
				buffer += '=';
				buffer += val;
				buffer += this->endl_;

				if (!sec.empty())
				{
					if (content.empty())
						content = std::move(buffer);
					else
						content = content + this->endl_ + buffer;
				}
				// if the sec is empty ( mean global), must add the key at the begin.
				else
				{
					if (content.empty())
						content = std::move(buffer);
					else
						content = buffer + content;
				}

				while (!content.empty() &&
					(pos_type(content.size()) > filesize) &&
					content.back() == ' ')
				{
					content.erase(content.size() - 1);
				}

				Stream::clear();
				Stream::seekp(0, std::ios::beg);
				//*this << content;
				Stream::write(content.data(), content.size());
				Stream::flush();

				return true;
			}
			return false;
		}

	protected:
		template<class Traits = std::char_traits<char_type>, class Allocator = std::allocator<char_type>>
		bool _get_impl(
			std::basic_string_view<char_type, Traits> sec,
			std::basic_string_view<char_type, Traits> key,
			std::basic_string<char_type, Traits, Allocator>& val)
		{
			std::shared_lock guard(this->mutex_);

			Stream::clear();
			if (Stream::operator bool())
			{
				std::basic_string<char_type, Traits, Allocator> line;
				std::basic_string<char_type, Traits, Allocator> s, k, v;
				pos_type posg;

				Stream::seekg(0, std::ios::beg);

				detail::iniutil::line_type ret;
				for (;;)
				{
					ret = this->_getline(line, s, k, v, posg);

					if (ret == detail::iniutil::line_type::eof)
						break;

					switch (ret)
					{
					case detail::iniutil::line_type::kv:
						if (s == sec)
						{
							if (k == key)
							{
								val = std::move(v);
								return true;
							}
						}
						break;
					case detail::iniutil::line_type::section:
						if (sec.empty() && !s.empty())
							return false;
						if (s == sec)
						{
							for (;;)
							{
								ret = this->_getline(line, s, k, v, posg);

								if (ret == detail::iniutil::line_type::section)
									break;
								if (ret == detail::iniutil::line_type::eof)
									break;

								if (ret == detail::iniutil::line_type::kv && k == key)
								{
									val = std::move(v);
									return true;
								}
							}

							return false;
						}
						break;
					default:break;
					}
				}
			}
			return false;
		}

		template<class Traits = std::char_traits<char_type>, class Allocator = std::allocator<char_type>>
		detail::iniutil::line_type _getline(
			std::basic_string<char_type, Traits, Allocator> & line,
			std::basic_string<char_type, Traits, Allocator> & sec,
			std::basic_string<char_type, Traits, Allocator> & key,
			std::basic_string<char_type, Traits, Allocator> & val,
			pos_type & posg)
		{
			Stream::clear();
			if (Stream::good() && !Stream::eof())
			{
				posg = Stream::tellg();

				if (posg != pos_type(-1) && std::getline(*this, line, this->endl_.back()))
				{
					auto trim_line = line;

					trim_left(trim_line);
					trim_right(trim_line);

					if (trim_line.empty())
						return detail::iniutil::line_type::other;

					if (trim_line[0] == ';' || trim_line[0] == '#' || trim_line[0] == ':')
						return detail::iniutil::line_type::comment;

					if (trim_line.size() > 1 && trim_line[0] == '/' && trim_line[1] == '/')
						return detail::iniutil::line_type::comment;

					if (trim_line.front() == '[' && trim_line.back() == ']')
					{
						sec = trim_line.substr(1, trim_line.size() - 2);

						trim_left(sec);
						trim_right(sec);

						return detail::iniutil::line_type::section;
					}

					auto sep = trim_line.find_first_of('=');

					// current line is key and val
					if (sep != std::basic_string<char_type, Traits, Allocator>::npos && sep > 0)
					{
						key = trim_line.substr(0, sep);
						trim_left(key);
						trim_right(key);

						val = trim_line.substr(sep + 1);
						trim_left(val);
						trim_right(val);

						return detail::iniutil::line_type::kv;
					}

					return detail::iniutil::line_type::other;
				}
			}

			return detail::iniutil::line_type::eof;
		}

		template<
			class CharT,
			class Traits = std::char_traits<CharT>,
			class Allocator = std::allocator<CharT>
		>
		std::basic_string<CharT, Traits, Allocator>& trim_left(std::basic_string<CharT, Traits, Allocator>& s)
		{
			if (s.empty())
				return s;
			using size_type = typename std::basic_string<CharT, Traits, Allocator>::size_type;
			std::locale l{};
			size_type pos = 0;
			for (; pos < s.size(); ++pos)
			{
				if (!std::isspace(s[pos], l))
					break;
			}
			s.erase(0, pos);
			return s;
		}

		template<
			class CharT,
			class Traits = std::char_traits<CharT>,
			class Allocator = std::allocator<CharT>
		>
		std::basic_string<CharT, Traits, Allocator>& trim_right(std::basic_string<CharT, Traits, Allocator>& s)
		{
			if (s.empty())
				return s;
			using size_type = typename std::basic_string<CharT, Traits, Allocator>::size_type;
			std::locale l{};
			size_type pos = s.size() - 1;
			for (; pos != size_type(-1); pos--)
			{
				if (!std::isspace(s[pos], l))
					break;
			}
			s.erase(pos + 1);
			return s;
		}

	protected:
		mutable std::shared_mutex   mutex_;

		std::basic_string<char_type> endl_;
	};

	using ini     = basic_ini<std::fstream>;
}

#if defined(__GNUC__) || defined(__GNUG__)
#  pragma GCC diagnostic pop
#endif
