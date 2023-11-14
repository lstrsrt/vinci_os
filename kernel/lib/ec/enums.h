#pragma once

#include <base.h>

#include "concepts.h"
#include "util.h"

namespace ec
{
    template<is_enum E>
    constexpr auto to_underlying(E e)
    {
        return static_cast<underlying_t<E>>(e);
    }
}

#define ENUM_OPERATOR(type, op) \
    template<class U = type> \
    constexpr type operator op(const type lhs, const U rhs) noexcept \
    { \
        return static_cast<type>(gstl::to_underlying(lhs) op gstl::to_underlying(rhs)); \
    } \
    template<class U = type> \
    inline type& operator op##=(type& lhs, const U rhs) noexcept \
    { \
        return lhs = static_cast<type>(gstl::to_underlying(lhs) op gstl::to_underlying(rhs)); \
    }

#define GSTL_ENUM_BIT_OPS(type) \
    ENUM_OPERATOR(type, &) \
    ENUM_OPERATOR(type, |) \
    ENUM_OPERATOR(type, ^) \
    ENUM_OPERATOR(type, >>) \
    ENUM_OPERATOR(type, <<) \
    constexpr type operator~(const type x) noexcept \
    { \
        return static_cast<type>(~gstl::to_underlying(x)); \
    }

#define GSTL_ENUM_MATH_OPS(type) \
    ENUM_OPERATOR(type, +) \
    ENUM_OPERATOR(type, -) \
    ENUM_OPERATOR(type, *) \
    ENUM_OPERATOR(type, /)

#define GSTL_ENUM_OPS(type) \
    GSTL_ENUM_BIT_OPS(type) \
    GSTL_ENUM_MATH_OPS(type)

