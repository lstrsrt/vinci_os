#pragma once

#include "../base.h"
#include "concepts.h"
#include "enums.h"

namespace ec
{
    template<$enum E>
    struct bitfield
    {
        using U = underlying_t<E>;

        constexpr bitfield() = default;
        constexpr bitfield(E bits)
            : bits(bits) { }
        constexpr bitfield(U bits)
            : bits(static_cast<E>(bits)) { }

        constexpr operator E() { return value(); }
        constexpr operator U() { return raw(); }
        constexpr operator bool() { return bits.raw; }
        constexpr bool operator&(E bit) const { return is_set(bit); }
        constexpr auto operator~() const { return ~bits.value; }
        constexpr auto operator&=(E rhs) { return bits.value &= rhs; }
        constexpr auto operator|=(E rhs) { return bits.value |= rhs; }
        constexpr auto operator^=(E rhs) { return bits.value ^= rhs; }

        auto value() const { return bits.value; }
        auto raw() const { return bits.raw; }

        template<class... Ts> requires($same<Ts, E> && ...)
        constexpr bool is_set(Ts... t) const
        {
            return bits.raw & (to_underlying(t) | ...);
        }

        constexpr bool is_empty() const
        {
            return !bits.raw;
        }

        template<class... Ts> requires($same<Ts, E> && ...)
        constexpr void set(Ts... t)
        {
            bits.raw |= (to_underlying(t) | ...);
        }

        template<class... Ts> requires($same<Ts, E> && ...)
        constexpr void unset(Ts... t)
        {
            bits.raw &= ~(to_underlying(t) | ...);
        }

        template<class... Ts> requires($same<Ts, E> && ...)
        constexpr void toggle(Ts... t)
        {
            bits.raw ^= (to_underlying(t) | ...);
        }

        constexpr void empty()
        {
            bits = E();
        }

    private:
        union
        {
            E value;
            U raw;
        } bits{};
    };
}
