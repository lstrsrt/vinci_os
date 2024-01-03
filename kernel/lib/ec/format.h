#pragma once

#include "concepts.h"
#include "string.h"

namespace ec
{
    template<size_t N>
    auto type_format(const char(&s)[N]) -> string
    {
        return string(s, N);
    }

    template<class T>
    concept formattable = is_char<T> || requires(T x)
    {
        { type_format(x) } -> convertible<string>;
    };

    template<formattable First, formattable... Args>
    string format_internal(string& out, First&& first, Args&&... args)
    {
        if constexpr (is_same<unqualified<First>, char>)
            out.push_back(first);
        else
            out.append(type_format(first));
        if constexpr (sizeof...(args) > 0)
            return format_internal(out, forward<Args>(args)...);
        return out;
    }

    template<formattable... Args>
    string format(Args&&... args)
    {
        string s{};
        return format_internal(s, forward<Args>(args)...);
    }
}
