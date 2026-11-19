// Derived from icu4c/source/common/udataswp.cpp and icu4c/source/common/uinvchar.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/udataswp.cpp
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uinvchar.cpp
//
// The upstream udataswp.cpp notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.
//
// The upstream uinvchar.cpp notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1999-2010, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/DataSwapper.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocation = 7;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t InvalidCharacter = 10;
constexpr std::int32_t Unsupported = 16;
constexpr std::uint8_t AsciiFamily = 0;
constexpr std::uint8_t EbcdicFamily = 1;
constexpr std::uint8_t HostIsBigEndian = 0;

struct MappedHeader {
    std::uint16_t HeaderSize;
    std::uint8_t Magic1;
    std::uint8_t Magic2;
};

struct DataInfoLayout {
    std::uint16_t Size;
    std::uint16_t ReservedWord;
    std::uint8_t IsBigEndian;
    std::uint8_t CharsetFamily;
    std::uint8_t SizeofUChar;
    std::uint8_t ReservedByte;
    std::uint8_t DataFormat[4];
    std::uint8_t FormatVersion[4];
    std::uint8_t DataVersion[4];
};

struct DataHeader {
    MappedHeader Mapped;
    DataInfoLayout Info;
};

static_assert(sizeof(DataHeader) == 24);

constexpr std::array<std::uint8_t, 256> AsciiFromEbcdic{
    0x00, 0x01, 0x02, 0x03, 0x00, 0x09, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x00, 0x0a, 0x08, 0x00, 0x18, 0x19, 0x00, 0x00, 0x1c, 0x1d, 0x1e, 0x1f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x17, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x06, 0x07,
    0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x14, 0x15, 0x00, 0x1a,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x3c, 0x28, 0x2b, 0x7c,
    0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
    0x2d, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,
    0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00,
    0x5e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x5d, 0x00, 0x5d, 0x00, 0x00,
    0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x5c, 0x00, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr std::array<std::uint8_t, 256> EbcdicFromAscii{
    0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f, 0x16, 0x05, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26, 0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f,
    0x40, 0x00, 0x7f, 0x00, 0x00, 0x6c, 0x50, 0x7d, 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x7a, 0x5e, 0x4c, 0x7e, 0x6e, 0x6f,
    0x00, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
    0xd7, 0xd8, 0xd9, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0x00, 0x00, 0x00, 0x00, 0x6d,
    0x00, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0x00, 0x00, 0x00, 0x00, 0x07};

constexpr std::array<std::uint8_t, 256> LowercaseAsciiFromEbcdic{
    0x00, 0x01, 0x02, 0x03, 0x00, 0x09, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x00, 0x0a, 0x08, 0x00, 0x18, 0x19, 0x00, 0x00, 0x1c, 0x1d, 0x1e, 0x1f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x17, 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x06, 0x07,
    0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x14, 0x15, 0x00, 0x1a,
    0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x3c, 0x28, 0x2b, 0x7c,
    0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
    0x2d, 0x2f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x25, 0x5f, 0x3e, 0x3f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22,
    0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00,
    0x5e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5b, 0x5d, 0x00, 0x5d, 0x00, 0x00,
    0x7b, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7d, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7c, 0x00, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

constexpr std::array<std::uint32_t, 4> InvariantChars{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};

bool IsInvariant(const std::uint32_t unit) {
    return unit <= 0x7f && (InvariantChars[unit >> 5] & (1u << (unit & 0x1f))) != 0;
}

bool Failed(const std::int32_t* status) {
    return status == nullptr || *status > 0;
}

std::uint16_t Swap16(const std::uint16_t value) {
    return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t Swap32(const std::uint32_t value) {
    return (value << 24) | ((value << 8) & 0xff0000) | ((value >> 8) & 0xff00) | (value >> 24);
}

std::uint64_t Swap64(std::uint64_t value) {
    return (value << 56) | ((value & 0xff00) << 40) | ((value & 0xff0000) << 24) | ((value & 0xff000000) << 8) | ((value >> 8) & 0xff000000) |
        ((value >> 24) & 0xff0000) | ((value >> 40) & 0xff00) | (value >> 56);
}

template<class TUnit, TUnit (*Swap)(TUnit)>
std::int32_t APS5_VABI SwapArray(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (swapper == nullptr || input == nullptr || length < 0 || (length & (sizeof(TUnit) - 1)) != 0 || output == nullptr) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* source = static_cast<const std::uint8_t*>(input);
    auto* destination = static_cast<std::uint8_t*>(output);
    for (std::int32_t offset = 0; offset < length; offset += static_cast<std::int32_t>(sizeof(TUnit))) {
        TUnit unit;
        std::memcpy(&unit, source + offset, sizeof(TUnit));
        unit = Swap(unit);
        std::memcpy(destination + offset, &unit, sizeof(TUnit));
    }
    return length;
}

template<class TUnit>
std::int32_t APS5_VABI CopyArray(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (swapper == nullptr || input == nullptr || length < 0 || (length & (sizeof(TUnit) - 1)) != 0 || output == nullptr) {
        *status = IllegalArgument;
        return 0;
    }
    if (length > 0 && input != output) std::memmove(output, input, static_cast<std::size_t>(length));
    return length;
}

std::uint16_t APS5_VABI ReadSwapUInt16(const std::uint16_t value) {
    return Swap16(value);
}

std::uint16_t APS5_VABI ReadDirectUInt16(const std::uint16_t value) {
    return value;
}

std::uint32_t APS5_VABI ReadSwapUInt32(const std::uint32_t value) {
    return Swap32(value);
}

std::uint32_t APS5_VABI ReadDirectUInt32(const std::uint32_t value) {
    return value;
}

void APS5_VABI WriteSwapUInt16(std::uint16_t* pointer, const std::uint16_t value) {
    *pointer = Swap16(value);
}

void APS5_VABI WriteDirectUInt16(std::uint16_t* pointer, const std::uint16_t value) {
    *pointer = value;
}

void APS5_VABI WriteSwapUInt32(std::uint32_t* pointer, const std::uint32_t value) {
    *pointer = Swap32(value);
}

void APS5_VABI WriteDirectUInt32(std::uint32_t* pointer, const std::uint32_t value) {
    *pointer = value;
}

bool CheckCharacters(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Failed(status)) return false;
    if (swapper == nullptr || input == nullptr || length < 0 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return false;
    }
    return true;
}

std::int32_t AsciiFromEbcdicUnit(const std::uint8_t unit) {
    if (unit == 0) return 0;
    const std::uint8_t ascii = AsciiFromEbcdic[unit];
    return ascii != 0 && IsInvariant(ascii) ? ascii : -1;
}

std::int32_t APS5_VABI CopyAscii(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (!CheckCharacters(swapper, input, length, output, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(input);
    for (std::int32_t index = 0; index < length; ++index) {
        if (!IsInvariant(source[index])) {
            udata_printError_nid_postfix(swapper, "uprv_copyFromAscii() string[%d] contains a variant character in position %d\n", length, index);
            *status = InvalidCharacter;
            return 0;
        }
    }
    if (length > 0 && input != output) std::memmove(output, input, static_cast<std::size_t>(length));
    return length;
}

std::int32_t APS5_VABI AsciiFromEbcdicChars(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (!CheckCharacters(swapper, input, length, output, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(input);
    auto* destination = static_cast<std::uint8_t*>(output);
    for (std::int32_t index = 0; index < length; ++index) {
        const std::int32_t ascii = AsciiFromEbcdicUnit(source[index]);
        if (ascii < 0) {
            udata_printError_nid_postfix(swapper, "uprv_asciiFromEbcdic() string[%d] contains a variant character in position %d\n", length, index);
            *status = InvalidCharacter;
            return 0;
        }
        destination[index] = static_cast<std::uint8_t>(ascii);
    }
    return length;
}

std::int32_t APS5_VABI CopyEbcdic(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (!CheckCharacters(swapper, input, length, output, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(input);
    for (std::int32_t index = 0; index < length; ++index) {
        if (AsciiFromEbcdicUnit(source[index]) < 0) {
            udata_printError_nid_postfix(swapper, "uprv_copyEbcdic() string[%] contains a variant character in position %d\n", length, index);
            *status = InvalidCharacter;
            return 0;
        }
    }
    if (length > 0 && input != output) std::memmove(output, input, static_cast<std::size_t>(length));
    return length;
}

std::int32_t Utf16Length(const char16_t* text) {
    std::int32_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}

template<bool Ebcdic>
std::int32_t APS5_VABI CompareInvariant(const DataSwapper*, const char* outString, std::int32_t outLength, const char16_t* localString, std::int32_t localLength) {
    if (outString == nullptr || outLength < -1 || localString == nullptr || localLength < -1) return 0;
    if (outLength < 0) outLength = static_cast<std::int32_t>(std::strlen(outString));
    if (localLength < 0) localLength = Utf16Length(localString);
    const std::int32_t minimum = outLength < localLength ? outLength : localLength;
    for (std::int32_t index = 0; index < minimum; ++index) {
        const auto unit = static_cast<std::uint8_t>(outString[index]);
        std::int32_t left;
        if constexpr (Ebcdic) {
            left = AsciiFromEbcdicUnit(unit);
        } else {
            left = IsInvariant(unit) ? unit : -1;
        }
        const std::int32_t right = IsInvariant(localString[index]) ? static_cast<std::int32_t>(localString[index]) : -2;
        if (left != right) return left - right;
    }
    return outLength - localLength;
}

bool LooksLikeData(const DataHeader* header, const std::int32_t length) {
    return !((length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) || header->Mapped.Magic1 != 0xda || header->Mapped.Magic2 != 0x27 ||
        header->Info.SizeofUChar != 2);
}

bool SizesMismatch(const std::uint16_t headerSize, const std::uint16_t infoSize, const std::int32_t length) {
    return headerSize < sizeof(DataHeader) || infoSize < sizeof(DataInfoLayout) || headerSize < sizeof(MappedHeader) + infoSize ||
        (length >= 0 && length < headerSize);
}

}

extern "C" {

std::int16_t APS5_VABI udata_readInt16_nid_postfix(const DataSwapper* swapper, const std::int16_t value) {
    return static_cast<std::int16_t>(swapper->ReadUInt16(static_cast<std::uint16_t>(value)));
}

std::int32_t APS5_VABI udata_readInt32_nid_postfix(const DataSwapper* swapper, const std::int32_t value) {
    return static_cast<std::int32_t>(swapper->ReadUInt32(static_cast<std::uint32_t>(value)));
}

void APS5_VABI udata_printError_nid_postfix(const DataSwapper* swapper, const char* format, ...) {
    if (swapper->PrintError == nullptr) return;
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
#else
    std::va_list arguments;
    va_start(arguments, format);
#endif
    swapper->PrintError(swapper->PrintErrorContext, format, reinterpret_cast<VaList*>(arguments));
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
}

std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (swapper == nullptr || input == nullptr || length < 0 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* characters = static_cast<const char*>(input);
    std::int32_t stringsLength = length;
    while (stringsLength > 0 && characters[stringsLength - 1] != 0) --stringsLength;
    swapper->SwapInvChars(swapper, input, stringsLength, output, status);
    if (input != output && length > stringsLength) {
        std::memcpy(static_cast<char*>(output) + stringsLength, characters + stringsLength, static_cast<std::size_t>(length - stringsLength));
    }
    return *status <= 0 ? length : 0;
}

std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (swapper == nullptr || input == nullptr || length < -1 || (length > 0 && output == nullptr)) {
        *status = IllegalArgument;
        return 0;
    }
    const auto* header = static_cast<const DataHeader*>(input);
    if (!LooksLikeData(header, length)) {
        udata_printError_nid_postfix(swapper, "udata_swapDataHeader(): initial bytes do not look like ICU data\n");
        *status = Unsupported;
        return 0;
    }
    const std::uint16_t headerSize = swapper->ReadUInt16(header->Mapped.HeaderSize);
    std::uint16_t infoSize = swapper->ReadUInt16(header->Info.Size);
    if (SizesMismatch(headerSize, infoSize, length)) {
        udata_printError_nid_postfix(swapper, "udata_swapDataHeader(): header size mismatch - headerSize %d infoSize %d length %d\n", headerSize, infoSize, length);
        *status = IndexOutOfBounds;
        return 0;
    }
    if (length > 0) {
        if (input != output) std::memcpy(output, input, headerSize);
        auto* outHeader = static_cast<DataHeader*>(output);
        outHeader->Info.IsBigEndian = swapper->OutIsBigEndian;
        outHeader->Info.CharsetFamily = swapper->OutCharset;
        swapper->SwapArray16(swapper, &header->Mapped.HeaderSize, 2, &outHeader->Mapped.HeaderSize, status);
        swapper->SwapArray16(swapper, &header->Info.Size, 4, &outHeader->Info.Size, status);
        infoSize = static_cast<std::uint16_t>(infoSize + sizeof(MappedHeader));
        const char* copyright = static_cast<const char*>(input) + infoSize;
        const std::int32_t maximum = headerSize - infoSize;
        std::int32_t copyrightLength = 0;
        while (copyrightLength < maximum && copyright[copyrightLength] != 0) ++copyrightLength;
        swapper->SwapInvChars(swapper, copyright, copyrightLength, static_cast<char*>(output) + infoSize, status);
    }
    return headerSize;
}

DataSwapper* APS5_VABI udata_openSwapper_nid_postfix(const std::uint8_t inIsBigEndian, const std::uint8_t inCharset, const std::uint8_t outIsBigEndian,
    const std::uint8_t outCharset, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (inCharset > EbcdicFamily || outCharset > EbcdicFamily) {
        *status = IllegalArgument;
        return nullptr;
    }
    auto* swapper = static_cast<DataSwapper*>(uprv_malloc_nid_postfix(sizeof(DataSwapper)));
    if (swapper == nullptr) {
        *status = MemoryAllocation;
        return nullptr;
    }
    std::memset(swapper, 0, sizeof(DataSwapper));
    swapper->InIsBigEndian = inIsBigEndian;
    swapper->InCharset = inCharset;
    swapper->OutIsBigEndian = outIsBigEndian;
    swapper->OutCharset = outCharset;
    swapper->ReadUInt16 = inIsBigEndian == HostIsBigEndian ? ReadDirectUInt16 : ReadSwapUInt16;
    swapper->ReadUInt32 = inIsBigEndian == HostIsBigEndian ? ReadDirectUInt32 : ReadSwapUInt32;
    swapper->WriteUInt16 = outIsBigEndian == HostIsBigEndian ? WriteDirectUInt16 : WriteSwapUInt16;
    swapper->WriteUInt32 = outIsBigEndian == HostIsBigEndian ? WriteDirectUInt32 : WriteSwapUInt32;
    swapper->CompareInvChars = outCharset == AsciiFamily ? CompareInvariant<false> : CompareInvariant<true>;
    if (inIsBigEndian == outIsBigEndian) {
        swapper->SwapArray16 = CopyArray<std::uint16_t>;
        swapper->SwapArray32 = CopyArray<std::uint32_t>;
        swapper->SwapArray64 = CopyArray<std::uint64_t>;
    } else {
        swapper->SwapArray16 = SwapArray<std::uint16_t, Swap16>;
        swapper->SwapArray32 = SwapArray<std::uint32_t, Swap32>;
        swapper->SwapArray64 = SwapArray<std::uint64_t, Swap64>;
    }
    if (inCharset == AsciiFamily) {
        swapper->SwapInvChars = outCharset == AsciiFamily ? CopyAscii : uprv_ebcdicFromAscii_nid_postfix;
    } else {
        swapper->SwapInvChars = outCharset == EbcdicFamily ? CopyEbcdic : AsciiFromEbcdicChars;
    }
    return swapper;
}

DataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, const std::int32_t length, const std::uint8_t outIsBigEndian,
    const std::uint8_t outCharset, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    if (data == nullptr || (length >= 0 && length < static_cast<std::int32_t>(sizeof(DataHeader))) || outCharset > EbcdicFamily) {
        *status = IllegalArgument;
        return nullptr;
    }
    const auto* header = static_cast<const DataHeader*>(data);
    if (!LooksLikeData(header, length)) {
        *status = Unsupported;
        return nullptr;
    }
    const std::uint8_t inIsBigEndian = header->Info.IsBigEndian;
    const auto inCharset = static_cast<std::int8_t>(header->Info.CharsetFamily);
    const bool direct = inIsBigEndian == HostIsBigEndian;
    const std::uint16_t headerSize = direct ? header->Mapped.HeaderSize : Swap16(header->Mapped.HeaderSize);
    const std::uint16_t infoSize = direct ? header->Info.Size : Swap16(header->Info.Size);
    if (SizesMismatch(headerSize, infoSize, length)) {
        *status = Unsupported;
        return nullptr;
    }
    return udata_openSwapper_nid_postfix(inIsBigEndian, static_cast<std::uint8_t>(inCharset), outIsBigEndian, outCharset, status);
}

void APS5_VABI udata_closeSwapper_nid_postfix(DataSwapper* swapper) {
    IcuReleaseMemory_nid_no_patch(swapper);
}

std::int32_t APS5_VABI uprv_ebcdicFromAscii_nid_postfix(const DataSwapper* swapper, const void* input, const std::int32_t length, void* output, std::int32_t* status) {
    if (!CheckCharacters(swapper, input, length, output, status)) return 0;
    const auto* source = static_cast<const std::uint8_t*>(input);
    auto* destination = static_cast<std::uint8_t*>(output);
    for (std::int32_t index = 0; index < length; ++index) {
        const std::uint8_t unit = source[index];
        if (!IsInvariant(unit)) {
            udata_printError_nid_postfix(swapper, "uprv_ebcdicFromAscii() string[%d] contains a variant character in position %d\n", length, index);
            *status = InvalidCharacter;
            return 0;
        }
        destination[index] = EbcdicFromAscii[unit];
    }
    return length;
}

std::uint8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* text, std::int32_t length) {
    for (;;) {
        std::uint8_t unit;
        if (length < 0) {
            unit = static_cast<std::uint8_t>(*text++);
            if (unit == 0) break;
        } else {
            if (length == 0) break;
            --length;
            unit = static_cast<std::uint8_t>(*text++);
            if (unit == 0) continue;
        }
        if (!IsInvariant(unit)) return 0;
    }
    return 1;
}

std::uint8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* text, std::int32_t length) {
    for (;;) {
        char16_t unit;
        if (length < 0) {
            unit = *text++;
            if (unit == 0) break;
        } else {
            if (length == 0) break;
            --length;
            unit = *text++;
        }
        if (!IsInvariant(unit)) return 0;
    }
    return 1;
}

std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char* left, const char* right) {
    for (;; ++left, ++right) {
        std::int32_t first = static_cast<std::uint8_t>(*left);
        std::int32_t second = static_cast<std::uint8_t>(*right);
        if (first != second) {
            if (first != 0 && ((first = AsciiFromEbcdic[static_cast<std::size_t>(first)]) == 0 || !IsInvariant(static_cast<std::uint32_t>(first)))) {
                first = -static_cast<std::int32_t>(static_cast<std::uint8_t>(*left));
            }
            if (second != 0 && ((second = AsciiFromEbcdic[static_cast<std::size_t>(second)]) == 0 || !IsInvariant(static_cast<std::uint32_t>(second)))) {
                second = -static_cast<std::int32_t>(static_cast<std::uint8_t>(*right));
            }
            return first - second;
        }
        if (first == 0) return 0;
    }
}

char APS5_VABI uprv_ebcdicToAscii_nid_postfix(const char value) {
    return static_cast<char>(AsciiFromEbcdic[static_cast<std::uint8_t>(value)]);
}

char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(const char value) {
    return static_cast<char>(LowercaseAsciiFromEbcdic[static_cast<std::uint8_t>(value)]);
}

std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t length) {
    std::uint8_t* original = destination;
    if (length == -1) length = static_cast<std::int32_t>(std::strlen(reinterpret_cast<const char*>(source)) + 1);
    while (*source != 0 && length > 0) {
        *destination++ = AsciiFromEbcdic[*source++];
        --length;
    }
    while (length > 0) {
        *destination++ = 0;
        --length;
    }
    return original;
}

std::uint8_t* APS5_VABI uprv_eastrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t length) {
    std::uint8_t* original = destination;
    if (length == -1) length = static_cast<std::int32_t>(std::strlen(reinterpret_cast<const char*>(source)) + 1);
    while (*source != 0 && length > 0) {
        std::uint8_t unit = EbcdicFromAscii[*source++];
        if (unit == 0) unit = EbcdicFromAscii[0x3f];
        *destination++ = unit;
        --length;
    }
    while (length > 0) {
        *destination++ = 0;
        --length;
    }
    return original;
}

}
