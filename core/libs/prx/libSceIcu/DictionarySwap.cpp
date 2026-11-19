// Derived from icu4c/source/common/dictionarydata.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/dictionarydata.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2014-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/DictionarySwap.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t IndexCount = 8;
constexpr std::int32_t Reserved1Offset = 1;
constexpr std::int32_t TotalSize = 3;
constexpr std::int32_t TrieType = 4;
constexpr std::int32_t TrieTypeBytes = 0;
constexpr std::int32_t TrieTypeUChars = 1;
constexpr std::int32_t TrieTypeMask = 7;

}

extern "C" {

std::int32_t APS5_VABI udict_swap_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    if (status == nullptr || *status > 0) return 0;
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    const std::uint8_t* formatVersion = info + 12;
    if (!(format[0] == 0x44 && format[1] == 0x69 && format[2] == 0x63 && format[3] == 0x74 && formatVersion[0] == 1)) {
        udata_printError_nid_postfix(swapper,
            "udict_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as dictionary data\n", format[0], format[1], format[2],
            format[3], formatVersion[0]);
        *status = Unsupported;
        return 0;
    }
    const auto* inBytes = static_cast<const std::uint8_t*>(input) + headerSize;
    if (length >= 0) {
        length -= headerSize;
        if (length < static_cast<std::int32_t>(IndexCount * sizeof(std::int32_t))) {
            udata_printError_nid_postfix(swapper, "udict_swap(): too few bytes (%d after header) for dictionary data\n", length);
            *status = IndexOutOfBounds;
            return 0;
        }
    }
    std::int32_t indexes[IndexCount];
    for (std::int32_t index = 0; index < IndexCount; ++index) {
        std::int32_t raw;
        std::memcpy(&raw, inBytes + index * 4, 4);
        indexes[index] = udata_readInt32_nid_postfix(swapper, raw);
    }
    const std::int32_t size = indexes[TotalSize];
    if (length >= 0) {
        if (length < size) {
            udata_printError_nid_postfix(swapper, "udict_swap(): too few bytes (%d after header) for all of dictionary data\n", length);
            *status = IndexOutOfBounds;
            return 0;
        }
        auto* outBytes = static_cast<std::uint8_t*>(output) + headerSize;
        if (inBytes != outBytes) std::memcpy(outBytes, inBytes, static_cast<std::size_t>(size));
        swapper->SwapArray32(swapper, inBytes, static_cast<std::int32_t>(sizeof(indexes)), outBytes, status);
        const auto offset = static_cast<std::int32_t>(sizeof(indexes));
        const std::int32_t type = indexes[TrieType] & TrieTypeMask;
        const std::int32_t nextOffset = indexes[Reserved1Offset];
        if (type == TrieTypeUChars) {
            swapper->SwapArray16(swapper, inBytes + offset, nextOffset - offset, outBytes + offset, status);
        } else if (type != TrieTypeBytes) {
            udata_printError_nid_postfix(swapper, "udict_swap(): unknown trie type!\n");
            *status = Unsupported;
            return 0;
        }
    }
    return headerSize + size;
}

}
