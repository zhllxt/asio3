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

#include <istream>
#include <ostream>
#include <streambuf>
#include <string>
#include <string_view>

#include <asio3/core/asio.hpp>
#include <asio3/core/cereal.hpp>

#include <asio3/rpc/detail/reusable_portable_binary.hpp>
#include <asio3/rpc/error.hpp>

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	class ostrbuf : public std::streambuf
	{
	public:
		using string_type = std::basic_string<char_type, traits_type>;
		using size_type = typename string_type::size_type;

		ostrbuf() {}
		virtual ~ostrbuf() noexcept {}

		inline string_type& str() noexcept
		{
			return this->str_;
		}

		inline void clear() noexcept
		{
			this->str_.clear();

			this->setp(this->str_.data(), this->str_.data() + this->str_.size());
		}

	protected:
		virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override
		{
			this->str_.append(s, size_type(count));

			this->setp(this->str_.data(), this->str_.data() + this->str_.size());

			return count;
		}

	protected:
		string_type str_;
	};

	class istrbuf : public std::streambuf
	{
	public:
		using string_type = std::basic_string<char_type, traits_type>;
		using size_type = typename string_type::size_type;

		istrbuf() {}
		virtual ~istrbuf() noexcept {}

		inline void setbuf(std::string_view s) noexcept
		{
			this->setbuf(const_cast<char_type*>(s.data()), std::streamsize(s.size()));
		}

		virtual std::streambuf* setbuf(char_type* s, std::streamsize n) override
		{
			this->setg(s, s, s + n);
			return this;
		}

	protected:
		virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override
		{
			if (this->in_avail() < count)
				return std::streamsize(0);

			std::memcpy((void*)s, (const void*)(this->gptr()), size_type(count));

			this->setg(this->eback(), this->gptr() + count, this->egptr());

			return count;
		}
	};

	class serializer
	{
	public:
		using oarchive = cereal::ReusablePortableBinaryOutputArchive;

		serializer()
			: obuffer_()
			, ostream_(std::addressof(obuffer_))
			, oarchive_(ostream_)
		{}
		~serializer() = default;

		template<typename T>
		inline serializer& operator<<(const T& v)
		{
			this->oarchive_ << v;
			return (*this);
		}

		inline serializer& operator<<(const error_code& ec)
		{
			this->oarchive_ << ec.value();
			return (*this);
		}

		template<class ...Args>
		inline serializer& save(const Args&... args)
		{
			((this->oarchive_ << args), ...);
			return (*this);
		}

		inline serializer& reset()
		{
			this->obuffer_.clear();
			this->oarchive_.save_endian();
			return (*this);
		}

		inline auto& str() noexcept
		{
			return this->obuffer_.str();
		}

		inline ostrbuf& buffer() noexcept { return this->obuffer_; }

	protected:
		ostrbuf         obuffer_;
		std::ostream    ostream_;
		oarchive        oarchive_;
	};

	class deserializer
	{
	public:
		using iarchive = cereal::ReusablePortableBinaryInputArchive;

		deserializer()
			: ibuffer_()
			, istream_(std::addressof(ibuffer_))
			, iarchive_(istream_)
		{}
		~deserializer() = default;

		template<typename T>
		inline deserializer& operator>>(T& v)
		{
			this->iarchive_ >> v;
			return (*this);
		}

		inline deserializer& operator>>(error_code& ec)
		{
			decltype(ec.value()) v;
			this->iarchive_ >> v;
			ec.assign(v, ec.category());
			return (*this);
		}

		template<class ...Args>
		inline deserializer& load(Args&... args)
		{
			((this->iarchive_ >> args), ...);
			return (*this);
		}

		inline deserializer& reset(std::string_view s)
		{
			this->ibuffer_.setbuf(s);
			this->iarchive_.load_endian();
			return (*this);
		}

		inline istrbuf& buffer() noexcept { return this->ibuffer_; }

	protected:
		istrbuf         ibuffer_;
		std::istream    istream_;
		iarchive        iarchive_;
	};
}

#ifdef ASIO_STANDALONE
namespace asio::rpc
#else
namespace boost::asio::rpc
#endif
{
	// for serializer
	using oarchive = cereal::ReusablePortableBinaryOutputArchive;

	// for deserializer
	using iarchive = cereal::ReusablePortableBinaryInputArchive;
}
