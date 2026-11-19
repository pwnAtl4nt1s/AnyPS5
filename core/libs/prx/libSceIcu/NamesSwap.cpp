// Derived from icu4c/source/common/unames.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/unames.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/NamesSwap.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::uint16_t LinesPerGroup = 32;

struct AlgorithmicRange {
    std::uint32_t Start;
    std::uint32_t End;
    std::uint8_t Type;
    std::uint8_t Variant;
    std::uint16_t Size;
};

static_assert(sizeof(AlgorithmicRange) == 12);

std::uint32_t Read32(const DataSwapper* swapper, const std::uint8_t* bytes) {
    std::uint32_t value;
    std::memcpy(&value, bytes, 4);
    return swapper->ReadUInt32(value);
}

std::uint16_t Read16(const DataSwapper* swapper, const std::uint8_t* bytes) {
    std::uint16_t value;
    std::memcpy(&value, bytes, 2);
    return swapper->ReadUInt16(value);
}

void MakeTokenMap(const DataSwapper* swapper, const std::int16_t* tokens, std::uint16_t tokenCount, std::uint8_t* map, std::int32_t* status) {
    if (*status > 0) return;
    if (swapper->InCharset == swapper->OutCharset) {
        for (std::uint16_t index = 0; index < 256; ++index) map[index] = static_cast<std::uint8_t>(index);
        return;
    }
    std::array<bool, 256> used{};
    std::memset(map, 0, 256);
    if (tokenCount > 256) tokenCount = 256;
    for (std::uint16_t index = 1; index < tokenCount; ++index) {
        if (tokens[index] == -1) {
            std::uint8_t from = static_cast<std::uint8_t>(index);
            std::uint8_t to = 0;
            swapper->SwapInvChars(swapper, &from, 1, &to, status);
            if (*status > 0) {
                udata_printError_nid_postfix(swapper, "unames/makeTokenMap() finds variant character 0x%02x used (input charset family %d)\n", index,
                    swapper->InCharset);
                return;
            }
            map[from] = to;
            used[to] = true;
        }
    }
    std::uint16_t next = 1;
    for (std::uint16_t index = 1; index < tokenCount; ++index) {
        if (map[index] == 0) {
            while (used[next]) ++next;
            map[index] = static_cast<std::uint8_t>(next++);
        }
    }
}

const std::uint8_t* ExpandGroupLengths(const std::uint8_t* s, std::uint16_t* offsets, std::uint16_t* lengths) {
    std::uint16_t index = 0;
    std::uint16_t offset = 0;
    std::uint16_t length = 0;
    while (index < LinesPerGroup) {
        std::uint8_t lengthByte = *s++;
        if (length >= 12) {
            length = static_cast<std::uint16_t>((((length & 0x3) << 4) | (lengthByte >> 4)) + 12);
            lengthByte &= 0xf;
        } else if (lengthByte >= 0xc0) {
            length = static_cast<std::uint16_t>((lengthByte & 0x3f) + 12);
        } else {
            length = static_cast<std::uint16_t>(lengthByte >> 4);
            lengthByte &= 0xf;
        }
        *offsets++ = offset;
        *lengths++ = length;
        offset = static_cast<std::uint16_t>(offset + length);
        ++index;
        if ((lengthByte & 0xf0) == 0) {
            length = lengthByte;
            if (length < 12) {
                *offsets++ = offset;
                *lengths++ = length;
                offset = static_cast<std::uint16_t>(offset + length);
                ++index;
            }
        } else {
            length = 0;
        }
    }
    return s;
}

}

extern "C" {

std::int32_t APS5_VABI uchar_swapNames_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    if (status == nullptr || *status > 0) return 0;
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    if (!(format[0] == 0x75 && format[1] == 0x6e && format[2] == 0x61 && format[3] == 0x6d && info[12] == 1)) {
        udata_printError_nid_postfix(swapper, "uchar_swapNames(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as unames.icu\n",
            format[0], format[1], format[2], format[3], info[12]);
        *status = Unsupported;
        return 0;
    }
    const auto* inBytes = static_cast<const std::uint8_t*>(input) + headerSize;
    std::uint32_t algorithmicOffset;
    if (length < 0) {
        algorithmicOffset = Read32(swapper, inBytes + 12);
    } else {
        length -= headerSize;
        if (length < 20 || static_cast<std::uint32_t>(length) < (algorithmicOffset = Read32(swapper, inBytes + 12))) {
            udata_printError_nid_postfix(swapper, "uchar_swapNames(): too few bytes (%d after header) for unames.icu\n", length);
            *status = IndexOutOfBounds;
            return 0;
        }
    }
    std::uint32_t offset = algorithmicOffset;
    if (length < 0) {
        const std::uint32_t count = Read32(swapper, inBytes + offset);
        offset += 4;
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto* range = reinterpret_cast<const AlgorithmicRange*>(inBytes + offset);
            offset += swapper->ReadUInt16(range->Size);
        }
        return headerSize + static_cast<std::int32_t>(offset);
    }
    auto* outBytes = static_cast<std::uint8_t*>(output) + headerSize;
    if (inBytes != outBytes) std::memcpy(outBytes, inBytes, static_cast<std::size_t>(length));
    const std::uint32_t tokenStringOffset = Read32(swapper, inBytes);
    const std::uint32_t groupsOffset = Read32(swapper, inBytes + 4);
    const std::uint32_t groupStringOffset = Read32(swapper, inBytes + 8);
    swapper->SwapArray32(swapper, inBytes, 16, outBytes, status);
    const auto* p = reinterpret_cast<const std::uint16_t*>(inBytes + 16);
    auto* q = reinterpret_cast<std::uint16_t*>(outBytes + 16);
    const std::uint16_t tokenCount = Read16(swapper, inBytes + 16);
    swapper->SwapArray16(swapper, p, 2, q, status);
    ++p;
    ++q;
    std::array<std::int16_t, 512> tokens{};
    const std::uint32_t limit = tokenCount <= 512 ? tokenCount : 512;
    for (std::uint32_t index = 0; index < limit; ++index) {
        std::uint16_t raw;
        std::memcpy(&raw, p + index, 2);
        tokens[index] = udata_readInt16_nid_postfix(swapper, static_cast<std::int16_t>(raw));
    }
    std::array<std::uint8_t, 256> map{};
    std::array<std::uint8_t, 256> trailMap{};
    MakeTokenMap(swapper, tokens.data(), tokenCount, map.data(), status);
    MakeTokenMap(swapper, tokens.data() + 256, static_cast<std::uint16_t>(tokenCount > 256 ? tokenCount - 256 : 0), trailMap.data(), status);
    if (*status > 0) return 0;
    std::vector<std::uint16_t> swapped(tokenCount);
    std::uint32_t index = 0;
    for (; index < tokenCount && index < 256; ++index) swapper->SwapArray16(swapper, p + index, 2, swapped.data() + map[index], status);
    for (; index < tokenCount; ++index) swapper->SwapArray16(swapper, p + index, 2, swapped.data() + (index & 0xffffff00u) + trailMap[index & 0xff], status);
    if (tokenCount > 0) std::memcpy(q, swapped.data(), static_cast<std::size_t>(tokenCount) * 2);
    udata_swapInvStringBlock_nid_postfix(swapper, inBytes + tokenStringOffset, static_cast<std::int32_t>(groupsOffset - tokenStringOffset),
        outBytes + tokenStringOffset, status);
    if (*status > 0) {
        udata_printError_nid_postfix(swapper, "uchar_swapNames(token strings) failed\n");
        return 0;
    }
    const std::uint32_t groupCount = Read16(swapper, inBytes + groupsOffset);
    swapper->SwapArray16(swapper, inBytes + groupsOffset, static_cast<std::int32_t>((1 + groupCount * 3) * 2), outBytes + groupsOffset, status);
    if (swapper->InCharset != swapper->OutCharset) {
        std::array<std::uint16_t, LinesPerGroup + 1> offsets{};
        std::array<std::uint16_t, LinesPerGroup + 1> lengths{};
        const std::uint8_t* inStrings = inBytes + groupStringOffset;
        std::uint8_t* outStrings = outBytes + groupStringOffset;
        std::uint32_t stringsCount = algorithmicOffset - groupStringOffset;
        while (stringsCount > 32) {
            const std::uint8_t* next = ExpandGroupLengths(inStrings, offsets.data(), lengths.data());
            stringsCount -= static_cast<std::uint32_t>(next - inStrings);
            outStrings += next - inStrings;
            inStrings = next;
            std::uint32_t count = static_cast<std::uint32_t>(offsets[31]) + lengths[31];
            stringsCount -= count;
            while (count > 0) {
                const std::uint8_t c = *inStrings++;
                *outStrings++ = map[c];
                if (tokens[c] != -2) {
                    --count;
                } else {
                    *outStrings++ = trailMap[*inStrings++];
                    count -= 2;
                }
            }
        }
    }
    offset = algorithmicOffset;
    const std::uint32_t rangeCount = Read32(swapper, inBytes + offset);
    swapper->SwapArray32(swapper, inBytes + offset, 4, outBytes + offset, status);
    offset += 4;
    for (std::uint32_t rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex) {
        if (offset > static_cast<std::uint32_t>(length)) {
            udata_printError_nid_postfix(swapper, "uchar_swapNames(): too few bytes (%d after header) for unames.icu algorithmic range %u\n", length, rangeIndex);
            *status = IndexOutOfBounds;
            return 0;
        }
        const auto* inRange = reinterpret_cast<const AlgorithmicRange*>(inBytes + offset);
        auto* outRange = reinterpret_cast<AlgorithmicRange*>(outBytes + offset);
        offset += swapper->ReadUInt16(inRange->Size);
        swapper->SwapArray32(swapper, inRange, 8, outRange, status);
        swapper->SwapArray16(swapper, &inRange->Size, 2, &outRange->Size, status);
        if (inRange->Type == 0) {
            const auto* prefix = reinterpret_cast<const char*>(inRange + 1);
            swapper->SwapInvChars(swapper, prefix, static_cast<std::int32_t>(std::strlen(prefix)), outRange + 1, status);
            if (*status > 0) {
                udata_printError_nid_postfix(swapper, "uchar_swapNames(prefix string of algorithmic range %u) failed\n", rangeIndex);
                return 0;
            }
        } else if (inRange->Type == 1) {
            const std::uint32_t factors = inRange->Variant;
            const auto* inFactors = reinterpret_cast<const std::uint16_t*>(inRange + 1);
            auto* outFactors = reinterpret_cast<std::uint16_t*>(outRange + 1);
            swapper->SwapArray16(swapper, inFactors, static_cast<std::int32_t>(factors * 2), outFactors, status);
            const auto* inStrings = reinterpret_cast<const std::uint8_t*>(inFactors + factors);
            auto* outStrings = reinterpret_cast<std::uint8_t*>(outFactors + factors);
            std::uint32_t stringsCount = static_cast<std::uint32_t>((inBytes + offset) - inStrings);
            while (stringsCount > 0 && inStrings[stringsCount - 1] != 0) --stringsCount;
            swapper->SwapInvChars(swapper, inStrings, static_cast<std::int32_t>(stringsCount), outStrings, status);
        } else {
            udata_printError_nid_postfix(swapper, "uchar_swapNames(): unknown type %u of algorithmic range %u\n", inRange->Type, rangeIndex);
            *status = Unsupported;
            return 0;
        }
    }
    return headerSize + static_cast<std::int32_t>(offset);
}

}
