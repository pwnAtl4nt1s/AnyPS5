// Derived from icu4c/source/common/rbbidata.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/rbbidata.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2014 International Business Machines Corporation
//   and others. All rights reserved.

#include "prx/libSceIcu/BreakSwap.hpp"
#include "prx/libSceIcu/TrieSwap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::uint8_t FormatVersion = 6;
constexpr std::uint32_t EightBitRows = 4;

struct BreakHeader {
    std::uint32_t Magic;
    std::uint8_t FormatVersion[4];
    std::uint32_t Length;
    std::uint32_t CategoryCount;
    std::uint32_t ForwardTable;
    std::uint32_t ForwardTableLength;
    std::uint32_t ReverseTable;
    std::uint32_t ReverseTableLength;
    std::uint32_t Trie;
    std::uint32_t TrieLength;
    std::uint32_t RuleSource;
    std::uint32_t RuleSourceLength;
    std::uint32_t StatusTable;
    std::uint32_t StatusTableLength;
    std::uint32_t Reserved[6];
};

struct StateTable {
    std::uint32_t StateCount;
    std::uint32_t RowLength;
    std::uint32_t DictionaryCategoriesStart;
    std::uint32_t LookAheadResultsSize;
    std::uint32_t Flags;
    char TableData[1];
};

static_assert(sizeof(BreakHeader) == 80);
constexpr std::int32_t TopSize = static_cast<std::int32_t>(offsetof(StateTable, TableData));

void SwapTable(const DataSwapper* swapper, const std::uint8_t* inBytes, std::uint8_t* outBytes, const std::uint32_t start, const std::uint32_t length32,
    std::int32_t* status) {
    const auto length = static_cast<std::int32_t>(length32);
    if (length <= 0) return;
    const auto* table = reinterpret_cast<const StateTable*>(inBytes + start);
    const bool eightBits = (swapper->ReadUInt32(table->Flags) & EightBitRows) != 0;
    swapper->SwapArray32(swapper, inBytes + start, TopSize, outBytes + start, status);
    if (eightBits) {
        if (outBytes != inBytes) std::memmove(outBytes + start + TopSize, inBytes + start + TopSize, static_cast<std::size_t>(length - TopSize));
    } else {
        swapper->SwapArray16(swapper, inBytes + start + TopSize, length - TopSize, outBytes + start + TopSize, status);
    }
}

}

extern "C" {

std::int32_t APS5_VABI ubrk_swap_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (status == nullptr || *status > 0) return 0;
    if (swapper == nullptr || input == nullptr || length < -1 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    if (!(format[0] == 0x42 && format[1] == 0x72 && format[2] == 0x6b && format[3] == 0x20 && info[12] == FormatVersion)) {
        udata_printError_nid_postfix(swapper, "ubrk_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized\n", format[0], format[1],
            format[2], format[3], info[12]);
        *status = Unsupported;
        return 0;
    }
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    const auto* inBytes = static_cast<const std::uint8_t*>(input) + headerSize;
    const auto* header = reinterpret_cast<const BreakHeader*>(inBytes);
    if (swapper->ReadUInt32(header->Magic) != 0xb1a0 || header->FormatVersion[0] != FormatVersion || swapper->ReadUInt32(header->Length) < sizeof(BreakHeader)) {
        udata_printError_nid_postfix(swapper, "ubrk_swap(): RBBI Data header is invalid.\n");
        *status = Unsupported;
        return 0;
    }
    const auto breakLength = static_cast<std::int32_t>(swapper->ReadUInt32(header->Length));
    const std::int32_t totalSize = headerSize + breakLength;
    if (length < 0) return totalSize;
    if (length < totalSize) {
        udata_printError_nid_postfix(swapper, "ubrk_swap(): too few bytes (%d after ICU Data header) for break data.\n", breakLength);
        *status = IndexOutOfBounds;
        return 0;
    }
    auto* outBytes = static_cast<std::uint8_t*>(output) + headerSize;
    auto* outHeader = reinterpret_cast<BreakHeader*>(outBytes);
    if (inBytes != outBytes) std::memset(outBytes, 0, static_cast<std::size_t>(breakLength));
    SwapTable(swapper, inBytes, outBytes, swapper->ReadUInt32(header->ForwardTable), swapper->ReadUInt32(header->ForwardTableLength), status);
    SwapTable(swapper, inBytes, outBytes, swapper->ReadUInt32(header->ReverseTable), swapper->ReadUInt32(header->ReverseTableLength), status);
    const std::uint32_t trie = swapper->ReadUInt32(header->Trie);
    ucptrie_swap_nid_postfix(swapper, inBytes + trie, static_cast<std::int32_t>(swapper->ReadUInt32(header->TrieLength)), outBytes + trie, status);
    if (outBytes != inBytes) {
        const std::uint32_t rules = swapper->ReadUInt32(header->RuleSource);
        std::memmove(outBytes + rules, inBytes + rules, swapper->ReadUInt32(header->RuleSourceLength));
    }
    const std::uint32_t statusTable = swapper->ReadUInt32(header->StatusTable);
    swapper->SwapArray32(swapper, inBytes + statusTable, static_cast<std::int32_t>(swapper->ReadUInt32(header->StatusTableLength)), outBytes + statusTable, status);
    swapper->SwapArray32(swapper, inBytes, sizeof(BreakHeader), outBytes, status);
    swapper->SwapArray32(swapper, outHeader->FormatVersion, 4, outHeader->FormatVersion, status);
    return totalSize;
}

}
