#pragma once
#include "prx/libSceIcu/DataSwapper.hpp"
#include <cstdint>

extern "C" {
std::int8_t APS5_VABI ucol_looksLikeCollationBinary_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length);
std::int32_t APS5_VABI ucol_swap_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
std::int32_t APS5_VABI ucol_swapInverseUCA_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
}
