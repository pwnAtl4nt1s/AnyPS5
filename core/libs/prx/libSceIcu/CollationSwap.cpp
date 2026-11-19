// Derived from icu4c/source/common/ucol_swp.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucol_swp.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2015, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/CollationSwap.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libSceIcu/TrieSwap.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
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

std::uint16_t Read16(const void* data, const std::size_t offset) {
    std::uint16_t value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(data) + offset, 2);
    return value;
}

std::int32_t SwapVersion3(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (*status > 0) return 0;
    if (swapper == nullptr || input == nullptr || length < -1 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* inBytes = static_cast<const std::uint8_t*>(input);
    auto* outBytes = static_cast<std::uint8_t*>(output);
    const auto read32 = [&](const std::size_t offset) { return swapper->ReadUInt32(Read32(input, offset)); };
    std::int32_t size;
    if (length < 0) {
        size = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, 0)));
    } else if (length < Version3HeaderSize || length < (size = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, 0))))) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=3): too few bytes (%d after header) for collation data\n", length);
        *status = IndexOutOfBounds;
        return 0;
    }
    const std::uint32_t magic = read32(MagicOffset);
    if (!(magic == HeaderMagic && inBytes[FormatVersionOffset] == 3)) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=3): magic 0x%08x or format version %02x.%02x is not a collation binary\n", magic,
            inBytes[FormatVersionOffset], inBytes[FormatVersionOffset + 1]);
        *status = Unsupported;
        return 0;
    }
    if (inBytes[IsBigEndianOffset] != swapper->InIsBigEndian || inBytes[CharsetOffset] != swapper->InCharset) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=3): endianness %d or charset %d does not match the swapper\n", inBytes[IsBigEndianOffset],
            inBytes[CharsetOffset]);
        *status = InvalidFormat;
        return 0;
    }
    if (length < 0) return size;
    if (inBytes != outBytes) std::memcpy(outBytes, inBytes, static_cast<std::size_t>(size));
    const std::uint32_t options = read32(4);
    const std::uint32_t constants = read32(8);
    const std::uint32_t combos = read32(12);
    const std::uint32_t mapping = read32(20);
    const std::uint32_t expansion = read32(24);
    const std::uint32_t contractionIndex = read32(28);
    const std::uint32_t contractionCEs = read32(32);
    const std::uint32_t contractionSize = read32(36);
    const std::uint32_t endExpansion = read32(40);
    const std::int32_t endExpansionCount = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, 48)));
    const std::int32_t combosSize = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, 60)));
    const std::uint32_t scriptToLead = read32(84);
    const std::uint32_t leadToScript = read32(88);
    swapper->SwapArray32(swapper, inBytes, 64, outBytes, status);
    swapper->SwapArray32(swapper, inBytes + 84, 8, outBytes + 84, status);
    outBytes[IsBigEndianOffset] = swapper->OutIsBigEndian;
    outBytes[CharsetOffset] = swapper->OutCharset;
    if (options != 0) swapper->SwapArray32(swapper, inBytes + options, static_cast<std::int32_t>(expansion - options), outBytes + options, status);
    if (mapping != 0 && expansion != 0) {
        const std::uint32_t count = contractionIndex != 0 ? contractionIndex - expansion : mapping - expansion;
        swapper->SwapArray32(swapper, inBytes + expansion, static_cast<std::int32_t>(count), outBytes + expansion, status);
    }
    if (contractionSize != 0) {
        swapper->SwapArray16(swapper, inBytes + contractionIndex, static_cast<std::int32_t>(contractionSize * 2), outBytes + contractionIndex, status);
        swapper->SwapArray32(swapper, inBytes + contractionCEs, static_cast<std::int32_t>(contractionSize * 4), outBytes + contractionCEs, status);
    }
    if (mapping != 0) {
        Icu::SwapTrieVersion1(swapper, inBytes + mapping, static_cast<std::int32_t>(endExpansion - mapping), outBytes + mapping, status);
    }
    if (endExpansionCount != 0) swapper->SwapArray32(swapper, inBytes + endExpansion, endExpansionCount * 4, outBytes + endExpansion, status);
    if (constants != 0) swapper->SwapArray32(swapper, inBytes + constants, static_cast<std::int32_t>(combos - constants), outBytes + constants, status);
    if (combosSize != 0) {
        const std::uint32_t count = static_cast<std::uint32_t>(combosSize) * inBytes[67] * 2;
        swapper->SwapArray16(swapper, inBytes + combos, static_cast<std::int32_t>(count), outBytes + combos, status);
    }
    if (scriptToLead != 0) {
        const std::int32_t indexCount = swapper->ReadUInt16(Read16(inBytes, scriptToLead));
        const std::int32_t dataCount = swapper->ReadUInt16(Read16(inBytes, scriptToLead + 2));
        swapper->SwapArray16(swapper, inBytes + scriptToLead, 4 + 4 * indexCount + 2 * dataCount, outBytes + scriptToLead, status);
    }
    if (leadToScript != 0) {
        const std::int32_t indexCount = swapper->ReadUInt16(Read16(inBytes, leadToScript));
        const std::int32_t dataCount = swapper->ReadUInt16(Read16(inBytes, leadToScript + 2));
        swapper->SwapArray16(swapper, inBytes + leadToScript, 4 + 2 * indexCount + 2 * dataCount, outBytes + leadToScript, status);
    }
    return size;
}

enum Index : std::int32_t {
    IndexesLength,
    ReorderCodesOffset = 5,
    TrieOffset = 7,
    Reserved8Offset,
    CesOffset,
    Reserved10Offset,
    Ce32sOffset,
    RootElementsOffset,
    ContextsOffset,
    UnsafeBackwardOffset,
    FastLatinTableOffset,
    ScriptsOffset,
    Reserved18Offset = 18,
    TotalSize
};

std::int32_t SwapVersion4(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    if (*status > 0) return 0;
    const auto* inBytes = static_cast<const std::uint8_t*>(input);
    auto* outBytes = static_cast<std::uint8_t*>(output);
    if (length >= 0 && length < 8) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=4): too few bytes (%d after header) for collation data\n", length);
        *status = IndexOutOfBounds;
        return 0;
    }
    std::int32_t indexes[TotalSize + 1];
    const std::int32_t indexesLength = indexes[0] = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, 0)));
    if (length >= 0 && length < indexesLength * 4) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=4): too few bytes (%d after header) for collation data\n", length);
        *status = IndexOutOfBounds;
        return 0;
    }
    for (std::int32_t index = 1; index <= TotalSize && index < indexesLength; ++index) {
        indexes[index] = udata_readInt32_nid_postfix(swapper, static_cast<std::int32_t>(Read32(input, static_cast<std::size_t>(index) * 4)));
    }
    for (std::int32_t index = indexesLength < 1 ? 1 : indexesLength; index <= TotalSize; ++index) indexes[index] = -1;
    std::int32_t size;
    if (indexesLength > TotalSize) {
        size = indexes[TotalSize];
    } else if (indexesLength > ReorderCodesOffset) {
        size = indexes[indexesLength - 1];
    } else {
        size = indexesLength * 4;
    }
    if (length < 0) return size;
    if (length < size) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=4): too few bytes (%d after header) for collation data\n", length);
        *status = IndexOutOfBounds;
        return 0;
    }
    if (inBytes != outBytes) std::memcpy(outBytes, inBytes, static_cast<std::size_t>(size));
    swapper->SwapArray32(swapper, inBytes, indexesLength * 4, outBytes, status);
    const auto part = [&](const std::int32_t index, std::int32_t& offset) {
        offset = indexes[index];
        return indexes[index + 1] - offset;
    };
    std::int32_t offset = 0;
    std::int32_t partLength = part(ReorderCodesOffset, offset);
    if (partLength > 0) swapper->SwapArray32(swapper, inBytes + offset, partLength, outBytes + offset, status);
    partLength = part(TrieOffset, offset);
    if (partLength > 0) utrie2_swap_nid_postfix(swapper, inBytes + offset, partLength, outBytes + offset, status);
    partLength = part(Reserved8Offset, offset);
    if (partLength > 0) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=4): unknown data at IX_RESERVED8_OFFSET\n", partLength);
        *status = Unsupported;
        return 0;
    }
    partLength = part(CesOffset, offset);
    if (partLength > 0) swapper->SwapArray64(swapper, inBytes + offset, partLength, outBytes + offset, status);
    partLength = part(Reserved10Offset, offset);
    if (partLength > 0) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=4): unknown data at IX_RESERVED10_OFFSET\n", partLength);
        *status = Unsupported;
        return 0;
    }
    for (const std::int32_t index : {Ce32sOffset, RootElementsOffset}) {
        partLength = part(index, offset);
        if (partLength > 0) swapper->SwapArray32(swapper, inBytes + offset, partLength, outBytes + offset, status);
    }
    for (const std::int32_t index : {ContextsOffset, UnsafeBackwardOffset, FastLatinTableOffset, ScriptsOffset}) {
        partLength = part(index, offset);
        if (partLength > 0) swapper->SwapArray16(swapper, inBytes + offset, partLength, outBytes + offset, status);
    }
    partLength = part(Reserved18Offset, offset);
    if (partLength > 0) {
        udata_printError_nid_postfix(swapper, "ucol_swap(formatVersion=4): unknown data at IX_RESERVED18_OFFSET\n", partLength);
        *status = Unsupported;
        return 0;
    }
    return size;
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

std::int32_t APS5_VABI ucol_swap_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    if (*status > 0) {
        *status = 0;
        return SwapVersion3(swapper, input, length, output, status);
    }
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    if (!(format[0] == 0x55 && format[1] == 0x43 && format[2] == 0x6f && format[3] == 0x6c && info[12] >= 3 && info[12] <= 5)) {
        udata_printError_nid_postfix(swapper, "ucol_swap(): data format %02x.%02x.%02x.%02x (format version %02x.%02x) is not recognized as collation data\n",
            format[0], format[1], format[2], format[3], info[12], info[13]);
        *status = Unsupported;
        return 0;
    }
    const auto* body = static_cast<const std::uint8_t*>(input) + headerSize;
    if (length >= 0) length -= headerSize;
    auto* outBody = output == nullptr ? nullptr : static_cast<std::uint8_t*>(output) + headerSize;
    const std::int32_t collationSize =
        info[12] >= 4 ? SwapVersion4(swapper, body, length, outBody, status) : SwapVersion3(swapper, body, length, outBody, status);
    return *status <= 0 ? headerSize + collationSize : 0;
}

}
