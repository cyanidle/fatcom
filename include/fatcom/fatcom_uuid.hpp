#pragma once

#include <cstdint>

namespace fatcom
{


struct UUID {
    uint64_t lo;
    uint64_t hi;
};

constexpr bool operator==(UUID l, UUID r) noexcept {
    return l.hi == r.hi && l.lo == r.lo;
}

constexpr uint64_t hex(char a) {
    uint64_t res =
        a < '0' ? -1 :
            a <= '9' ? a - '0' :
            a < 'A' ? -1 :
            a <= 'F' ? a - 'A' + 10 :
            a < 'a' ? -1 :
            a <= 'f' ? a - 'a' + 10 :
            -1;
    if (res == -1) throw "Cannot parse UUID: Invalid hex digit";
    return res;
}

constexpr void dash(char ch) {
    if (ch != '-') throw "Cannot parse UUID: missing '-'";
}

constexpr uint64_t hex_int(const char*& iid) {
    uint64_t res = hex(*iid++);
    return res | hex(*iid++) << 4;
}

constexpr UUID ParseIID(const char* iid) {
    UUID res{};
    res.lo |= hex_int(iid) << 56;
    res.lo |= hex_int(iid) << 48;
    res.lo |= hex_int(iid) << 40;
    res.lo |= hex_int(iid) << 32;
    dash(*iid++);
    res.lo |= hex_int(iid) << 24;
    res.lo |= hex_int(iid) << 16;
    dash(*iid++);
    res.lo |= hex_int(iid) << 8;
    res.lo |= hex_int(iid);
    dash(*iid++);
    res.hi |= hex_int(iid);
    res.hi |= hex_int(iid) << 8;
    dash(*iid++);
    res.hi |= hex_int(iid) << 16;
    res.hi |= hex_int(iid) << 24;
    res.hi |= hex_int(iid) << 32;
    res.hi |= hex_int(iid) << 40;
    res.hi |= hex_int(iid) << 48;
    res.hi |= hex_int(iid) << 56;
    return res;
}

}
