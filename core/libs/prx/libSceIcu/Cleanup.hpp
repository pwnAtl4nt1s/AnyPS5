#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

using LibraryCleanup = std::int8_t (APS5_VABI*)();

namespace Icu {
void RunLibraryCleanups();
}

extern "C" {
void APS5_VABI ucln_registerCleanup_nid_postfix(std::int32_t type, LibraryCleanup function);
void APS5_VABI ucln_cleanupOne_nid_postfix(std::int32_t type);
}
