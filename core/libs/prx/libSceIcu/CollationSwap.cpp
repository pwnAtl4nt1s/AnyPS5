// Derived from icu4c/source/common/ucol_swp.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucol_swp.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2015, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/CollationSwap.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::uint32_t HeaderMagic = 0x20030618;
constexpr std::int32_t Version3HeaderSize = 42 * 4;
constexpr std::size_t MagicOffset = 16;
constexpr std::size_t IsBigEndianOffset = 65;
constexpr std::size_t CharsetOffset = 66;
constexpr std::size_t FormatVersionOffset = 80;

std::uint32_t Read32(const void* data, const std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(data) + offset, 4);
    return value;
}

}

extern "C" {

std::int8_t APS5_VABI ucol_looksLikeCollationBinary_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length) {
    if (swapper == nullptr || input == nullptr || length < -1) return 0;
    std::int32_t status = 0;
    udata_swapDataHeader_nid_postfix(swapper, input, -1, nullptr, &status);
    if (status <= 0) {
        const auto* format = static_cast<const std::uint8_t*>(input) + 12;
        if (format[0] == 0x55 && format[1] == 0x43 && format[2] == 0x6f && format[3] == 0x6c) return 1;
    }
    if (length >= 0) {
        if (length < Version3HeaderSize) return 0;
        const std::int32_t size = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, 0)));
        if (length < size) return 0;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(input);
    if (swapper->ReadUInt32(Read32(input, MagicOffset)) != HeaderMagic || bytes[FormatVersionOffset] != 3) return 0;
    if (bytes[IsBigEndianOffset] != swapper->InIsBigEndian || bytes[CharsetOffset] != swapper->InCharset) return 0;
    return 1;
}

std::int32_t APS5_VABI ucol_swapInverseUCA_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    if (status == nullptr || *status > 0) return 0;
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    if (!(format[0] == 0x49 && format[1] == 0x6e && format[2] == 0x76 && format[3] == 0x43 && info[12] == 2 && info[13] >= 1)) {
        udata_printError_nid_postfix(swapper,
            "ucol_swapInverseUCA(): data format %02x.%02x.%02x.%02x (format version %02x.%02x) is not an inverse UCA collation file\n", format[0], format[1],
            format[2], format[3], info[12], info[13]);
        *status = Unsupported;
        return 0;
    }
    const auto* inBytes = static_cast<const std::uint8_t*>(input) + headerSize;
    std::uint32_t byteSize;
    if (length < 0) {
        byteSize = static_cast<std::uint32_t>(udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(inBytes, 0))));
    } else if (length - headerSize < 8 * 4 ||
        static_cast<std::uint32_t>(length - headerSize) <
            (byteSize = static_cast<std::uint32_t>(udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(inBytes, 0)))))) {
        udata_printError_nid_postfix(swapper, "ucol_swapInverseUCA(): too few bytes (%d after header) for inverse UCA collation data\n", length);
        *status = IndexOutOfBounds;
        return 0;
    }
    if (length >= 0) {
        auto* outBytes = static_cast<std::uint8_t*>(output) + headerSize;
        if (inBytes != outBytes) std::memcpy(outBytes, inBytes, byteSize);
        const std::uint32_t tableSize = swapper->ReadUInt32(Read32(inBytes, 4));
        const std::uint32_t contsSize = swapper->ReadUInt32(Read32(inBytes, 8));
        const std::uint32_t table = swapper->ReadUInt32(Read32(inBytes, 12));
        const std::uint32_t conts = swapper->ReadUInt32(Read32(inBytes, 16));
        swapper->SwapArray32(swapper, inBytes, 5 * 4, outBytes, status);
        swapper->SwapArray32(swapper, inBytes + table, static_cast<std::int32_t>(tableSize * 3 * 4), outBytes + table, status);
        swapper->SwapArray16(swapper, inBytes + conts, static_cast<std::int32_t>(contsSize * 2), outBytes + conts, status);
    }
    return headerSize + static_cast<std::int32_t>(byteSize);
}

}
