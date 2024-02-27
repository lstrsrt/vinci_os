#pragma once

#include "../base.h"

namespace ec::inline concepts
{
    namespace impl
    {
        template<class T> struct remove_const { using type = T; };

        template<class T> struct remove_volatile { using type = T; };

        template<class T> struct remove_reference { using type = T; };
        template<class T> struct remove_reference<T&> { using type = T; };
        template<class T> struct remove_reference<T&&> { using type = T; };

        template<class T> struct remove_cv { using type = T; };
        template<class T> struct remove_cv<const T> { using type = T; };
        template<class T> struct remove_cv<volatile T> { using type = T; };
        template<class T> struct remove_cv<const volatile T> { using type = T; };

        template<class> struct as_unsigned { using type = void; };
        template<> struct as_unsigned<i8> { using type = u8; };
        template<> struct as_unsigned<i16> { using type = u16; };
        template<> struct as_unsigned<i32> { using type = u32; };
        template<> struct as_unsigned<i64> { using type = u64; };

        template<class, class> constexpr bool is_same = false;
        template<class T> constexpr bool is_same<T, T> = true;

        template<class T, class... Ts> constexpr bool is_any_of = (is_same<T, Ts> || ...);

        template<class> constexpr bool is_pointer = false;
        template<class T> constexpr bool is_pointer<T*> = true;
        template<class T> constexpr bool is_pointer<T* const> = true;
        template<class T> constexpr bool is_pointer<T* volatile> = true;
        template<class T> constexpr bool is_pointer<T* const volatile> = true;

        template<class> constexpr bool is_array = false;
        template<class T, size_t N> constexpr bool is_array<T[N]> = true;
        template<class T> constexpr bool is_array<T[]> = true;

        template<class T, class U> constexpr bool is_convertible = __is_convertible_to(T, U);
    }

    template<class T> using remove_const = typename impl::remove_const<T>::type;

    template<class T> using remove_volatile = typename impl::remove_volatile<T>::type;

    template<class T> using remove_cv = typename impl::remove_cv<T>::type;

    template<class T> using remove_reference = typename impl::remove_reference<T>::type;

    template<class T> using unqualified = remove_cv<remove_reference<T>>;

    template<class T> using unsigned_t = impl::as_unsigned<T>::type;

    template<class T>
    auto declval() -> T;

    template<class T, class U>
    concept $same = impl::is_same<T, U>;

    template<class T, class... Ts>
    concept $any_of = impl::is_any_of<T, Ts...>;

    template<class T>
    concept $integral = $any_of<remove_cv<T>,
        bool, unsigned char, char8_t, char16_t, char32_t, wchar_t,
        u8, u16, u32, u64, i8, i16, i32, i64>;

    template<class T>
    concept $float = $any_of<remove_cv<T>, f32, f64>;

    template<class T>
    concept $char = $any_of<remove_cv<T>, char, wchar_t, char8_t, char16_t, char32_t>;

    template<class T>
    concept $number = $integral<T> || $float<T>;

    template<class T>
    concept $signed_int = $integral<T> && static_cast<remove_cv<T>>(-1) < static_cast<remove_cv<T>>(0);

    template<class T>
    concept $unsigned_int = $integral<T> && !$signed_int<T>;

    template<class T, class... Args>
    concept $callable = requires(T t) { T(declval<Args>()...); };

    template<class T>
    concept $enum = __is_enum(T);

    template<class T>
    concept $class = __is_class(T);

    template<class T>
    concept $union = __is_union(T);

    template<class T>
    concept $pointer = impl::is_pointer<T>;

    template<class T>
    concept $array = impl::is_array<T>;

    template<class T, class U>
    concept $convertible = impl::is_convertible<T, U> && requires { static_cast<U>(declval<T>()); };

    template<$enum T> using underlying_t = __underlying_type(T);

    template<class F, class... Args>
    concept $predicate = requires(F function, Args... args)
    {
        { function(args...) } -> $convertible<bool>;
    };
}
