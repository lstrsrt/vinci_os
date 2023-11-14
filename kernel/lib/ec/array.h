#pragma once

#include "iterator.h"

namespace ec
{
    template<class T, size_t N>
    struct array
    {
        constexpr size_t size() const { return N; }
        constexpr const T* data() const { return m_data; }
        constexpr T* data() { return m_data; }

        constexpr const T& at(size_t index) const { return m_data[index]; }
        constexpr T& at(size_t index) { return m_data[index]; }

        constexpr const T& first() const { return at(0); }
        constexpr T& first() { return at(0); }
        constexpr const T& last() const { return at(N - 1); }
        constexpr T& last() { return at(N - 1); }

        constexpr const T& operator[](size_t index) const { return at(index); }
        constexpr T& operator[](size_t index) { return at(index); }

        using iterator = generic_iterator<T>;
        auto begin() { return iterator(m_data); }
        auto end() { return iterator(m_data + N); }

        T m_data[N];
    };

    template<class T, class... Ts>
    array(T, Ts...) -> array<T, sizeof...(Ts) + 1>;
}
