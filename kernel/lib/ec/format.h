#pragma once

#include "../libc/print.h"
#include "concepts.h"
#include "string.h"

namespace ec
{
    template<$number T>
    string num_to_string(T i, i32 radix = 10, bool sign = true)
    {
        if constexpr ($integral<T>)
        {
            if constexpr (sizeof(T) <= sizeof(i32))
            {
                char buf[32];
                i32toa(i, buf, radix, sign);
                return string(buf);
            }
            else
            {
                char buf[64];
                i64toa(i, buf, radix, sign);
                return string(buf);
            }
        }
        else if ($float<T>)
        {
            return "(float)"; // placeholder
        }
    }

    template<size_t N>
    auto type_format(const char(&s)[N])
    {
        return string(s, N);
    }

    auto type_format($signed_int auto i)
    {
        return num_to_string(i);
    }

    auto type_format($unsigned_int auto i)
    {
        return num_to_string(i, 10, false);
    }

    template<class T>
    concept $formattable = $char<T> || requires(T x)
    {
        { type_format(x) } -> $convertible<string>;
    };

    template<$formattable First, $formattable... Rest>
    string format_impl(string& out, First&& first, Rest&&... rest)
    {
        if constexpr ($same<unqualified<First>, char>)
            out.push_back(first);
        else
            out.append(type_format(first));

        if constexpr (sizeof...(rest) > 0)
            return format_impl(out, forward<Rest>(rest)...);
        return out;
    }

    namespace fmt
    {

    }

    template<$formattable... Args>
    string format(Args&&... args)
    {
        string s{};
        return format_impl(s, forward<Args>(args)...);
    }
}
