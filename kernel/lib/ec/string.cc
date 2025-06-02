#include "string.h"

namespace ec
{
    string& string::operator=(const char* cstr)
    {
        if (!cstr)
        {
            clear();
            return *this;
        }

        init(strlen(cstr));
        strlcpy(data(), cstr, length() + 1);
        return *this;
    }

    string string::substr(size_t from, size_t count) const
    {
        const auto len = length();

        if (count == npos || from + count > len)
            count = len - from;

        if (from >= len)
            return string();

        if (from == 0 && count >= len)
            return string(*this);

        return string(&(chars()[from]), count);
    }

    size_t string::count(char c, size_t from, bool ignore_case) const
    {
        size_t n = 0;
        auto pos = find(c, from, ignore_case);

        while (pos != npos)
        {
            n++;
            from = pos + 1;
            pos = find(c, from, ignore_case);
        }

        return n;
    }

    size_t string::count(const char* cstr, size_t length, size_t from, bool ignore_case) const
    {
        size_t n = 0;
        auto pos = find(cstr, length, from, ignore_case);

        while (pos != npos)
        {
            n++;
            from = pos + length;
            pos = find(cstr, length, from, ignore_case);
        }

        return n;
    }

    size_t string::find(char c, size_t from, bool ignore_case) const
    {
        const auto len = length();

        if (!len || from >= len)
            return npos;

        if (ignore_case)
        {
            const auto tmp = lower();
            const auto ptr = strchr(tmp.chars() + from, ( char )tolower(c));
            return ptr ? ptr - tmp.chars() : npos;
        }
        else
        {
            const auto ptr = strchr(chars() + from, c);
            return ptr ? ptr - chars() : npos;
        }
    }

    size_t string::find(const char* cstr, size_t length, size_t from, bool ignore_case) const
    {
        const auto len = this->length();

        if (!len || from >= len || length > len)
            return npos;

        if (ignore_case)
        {
            const auto ptr = stristr(chars() + from, cstr);
            return ptr ? ptr - chars() : npos;
        }
        else
        {
            const auto ptr = strstr(chars() + from, cstr);
            return ptr ? ptr - chars() : npos;
        }
    }

    string& string::append(const char* cstr, size_t length)
    {
        const auto new_cap = this->length() + length + 1;

        if (uses_local())
        {
            if (new_cap < max_local_cap)
            {
                // Stay in the current local buffer
                m_short.length += ( u8 )strlcpy(m_short.buffer + m_short.length, cstr, length + 1);
            }
            else
            {
                // Switch from local to heap
                // Double capacity to save on future allocations
                reserve(new_cap * 2);
                m_long.length += strlcpy(m_long.buffer + m_long.length, cstr, length + 1);
            }
        }
        else
        {
            if (new_cap < m_long.capacity)
            {
                // Stay in the current heap buffer
                m_long.length += strlcpy(m_long.buffer + m_long.length, cstr, length + 1);
            }
            else
            {
                // Expand heap buffer
                reserve(new_cap * 2);
                m_long.length += strlcpy(m_long.buffer + m_long.length, cstr, length + 1);
            }
        }

        return *this;
    }

    string& string::push_back(char c)
    {
        const auto new_cap = length() + sizeof(c);

        if (uses_local())
        {
            if (new_cap < max_local_cap)
            {
                // Stay in the current local buffer
                m_short.buffer[m_short.length++] = c;
            }
            else
            {
                // Switch from local to heap
                // Double capacity to save on future allocations
                reserve(new_cap * 2);
                m_long.buffer[m_long.length++] = c;
            }
        }
        else
        {
            if (new_cap < m_long.capacity)
            {
                // Stay in the current heap buffer
                m_long.buffer[m_long.length++] = c;
            }
            else
            {
                // Expand heap buffer
                reserve(new_cap * 2);
                m_long.buffer[m_long.length++] = c;
            }
        }

        return *this;
    }

    string& string::reserve(size_t new_cap)
    {
        // No need to do anything in this case, keep the current buffer
        if (new_cap <= capacity())
            return *this;

        // Switch to the heap (or expand)
        auto new_buffer = new char[new_cap];
        auto cur_length = length();

        strlcpy(new_buffer, data(), cur_length + 1);
        strnset(new_buffer + cur_length, '\0', new_cap - cur_length);

        if (!uses_local() && m_long.buffer)
            delete[] m_long.buffer;

        m_long.buffer = new_buffer;
        // If we were using the local buffer before,
        // this field overlaps it and has to be set again!
        m_long.length = cur_length;
        m_long.capacity = ( u32 )new_cap;
        m_long.local_flag = false;

        return *this;
    }
}
