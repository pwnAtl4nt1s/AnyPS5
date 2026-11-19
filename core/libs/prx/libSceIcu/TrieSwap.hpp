#pragma once
#include "prx/libSceIcu/DataSwapper.hpp"
#include <cstdint>

namespace Icu {
std::int32_t SwapTrieVersion1(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
}

extern "C" {
std::int32_t APS5_VABI utrie2_swap_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
std::int32_t APS5_VABI ucptrie_swap_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
std::int32_t APS5_VABI utrie_swapAnyVersion_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
}
