#pragma once

#include "../libc/mem.h"
#include "../libc/str.h"

#include "const.h"
#include "iterator.h"
#include "new.h"
#include "util.h"

//
//     TODO:
//        replace()
//        erase()
//        join()
//        insert()
//        prepend()
//        rfind()
//        ... probably more
//

namespace ec
{
    struct string
    {
        inline explicit string()
        {
            m_short.local_flag = true;
            m_short.buffer[0] = '\0';
        }

        inline string(const char* cstr, size_t length)
        {
            init(length);
            strlcpy(data(), cstr, length + 1);
        }

        inline string(const char* cstr)
            : string(cstr, cstr ? strlen(cstr) : 0)
        {
        }

        inline string(char c, size_t length)
        {
            // size does not include null terminator, which always gets allocated
            init(length);
            strnset(data(), c, length);
        }

        inline string(string&& str) noexcept
        {
            memcpy(( void* )this, ( const void* )&str, sizeof str);

            if (!str.uses_local())
            {
                // We own the pointer now, set the other one to null to only run one destructor
                exchange(str.m_long.buffer, nullptr);
                str.m_long.capacity = str.m_long.length = 0;
            }
        }

        // Strings with extra reserved capacity do not carry it over to new strings!
        inline string(const string& str)
        {
            init(str.length());
            strlcpy(data(), str.chars(), length() + 1);
        }

        ~string()
        {
            if (!uses_local() && m_long.buffer)
                delete[] m_long.buffer;
        }

        inline const char* chars() const { return uses_local() ? m_short.buffer : m_long.buffer; }
        inline char* data() { return uses_local() ? m_short.buffer : m_long.buffer; }
        inline u64 length() const { return uses_local() ? m_short.length : m_long.length; }
        inline u64 capacity() const { return uses_local() ? max_local_cap : m_long.capacity; }

        inline char at(size_t pos) const { return chars()[pos]; }
        inline char& at(size_t pos) { return data()[pos]; }

        inline char back() const { return at(0); }
        inline char& back() { return at(0); }
        inline char front() const { return at(length() - 1); }
        inline char& front() { return at(length() - 1); }

        using iterator = generic_iterator<char>;
        inline auto begin() const { return iterator(const_cast<char*>(chars())); }
        inline auto end() const { return iterator(const_cast<char*>(chars()) + length()); }

        inline char operator[](size_t pos) const { return at(pos); }
        inline char& operator[](size_t pos) { return at(pos); }

        // operator+
        // declare outside class so we can switch operand order?

        string& operator=(const char* cstr);
        inline string& operator=(const string& str)
        {
            *this = string(str);
            return *this;
        }
        inline string& operator=(string&& str) noexcept
        {
            if (this != &str)
            {
                memcpy((void*)this, (const void*)&str, sizeof(str));
                if (!str.uses_local())
                    exchange(str.m_long.buffer, nullptr);
            }
            return *this;
        }

        inline string& operator+=(const char* cstr) { return append(cstr); }
        inline string& operator+=(const string& str) { return append(str.chars(), str.length()); }

        inline bool operator==(const char* cstr) const { return equals(cstr); }
        inline bool operator==(const string& str) const { return equals(str); }

        string substr(size_t from, size_t count) const;

        inline i32 compare(const char* cstr, bool ignore_case = false) const
        {
            return ignore_case ? stricmp(chars(), cstr) : strcmp(chars(), cstr);
        }

        inline bool equals(const char* cstr, bool ignore_case = false) const
        {
            return compare(cstr, ignore_case) == 0;
        }

        inline bool equals(const string& str, bool ignore_case = false) const
        {
            if (length() != str.length()) // Try to avoid a strcmp
                return false;

            return ignore_case ?
                (strnicmp(chars(), str.chars(), length()) == 0) :
                (strncmp(chars(), str.chars(), length()) == 0);
        }

        size_t count(char c, size_t from = 0, bool ignore_case = false) const;
        size_t count(const char* cstr, size_t length, size_t from = 0, bool ignore_case = false) const;
        inline size_t count(const char* cstr, size_t from = 0, bool ignore_case = false) const
        {
            return count(cstr, strlen(cstr), from, ignore_case);
        }
        inline size_t count(const string& str, size_t from = 0, bool ignore_case = false) const
        {
            return count(str.chars(), str.length(), from, ignore_case);
        }

        size_t find(char c, size_t from = 0, bool ignore_case = false) const;
        size_t find(const char* cstr, size_t length, size_t from = 0, bool ignore_case = false) const;
        inline size_t find(const char* cstr, size_t from = 0, bool ignore_case = false) const
        {
            return find(cstr, strlen(cstr), from, ignore_case);
        }
        inline size_t find(const string& str, size_t from = 0, bool ignore_case = false) const
        {
            return find(str.chars(), str.length(), from, ignore_case);
        }

        inline bool contains(const char* cstr, size_t from = 0, bool ignore_case = false) const
        {
            return find(cstr, from, ignore_case) != npos;
        }
        inline bool contains(const string& str, size_t from = 0, bool ignore_case = false) const
        {
            return find(str.chars(), from, ignore_case) != npos;
        }

        inline bool starts_with(const char c, bool ignore_case = false) const
        {
            if (ignore_case)
                return tolower(at(0)) == tolower(c);

            return at(0) == c;
        }
        inline bool starts_with(const char* cstr, size_t length, bool ignore_case = false) const
        {
            if (length > this->length())
                return false;

            if (ignore_case)
                return !strnicmp(chars(), cstr, length);

            return !strncmp(chars(), cstr, length);
        }
        inline bool starts_with(const char* cstr, bool ignore_case = false) const
        {
            return starts_with(cstr, strlen(cstr), ignore_case);
        }
        inline bool starts_with(const string& str, bool ignore_case = false) const
        {
            return starts_with(str.chars(), str.length(), ignore_case);
        }

        inline bool ends_with(const char c, bool ignore_case = false) const
        {
            if (ignore_case)
                return tolower(c) == tolower(front());

            return c == front();
        }
        inline bool ends_with(const char* cstr, size_t length, bool ignore_case = false) const
        {
            const auto len = this->length();

            if (length > len)
                return false;

            const auto start = len - length;
            if (ignore_case)
                return !strnicmp(chars() + start, cstr, length);

            return !strncmp(chars() + start, cstr, length);
        }
        inline bool ends_with(const char* cstr, bool ignore_case = false) const { return ends_with(cstr, strlen(cstr), ignore_case); }
        inline bool ends_with(const string& str, bool ignore_case = false) { return ends_with(str.chars(), str.length(), ignore_case); }

        inline void clear()
        {
            strnset(data(), '\0', length());
            if (uses_local())
                m_short.length = 0;
            else
                m_long.length = 0;
        }

        inline string upper() const
        {
            string ret{ *this };

            for (size_t i = 0; i < length(); i++)
                ret[i] = ( char )toupper(at(i));

            return ret;
        }

        inline string lower() const
        {
            string ret{ *this };

            for (size_t i = 0; i < length(); i++)
                ret[i] = ( char )tolower(ret[i]);

            return ret;
        }

        inline string reversed() const
        {
            string ret{ *this };

            strrev(ret.data());

            return ret;
        }

        inline string& to_upper()
        {
            for (size_t i = 0; i < length(); i++)
                at(i) = ( char )toupper(at(i));

            return *this;
        }

        inline string& to_lower()
        {
            for (size_t i = 0; i < length(); i++)
                at(i) = ( char )tolower(at(i));

            return *this;
        }

        inline string& reverse()
        {
            strrev(data());

            return *this;
        }

        string& append(const char* cstr, size_t length);
        inline string& append(const char* cstr) { return append(cstr, strlen(cstr)); }
        inline string& append(const string& str) { return append(str.chars(), str.length()); }

        string& push_back(char c);

        inline string& pop_back()
        {
            if (uses_local())
                m_short.buffer[--m_short.length] = '\0';
            else
                m_long.buffer[--m_long.length] = '\0';

            return *this;
        }

        // TODO - resize

        string& reserve(size_t new_cap);

        static constexpr size_t npos = umax_v<size_t>;

    private:
        bool uses_local() const { return m_short.local_flag; }

        inline void init(size_t length)
        {
            m_short.local_flag = length < max_local_cap;

            if (m_short.local_flag)
            {
                m_short.length = ( u8 )length;
                return;
            }

            m_long.buffer = new char[length + 1];
            m_long.buffer[length] = '\0';
            m_long.length = length;
            m_long.capacity = length + 1;
        }

        static constexpr size_t max_local_cap = 23;

        union
        {
            struct stack_t
            {
                char buffer[max_local_cap];
                u8 length : 7;
                u8 local_flag : 1;
            } m_short{};

            struct heap_t
            {
                char* buffer;
                u64 length;
                u64 capacity : 63;
                u64 local_flag : 1;
            } m_long;
        };
    };
}

namespace ec::literals
{
    inline string operator""_str(const char* cstr, size_t length)
    {
        return string(cstr, length);
    }
}
