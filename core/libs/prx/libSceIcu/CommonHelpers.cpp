// Derived from icu4c/source/common/ustrfmt.cpp, icu4c/source/common/propname.cpp and icu4c/source/common/putil.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ustrfmt.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/propname.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/putil.cpp
//
// The upstream ustrfmt.cpp notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2001-2006, International Business Machines
//   Corporation and others.  All Rights Reserved.
//
// The upstream propname.cpp notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (c) 2002-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.
//
// The upstream putil.cpp notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/CommonHelpers.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

namespace {

char AsciiLower(const char c) {
    return c >= 0x41 && c <= 0x5a ? static_cast<char>(c + 0x20) : c;
}

char EbcdicLower(const char c) {
    const auto unit = static_cast<std::uint8_t>(c);
    if ((unit >= 0xc1 && unit <= 0xc9) || (unit >= 0xd1 && unit <= 0xd9) || (unit >= 0xe2 && unit <= 0xe9)) return static_cast<char>(c - 0x40);
    return c;
}

bool AsciiIgnorable(const char c) {
    return c == 0x2d || c == 0x5f || c == 0x20 || (c >= 0x09 && c <= 0x0d);
}

bool EbcdicIgnorable(const char c) {
    return c == 0x60 || c == 0x6d || c == 0x40 || c == 0x05 || c == 0x15 || c == 0x25 || c == 0x0b || c == 0x0c || c == 0x0d;
}

template<bool (*Ignorable)(char), char (*Lower)(char)>
std::int32_t NextNameChar(const char* name) {
    std::int32_t index = 0;
    char c;
    do {
        c = name[index++];
    } while (Ignorable(c));
    if (c == 0) return index << 8;
    return (index << 8) | static_cast<std::uint8_t>(Lower(c));
}

template<bool (*Ignorable)(char), char (*Lower)(char)>
std::int32_t CompareNames(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) APS5_INVALID_ARG_EX;
    for (;;) {
        const std::int32_t first = NextNameChar<Ignorable, Lower>(left);
        const std::int32_t second = NextNameChar<Ignorable, Lower>(right);
        if (((first | second) & 0xff) == 0) return 0;
        if (first != second) {
            const std::int32_t difference = (first & 0xff) - (second & 0xff);
            if (difference != 0) return difference;
        }
        left += first >> 8;
        right += second >> 8;
    }
}

}

extern "C" {

std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, const std::int32_t capacity, std::uint32_t value, const std::uint32_t radix, const std::int32_t minimumWidth) {
    if (buffer == nullptr || radix == 0) APS5_INVALID_ARG_EX;
    std::int32_t length = 0;
    do {
        const auto digit = static_cast<std::int32_t>(value % radix);
        buffer[length++] = static_cast<char16_t>(digit <= 9 ? 0x30 + digit : 0x30 + digit + 7);
        value /= radix;
    } while (value != 0 && length < capacity);
    while (length < minimumWidth) buffer[length++] = u'0';
    if (length < capacity) buffer[length] = 0;
    for (std::int32_t index = 0; index < length / 2; ++index) {
        const char16_t swap = buffer[length - 1 - index];
        buffer[length - 1 - index] = buffer[index];
        buffer[index] = swap;
    }
    return length;
}

std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* left, const char* right) {
    return CompareNames<AsciiIgnorable, AsciiLower>(left, right);
}

std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char* left, const char* right) {
    return CompareNames<EbcdicIgnorable, EbcdicLower>(left, right);
}

std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path) {
    return path != nullptr && path[0] == '/' ? 1 : 0;
}

}
