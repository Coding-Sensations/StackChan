#pragma once
#include <cstddef>
#include <climits>
namespace companion {
// A frame or request is sent only when the transport accepted every byte.
inline int CompleteTransportWrite(int actual,std::size_t expected) noexcept {
    if(actual<0)return actual;
    if(!expected || expected>INT_MAX || actual==0 || static_cast<std::size_t>(actual)!=expected)return -1;
    return actual;
}
}
