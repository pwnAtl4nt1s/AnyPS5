#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>

struct DataSwapper;

using DataSwapFunction = std::int32_t (APS5_VABI*)(const DataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
using DataReadUInt16 = std::uint16_t (APS5_VABI*)(std::uint16_t);
using DataReadUInt32 = std::uint32_t (APS5_VABI*)(std::uint32_t);
using DataWriteUInt16 = void (APS5_VABI*)(std::uint16_t*, std::uint16_t);
using DataWriteUInt32 = void (APS5_VABI*)(std::uint32_t*, std::uint32_t);
using DataCompareInvChars = std::int32_t (APS5_VABI*)(const DataSwapper*, const char*, std::int32_t, const char16_t*, std::int32_t);
using DataPrintError = void (APS5_VABI*)(void*, const char*, VaList*);

struct DataSwapper {
    std::uint8_t InIsBigEndian;
    std::uint8_t InCharset;
    std::uint8_t OutIsBigEndian;
    std::uint8_t OutCharset;
    DataReadUInt16 ReadUInt16;
    DataReadUInt32 ReadUInt32;
    DataCompareInvChars CompareInvChars;
    DataWriteUInt16 WriteUInt16;
    DataWriteUInt32 WriteUInt32;
    DataSwapFunction SwapArray16;
    DataSwapFunction SwapArray32;
    DataSwapFunction SwapArray64;
    DataSwapFunction SwapInvChars;
    DataPrintError PrintError;
    void* PrintErrorContext;
};

static_assert(sizeof(DataSwapper) == 96);

extern "C" {
DataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::uint8_t inIsBigEndian, std::uint8_t inCharset, std::uint8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status);
DataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length, std::uint8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status);
void APS5_VABI udata_closeSwapper_nid_postfix(DataSwapper* swapper);
std::int16_t APS5_VABI udata_readInt16_nid_postfix(const DataSwapper* swapper, std::int16_t value);
std::int32_t APS5_VABI udata_readInt32_nid_postfix(const DataSwapper* swapper, std::int32_t value);
std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
void APS5_VABI udata_printError_nid_postfix(const DataSwapper* swapper, const char* format, ...);
std::int32_t APS5_VABI uprv_ebcdicFromAscii_nid_postfix(const DataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
std::uint8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* text, std::int32_t length);
std::uint8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* text, std::int32_t length);
std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char* left, const char* right);
char APS5_VABI uprv_ebcdicToAscii_nid_postfix(char value);
char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(char value);
std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t length);
std::uint8_t* APS5_VABI uprv_eastrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t length);
}
