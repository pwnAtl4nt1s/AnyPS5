// Derived from icu4c/source/common/ucln_cmn.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucln_cmn.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2001-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/Cleanup.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <atomic>
#include <cstdint>

namespace {

constexpr std::int32_t LibraryCount = 8;

std::array<std::atomic<LibraryCleanup>, LibraryCount> cleanups{};

}

namespace Icu {

void RunLibraryCleanups() {
    for (std::int32_t type = 0; type < LibraryCount; ++type) ucln_cleanupOne_nid_postfix(type);
}

}

extern "C" {

void APS5_VABI ucln_registerCleanup_nid_postfix(const std::int32_t type, const LibraryCleanup function) {
    if (type >= 0 && type < LibraryCount) cleanups[static_cast<std::size_t>(type)].store(function);
}

void APS5_VABI ucln_cleanupOne_nid_postfix(const std::int32_t type) {
    if (type < 0 || type >= LibraryCount) APS5_INVALID_ARG_EX;
    if (const LibraryCleanup function = cleanups[static_cast<std::size_t>(type)].exchange(nullptr)) function();
}

}
