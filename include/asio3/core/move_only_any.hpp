// referenced from any standard header of msvc

#pragma once

#include <cstdint>
#include <cstring>

#include <iterator>
#include <utility>

#include <any>

namespace std::experimental
{
	namespace detail
	{
		inline constexpr int small_object_num_ptrs = 6 + 16 / sizeof(void*);

		inline constexpr size_t trivial_space_size = (small_object_num_ptrs - 1) * sizeof(void*);
		inline constexpr size_t small_space_size = (small_object_num_ptrs - 2) * sizeof(void*);

		template <class T>
		inline constexpr bool any_is_trivial =
			alignof(T) <= alignof(max_align_t) &&
			is_trivially_copyable_v<T> &&
			sizeof(T) <= small_space_size;

		template <class T>
		inline constexpr bool any_is_small =
			alignof(T) <= alignof(max_align_t) &&
			is_nothrow_move_constructible_v<T> &&
			sizeof(T) <= small_space_size;

		template <class T, class... Args>
		inline void construct_in_place(T& obj, Args&&... args) noexcept(is_nothrow_constructible_v<T, Args...>)
		{
			::new (static_cast<void*>(std::addressof(obj))) T(std::forward<Args>(args)...);
		}

		template <class NoThrowFwdIt, class NoThrowSentinel>
		inline void destroy_range(NoThrowFwdIt first, const NoThrowSentinel last) noexcept;

		template <class T>
		inline void destroy_in_place(T& obj) noexcept
		{
			if constexpr (is_array_v<T>)
			{
				destroy_range(obj, obj + extent_v<T>);
			}
			else
			{
				obj.~T();
			}
		}

		template <class NoThrowFwdIt, class NoThrowSentinel>
		inline void destroy_range(NoThrowFwdIt first, const NoThrowSentinel last) noexcept
		{
			// note that this is an optimization for debug mode codegen; in release mode the BE removes all of this
			if constexpr (!is_trivially_destructible_v<iter_value_t<NoThrowFwdIt>>)
			{
				for (; first != last; ++first)
				{
					destroy_in_place(*first);
				}
			}
		}

		struct big_rtti
		{
			// Hand-rolled vtable for types that must be heap allocated in an move_only_any
			using destroy_fn = void(void*) noexcept;
			using type_fn = const std::type_info& () noexcept;

			template <class T>
			static void destroy_impl(void* const target) noexcept
			{
				::delete static_cast<T*>(target);
			}

			template <class T>
			[[nodiscard]] static const std::type_info& type_impl() noexcept
			{
				return typeid(T);
			}

			destroy_fn* destroy;
			type_fn* type;
		};

		struct small_rtti
		{
			// Hand-rolled vtable for nontrivial types that can be stored internally in an move_only_any
			using destroy_fn = void(void*) noexcept;
			using move_fn = void(void*, void*) noexcept;
			using type_fn = const std::type_info& () noexcept;

			template <class T>
			static void destroy_impl(void* const target) noexcept
			{
				destroy_in_place(*static_cast<T*>(target));
			}

			template <class T>
			static void move_impl(void* const target, void* const source) noexcept
			{
				construct_in_place(*static_cast<T*>(target), std::move(*static_cast<T*>(source)));
			}

			template <class T>
			[[nodiscard]] static const std::type_info& type_impl() noexcept
			{
				return typeid(T);
			}

			destroy_fn* destroy;
			move_fn* move;
			type_fn* type;
		};

		struct trivial_rtti
		{
			// Hand-rolled vtable for nontrivial types that can be stored internally in an move_only_any
			using type_fn = const std::type_info& () noexcept;

			template <class T>
			[[nodiscard]] static const std::type_info& type_impl() noexcept
			{
				return typeid(T);
			}

			type_fn* type;
		};

		// true if and only if T is a specialization of U
		template <class T, template <class...> class U>
		inline constexpr bool is_specialization_v = false;
		template <template <class...> class U, class... Args>
		inline constexpr bool is_specialization_v<U<Args...>, U> = true;

		template <class T, template <class...> class U>
		struct is_specialization : bool_constant<is_specialization_v<T, U>> {};

		template <class T>
		inline constexpr big_rtti big_rtti_obj = {
			&big_rtti::destroy_impl<T>, &big_rtti::type_impl<T> };

		template <class T>
		inline constexpr small_rtti small_rtti_obj = {
			&small_rtti::destroy_impl<T>, &small_rtti::move_impl<T>, &small_rtti::type_impl<T> };

		template <class T>
		inline constexpr trivial_rtti trivial_rtti_obj = {
			&trivial_rtti::type_impl<T> };
	}

	class move_only_any final
	{
	private:
		enum class any_representation : uint8_t { none, trivial, big, smally };

	public:
		constexpr move_only_any() noexcept
		{
		}
		~move_only_any() noexcept
		{
			reset();
		}

		move_only_any(const move_only_any&) = delete;
		move_only_any& operator=(const move_only_any&) = delete;

		move_only_any(move_only_any&& other) noexcept
		{
			_move_from(other);
		}
		move_only_any& operator=(move_only_any&& other) noexcept
		{
			reset();
			_move_from(other);
			return *this;
		}

		template <class T, enable_if_t<conjunction_v<
			negation<is_same<decay_t<T>, move_only_any>>,
			negation<detail::is_specialization<decay_t<T>, in_place_type_t>>,
			is_move_constructible<decay_t<T>>>, int> = 0>
		move_only_any(T&& value)
		{
			// initialize with value
			_emplace<decay_t<T>>(std::forward<T>(value));
		}

		template <class T, class... Args, enable_if_t<conjunction_v<
			is_constructible<decay_t<T>, Args...>,
			is_move_constructible<decay_t<T>>>, int> = 0>
		explicit move_only_any(in_place_type_t<T>, Args&&... args)
		{
			// in-place initialize a value of type decay_t<T> with args...
			_emplace<decay_t<T>>(std::forward<Args>(args)...);
		}

		template <class T, class E, class... Args, enable_if_t<conjunction_v<
			is_constructible<decay_t<T>, initializer_list<E>&, Args...>,
			is_move_constructible<decay_t<T>>>, int> = 0>
		explicit move_only_any(in_place_type_t<T>, initializer_list<E> initlist, Args&&... args)
		{
			// in-place initialize a value of type decay_t<T> with initlist and args...
			_emplace<decay_t<T>>(initlist, std::forward<Args>(args)...);
		}

		void reset() noexcept
		{
			switch (storage.type)
			{
			case any_representation::smally:
				storage.small_storage.rtti->destroy(&storage.small_storage.data);
				break;
			case any_representation::big:
				storage.big_storage.rtti->destroy(storage.big_storage.ptr);
				break;
			case any_representation::trivial:
			default:
				break;
			}
			storage.type = any_representation::none;
		}

		void swap(move_only_any& other) noexcept
		{
			other = std::exchange(*this, std::move(other));
		}

		[[nodiscard]] bool has_value() const noexcept
		{
			return storage.type != any_representation::none;
		}

		[[nodiscard]] const type_info& type() const noexcept
		{
			switch (storage.type)
			{
			case any_representation::smally:
				return storage.small_storage.rtti->type();
			case any_representation::big:
				return storage.big_storage.rtti->type();
			case any_representation::trivial:
				return storage.trivial_storage.rtti->type();
			default:
				break;
			}

			return typeid(void);
		}

		template <class T>
		[[nodiscard]] const T* cast() const noexcept
		{
			// if *this contains a value of type T, return a pointer to it
			const type_info& info = type();
			if (info != typeid(T))
			{
				return nullptr;
			}

			if constexpr (detail::any_is_trivial<T>)
			{
				// get a pointer to the contained _Trivial value of type T
				return reinterpret_cast<const T*>(&storage.trivial_storage.data);
			}
			else if constexpr (detail::any_is_small<T>)
			{
				// get a pointer to the contained _Small value of type T
				return reinterpret_cast<const T*>(&storage.small_storage.data);
			}
			else
			{
				// get a pointer to the contained _Big value of type T
				return static_cast<const T*>(storage.big_storage.ptr);
			}
		}

		template <class T>
		[[nodiscard]] T* cast() noexcept
		{
			// if *this contains a value of type T, return a pointer to it
			return const_cast<T*>(static_cast<const move_only_any*>(this)->cast<T>());
		}

	private:
		void _move_from(move_only_any& other) noexcept
		{
			storage.type = other.storage.type;
			other.storage.type = any_representation::none;
			switch (storage.type)
			{
			case any_representation::smally:
				storage.small_storage.rtti = other.storage.small_storage.rtti;
				storage.small_storage.rtti->move(&storage.small_storage.data, &other.storage.small_storage.data);
				break;
			case any_representation::big:
				storage.big_storage.rtti = other.storage.big_storage.rtti;
				storage.big_storage.ptr = other.storage.big_storage.ptr;
				break;
			case any_representation::trivial:
				storage.trivial_storage.rtti = other.storage.trivial_storage.rtti;
				std::memcpy(
					std::addressof(storage.trivial_storage.data),
					std::addressof(other.storage.trivial_storage.data),
					sizeof(storage.trivial_storage.data));
				break;
			default:
				break;
			}
		}

		template <class T, class... Args>
		T& _emplace(Args&&... args)
		{
			// emplace construct T
			if constexpr (detail::any_is_trivial<T>)
			{
				// using the trivial representation
				auto& obj = reinterpret_cast<T&>(storage.trivial_storage.data);
				detail::construct_in_place(obj, std::forward<Args>(args)...);
				storage.trivial_storage.rtti = &detail::trivial_rtti_obj<T>;
				storage.type = any_representation::trivial;
				return obj;
			}
			else if constexpr (detail::any_is_small<T>)
			{
				// using the small representation
				auto& obj = reinterpret_cast<T&>(storage.small_storage.data);
				detail::construct_in_place(obj, std::forward<Args>(args)...);
				storage.small_storage.rtti = &detail::small_rtti_obj<T>;
				storage.type = any_representation::smally;
				return obj;
			}
			else
			{
				// using the big representation
				T* const ptr = ::new T(std::forward<Args>(args)...);
				storage.big_storage.ptr = ptr;
				storage.big_storage.rtti = &detail::big_rtti_obj<T>;
				storage.type = any_representation::big;
				return *ptr;
			}
		}

		struct trivial_storage_t
		{
			unsigned char data[detail::small_space_size];
			const detail::trivial_rtti* rtti;
		};
		static_assert(sizeof(trivial_storage_t) == detail::trivial_space_size);

		struct small_storage_t
		{
			unsigned char data[detail::small_space_size];
			const detail::small_rtti* rtti;
		};
		static_assert(sizeof(small_storage_t) == detail::trivial_space_size);

		struct big_storage_t
		{
			// Pad so that ptr and rtti might share type's cache line
			unsigned char padding[detail::small_space_size - sizeof(void*)];
			void* ptr;
			const detail::big_rtti* rtti;
		};
		static_assert(sizeof(big_storage_t) == detail::trivial_space_size);

		struct storage_t
		{
			union
			{
				trivial_storage_t trivial_storage;
				small_storage_t small_storage;
				big_storage_t big_storage;
			};
			any_representation type;
		};
		static_assert(sizeof(storage_t) == detail::trivial_space_size + sizeof(void*));
		static_assert(is_standard_layout_v<storage_t>);

		union
		{
			storage_t storage{};
			max_align_t dummy;
		};
	};

	template <class T, class... Args, enable_if_t<
		is_constructible_v<move_only_any, in_place_type_t<T>, Args...>, int> = 0>
	[[nodiscard]] move_only_any make_move_only_any(Args&&... args)
	{
		// construct an move_only_any containing a T initialized with args...
		return move_only_any{ in_place_type<T>, std::forward<Args>(args)... };
	}

	template <class T, class E, class... Args, enable_if_t<
		is_constructible_v<move_only_any, in_place_type_t<T>, initializer_list<E>&, Args...>, int> = 0>
	[[nodiscard]] move_only_any make_move_only_any(initializer_list<E> initlist, Args&&... args)
	{
		// construct an move_only_any containing a T initialized with initlist and args...
		return move_only_any{ in_place_type<T>, initlist, std::forward<Args>(args)... };
	}

	template <class T>
	[[nodiscard]] const T* any_cast(const move_only_any* const value) noexcept
	{
		// retrieve a pointer to the T contained in value, or null
		static_assert(!is_void_v<T>, "std::move_only_any cannot contain void.");

		if constexpr (is_function_v<T> || is_array_v<T>)
		{
			return nullptr;
		}
		else
		{
			if (!value)
			{
				return nullptr;
			}

			return value->cast<remove_cvref_t<T>>();
		}
	}
	template <class T>
	[[nodiscard]] T* any_cast(move_only_any* const value) noexcept
	{
		// retrieve a pointer to the T contained in value, or null
		static_assert(!is_void_v<T>, "std::move_only_any cannot contain void.");

		if constexpr (is_function_v<T> || is_array_v<T>)
		{
			return nullptr;
		}
		else
		{
			if (!value)
			{
				return nullptr;
			}

			return value->cast<remove_cvref_t<T>>();
		}
	}

	template <class T>
	[[nodiscard]] remove_cv_t<T> any_cast(move_only_any&& value)
	{
		static_assert(is_constructible_v<remove_cv_t<T>, remove_cvref_t<T>>,
			"any_cast<T>(move_only_any&&) requires remove_cv_t<T> to be constructible from remove_cv_t<remove_reference_t<T>>");

		static_assert(!is_void_v<T>, "std::move_only_any cannot contain void.");

		if constexpr (is_function_v<T> || is_array_v<T>)
		{
			throw bad_any_cast{};
		}
		else
		{
			const auto ptr = value.cast<remove_cvref_t<T>>();
			if (!ptr)
			{
				throw bad_any_cast{};
			}

			return static_cast<remove_cv_t<T>>(std::move(*ptr));
		}
	}
}

namespace std
{
	inline void swap(experimental::move_only_any& lhs, experimental::move_only_any& rhs) noexcept
	{
		lhs.swap(rhs);
	}

	using experimental::move_only_any;
	using experimental::make_move_only_any;
	using experimental::any_cast;
}
