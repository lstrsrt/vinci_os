#pragma once

#include "../base.h"
#include "array.h"
#include "concepts.h"

namespace ec
{
    template<ec::$integral T, size_t N>
    struct const_bitmap : ec::array<T, N>
    {
        constexpr auto has_bit(size_t b) const
        {
            return this->m_data[b / sizeof(T)] & (1 << (b % sizeof(T)));
        }

        constexpr void set_bit(size_t b)
        {
            this->m_data[b / sizeof(T)] |= (1 << (b % sizeof(T)));
        }

        constexpr void clear_bit(size_t b)
        {
            this->m_data[b / sizeof(T)] &= ~(1 << (b % sizeof(T)));
        }

        constexpr u64 bit_count() const
        {
            return this->size() * sizeof(T);
        }
    };
}