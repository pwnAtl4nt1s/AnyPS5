#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* text, std::int32_t length);
std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* text, std::int32_t length);
std::int32_t APS5_VABI ustr_hashICharsN_nid_postfix(const char* text, std::int32_t length);
}
