#pragma once

#include "../base.h"
#include "array.h"
#include "concepts.h"

namespace ec
{
    template<ec::$integral T, size_t N>
    struct const_bitmap : ec::array<T, N>
    {
        constexpr auto has_bit(u64 b) const
        {
            return this->m_data[b / bits_per_member] & (1ULL << (b % bits_per_member));
        }

        constexpr void set_bit(u64 b)
        {
            this->m_data[b / bits_per_member] |= (1ULL << (b % bits_per_member));
        }

        constexpr void clear_bit(u64 b)
        {
            this->m_data[b / bits_per_member] &= ~(1ULL << (b % bits_per_member));
        }

        constexpr size_t bit_count() const
        {
            return this->size() * bits_per_member;
        }

        static constexpr auto bits_per_member = sizeof(T) * 8;
    };
}