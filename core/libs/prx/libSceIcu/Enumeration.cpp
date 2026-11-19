#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <string>

namespace {

constexpr std::int32_t UnsupportedError = 16;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t BufferPad = 8;

struct StringEnumeration {
    UEnumeration Base;
    std::int32_t Index;
    std::int32_t Count;
};

struct Buffer {
    std::int32_t Capacity;
    char Data[1];
};

void* EnumerationBuffer(UEnumeration* enumeration, std::int32_t capacity) {
    auto* buffer = static_cast<Buffer*>(enumeration->baseContext);
    if (buffer && buffer->Capacity >= capacity) return buffer->Data;
    capacity += BufferPad;
    const auto bytes = sizeof(std::int32_t) + static_cast<std::size_t>(capacity);
    void* grown = buffer ? uprv_realloc_nid_postfix(buffer, bytes) : uprv_malloc_nid_postfix(bytes);
    enumeration->baseContext = grown;
    if (!grown) return nullptr;
    buffer = static_cast<Buffer*>(grown);
    buffer->Capacity = capacity;
    return buffer->Data;
}

void APS5_VABI CloseStrings(UEnumeration* enumeration) {
    IcuReleaseMemory_nid_no_patch(enumeration);
}

std::int32_t APS5_VABI CountStrings(UEnumeration* enumeration, std::int32_t*) {
    return reinterpret_cast<StringEnumeration*>(enumeration)->Count;
}

template<class TUnit>
const TUnit* NextString(UEnumeration* enumeration, std::int32_t* resultLength) {
    auto* strings = reinterpret_cast<StringEnumeration*>(enumeration);
    if (strings->Index >= strings->Count) return nullptr;
    const TUnit* result = static_cast<const TUnit* const*>(strings->Base.context)[strings->Index++];
    if (resultLength) *resultLength = static_cast<std::int32_t>(std::char_traits<TUnit>::length(result));
    return result;
}

const char* APS5_VABI NextCharString(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t*) {
    return NextString<char>(enumeration, resultLength);
}

const char16_t* APS5_VABI NextUnicodeString(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t*) {
    return NextString<char16_t>(enumeration, resultLength);
}

void APS5_VABI ResetStrings(UEnumeration* enumeration, std::int32_t*) {
    reinterpret_cast<StringEnumeration*>(enumeration)->Index = 0;
}

UEnumeration* OpenStrings(const void* strings, std::int32_t count, std::int32_t* status, EnumerationUnicodeNext unicodeNext, EnumerationNext next) {
    if (!status) APS5_INVALID_ARG_EX;
    if (*status > 0 || count < 0 || (count != 0 && strings == nullptr)) return nullptr;
    auto* result = static_cast<StringEnumeration*>(uprv_malloc_nid_postfix(sizeof(StringEnumeration)));
    if (!result) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    result->Base = {nullptr, const_cast<void*>(strings), CloseStrings, CountStrings, unicodeNext, next, ResetStrings};
    result->Index = 0;
    result->Count = count;
    return &result->Base;
}

bool Failed(const UEnumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return true;
    if (!status) APS5_INVALID_ARG_EX;
    return *status > 0;
}

}

extern "C" {

void APS5_VABI uenum_close_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    if (!enumeration->close) {
        IcuReleaseMemory_nid_no_patch(enumeration);
        return;
    }
    if (enumeration->baseContext) IcuReleaseMemory_nid_no_patch(enumeration->baseContext);
    enumeration->close(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = UnsupportedError;
    return -1;
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (enumeration->uNext) return enumeration->uNext(enumeration, resultLength, status);
    *status = UnsupportedError;
    return nullptr;
}

const char* APS5_VABI uenum_next_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (!enumeration->next) {
        *status = UnsupportedError;
        return nullptr;
    }
    std::int32_t ignoredLength = 0;
    return enumeration->next(enumeration, resultLength ? resultLength : &ignoredLength, status);
}

void APS5_VABI uenum_reset_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return;
    if (enumeration->reset)
        enumeration->reset(enumeration, status);
    else
        *status = UnsupportedError;
}

const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (!enumeration || !status) APS5_INVALID_ARG_EX;
    char16_t* result = nullptr;
    std::int32_t length = 0;
    if (enumeration->next) {
        const char* text = enumeration->next(enumeration, &length, status);
        if (text) {
            result = static_cast<char16_t*>(EnumerationBuffer(enumeration, (length + 1) * static_cast<std::int32_t>(sizeof(char16_t))));
            if (!result)
                *status = MemoryAllocationError;
            else
                u_charsToUChars_nid_postfix(text, result, length + 1);
        }
    } else {
        *status = UnsupportedError;
    }
    if (resultLength) *resultLength = length;
    return result;
}

const char* APS5_VABI uenum_nextDefault_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (!enumeration || !status || !resultLength) APS5_INVALID_ARG_EX;
    if (!enumeration->uNext) {
        *status = UnsupportedError;
        return nullptr;
    }
    const char16_t* text = enumeration->uNext(enumeration, resultLength, status);
    if (!text) return nullptr;
    auto* result = static_cast<char*>(EnumerationBuffer(enumeration, *resultLength + 1));
    if (!result) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    u_UCharsToChars_nid_postfix(text, result, *resultLength + 1);
    return result;
}

UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const* strings, std::int32_t count, std::int32_t* status) {
    return OpenStrings(strings, count, status, uenum_unextDefault_nid_postfix, NextCharString);
}

UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const* strings, std::int32_t count, std::int32_t* status) {
    return OpenStrings(strings, count, status, NextUnicodeString, uenum_nextDefault_nid_postfix);
}

}
