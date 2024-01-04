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

#include <mutex>
#include <shared_mutex>

// https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
// https://stackoverflow.com/questions/33608378/clang-thread-safety-annotation-and-shared-capabilities

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))
#else
#define ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(x)   // no-op
#endif

#define ASIO3_CAPABILITY(x) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define ASIO3_SCOPED_CAPABILITY \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define ASIO3_GUARDED_BY(x) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define ASIO3_PT_GUARDED_BY(x) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ASIO3_ACQUIRED_BEFORE(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ASIO3_ACQUIRED_AFTER(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define ASIO3_REQUIRES(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define ASIO3_REQUIRES_SHARED(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ASIO3_ACQUIRE(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ASIO3_ACQUIRE_SHARED(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define ASIO3_RELEASE(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define ASIO3_RELEASE_SHARED(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define ASIO3_RELEASE_GENERIC(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))

#define ASIO3_TRY_ACQUIRE(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define ASIO3_TRY_ACQUIRE_SHARED(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define ASIO3_EXCLUDES(...) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASIO3_ASSERT_CAPABILITY(x) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASIO3_ASSERT_SHARED_CAPABILITY(x) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define ASIO3_RETURN_CAPABILITY(x) \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define ASIO3_NO_THREAD_SAFETY_ANALYSIS \
  ASIO3_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#include <asio3/config.hpp>

#ifdef ASIO3_HEADER_ONLY
namespace asio
#else
namespace boost::asio
#endif
{
	// Defines an annotated interface for mutexes.
	// These methods can be implemented to use any internal mutex implementation.
	class ASIO3_CAPABILITY("mutex") shared_mutexer
	{
	public:
		inline std::shared_mutex& native_handle() { return mutex_; }

		// Good software engineering practice dictates that mutexes should be private members,
		// because the locking mechanism used by a thread-safe class is part of its internal
		// implementation. However, private mutexes can sometimes leak into the public interface
		// of a class. Thread safety attributes follow normal C++ access restrictions, so if is
		// a private member of , then it is an error to write in an attribute.mucc.mu
	private:
		mutable std::shared_mutex mutex_;
	};

	// shared_locker is an RAII class that acquires a mutex in its constructor, and
	// releases it in its destructor.
	class ASIO3_SCOPED_CAPABILITY shared_locker
	{
	public:
		// Acquire mutex in shared mode, implicitly acquire *this and associate it with mutex.
		explicit shared_locker(shared_mutexer& m) ASIO3_ACQUIRE_SHARED(m) : lock_(m.native_handle()) {}

		// Release *this and all associated mutexes, if they are still held.
		// There is no warning if the scope was already unlocked before.
		// Note: can't use ASIO3_RELEASE_SHARED
		// @see: https://stackoverflow.com/questions/33608378/clang-thread-safety-annotation-and-shared-capabilities
		~shared_locker() ASIO3_RELEASE()
		{
		}

	private:
		std::shared_lock<std::shared_mutex> lock_;
	};

	// unique_locker is an RAII class that acquires a mutex in its constructor, and
	// releases it in its destructor.
	class ASIO3_SCOPED_CAPABILITY unique_locker
	{
	public:
		// Acquire mutex, implicitly acquire *this and associate it with mutex.
		explicit unique_locker(shared_mutexer& m) ASIO3_ACQUIRE(m) : lock_(m.native_handle()) {}

		// Release *this and all associated mutexes, if they are still held.
		// There is no warning if the scope was already unlocked before.
		~unique_locker() ASIO3_RELEASE()
		{
		}

	private:
		std::unique_lock<std::shared_mutex> lock_;
	};
}
