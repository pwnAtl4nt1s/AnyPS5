#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <string>

namespace {

template<class TUnit, class TRead>
std::int32_t StringHash(const TUnit* text, const std::int32_t length, TRead read) {
    std::uint32_t hash = 0;
    if (text == nullptr || length <= 0) return 0;
    const std::int32_t step = ((length - 32) / 32) + 1;
    for (std::int64_t index = 0; index < length; index += step) hash = hash * 37u + read(text[index]);
    return static_cast<std::int32_t>(hash);
}

char Lower(const char unit) {
    return unit >= 'A' && unit <= 'Z' ? static_cast<char>(unit + ('a' - 'A')) : unit;
}

template<class TUnit, class TFold>
std::int8_t SameStrings(const HashKey first, const HashKey second, TFold fold) {
    const auto* left = static_cast<const TUnit*>(first.Pointer);
    const auto* right = static_cast<const TUnit*>(second.Pointer);
    if (left == right) return 1;
    if (left == nullptr || right == nullptr) return 0;
    while (*left != 0 && fold(*left) == fold(*right)) {
        ++left;
        ++right;
    }
    return *left == *right ? 1 : 0;
}

template<class TUnit>
std::int32_t Length(const TUnit* text) {
    return static_cast<std::int32_t>(std::char_traits<TUnit>::length(text));
}

}

extern "C" {

std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* text, const std::int32_t length) {
    return StringHash(text, length, [](const char16_t unit) { return static_cast<std::uint32_t>(unit); });
}

std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* text, const std::int32_t length) {
    return StringHash(text, length, [](const char unit) { return static_cast<std::uint32_t>(static_cast<std::uint8_t>(unit)); });
}

std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* text, const std::int32_t length) {
    return StringHash(text, length, [](const char unit) {
        const auto byte = static_cast<std::uint8_t>(unit);
        return static_cast<std::uint32_t>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte);
    });
}

std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(const HashKey key) {
    const auto* text = static_cast<const char16_t*>(key.Pointer);
    return text == nullptr ? 0 : ustr_hashUCharsN_nid_postfix(text, Length(text));
}

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(const HashKey key) {
    const auto* text = static_cast<const char*>(key.Pointer);
    return text == nullptr ? 0 : ustr_hashCharsN_nid_postfix(text, Length(text));
}

std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(const HashKey key) {
    const auto* text = static_cast<const char*>(key.Pointer);
    return text == nullptr ? 0 : ustr_hashICharsN_nid_postfix(text, Length(text));
}

std::int32_t APS5_VABI uhash_hashLong_nid_postfix(const HashKey key) {
    return key.Integer;
}

std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(const HashKey first, const HashKey second) {
    return SameStrings<char16_t>(first, second, [](const char16_t unit) { return unit; });
}

std::int8_t APS5_VABI uhash_compareChars_nid_postfix(const HashKey first, const HashKey second) {
    return SameStrings<char>(first, second, [](const char unit) { return unit; });
}

std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(const HashKey first, const HashKey second) {
    return SameStrings<char>(first, second, Lower);
}

std::int8_t APS5_VABI uhash_compareLong_nid_postfix(const HashKey first, const HashKey second) {
    return first.Integer == second.Integer ? 1 : 0;
}

}
