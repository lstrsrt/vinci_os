#pragma once

#include <base.h>

#include "concepts.h"

namespace ec
{
    template<class T>
    [[msvc::intrinsic]] constexpr auto&& move(T&& t)
    {
        return static_cast<remove_reference<T>&&>(t);
    }

    template<class T>
    [[msvc::intrinsic]] constexpr auto&& forward(remove_reference<T>& t)
    {
        return static_cast<T&&>(t);
    }

    template<class T>
    [[msvc::intrinsic]] constexpr auto&& forward(remove_reference<T>&& t)
    {
        return static_cast<T&&>(t);
    }

    template<class T, class U = T>
    constexpr T exchange(T& t, U&& new_value)
    {
        T old = move(t);
        t = forward<U>(new_value);
        return old;
    }

    template<class T>
    constexpr void swap(T& a, T& b)
    {
        T tmp = a;
        a = b;
        b = tmp;
    }

    constexpr bool is_consteval()
    {
        return __builtin_is_constant_evaluated();
    }

    template<$array A>
    constexpr size_t array_size(const A& a)
    {
        return sizeof a / sizeof a[0];
    }
}
