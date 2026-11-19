#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>

namespace {

template<class TUnit, class TRead>
std::int32_t StringHash(const TUnit* text, const std::int32_t length, TRead read) {
    std::uint32_t hash = 0;
    if (text == nullptr || length <= 0) return 0;
    const std::int32_t step = ((length - 32) / 32) + 1;
    for (std::int64_t index = 0; index < length; index += step) hash = hash * 37u + read(text[index]);
    return static_cast<std::int32_t>(hash);
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

}
