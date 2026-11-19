#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <type_traits>

namespace {

template<class TValue>
std::int32_t FormatInteger(char* buffer, TValue value, std::uint32_t radix) {
    using TUnsigned = std::make_unsigned_t<TValue>;
    auto magnitude = static_cast<TUnsigned>(value);
    const bool negative = value < 0 && radix == 10;
    char local[70] = {};
    std::int32_t length = 0;
    if (negative) {
        magnitude = TUnsigned{0} - magnitude;
        local[length++] = '-';
    }
    const std::int32_t firstDigit = length;
    do {
        local[length++] = "0123456789ABCDEF"[magnitude % radix];
        magnitude /= radix;
    } while (magnitude);
    std::reverse(local + firstDigit, local + length);
    local[length] = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(buffer, local, static_cast<std::size_t>(length + 1));
    errno = savedErrno;
    if (!copied) APS5_INVALID_ARG_EX;
    return length;
}

char* ConvertCase(char* text, char first, char last, int offset) {
    if (!text) return nullptr;
    for (char* cursor = text;; ++cursor) {
        const char value = *cursor;
        *cursor = value >= first && value <= last ? static_cast<char>(value + offset) : value;
        if (!value) return text;
    }
}

char AsciiLower(char value) {
    return value >= 0x41 && value <= 0x5a ? static_cast<char>(value + 0x20) : value;
}

int CompareIgnoringCase(const char* first, const char* second, std::uint64_t count) {
    if (first == nullptr) return second == nullptr ? 0 : -1;
    if (second == nullptr) return 1;
    for (; count != 0; --count, ++first, ++second) {
        const auto left = static_cast<unsigned char>(*first);
        const auto right = static_cast<unsigned char>(*second);
        if (left == 0) return right == 0 ? 0 : -1;
        if (right == 0) return 1;
        const int difference = static_cast<int>(static_cast<unsigned char>(AsciiLower(static_cast<char>(left)))) -
                               static_cast<int>(static_cast<unsigned char>(AsciiLower(static_cast<char>(right))));
        if (difference != 0) return difference;
    }
    return 0;
}

}

extern "C" {

std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t value, std::int32_t radix) {
    if (!buffer || radix < 2 || radix > 16) APS5_INVALID_ARG_EX;
    return FormatInteger(buffer, value, static_cast<std::uint32_t>(radix));
}

std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t value, std::uint32_t radix) {
    if (!buffer || radix < 2 || radix > 16) APS5_INVALID_ARG_EX;
    return FormatInteger(buffer, value, radix);
}

std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix) {
    if (!text) APS5_INVALID_ARG_EX;
    const auto value = static_cast<std::uint32_t>(strtoul_nid_postfix(text, nullptr, radix));
    return std::bit_cast<std::int32_t>(value);
}

char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text) {
    return ConvertCase(text, 'A', 'Z', 'a' - 'A');
}

char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text) {
    return ConvertCase(text, 'a', 'z', 'A' - 'a');
}

std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(const char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ? 1 : 0;
}

char APS5_VABI uprv_toupper_nid_postfix(const char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value + ('A' - 'a')) : value;
}

char16_t APS5_VABI u_asciiToUpper_nid_postfix(const char16_t value) {
    return value >= u'a' && value <= u'z' ? static_cast<char16_t>(value - (u'a' - u'A')) : value;
}

char APS5_VABI uprv_asciitolower_nid_postfix(const char value) {
    return AsciiLower(value);
}

char APS5_VABI uprv_ebcdictolower_nid_postfix(const char value) {
    const auto byte = static_cast<std::uint8_t>(value);
    if ((byte >= 0xc1 && byte <= 0xc9) || (byte >= 0xd1 && byte <= 0xd9) || (byte >= 0xe2 && byte <= 0xe9)) return static_cast<char>(byte - 0x40);
    return value;
}

int APS5_VABI uprv_stricmp_nid_postfix(const char* first, const char* second) {
    return CompareIgnoringCase(first, second, ~std::uint64_t{0});
}

int APS5_VABI uprv_strnicmp_nid_postfix(const char* first, const char* second, const std::uint32_t count) {
    return CompareIgnoringCase(first, second, count);
}

}
