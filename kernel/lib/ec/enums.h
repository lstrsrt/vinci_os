#pragma once

#include "../base.h"
#include "concepts.h"
#include "util.h"

namespace ec
{
    template<$enum E>
    MSVC_INTRINSIC constexpr auto to_underlying(E e)
    {
        return static_cast<underlying_t<E>>(e);
    }
}

#define EC_ENUM_OPERATOR(type, op) \
    constexpr type operator op(const type lhs, const type rhs) noexcept \
    { \
        return static_cast<type>(ec::to_underlying(lhs) op ec::to_underlying(rhs)); \
    } \
    constexpr type& operator op##=(type& lhs, const type rhs) noexcept \
    { \
        return lhs = static_cast<type>(ec::to_underlying(lhs) op ec::to_underlying(rhs)); \
    }

#define EC_ENUM_BIT_OPS(type) \
    EC_ENUM_OPERATOR(type, &) \
    EC_ENUM_OPERATOR(type, |) \
    EC_ENUM_OPERATOR(type, ^) \
    EC_ENUM_OPERATOR(type, >>) \
    EC_ENUM_OPERATOR(type, <<) \
    constexpr type operator~(const type x) noexcept \
    { \
        return static_cast<type>(~ec::to_underlying(x)); \
    } \
    constexpr auto operator!(const type x) noexcept \
    { \
        return !ec::to_underlying(x); \
    }

#define enum_flags(name, underlying) \
  enum class name : underlying; \
  EC_ENUM_BIT_OPS(name) \
  enum class name : underlying
