// Derived from icu4c/source/common/utrie_swap.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utrie_swap.cpp
//
// The upstream file's notice, verbatim:
//   © 2018 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html

#include "prx/libSceIcu/TrieSwap.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t InvalidFormat = 3;

constexpr std::uint32_t TrieSignature = 0x54726965;
constexpr std::uint32_t TrieOppositeSignature = 0x65697254;
constexpr std::uint32_t Trie2Signature = 0x54726932;
constexpr std::uint32_t Trie2OppositeSignature = 0x32697254;
constexpr std::uint32_t CodePointTrieSignature = 0x54726933;
constexpr std::uint32_t CodePointTrieOppositeSignature = 0x33697254;

constexpr std::int32_t TrieShift = 5;
constexpr std::int32_t TrieIndexShift = 2;
constexpr std::uint32_t TrieShiftMask = 0xf;
constexpr std::int32_t TrieOptionsIndexShift = 4;
constexpr std::uint32_t TrieDataIs32Bit = 0x100;
constexpr std::uint32_t TrieLatin1IsLinear = 0x200;
constexpr std::int32_t TrieDataBlockLength = 1 << TrieShift;
constexpr std::int32_t TrieDataGranularity = 1 << TrieIndexShift;
constexpr std::int32_t TrieSurrogateBlockCount = 1 << (10 - TrieShift);
constexpr std::int32_t TrieBmpIndexLength = 0x10000 >> TrieShift;

constexpr std::int32_t Trie2IndexShift = 2;
constexpr std::int32_t Trie2Index1Offset = 2112;
constexpr std::int32_t Trie2DataStartOffset = 0xc0;
constexpr std::uint16_t Trie2ValueBitsMask = 0xf;
constexpr std::int32_t Trie2Value16 = 0;
constexpr std::int32_t Trie2Value32 = 1;

constexpr std::uint16_t CodePointTrieDataLengthMask = 0xf000;
constexpr std::uint16_t CodePointTrieReservedMask = 0x38;
constexpr std::uint16_t CodePointTrieValueBitsMask = 7;
constexpr std::int32_t CodePointTrieFast = 0;
constexpr std::int32_t CodePointTrieSmall = 1;
constexpr std::int32_t CodePointTrieValue16 = 0;
constexpr std::int32_t CodePointTrieValue32 = 1;
constexpr std::int32_t CodePointTrieValue8 = 2;
constexpr std::int32_t CodePointTrieBmpIndexLength = 1024;
constexpr std::int32_t CodePointTrieSmallIndexLength = 64;
constexpr std::int32_t AsciiLimit = 0x80;

struct TrieHeader {
    std::uint32_t Signature;
    std::uint32_t Options;
    std::int32_t IndexLength;
    std::int32_t DataLength;
};

struct Trie2Header {
    std::uint32_t Signature;
    std::uint16_t Options;
    std::uint16_t IndexLength;
    std::uint16_t ShiftedDataLength;
    std::uint16_t Index2NullOffset;
    std::uint16_t DataNullOffset;
    std::uint16_t ShiftedHighStart;
};

struct CodePointTrieHeader {
    std::uint32_t Signature;
    std::uint16_t Options;
    std::uint16_t IndexLength;
    std::uint16_t DataLength;
    std::uint16_t Index3NullOffset;
    std::uint16_t DataNullOffset;
    std::uint16_t ShiftedHighStart;
};

static_assert(sizeof(TrieHeader) == 16 && sizeof(Trie2Header) == 16 && sizeof(CodePointTrieHeader) == 16);

template<class THeader>
bool Prepare(const DataSwapper* swapper, const void* input, const std::int32_t length, const void* output, std::int32_t* status) {
    if (swapper == nullptr || input == nullptr || (length >= 0 && output == nullptr)) {
        *status = IllegalArgument;
        return false;
    }
    if (length >= 0 && length < static_cast<std::int32_t>(sizeof(THeader))) {
        *status = IndexOutOfBounds;
        return false;
    }
    return true;
}

std::int32_t Version(const void* data, const std::int32_t length) {
    if (length < 16 || data == nullptr || (reinterpret_cast<std::uintptr_t>(data) & 3) != 0) return 0;
    const std::uint32_t signature = *static_cast<const std::uint32_t*>(data);
    if (signature == CodePointTrieSignature || signature == CodePointTrieOppositeSignature) return 3;
    if (signature == Trie2Signature || signature == Trie2OppositeSignature) return 2;
    if (signature == TrieSignature || signature == TrieOppositeSignature) return 1;
    return 0;
}

}

namespace Icu {

std::int32_t SwapTrieVersion1(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (status == nullptr || *status > 0) return 0;
    if (!Prepare<TrieHeader>(swapper, input, length, output, status)) return 0;
    const auto* inTrie = static_cast<const TrieHeader*>(input);
    const std::uint32_t signature = swapper->ReadUInt32(inTrie->Signature);
    const std::uint32_t options = swapper->ReadUInt32(inTrie->Options);
    const std::int32_t indexLength = udata_readInt32_nid_postfix(swapper, inTrie->IndexLength);
    const std::int32_t dataLength = udata_readInt32_nid_postfix(swapper, inTrie->DataLength);
    if (signature != TrieSignature || (options & TrieShiftMask) != TrieShift || ((options >> TrieOptionsIndexShift) & TrieShiftMask) != TrieIndexShift ||
        indexLength < TrieBmpIndexLength || (indexLength & (TrieSurrogateBlockCount - 1)) != 0 || dataLength < TrieDataBlockLength ||
        (dataLength & (TrieDataGranularity - 1)) != 0 || ((options & TrieLatin1IsLinear) != 0 && dataLength < TrieDataBlockLength + 0x100)) {
        *status = InvalidFormat;
        return 0;
    }
    const bool dataIs32 = (options & TrieDataIs32Bit) != 0;
    const std::int32_t size = static_cast<std::int32_t>(sizeof(TrieHeader)) + indexLength * 2 + dataLength * (dataIs32 ? 4 : 2);
    if (length >= 0) {
        if (length < size) {
            *status = IndexOutOfBounds;
            return 0;
        }
        auto* outTrie = static_cast<TrieHeader*>(output);
        swapper->SwapArray32(swapper, inTrie, sizeof(TrieHeader), outTrie, status);
        if (dataIs32) {
            swapper->SwapArray16(swapper, inTrie + 1, indexLength * 2, outTrie + 1, status);
            swapper->SwapArray32(swapper, reinterpret_cast<const std::uint16_t*>(inTrie + 1) + indexLength, dataLength * 4,
                reinterpret_cast<std::uint16_t*>(outTrie + 1) + indexLength, status);
        } else {
            swapper->SwapArray16(swapper, inTrie + 1, (indexLength + dataLength) * 2, outTrie + 1, status);
        }
    }
    return size;
}

}

extern "C" {

std::int32_t APS5_VABI utrie2_swap_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (!Prepare<Trie2Header>(swapper, input, length, output, status)) return 0;
    const auto* inTrie = static_cast<const Trie2Header*>(input);
    const std::uint32_t signature = swapper->ReadUInt32(inTrie->Signature);
    const std::uint16_t options = swapper->ReadUInt16(inTrie->Options);
    const std::int32_t indexLength = swapper->ReadUInt16(inTrie->IndexLength);
    const std::uint16_t shiftedDataLength = swapper->ReadUInt16(inTrie->ShiftedDataLength);
    const std::int32_t valueBits = options & Trie2ValueBitsMask;
    const std::int32_t dataLength = static_cast<std::int32_t>(shiftedDataLength) << Trie2IndexShift;
    if (signature != Trie2Signature || valueBits > Trie2Value32 || indexLength < Trie2Index1Offset || dataLength < Trie2DataStartOffset) {
        *status = InvalidFormat;
        return 0;
    }
    const std::int32_t size = static_cast<std::int32_t>(sizeof(Trie2Header)) + indexLength * 2 + dataLength * (valueBits == Trie2Value16 ? 2 : 4);
    if (length >= 0) {
        if (length < size) {
            *status = IndexOutOfBounds;
            return 0;
        }
        auto* outTrie = static_cast<Trie2Header*>(output);
        swapper->SwapArray32(swapper, &inTrie->Signature, 4, &outTrie->Signature, status);
        swapper->SwapArray16(swapper, &inTrie->Options, 12, &outTrie->Options, status);
        if (valueBits == Trie2Value16) {
            swapper->SwapArray16(swapper, inTrie + 1, (indexLength + dataLength) * 2, outTrie + 1, status);
        } else {
            swapper->SwapArray16(swapper, inTrie + 1, indexLength * 2, outTrie + 1, status);
            swapper->SwapArray32(swapper, reinterpret_cast<const std::uint16_t*>(inTrie + 1) + indexLength, dataLength * 4,
                reinterpret_cast<std::uint16_t*>(outTrie + 1) + indexLength, status);
        }
    }
    return size;
}

std::int32_t APS5_VABI ucptrie_swap_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (!Prepare<CodePointTrieHeader>(swapper, input, length, output, status)) return 0;
    const auto* inTrie = static_cast<const CodePointTrieHeader*>(input);
    const std::uint32_t signature = swapper->ReadUInt32(inTrie->Signature);
    const std::uint16_t options = swapper->ReadUInt16(inTrie->Options);
    const std::int32_t indexLength = swapper->ReadUInt16(inTrie->IndexLength);
    const std::uint16_t lowDataLength = swapper->ReadUInt16(inTrie->DataLength);
    const std::int32_t type = (options >> 6) & 3;
    const std::int32_t valueWidth = options & CodePointTrieValueBitsMask;
    const std::int32_t dataLength = (static_cast<std::int32_t>(options & CodePointTrieDataLengthMask) << 4) | lowDataLength;
    const std::int32_t minimumIndexLength = type == CodePointTrieFast ? CodePointTrieBmpIndexLength : CodePointTrieSmallIndexLength;
    if (signature != CodePointTrieSignature || type > CodePointTrieSmall || (options & CodePointTrieReservedMask) != 0 || valueWidth > CodePointTrieValue8 ||
        indexLength < minimumIndexLength || dataLength < AsciiLimit) {
        *status = InvalidFormat;
        return 0;
    }
    std::int32_t size = static_cast<std::int32_t>(sizeof(CodePointTrieHeader)) + indexLength * 2;
    if (valueWidth == CodePointTrieValue16) {
        size += dataLength * 2;
    } else if (valueWidth == CodePointTrieValue32) {
        size += dataLength * 4;
    } else {
        size += dataLength;
    }
    if (length >= 0) {
        if (length < size) {
            *status = IndexOutOfBounds;
            return 0;
        }
        auto* outTrie = static_cast<CodePointTrieHeader*>(output);
        swapper->SwapArray32(swapper, &inTrie->Signature, 4, &outTrie->Signature, status);
        swapper->SwapArray16(swapper, &inTrie->Options, 12, &outTrie->Options, status);
        const auto* inIndex = reinterpret_cast<const std::uint16_t*>(inTrie + 1);
        auto* outIndex = reinterpret_cast<std::uint16_t*>(outTrie + 1);
        swapper->SwapArray16(swapper, inIndex, indexLength * 2, outIndex, status);
        const std::uint16_t* inData = inIndex + indexLength;
        std::uint16_t* outData = outIndex + indexLength;
        if (valueWidth == CodePointTrieValue16) {
            swapper->SwapArray16(swapper, inData, dataLength * 2, outData, status);
        } else if (valueWidth == CodePointTrieValue32) {
            swapper->SwapArray32(swapper, inData, dataLength * 4, outData, status);
        } else if (static_cast<const void*>(inTrie) != static_cast<void*>(outTrie)) {
            std::memmove(outData, inData, static_cast<std::size_t>(dataLength));
        }
    }
    return size;
}

std::int32_t APS5_VABI utrie_swapAnyVersion_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    switch (Version(input, length)) {
    case 1:
        return Icu::SwapTrieVersion1(swapper, input, length, output, status);
    case 2:
        return utrie2_swap_nid_postfix(swapper, input, length, output, status);
    case 3:
        return ucptrie_swap_nid_postfix(swapper, input, length, output, status);
    default:
        *status = InvalidFormat;
        return 0;
    }
}

}
