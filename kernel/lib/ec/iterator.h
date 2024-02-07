#pragma once

#include "../base.h"

namespace ec
{
    template<class T>
    struct generic_iterator
    {
        generic_iterator(T* ptr)
            : m_ptr(ptr)
        {
        }

        bool operator==(const generic_iterator& rhs) const { return m_ptr == rhs.m_ptr; }
        generic_iterator operator++() { ++m_ptr; return *this; }
        generic_iterator operator++(int) { ++m_ptr; return m_ptr - 1; }
        T operator*() const { return *m_ptr; }
        T& operator*() { return *m_ptr; }

    private:
        T* m_ptr{};
    };
}
