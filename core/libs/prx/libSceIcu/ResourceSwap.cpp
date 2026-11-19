// Derived from icu4c/source/common/uresdata.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresdata.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2016, International Business Machines Corporation
//   and others. All Rights Reserved.

#include "prx/libSceIcu/ResourceSwap.hpp"
#include "prx/libSceIcu/ArraySort.hpp"
#include "prx/libSceIcu/CollationSwap.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t RowCapacity = 200;

enum Type : std::int32_t {
    String = 0,
    Binary = 1,
    Table = 2,
    Alias = 3,
    Table32 = 4,
    Table16 = 5,
    StringV2 = 6,
    Integer = 7,
    Array = 8,
    Array16 = 9,
    IntVector = 14
};

enum Index : std::int32_t {
    IndexLength,
    KeysTop,
    BundleTop = 3,
    MaxTableLength,
    SixteenBitTop = 6
};

using Resource = std::uint32_t;

struct Row {
    std::int32_t KeyIndex;
    std::int32_t SortIndex;
};

struct TempTable {
    const char* KeyChars;
    std::vector<Row> Rows;
    std::vector<std::int32_t> Resort;
    std::vector<std::uint32_t> Flags;
    std::int32_t LocalKeyLimit;
    std::uint8_t MajorFormatVersion;
};

const char UnknownKey[] = "";
constexpr std::array<char16_t, 14> CollationBinaryKey{u'%', u'%', u'C', u'o', u'l', u'l', u'a', u't', u'i', u'o', u'n', u'B', u'i', u'n'};

std::int32_t APS5_VABI CompareRows(const void* context, const void* left, const void* right) {
    const auto* keys = static_cast<const char*>(context);
    return std::strcmp(keys + static_cast<const Row*>(left)->KeyIndex, keys + static_cast<const Row*>(right)->KeyIndex);
}

std::int32_t ReadInt(const DataSwapper* swapper, const void* pointer) {
    std::int32_t raw;
    std::memcpy(&raw, pointer, 4);
    return udata_readInt32_nid_postfix(swapper, raw);
}

std::uint32_t ReadUInt(const DataSwapper* swapper, const void* pointer) {
    std::uint32_t raw;
    std::memcpy(&raw, pointer, 4);
    return swapper->ReadUInt32(raw);
}

std::uint16_t ReadShort(const DataSwapper* swapper, const void* pointer) {
    std::uint16_t raw;
    std::memcpy(&raw, pointer, 2);
    return swapper->ReadUInt16(raw);
}

void SwapResource(const DataSwapper* swapper, const Resource* inBundle, Resource* outBundle, const Resource resource, const char* key, TempTable& table,
    std::int32_t* status) {
    const auto type = static_cast<std::int32_t>(resource >> 28);
    if (type == Table16 || type == StringV2 || type == Integer || type == Array16) return;
    std::int32_t offset = static_cast<std::int32_t>(resource & 0x0fffffff);
    if (offset == 0) return;
    std::uint32_t& flags = table.Flags[static_cast<std::size_t>(offset >> 5)];
    const std::uint32_t bit = 1u << (offset & 0x1f);
    if ((flags & bit) != 0) return;
    flags |= bit;
    const Resource* p = inBundle + offset;
    Resource* q = outBundle + offset;
    switch (type) {
    case Alias:
    case String: {
        const std::int32_t count = ReadInt(swapper, p);
        swapper->SwapArray32(swapper, p, 4, q, status);
        swapper->SwapArray16(swapper, p + 1, 2 * count, q + 1, status);
        break;
    }
    case Binary: {
        const std::int32_t count = ReadInt(swapper, p);
        swapper->SwapArray32(swapper, p, 4, q, status);
        if (key != nullptr &&
            (key != UnknownKey ? swapper->CompareInvChars(swapper, key, -1, CollationBinaryKey.data(), static_cast<std::int32_t>(CollationBinaryKey.size())) == 0
                               : ucol_looksLikeCollationBinary_nid_postfix(swapper, p + 1, count) != 0)) {
            ucol_swap_nid_postfix(swapper, p + 1, count, q + 1, status);
        }
        break;
    }
    case Table:
    case Table32: {
        const std::uint16_t* pKey16 = nullptr;
        std::uint16_t* qKey16 = nullptr;
        const std::int32_t* pKey32 = nullptr;
        std::int32_t* qKey32 = nullptr;
        std::int32_t count;
        if (type == Table) {
            pKey16 = reinterpret_cast<const std::uint16_t*>(p);
            qKey16 = reinterpret_cast<std::uint16_t*>(q);
            count = ReadShort(swapper, pKey16);
            swapper->SwapArray16(swapper, pKey16++, 2, qKey16++, status);
            offset += ((1 + count) + 1) / 2;
        } else {
            pKey32 = reinterpret_cast<const std::int32_t*>(p);
            qKey32 = reinterpret_cast<std::int32_t*>(q);
            count = ReadInt(swapper, pKey32);
            swapper->SwapArray32(swapper, pKey32++, 4, qKey32++, status);
            offset += 1 + count;
        }
        if (count == 0) break;
        p = inBundle + offset;
        q = outBundle + offset;
        for (std::int32_t index = 0; index < count; ++index) {
            const char* itemKey = UnknownKey;
            if (pKey16 != nullptr) {
                const std::int32_t keyOffset = ReadShort(swapper, pKey16 + index);
                if (keyOffset < table.LocalKeyLimit) itemKey = reinterpret_cast<const char*>(outBundle) + keyOffset;
            } else {
                const std::int32_t keyOffset = ReadInt(swapper, pKey32 + index);
                if (keyOffset >= 0) itemKey = reinterpret_cast<const char*>(outBundle) + keyOffset;
            }
            const Resource item = ReadUInt(swapper, p + index);
            SwapResource(swapper, inBundle, outBundle, item, itemKey, table, status);
            if (*status > 0) {
                udata_printError_nid_postfix(swapper, "ures_swapResource(table res=%08x)[%d].recurse(%08x) failed\n", resource, index, item);
                return;
            }
        }
        if (table.MajorFormatVersion > 1 || swapper->InCharset == swapper->OutCharset) {
            if (pKey16 != nullptr) {
                swapper->SwapArray16(swapper, pKey16, count * 2, qKey16, status);
                swapper->SwapArray32(swapper, p, count * 4, q, status);
            } else {
                swapper->SwapArray32(swapper, pKey32, count * 2 * 4, qKey32, status);
            }
            break;
        }
        if (table.Rows.size() < static_cast<std::size_t>(count)) {
            table.Rows.resize(static_cast<std::size_t>(count));
            table.Resort.resize(static_cast<std::size_t>(count));
        }
        for (std::int32_t index = 0; index < count; ++index) {
            table.Rows[static_cast<std::size_t>(index)].KeyIndex = pKey16 != nullptr ? ReadShort(swapper, pKey16 + index) : ReadInt(swapper, pKey32 + index);
            table.Rows[static_cast<std::size_t>(index)].SortIndex = index;
        }
        uprv_sortArray_nid_postfix(table.Rows.data(), count, sizeof(Row), CompareRows, table.KeyChars, 0, status);
        if (*status > 0) {
            udata_printError_nid_postfix(swapper, "ures_swapResource(table res=%08x).uprv_sortArray(%d items) failed\n", resource, count);
            return;
        }
        if (pKey16 != nullptr) {
            auto* rKey16 = pKey16 != qKey16 ? qKey16 : reinterpret_cast<std::uint16_t*>(table.Resort.data());
            for (std::int32_t index = 0; index < count; ++index) {
                swapper->SwapArray16(swapper, pKey16 + table.Rows[static_cast<std::size_t>(index)].SortIndex, 2, rKey16 + index, status);
            }
            if (qKey16 != rKey16) std::memcpy(qKey16, rKey16, static_cast<std::size_t>(count) * 2);
        } else {
            std::int32_t* rKey32 = pKey32 != qKey32 ? qKey32 : table.Resort.data();
            for (std::int32_t index = 0; index < count; ++index) {
                swapper->SwapArray32(swapper, pKey32 + table.Rows[static_cast<std::size_t>(index)].SortIndex, 4, rKey32 + index, status);
            }
            if (qKey32 != rKey32) std::memcpy(qKey32, rKey32, static_cast<std::size_t>(count) * 4);
        }
        Resource* r = p != q ? q : reinterpret_cast<Resource*>(table.Resort.data());
        for (std::int32_t index = 0; index < count; ++index) {
            swapper->SwapArray32(swapper, p + table.Rows[static_cast<std::size_t>(index)].SortIndex, 4, r + index, status);
        }
        if (q != r) std::memcpy(q, r, static_cast<std::size_t>(count) * 4);
        break;
    }
    case Array: {
        const std::int32_t count = ReadInt(swapper, p);
        swapper->SwapArray32(swapper, p++, 4, q++, status);
        for (std::int32_t index = 0; index < count; ++index) {
            const Resource item = ReadUInt(swapper, p + index);
            SwapResource(swapper, inBundle, outBundle, item, nullptr, table, status);
            if (*status > 0) {
                udata_printError_nid_postfix(swapper, "ures_swapResource(array res=%08x)[%d].recurse(%08x) failed\n", resource, index, item);
                return;
            }
        }
        swapper->SwapArray32(swapper, p, 4 * count, q, status);
        break;
    }
    case IntVector: {
        const std::int32_t count = ReadInt(swapper, p);
        swapper->SwapArray32(swapper, p, 4 * (1 + count), q, status);
        break;
    }
    default:
        break;
    }
}

}

extern "C" {

std::int32_t APS5_VABI ures_swap_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    const std::int32_t headerSize = udata_swapDataHeader_nid_postfix(swapper, input, length, output, status);
    if (status == nullptr || *status > 0) return 0;
    const auto* info = static_cast<const std::uint8_t*>(input) + 4;
    const std::uint8_t* format = info + 8;
    if (!(format[0] == 0x52 && format[1] == 0x65 && format[2] == 0x73 && format[3] == 0x42 &&
            ((info[12] == 1 && info[13] >= 1) || info[12] == 2 || info[12] == 3))) {
        udata_printError_nid_postfix(swapper, "ures_swap(): data format %02x.%02x.%02x.%02x (format version %02x.%02x) is not a resource bundle\n", format[0],
            format[1], format[2], format[3], info[12], info[13]);
        *status = Unsupported;
        return 0;
    }
    TempTable table{};
    table.MajorFormatVersion = info[12];
    std::int32_t bundleLength = -1;
    if (length >= 0) {
        bundleLength = (length - headerSize) / 4;
        if (bundleLength < 1 + 5) {
            udata_printError_nid_postfix(swapper, "ures_swap(): too few bytes (%d after header) for a resource bundle\n", length - headerSize);
            *status = IndexOutOfBounds;
            return 0;
        }
    }
    const auto* inBundle = reinterpret_cast<const Resource*>(static_cast<const std::uint8_t*>(input) + headerSize);
    const Resource root = ReadUInt(swapper, inBundle);
    const auto* indexes = inBundle + 1;
    const std::int32_t indexLength = ReadInt(swapper, indexes + IndexLength) & 0xff;
    if (indexLength <= MaxTableLength) {
        udata_printError_nid_postfix(swapper, "ures_swap(): too few indexes for a 1.1+ resource bundle\n");
        *status = IndexOutOfBounds;
        return 0;
    }
    const std::int32_t keysBottom = 1 + indexLength;
    const std::int32_t keysTop = ReadInt(swapper, indexes + KeysTop);
    const std::int32_t resourcesBottom = indexLength > SixteenBitTop ? ReadInt(swapper, indexes + SixteenBitTop) : keysTop;
    const std::int32_t top = ReadInt(swapper, indexes + BundleTop);
    const std::int32_t maxTableLength = ReadInt(swapper, indexes + MaxTableLength);
    if (bundleLength >= 0 && bundleLength < top) {
        udata_printError_nid_postfix(swapper, "ures_swap(): resource top %d exceeds bundle length %d\n", top, bundleLength);
        *status = IndexOutOfBounds;
        return 0;
    }
    table.LocalKeyLimit = keysTop > 1 + indexLength ? keysTop << 2 : 0;
    if (length >= 0) {
        auto* outBundle = reinterpret_cast<Resource*>(static_cast<std::uint8_t*>(output) + headerSize);
        const std::int32_t flagBytes = (((length + 31) >> 5) + 3) & ~3;
        table.Flags.assign(static_cast<std::size_t>(flagBytes / 4), 0);
        if (input != output) std::memcpy(outBundle, inBundle, static_cast<std::size_t>(top) * 4);
        udata_swapInvStringBlock_nid_postfix(swapper, inBundle + keysBottom, 4 * (keysTop - keysBottom), outBundle + keysBottom, status);
        if (*status > 0) {
            udata_printError_nid_postfix(swapper, "ures_swap().udata_swapInvStringBlock(keys[%d]) failed\n", 4 * (keysTop - keysBottom));
            return 0;
        }
        if (keysTop < resourcesBottom) {
            swapper->SwapArray16(swapper, inBundle + keysTop, (resourcesBottom - keysTop) * 4, outBundle + keysTop, status);
            if (*status > 0) {
                udata_printError_nid_postfix(swapper, "ures_swap().swapArray16(16-bit units[%d]) failed\n", 2 * (resourcesBottom - keysTop));
                return 0;
            }
        }
        table.KeyChars = reinterpret_cast<const char*>(outBundle);
        const std::int32_t rows = table.MajorFormatVersion > 1 || maxTableLength <= RowCapacity ? RowCapacity : maxTableLength;
        table.Rows.resize(static_cast<std::size_t>(rows));
        table.Resort.resize(static_cast<std::size_t>(rows));
        SwapResource(swapper, inBundle, outBundle, root, nullptr, table, status);
        if (*status > 0) udata_printError_nid_postfix(swapper, "ures_swapResource(root res=%08x) failed\n", root);
        swapper->SwapArray32(swapper, inBundle, keysBottom * 4, outBundle, status);
    }
    return headerSize + 4 * top;
}

}
