#pragma once

#include <base.h>

#include "concepts.h"

namespace ec::inline constants
{
    template<$unsigned_int T>
    inline constexpr T umax_v = ( T )~0;
    
    template<$signed_int T>
    inline constexpr T smax_v = (umax_v<unsigned_t<T>>) >> 1;
    
    template<$signed_int T>
    inline constexpr T min_v = ~smax_v<T>;
}
