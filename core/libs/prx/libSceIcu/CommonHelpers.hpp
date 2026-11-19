#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {
std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value, std::uint32_t radix, std::int32_t minimumWidth);
std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* left, const char* right);
std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char* left, const char* right);
std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path);
}
