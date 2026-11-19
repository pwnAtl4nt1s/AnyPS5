#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

using ArrayComparator = std::int32_t (APS5_VABI*)(const void*, const void*, const void*);

extern "C" {
std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char* array, std::int32_t limit, void* item, std::int32_t itemSize, ArrayComparator compare, const void* context);
void APS5_VABI uprv_sortArray_nid_postfix(void* array, std::int32_t length, std::int32_t itemSize, ArrayComparator compare, const void* context, std::int8_t sortStable,
    std::int32_t* status);
}
