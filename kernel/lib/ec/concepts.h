#pragma once

#include <base.h>

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

        template<class> struct make_unsigned { using type = void; };
        template<> struct make_unsigned<i8> { using type = u8; };
        template<> struct make_unsigned<i16> { using type = u16; };
        template<> struct make_unsigned<i32> { using type = u32; };
        template<> struct make_unsigned<i64> { using type = u64; };

        template<class, class> constexpr bool is_same = false;
        template<class T> constexpr bool is_same<T, T> = true;

        template<class T, class... Ts> constexpr bool is_any_of = (is_same<T, Ts> || ...);

        template<class T> constexpr bool is_unsigned = is_same<T, make_unsigned<T>>;

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

    template <class T> using remove_cv = typename impl::remove_cv<T>::type;

    template <class T> using remove_reference = typename impl::remove_reference<T>::type;

    template <class T> using unqualified [[msvc::known_semantics]] = remove_cv<remove_reference<T>>;

    template<class T>
    auto declval() -> T;

    template<class T, class U>
    concept is_same = impl::is_same<T, U>;

    template<class T, class... Ts>
    concept is_any_of = impl::is_any_of<T, Ts...>;

    template<class T>
    concept is_integral = is_any_of<remove_cv<T>,
        bool, unsigned char, char8_t, char16_t, char32_t, wchar_t,
        u8, u16, u32, u64, i8, i16, i32, i64>;

    template<class T>
    concept is_float = is_any_of<remove_cv<T>, f32, f64>;

    template<class T>
    concept is_char = is_any_of<remove_cv<T>, char, wchar_t, char8_t, char16_t, char32_t>;

    template<class T>
    concept is_number = is_integral<T> || is_float<T>;

    template<class T>
    concept is_unsigned = impl::is_unsigned<T>;

    template<class T, class... Args>
    concept is_callable = requires(T t) { T(declval<Args>()...); };

    template<class T>
    concept is_enum = __is_enum(T);

    template<class T>
    concept is_class = __is_class(T);

    template<class T>
    concept is_union = __is_union(T);

    template<class T>
    concept is_pointer = impl::is_pointer<T>;

    template<class T>
    concept is_array = impl::is_array<T>;

    template<class T, class U>
    concept convertible = impl::is_convertible<T, U> && requires { static_cast<U>(declval<T>()); };;

    template<is_enum T>
    using underlying_t = __underlying_type(T);
}
