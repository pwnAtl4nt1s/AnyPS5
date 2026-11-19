#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

union HashKey {
    void* Pointer;
    std::int32_t Integer;
};

static_assert(sizeof(HashKey) == 8);

extern "C" {
std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* text, std::int32_t length);
std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* text, std::int32_t length);
std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* text, std::int32_t length);
std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(HashKey key);
std::int32_t APS5_VABI uhash_hashChars_nid_postfix(HashKey key);
std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(HashKey key);
std::int32_t APS5_VABI uhash_hashLong_nid_postfix(HashKey key);
std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(HashKey first, HashKey second);
std::int8_t APS5_VABI uhash_compareChars_nid_postfix(HashKey first, HashKey second);
std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(HashKey first, HashKey second);
std::int8_t APS5_VABI uhash_compareLong_nid_postfix(HashKey first, HashKey second);
}
