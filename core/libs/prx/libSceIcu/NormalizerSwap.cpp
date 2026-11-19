// Derived from icu4c/source/common/normalizer2impl.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/normalizer2impl.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2009-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/NormalizerSwap.hpp"
#include "prx/libSceIcu/TrieSwap.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t NormTrieOffset = 0;
constexpr std::int32_t ExtraDataOffset = 1;
constexpr std::int32_t SmallFcdOffset = 2;
constexpr std::int32_t TotalSize = 7;
constexpr std::int32_t MinimumMaybeYes = 13;
constexpr std::int32_t MinimumYesNoMappingsOnly = 14;
constexpr std::int32_t MinimumLcccCodePoint = 18;

}

extern "C" {

std::int32_t APS5_VABI unorm2_swap_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    if (status == nullptr || *status > 0) return 0;
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    const std::uint8_t formatVersion = info[12];
    if (!(format[0] == 0x4e && format[1] == 0x72 && format[2] == 0x6d && format[3] == 0x32 && formatVersion >= 1 && formatVersion <= 4)) {
        udata_printError_nid_postfix(swapper,
            "unorm2_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as Normalizer2 data\n", format[0], format[1], format[2],
            format[3], formatVersion);
        *status = Unsupported;
        return 0;
    }
    const auto* inBytes = static_cast<const std::uint8_t*>(input) + headerSize;
    std::int32_t minimumIndexes;
    if (formatVersion == 1) {
        minimumIndexes = MinimumMaybeYes + 1;
    } else if (formatVersion == 2) {
        minimumIndexes = MinimumYesNoMappingsOnly + 1;
    } else {
        minimumIndexes = MinimumLcccCodePoint + 1;
    }
    if (length >= 0) {
        length -= headerSize;
        if (length < minimumIndexes * 4) {
            udata_printError_nid_postfix(swapper, "unorm2_swap(): too few bytes (%d after header) for Normalizer2 data\n", length);
            *status = IndexOutOfBounds;
            return 0;
        }
    }
    std::int32_t indexes[TotalSize + 1];
    for (std::int32_t index = 0; index <= TotalSize; ++index) {
        std::int32_t raw;
        std::memcpy(&raw, inBytes + index * 4, 4);
        indexes[index] = udata_readInt32_nid_postfix(swapper, raw);
    }
    const std::int32_t size = indexes[TotalSize];
    if (length >= 0) {
        if (length < size) {
            udata_printError_nid_postfix(swapper, "unorm2_swap(): too few bytes (%d after header) for all of Normalizer2 data\n", length);
            *status = IndexOutOfBounds;
            return 0;
        }
        auto* outBytes = static_cast<std::uint8_t*>(output) + headerSize;
        if (inBytes != outBytes) std::memcpy(outBytes, inBytes, static_cast<std::size_t>(size));
        std::int32_t offset = 0;
        std::int32_t next = indexes[NormTrieOffset];
        swapper->SwapArray32(swapper, inBytes, next - offset, outBytes, status);
        offset = next;
        next = indexes[ExtraDataOffset];
        utrie_swapAnyVersion_nid_postfix(swapper, inBytes + offset, next - offset, outBytes + offset, status);
        offset = next;
        next = indexes[SmallFcdOffset];
        swapper->SwapArray16(swapper, inBytes + offset, next - offset, outBytes + offset, status);
    }
    return headerSize + size;
}

}
