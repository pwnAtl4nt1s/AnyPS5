// Derived from icu4c/source/common/utext.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utext.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2005-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/utf16.h"
#include "unicode/utf8.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#undef utext_setup
#undef utext_close
#undef utext_openUChars

namespace {

struct GuestText;

using CloneFunction = GuestText* (APS5_VABI*)(GuestText*, const GuestText*, std::int8_t, std::int32_t*);
using LengthFunction = std::int64_t (APS5_VABI*)(GuestText*);
using AccessFunction = std::int8_t (APS5_VABI*)(GuestText*, std::int64_t, std::int8_t);
using ExtractFunction = std::int32_t (APS5_VABI*)(GuestText*, std::int64_t, std::int64_t, char16_t*, std::int32_t, std::int32_t*);
using ReplaceFunction = std::int32_t (APS5_VABI*)(GuestText*, std::int64_t, std::int64_t, const char16_t*, std::int32_t, std::int32_t*);
using CopyFunction = void (APS5_VABI*)(GuestText*, std::int64_t, std::int64_t, std::int64_t, std::int8_t, std::int32_t*);
using MapOffsetFunction = std::int64_t (APS5_VABI*)(const GuestText*);
using MapIndexFunction = std::int32_t (APS5_VABI*)(const GuestText*, std::int64_t);
using CloseFunction = void (APS5_VABI*)(GuestText*);

struct GuestFunctions {
    std::int32_t tableSize;
    std::int32_t reserved1;
    std::int32_t reserved2;
    std::int32_t reserved3;
    CloneFunction clone;
    LengthFunction nativeLength;
    AccessFunction access;
    ExtractFunction extract;
    ReplaceFunction replace;
    CopyFunction copy;
    MapOffsetFunction mapOffsetToNative;
    MapIndexFunction mapNativeIndexToUTF16;
    CloseFunction close;
    void* spare1;
    void* spare2;
    void* spare3;
};

struct GuestText {
    std::uint32_t magic;
    std::int32_t flags;
    std::int32_t providerProperties;
    std::int32_t sizeOfStruct;
    std::int64_t chunkNativeLimit;
    std::int32_t extraSize;
    std::int32_t nativeIndexingLimit;
    std::int64_t chunkNativeStart;
    std::int32_t chunkOffset;
    std::int32_t chunkLength;
    const char16_t* chunkContents;
    const GuestFunctions* pFuncs;
    void* pExtra;
    const void* context;
    const void* p;
    const void* q;
    const void* r;
    void* privP;
    std::int64_t a;
    std::int32_t b;
    std::int32_t c;
    std::int64_t privA;
    std::int32_t privB;
    std::int32_t privC;
};

static_assert(sizeof(GuestFunctions) == 112);
static_assert(offsetof(GuestFunctions, close) == 80);
static_assert(sizeof(GuestText) == 144);
static_assert(offsetof(GuestText, pFuncs) == 56 && offsetof(GuestText, pExtra) == 64 && offsetof(GuestText, a) == 112 && offsetof(GuestText, privC) == 140);

constexpr std::uint32_t Magic = 0x345ad82c;
constexpr std::int32_t HeapAllocated = 1;
constexpr std::int32_t ExtraHeapAllocated = 2;
constexpr std::int32_t Open = 4;
constexpr std::int32_t LengthIsExpensive = 1 << 1;
constexpr std::int32_t StableChunks = 1 << 2;
constexpr std::int32_t Writable = 1 << 3;
constexpr std::int32_t HasMetaData = 1 << 4;
constexpr std::int32_t OwnsText = 1 << 5;
constexpr std::int32_t Sentinel = -1;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t IndexOutOfBounds = 8;
constexpr std::int32_t NoWritePermission = 30;
constexpr std::int32_t ChunkSize = 32;
constexpr std::size_t ExtensionOffset = 144;

struct Utf8Buffer {
    std::int32_t bufNativeStart;
    std::int32_t bufNativeLimit;
    std::int32_t bufStartIdx;
    std::int32_t bufLimitIdx;
    std::int32_t bufNILimit;
    std::int32_t toUCharsMapStart;
    char16_t buf[ChunkSize + 4];
    std::uint8_t mapToNative[ChunkSize + 4];
    std::uint8_t mapToUChars[ChunkSize * 3 + 6];
    std::int32_t align;
};

static_assert(sizeof(Utf8Buffer) == 240);

GuestText* Text(UText* text) {
    return reinterpret_cast<GuestText*>(text);
}

const GuestText* Text(const UText* text) {
    return reinterpret_cast<const GuestText*>(text);
}

UText* Public(GuestText* text) {
    return reinterpret_cast<UText*>(text);
}

GuestText* Required(UText* text) {
    if (!text) APS5_INVALID_ARG_EX;
    return Text(text);
}

const GuestText* Required(const UText* text) {
    if (!text) APS5_INVALID_ARG_EX;
    return Text(text);
}

std::int32_t* Status(std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    return status;
}

bool Failed(const std::int32_t* status) {
    return *status > 0;
}

bool Access(GuestText* text, const std::int64_t index, const bool forward) {
    return text->pFuncs->access(text, index, forward ? 1 : 0) != 0;
}

std::int64_t NativeIndex(const GuestText* text) {
    if (text->chunkOffset <= text->nativeIndexingLimit) return text->chunkNativeStart + text->chunkOffset;
    return text->pFuncs->mapOffsetToNative(text);
}

std::int32_t PinIndex(std::int64_t& index, const std::int64_t limit) {
    if (index < 0)
        index = 0;
    else if (index > limit)
        index = limit;
    return static_cast<std::int32_t>(index);
}

GuestText* Setup(GuestText* text, std::int32_t extraSpace, std::int32_t* status);
std::int32_t Next(GuestText* text);
std::int32_t Previous(GuestText* text);
void SetNativeIndex(GuestText* text, std::int64_t index);

void AdjustPointer(GuestText* destination, const void** pointer, const GuestText* source) {
    const auto* target = static_cast<const char*>(*pointer);
    const auto* sourceStart = reinterpret_cast<const char*>(source);
    const auto* sourceExtra = static_cast<const char*>(source->pExtra);
    if (target >= sourceExtra && target < sourceExtra + source->extraSize)
        *pointer = static_cast<char*>(destination->pExtra) + (target - sourceExtra);
    else if (target >= sourceStart && target < sourceStart + source->sizeOfStruct)
        *pointer = reinterpret_cast<char*>(destination) + (target - sourceStart);
}

GuestText* ShallowClone(GuestText* destination, const GuestText* source, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    const std::int32_t sourceExtraSize = source->extraSize;
    destination = Setup(destination, sourceExtraSize, status);
    if (Failed(status)) return destination;
    void* destinationExtra = destination->pExtra;
    const std::int32_t flags = destination->flags;
    std::int32_t size = source->sizeOfStruct;
    if (size > destination->sizeOfStruct) size = destination->sizeOfStruct;
    std::memcpy(destination, source, static_cast<std::size_t>(size));
    destination->pExtra = destinationExtra;
    destination->flags = flags;
    if (sourceExtraSize > 0) std::memcpy(destination->pExtra, source->pExtra, static_cast<std::size_t>(sourceExtraSize));
    AdjustPointer(destination, &destination->context, source);
    AdjustPointer(destination, &destination->p, source);
    AdjustPointer(destination, &destination->q, source);
    AdjustPointer(destination, &destination->r, source);
    AdjustPointer(destination, reinterpret_cast<const void**>(&destination->chunkContents), source);
    destination->providerProperties &= ~OwnsText;
    return destination;
}

GuestText* Setup(GuestText* text, const std::int32_t extraSpace, std::int32_t* status) {
    if (Failed(status)) return text;
    if (text == nullptr) {
        std::size_t required = sizeof(GuestText);
        if (extraSpace > 0) required = ExtensionOffset + static_cast<std::size_t>(extraSpace);
        text = static_cast<GuestText*>(uprv_malloc_nid_postfix(required));
        if (text == nullptr) {
            *status = MemoryAllocationError;
            return nullptr;
        }
        std::memset(text, 0, sizeof(GuestText));
        text->magic = Magic;
        text->sizeOfStruct = sizeof(GuestText);
        text->flags |= HeapAllocated;
        text->extraSize = extraSpace;
        text->pExtra = reinterpret_cast<char*>(text) + ExtensionOffset;
    } else {
        if (text->magic != Magic) {
            *status = IllegalArgument;
            return text;
        }
        if ((text->flags & Open) != 0 && text->pFuncs->close != nullptr) text->pFuncs->close(text);
        text->flags &= ~Open;
        if (extraSpace > text->extraSize) {
            if ((text->flags & ExtraHeapAllocated) != 0) {
                IcuReleaseMemory_nid_no_patch(text->pExtra);
                text->extraSize = 0;
            }
            text->pExtra = uprv_malloc_nid_postfix(static_cast<std::size_t>(extraSpace));
            if (text->pExtra == nullptr) {
                *status = MemoryAllocationError;
            } else {
                text->extraSize = extraSpace;
                text->flags |= ExtraHeapAllocated;
            }
        }
    }
    if (!Failed(status)) {
        text->flags |= Open;
        text->context = nullptr;
        text->chunkContents = nullptr;
        text->p = nullptr;
        text->q = nullptr;
        text->r = nullptr;
        text->a = 0;
        text->b = 0;
        text->c = 0;
        text->chunkOffset = 0;
        text->chunkLength = 0;
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = 0;
        text->nativeIndexingLimit = 0;
        text->providerProperties = 0;
        text->privA = 0;
        text->privB = 0;
        text->privC = 0;
        text->privP = nullptr;
        if (text->pExtra != nullptr && text->extraSize > 0) std::memset(text->pExtra, 0, static_cast<std::size_t>(text->extraSize));
    }
    return text;
}

std::int32_t Next(GuestText* text) {
    if (text->chunkOffset >= text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return Sentinel;
    const std::int32_t c = text->chunkContents[text->chunkOffset++];
    if (!U16_IS_LEAD(c)) return c;
    if (text->chunkOffset >= text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return c;
    const std::int32_t trail = text->chunkContents[text->chunkOffset];
    if (!U16_IS_TRAIL(trail)) return c;
    text->chunkOffset++;
    return U16_GET_SUPPLEMENTARY(c, trail);
}

std::int32_t Previous(GuestText* text) {
    if (text->chunkOffset <= 0 && !Access(text, text->chunkNativeStart, false)) return Sentinel;
    text->chunkOffset--;
    const std::int32_t c = text->chunkContents[text->chunkOffset];
    if (!U16_IS_TRAIL(c)) return c;
    if (text->chunkOffset <= 0 && !Access(text, text->chunkNativeStart, false)) return c;
    const std::int32_t lead = text->chunkContents[text->chunkOffset - 1];
    if (!U16_IS_LEAD(lead)) return c;
    text->chunkOffset--;
    return U16_GET_SUPPLEMENTARY(lead, c);
}

void SetNativeIndex(GuestText* text, const std::int64_t index) {
    if (index < text->chunkNativeStart || index >= text->chunkNativeLimit)
        Access(text, index, true);
    else if (static_cast<std::int32_t>(index - text->chunkNativeStart) <= text->nativeIndexingLimit)
        text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    else
        text->chunkOffset = text->pFuncs->mapNativeIndexToUTF16(text, index);
    if (text->chunkOffset < text->chunkLength) {
        const char16_t c = text->chunkContents[text->chunkOffset];
        if (U16_IS_TRAIL(c)) {
            if (text->chunkOffset == 0) Access(text, text->chunkNativeStart, false);
            if (text->chunkOffset > 0) {
                const char16_t lead = text->chunkContents[text->chunkOffset - 1];
                if (U16_IS_LEAD(lead)) text->chunkOffset--;
            }
        }
    }
}

std::int32_t Current(GuestText* text) {
    if (text->chunkOffset == text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return Sentinel;
    const std::int32_t c = text->chunkContents[text->chunkOffset];
    if (!U16_IS_LEAD(c)) return c;
    std::int32_t trail = 0;
    if (text->chunkOffset + 1 < text->chunkLength) {
        trail = text->chunkContents[text->chunkOffset + 1];
    } else {
        const std::int64_t position = text->chunkNativeLimit;
        const std::int32_t original = text->chunkOffset;
        if (Access(text, position, true)) trail = text->chunkContents[text->chunkOffset];
        const bool restored = Access(text, position, false);
        text->chunkOffset = original;
        if (!restored) return Sentinel;
    }
    if (U16_IS_TRAIL(trail)) return U16_GET_SUPPLEMENTARY(c, trail);
    return c;
}

std::int64_t APS5_VABI Utf8Length(GuestText* text) {
    if (text->b < 0) {
        const char* end = static_cast<const char*>(text->context) + text->c;
        while (*end != 0) ++end;
        const auto length = end - static_cast<const char*>(text->context);
        text->b = length < 0x7fffffff ? static_cast<std::int32_t>(length) : 0x7fffffff;
        text->providerProperties &= ~LengthIsExpensive;
    }
    return text->b;
}

void Utf8Swap(GuestText* text) {
    const void* swap = text->q;
    text->q = text->p;
    text->p = swap;
}

void Utf8Use(GuestText* text, Utf8Buffer* buffer) {
    text->chunkContents = &buffer->buf[buffer->bufStartIdx];
    text->chunkLength = buffer->bufLimitIdx - buffer->bufStartIdx;
    text->chunkNativeStart = buffer->bufNativeStart;
    text->chunkNativeLimit = buffer->bufNativeLimit;
    text->nativeIndexingLimit = buffer->bufNILimit;
}

std::int8_t Utf8SwapBuffers(GuestText* text, const std::int32_t index) {
    auto* buffer = static_cast<Utf8Buffer*>(const_cast<void*>(text->q));
    Utf8Swap(text);
    Utf8Use(text, buffer);
    text->chunkOffset = buffer->mapToUChars[index - buffer->toUCharsMapStart] - buffer->bufStartIdx;
    return 1;
}

std::int8_t Utf8SwapBuffersAndFail(GuestText* text, const std::int32_t index) {
    auto* buffer = static_cast<Utf8Buffer*>(const_cast<void*>(text->q));
    Utf8Swap(text);
    Utf8Use(text, buffer);
    text->chunkOffset = index == buffer->bufNativeLimit ? text->chunkLength : 0;
    return 0;
}

std::int8_t Utf8StubBuffer(GuestText* text, const std::int32_t index) {
    auto* buffer = static_cast<Utf8Buffer*>(const_cast<void*>(text->q));
    buffer->bufNativeStart = index;
    buffer->bufNativeLimit = index;
    buffer->bufStartIdx = 0;
    buffer->bufLimitIdx = 0;
    buffer->bufNILimit = 0;
    buffer->toUCharsMapStart = index;
    buffer->mapToNative[0] = 0;
    buffer->mapToUChars[0] = 0;
    return Utf8SwapBuffersAndFail(text, index);
}

std::int8_t Utf8FillForward(GuestText* text, std::int32_t index) {
    const auto* s8 = static_cast<const std::uint8_t*>(text->context);
    U8_SET_CP_START(s8, 0, index);
    Utf8Swap(text);
    auto* buffer = static_cast<Utf8Buffer*>(const_cast<void*>(text->p));
    std::int32_t length = text->b;
    bool terminated = false;
    if (length < 0) {
        length = 0x7fffffff;
        terminated = true;
    }
    char16_t* out = buffer->buf;
    std::uint8_t* toNative = buffer->mapToNative;
    std::uint8_t* toUnits = buffer->mapToUChars;
    std::int32_t destination = 0;
    std::int32_t source = index;
    bool nonAscii = false;
    std::int32_t c = 0;
    while (destination < ChunkSize) {
        c = s8[source];
        if (c > 0 && c < 0x80) {
            out[destination] = static_cast<char16_t>(c);
            toNative[destination] = static_cast<std::uint8_t>(source - index);
            toUnits[source - index] = static_cast<std::uint8_t>(destination);
            ++source;
            ++destination;
        } else {
            if (!nonAscii) {
                nonAscii = true;
                buffer->bufNILimit = destination;
            }
            std::int32_t start = source;
            std::int32_t unit = destination;
            const std::int32_t saved = destination;
            U8_NEXT_OR_FFFD(s8, source, length, c);
            if (c == 0 && terminated) {
                --source;
                break;
            }
            U16_APPEND_UNSAFE(out, destination, c);
            do {
                toNative[unit++] = static_cast<std::uint8_t>(start - index);
            } while (unit < destination);
            do {
                toUnits[start++ - index] = static_cast<std::uint8_t>(saved);
            } while (start < source);
        }
        if (source >= length) break;
    }
    toNative[destination] = static_cast<std::uint8_t>(source - index);
    toUnits[source - index] = static_cast<std::uint8_t>(destination);
    buffer->bufNativeStart = index;
    buffer->bufNativeLimit = source;
    buffer->bufStartIdx = 0;
    buffer->bufLimitIdx = destination;
    if (!nonAscii) buffer->bufNILimit = destination;
    buffer->toUCharsMapStart = buffer->bufNativeStart;
    text->chunkContents = out;
    text->chunkOffset = 0;
    text->chunkLength = buffer->bufLimitIdx;
    text->chunkNativeStart = buffer->bufNativeStart;
    text->chunkNativeLimit = buffer->bufNativeLimit;
    text->nativeIndexingLimit = buffer->bufNILimit;
    if (terminated && source > text->c) {
        text->c = source;
        if (c == 0) {
            text->b = source;
            text->providerProperties &= ~LengthIsExpensive;
        }
    }
    return 1;
}

std::int8_t Utf8FillReverse(GuestText* text, std::int32_t index) {
    const auto* s8 = static_cast<const std::uint8_t*>(text->context);
    if (index != text->b) U8_SET_CP_START(s8, 0, index);
    Utf8Swap(text);
    auto* buffer = static_cast<Utf8Buffer*>(const_cast<void*>(text->p));
    char16_t* out = buffer->buf;
    std::uint8_t* toNative = buffer->mapToNative;
    std::uint8_t* toUnits = buffer->mapToUChars;
    const std::int32_t mapStart = index - static_cast<std::int32_t>(sizeof(Utf8Buffer::mapToUChars)) + 1;
    std::int32_t destination = ChunkSize + 2;
    std::int32_t source = index;
    std::int32_t nativeLimit = destination;
    toNative[destination] = static_cast<std::uint8_t>(source - mapStart);
    toUnits[source - mapStart] = static_cast<std::uint8_t>(destination);
    while (destination > 2 && source - mapStart > 5 && source > 0) {
        --source;
        --destination;
        std::int32_t c = s8[source];
        if (c < 0x80) {
            out[destination] = static_cast<char16_t>(c);
            toUnits[source - mapStart] = static_cast<std::uint8_t>(destination);
            toNative[destination] = static_cast<std::uint8_t>(source - mapStart);
        } else {
            std::int32_t last = source;
            c = utf8_prevCharSafeBody(s8, 0, &source, c, -3);
            if (c < 0x10000) {
                out[destination] = static_cast<char16_t>(c);
                toNative[destination] = static_cast<std::uint8_t>(source - mapStart);
            } else {
                out[destination] = U16_TRAIL(c);
                toNative[destination] = static_cast<std::uint8_t>(source - mapStart);
                out[--destination] = U16_LEAD(c);
                toNative[destination] = static_cast<std::uint8_t>(source - mapStart);
            }
            do {
                toUnits[last-- - mapStart] = static_cast<std::uint8_t>(destination);
            } while (last >= source);
            nativeLimit = destination;
        }
    }
    buffer->bufNativeStart = source;
    buffer->bufNativeLimit = index;
    buffer->bufStartIdx = destination;
    buffer->bufLimitIdx = ChunkSize + 2;
    buffer->bufNILimit = nativeLimit - buffer->bufStartIdx;
    buffer->toUCharsMapStart = mapStart;
    text->chunkContents = &out[buffer->bufStartIdx];
    text->chunkLength = buffer->bufLimitIdx - buffer->bufStartIdx;
    text->chunkOffset = text->chunkLength;
    text->chunkNativeStart = buffer->bufNativeStart;
    text->chunkNativeLimit = buffer->bufNativeLimit;
    text->nativeIndexingLimit = buffer->bufNILimit;
    return 1;
}

std::int8_t APS5_VABI Utf8Access(GuestText* text, const std::int64_t requested, const std::int8_t forward) {
    const auto* s8 = static_cast<const std::uint8_t*>(text->context);
    std::int32_t length = text->b;
    auto index = static_cast<std::int32_t>(requested);
    if (requested < 0)
        index = 0;
    else if (requested > 0x7fffffff)
        index = 0x7fffffff;
    if (index > length) {
        if (length >= 0) {
            index = length;
        } else if (index >= text->c) {
            while (text->c < index && s8[text->c] != 0) text->c++;
            if (s8[text->c] == 0) {
                index = text->c;
                text->b = text->c;
                length = text->c;
                text->providerProperties &= ~LengthIsExpensive;
            }
        }
    }
    if (forward != 0) {
        if (index == text->chunkNativeLimit) {
            if (index == length) {
                text->chunkOffset = text->chunkLength;
                return 0;
            }
            const auto* other = static_cast<const Utf8Buffer*>(text->q);
            if (index >= other->bufNativeStart && index < other->bufNativeLimit) return Utf8SwapBuffers(text, index);
        }
        const auto* other = static_cast<const Utf8Buffer*>(text->q);
        if (index >= other->bufNativeStart && index < other->bufNativeLimit) return Utf8SwapBuffers(text, index);
        if (index == length) {
            if (index == text->chunkNativeLimit) {
                text->chunkOffset = text->chunkLength;
                return 0;
            }
            if (index == other->bufNativeLimit) return Utf8SwapBuffersAndFail(text, index);
            return Utf8StubBuffer(text, index);
        }
        if (index < text->chunkNativeStart || index >= text->chunkNativeLimit) return Utf8FillForward(text, index);
        const auto* current = static_cast<const Utf8Buffer*>(text->p);
        text->chunkOffset = current->mapToUChars[index - current->toUCharsMapStart] - current->bufStartIdx;
        return 1;
    }
    if (index == text->chunkNativeStart) {
        if (index == 0) {
            text->chunkOffset = 0;
            return 0;
        }
        const auto* other = static_cast<const Utf8Buffer*>(text->q);
        if (index > other->bufNativeStart && index <= other->bufNativeLimit) return Utf8SwapBuffers(text, index);
    }
    const auto* other = static_cast<const Utf8Buffer*>(text->q);
    if (index > other->bufNativeStart && index <= other->bufNativeLimit) return Utf8SwapBuffers(text, index);
    if (index == 0) {
        if (other->bufNativeStart == 0) return Utf8SwapBuffersAndFail(text, index);
        return Utf8StubBuffer(text, index);
    }
    if (index <= text->chunkNativeStart || index > text->chunkNativeLimit) return Utf8FillReverse(text, index);
    const auto* current = static_cast<const Utf8Buffer*>(text->p);
    text->chunkOffset = current->mapToUChars[index - current->toUCharsMapStart] - current->bufStartIdx;
    return text->chunkOffset == 0 ? 0 : 1;
}

void FromUtf8(char16_t* destination, const std::int32_t capacity, std::int32_t* destinationLength, const char* source, const std::int32_t sourceLength,
    std::int32_t* status) {
    char16_t* out = destination;
    char16_t* limit = destination != nullptr ? destination + capacity : nullptr;
    std::int32_t c = 0;
    std::int32_t index = 0;
    std::int32_t required = 0;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(source);
    while (index < sourceLength && out < limit) {
        c = bytes[index++];
        if (c <= 0x7f) {
            *out++ = static_cast<char16_t>(c);
        } else {
            c = utf8_nextCharSafeBody(bytes, &index, sourceLength, c, -3);
            if (U_IS_BMP(c)) {
                *out++ = static_cast<char16_t>(c);
            } else {
                *out++ = U16_LEAD(c);
                if (out < limit) {
                    *out++ = U16_TRAIL(c);
                } else {
                    ++required;
                    break;
                }
            }
        }
    }
    while (index < sourceLength) {
        c = bytes[index++];
        if (c <= 0x7f) {
            ++required;
        } else {
            c = utf8_nextCharSafeBody(bytes, &index, sourceLength, c, -3);
            required += U16_LENGTH(c);
        }
    }
    required += static_cast<std::int32_t>(out - destination);
    if (destinationLength) *destinationLength = required;
    u_terminateUChars_nid_postfix(destination, capacity, required, status);
}

std::int32_t APS5_VABI Utf8Extract(GuestText* text, std::int64_t start, std::int64_t limit, char16_t* destination, const std::int32_t capacity, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (capacity < 0 || (destination == nullptr && capacity > 0)) {
        *status = IllegalArgument;
        return 0;
    }
    const std::int32_t length = text->b;
    std::int32_t start32 = PinIndex(start, length);
    std::int32_t limit32 = PinIndex(limit, length);
    if (start32 > limit32) {
        *status = IndexOutOfBounds;
        return 0;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(text->context);
    if (start32 < text->chunkNativeLimit)
        for (int i = 0; i < 3; ++i) {
            if (U8_IS_SINGLE(bytes[start32]) || U8_IS_LEAD(bytes[start32]) || start32 == 0) break;
            --start32;
        }
    if (limit32 < text->chunkNativeLimit)
        for (int i = 0; i < 3; ++i) {
            if (U8_IS_SINGLE(bytes[limit32]) || U8_IS_LEAD(bytes[limit32]) || limit32 == 0) break;
            --limit32;
        }
    std::int32_t destinationLength = 0;
    FromUtf8(destination, capacity, &destinationLength, static_cast<const char*>(text->context) + start32, limit32 - start32, status);
    Utf8Access(text, limit32, 1);
    return destinationLength;
}

std::int64_t APS5_VABI Utf8MapOffsetToNative(const GuestText* text) {
    const auto* buffer = static_cast<const Utf8Buffer*>(text->p);
    return buffer->mapToNative[text->chunkOffset + buffer->bufStartIdx] + buffer->toUCharsMapStart;
}

std::int32_t APS5_VABI Utf8MapIndexToUtf16(const GuestText* text, const std::int64_t index64) {
    const auto index = static_cast<std::int32_t>(index64);
    const auto* buffer = static_cast<const Utf8Buffer*>(text->p);
    return buffer->mapToUChars[index - buffer->toUCharsMapStart] - buffer->bufStartIdx;
}

GuestText* APS5_VABI Utf8Clone(GuestText* destination, const GuestText* source, const std::int8_t deep, std::int32_t* status) {
    destination = ShallowClone(destination, source, status);
    if (deep != 0 && !Failed(status)) {
        const auto length = static_cast<std::int32_t>(source->pFuncs->nativeLength(const_cast<GuestText*>(source)));
        auto* copy = static_cast<char*>(uprv_malloc_nid_postfix(static_cast<std::size_t>(length) + 1));
        if (copy == nullptr) {
            *status = MemoryAllocationError;
        } else {
            std::memcpy(copy, source->context, static_cast<std::size_t>(length) + 1);
            destination->context = copy;
            destination->providerProperties |= OwnsText;
        }
    }
    return destination;
}

void APS5_VABI Utf8Close(GuestText* text) {
    if ((text->providerProperties & OwnsText) != 0) {
        IcuReleaseMemory_nid_no_patch(const_cast<void*>(text->context));
        text->context = nullptr;
    }
}

const GuestFunctions Utf8Functions{sizeof(GuestFunctions), 0, 0, 0, Utf8Clone, Utf8Length, Utf8Access, Utf8Extract, nullptr, nullptr, Utf8MapOffsetToNative,
    Utf8MapIndexToUtf16, Utf8Close, nullptr, nullptr, nullptr};

std::int64_t APS5_VABI UnitsLength(GuestText* text) {
    if (text->a < 0) {
        const auto* string = static_cast<const char16_t*>(text->context);
        while (string[text->chunkNativeLimit] != 0) text->chunkNativeLimit++;
        text->a = text->chunkNativeLimit;
        text->chunkLength = static_cast<std::int32_t>(text->chunkNativeLimit);
        text->nativeIndexingLimit = text->chunkLength;
        text->providerProperties &= ~LengthIsExpensive;
    }
    return text->a;
}

std::int8_t APS5_VABI UnitsAccess(GuestText* text, std::int64_t index, const std::int8_t forward) {
    const auto* string = static_cast<const char16_t*>(text->context);
    if (index < 0) {
        index = 0;
    } else if (index < text->chunkNativeLimit) {
        U16_SET_CP_START(string, 0, index);
    } else if (text->a >= 0) {
        index = text->a;
    } else {
        std::int32_t scanLimit = static_cast<std::int32_t>(index) + 32;
        if (index + 32 > std::numeric_limits<std::int32_t>::max() || index + 32 < 0) scanLimit = std::numeric_limits<std::int32_t>::max();
        auto chunkLimit = static_cast<std::int32_t>(text->chunkNativeLimit);
        bool found = false;
        for (; chunkLimit < scanLimit; ++chunkLimit) {
            if (string[chunkLimit] == 0) {
                text->a = chunkLimit;
                text->chunkLength = chunkLimit;
                text->nativeIndexingLimit = chunkLimit;
                if (index >= chunkLimit)
                    index = chunkLimit;
                else
                    U16_SET_CP_START(string, 0, index);
                text->chunkNativeLimit = chunkLimit;
                text->providerProperties &= ~LengthIsExpensive;
                found = true;
                break;
            }
        }
        if (!found) {
            U16_SET_CP_START(string, 0, index);
            if (chunkLimit == std::numeric_limits<std::int32_t>::max()) {
                text->a = chunkLimit;
                text->chunkLength = chunkLimit;
                text->nativeIndexingLimit = chunkLimit;
                if (index > chunkLimit) index = chunkLimit;
                text->chunkNativeLimit = chunkLimit;
                text->providerProperties &= ~LengthIsExpensive;
            } else {
                if (U16_IS_LEAD(string[chunkLimit - 1])) --chunkLimit;
                text->chunkNativeLimit = chunkLimit;
                text->nativeIndexingLimit = chunkLimit;
                text->chunkLength = chunkLimit;
            }
        }
    }
    text->chunkOffset = static_cast<std::int32_t>(index);
    return ((forward != 0 && index < text->chunkNativeLimit) || (forward == 0 && index > 0)) ? 1 : 0;
}

std::int32_t APS5_VABI UnitsExtract(GuestText* text, const std::int64_t start, std::int64_t limit, char16_t* destination, const std::int32_t capacity, std::int32_t* status) {
    if (Failed(status)) return 0;
    if (capacity < 0 || (destination == nullptr && capacity > 0) || start > limit) {
        *status = IllegalArgument;
        return 0;
    }
    UnitsAccess(text, start, 1);
    const char16_t* string = text->chunkContents;
    const std::int32_t start32 = text->chunkOffset;
    auto length = static_cast<std::int32_t>(text->a);
    std::int32_t limit32 = length >= 0 ? PinIndex(limit, length) : PinIndex(limit, std::numeric_limits<std::int32_t>::max());
    std::int32_t out = 0;
    std::int32_t index = start32;
    for (; index < limit32; ++index) {
        if (length < 0 && string[index] == 0) {
            text->a = index;
            text->chunkNativeLimit = index;
            text->chunkLength = index;
            text->nativeIndexingLimit = index;
            length = index;
            limit32 = index;
            break;
        }
        if (out < capacity) {
            destination[out] = string[index];
        } else if (length >= 0) {
            out = limit32 - start32;
            index = limit32;
            break;
        }
        ++out;
    }
    if (index > 0 && U16_IS_LEAD(string[index - 1]) && ((index < length || length < 0) && U16_IS_TRAIL(string[index]))) {
        if (out < capacity) destination[out++] = string[index];
        ++index;
    }
    if (index <= text->chunkNativeLimit)
        text->chunkOffset = index;
    else
        UnitsAccess(text, index, 1);
    u_terminateUChars_nid_postfix(destination, capacity, out, status);
    return out;
}

GuestText* APS5_VABI UnitsClone(GuestText* destination, const GuestText* source, const std::int8_t deep, std::int32_t* status) {
    destination = ShallowClone(destination, source, status);
    if (deep != 0 && !Failed(status)) {
        const auto length = static_cast<std::int32_t>(destination->pFuncs->nativeLength(destination));
        const auto* string = static_cast<const char16_t*>(source->context);
        auto* copy = static_cast<char16_t*>(uprv_malloc_nid_postfix((static_cast<std::size_t>(length) + 1) * sizeof(char16_t)));
        if (copy == nullptr) {
            *status = MemoryAllocationError;
        } else {
            for (std::int64_t i = 0; i < length; ++i) copy[i] = string[i];
            copy[length] = 0;
            destination->context = copy;
            destination->providerProperties |= OwnsText;
        }
    }
    return destination;
}

void APS5_VABI UnitsClose(GuestText* text) {
    if ((text->providerProperties & OwnsText) != 0) {
        IcuReleaseMemory_nid_no_patch(const_cast<void*>(text->context));
        text->context = nullptr;
    }
}

const GuestFunctions UnitsFunctions{sizeof(GuestFunctions), 0, 0, 0, UnitsClone, UnitsLength, UnitsAccess, UnitsExtract, nullptr, nullptr, nullptr, nullptr, UnitsClose,
    nullptr, nullptr, nullptr};

const char EmptyString[] = {0};
const char16_t EmptyUnits[] = {0};

}

extern "C" {

UText* APS5_VABI utext_setup(UText* ut, const std::int32_t extraSpace, std::int32_t* status) {
    return Public(Setup(Text(ut), extraSpace, Status(status)));
}

UText* APS5_VABI utext_close(UText* ut) {
    GuestText* text = Text(ut);
    if (text == nullptr || text->magic != Magic || (text->flags & Open) == 0) return ut;
    if (text->pFuncs->close != nullptr) text->pFuncs->close(text);
    text->flags &= ~Open;
    if ((text->flags & ExtraHeapAllocated) != 0) {
        IcuReleaseMemory_nid_no_patch(text->pExtra);
        text->pExtra = nullptr;
        text->flags &= ~ExtraHeapAllocated;
        text->extraSize = 0;
    }
    text->pFuncs = nullptr;
    if ((text->flags & HeapAllocated) != 0) {
        text->magic = 0;
        IcuReleaseMemory_nid_no_patch(text);
        return nullptr;
    }
    return ut;
}

UText* APS5_VABI utext_openUChars(UText* ut, const char16_t* s, const std::int64_t length, std::int32_t* status) {
    Status(status);
    if (Failed(status)) return nullptr;
    if (s == nullptr && length == 0) s = EmptyUnits;
    if (s == nullptr || length < -1 || length > std::numeric_limits<std::int32_t>::max()) {
        *status = IllegalArgument;
        return nullptr;
    }
    GuestText* text = Setup(Text(ut), 0, status);
    if (!Failed(status)) {
        text->pFuncs = &UnitsFunctions;
        text->context = s;
        text->providerProperties = StableChunks;
        if (length == -1) text->providerProperties |= LengthIsExpensive;
        text->a = length;
        text->chunkContents = s;
        text->chunkNativeStart = 0;
        text->chunkNativeLimit = length >= 0 ? length : 0;
        text->chunkLength = static_cast<std::int32_t>(text->chunkNativeLimit);
        text->chunkOffset = 0;
        text->nativeIndexingLimit = text->chunkLength;
    }
    return Public(text);
}

UText* APS5_VABI utext_openUTF8_nid_postfix(UText* ut, const char* s, const std::int64_t length, std::int32_t* status) {
    Status(status);
    if (Failed(status)) return nullptr;
    if (s == nullptr && length == 0) s = EmptyString;
    if (s == nullptr || length < -1 || length > std::numeric_limits<std::int32_t>::max()) {
        *status = IllegalArgument;
        return nullptr;
    }
    GuestText* text = Setup(Text(ut), static_cast<std::int32_t>(sizeof(Utf8Buffer) * 2), status);
    if (Failed(status)) return Public(text);
    text->pFuncs = &Utf8Functions;
    text->context = s;
    text->b = static_cast<std::int32_t>(length);
    text->c = static_cast<std::int32_t>(length);
    if (text->c < 0) {
        text->c = 0;
        text->providerProperties |= LengthIsExpensive;
    }
    text->p = text->pExtra;
    text->q = static_cast<char*>(text->pExtra) + sizeof(Utf8Buffer);
    return Public(text);
}

std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(UText* ut, std::int32_t delta) {
    GuestText* text = Required(ut);
    if (delta > 0) {
        do {
            if (text->chunkOffset >= text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return 0;
            const char16_t c = text->chunkContents[text->chunkOffset];
            if (U16_IS_SURROGATE(c)) {
                if (Next(text) == Sentinel) return 0;
            } else {
                text->chunkOffset++;
            }
        } while (--delta > 0);
    } else if (delta < 0) {
        do {
            if (text->chunkOffset <= 0 && !Access(text, text->chunkNativeStart, false)) return 0;
            const char16_t c = text->chunkContents[text->chunkOffset - 1];
            if (U16_IS_SURROGATE(c)) {
                if (Previous(text) == Sentinel) return 0;
            } else {
                text->chunkOffset--;
            }
        } while (++delta < 0);
    }
    return 1;
}

std::int64_t APS5_VABI utext_nativeLength_nid_postfix(UText* ut) {
    GuestText* text = Required(ut);
    return text->pFuncs->nativeLength(text);
}

std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const UText* ut) {
    return (Required(ut)->providerProperties & LengthIsExpensive) != 0 ? 1 : 0;
}

std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const UText* ut) {
    return NativeIndex(Required(ut));
}

void APS5_VABI utext_setNativeIndex_nid_postfix(UText* ut, const std::int64_t index) {
    SetNativeIndex(Required(ut), index);
}

std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(UText* ut) {
    GuestText* text = Required(ut);
    const std::int32_t i = text->chunkOffset - 1;
    if (i >= 0) {
        const char16_t c = text->chunkContents[i];
        if (!U16_IS_TRAIL(c)) {
            if (i <= text->nativeIndexingLimit) return text->chunkNativeStart + i;
            text->chunkOffset = i;
            const std::int64_t result = text->pFuncs->mapOffsetToNative(text);
            text->chunkOffset++;
            return result;
        }
    }
    if (text->chunkOffset == 0 && text->chunkNativeStart == 0) return 0;
    Previous(text);
    const std::int64_t result = NativeIndex(text);
    Next(text);
    return result;
}

std::int32_t APS5_VABI utext_current32_nid_postfix(UText* ut) {
    return Current(Required(ut));
}

std::int32_t APS5_VABI utext_char32At_nid_postfix(UText* ut, const std::int64_t nativeIndex) {
    GuestText* text = Required(ut);
    std::int32_t c = Sentinel;
    if (nativeIndex >= text->chunkNativeStart && nativeIndex < text->chunkNativeStart + text->nativeIndexingLimit) {
        text->chunkOffset = static_cast<std::int32_t>(nativeIndex - text->chunkNativeStart);
        c = text->chunkContents[text->chunkOffset];
        if (!U16_IS_SURROGATE(c)) return c;
    }
    SetNativeIndex(text, nativeIndex);
    if (nativeIndex >= text->chunkNativeStart && text->chunkOffset < text->chunkLength) {
        c = text->chunkContents[text->chunkOffset];
        if (U16_IS_SURROGATE(c)) c = Current(text);
    }
    return c;
}

std::int32_t APS5_VABI utext_next32_nid_postfix(UText* ut) {
    return Next(Required(ut));
}

std::int32_t APS5_VABI utext_previous32_nid_postfix(UText* ut) {
    return Previous(Required(ut));
}

std::int32_t APS5_VABI utext_next32From_nid_postfix(UText* ut, const std::int64_t index) {
    GuestText* text = Required(ut);
    if (index < text->chunkNativeStart || index >= text->chunkNativeLimit) {
        if (!Access(text, index, true)) return Sentinel;
    } else if (index - text->chunkNativeStart <= static_cast<std::int64_t>(text->nativeIndexingLimit)) {
        text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    } else {
        text->chunkOffset = text->pFuncs->mapNativeIndexToUTF16(text, index);
    }
    std::int32_t c = text->chunkContents[text->chunkOffset++];
    if (U16_IS_SURROGATE(c)) {
        SetNativeIndex(text, index);
        c = Next(text);
    }
    return c;
}

std::int32_t APS5_VABI utext_previous32From_nid_postfix(UText* ut, const std::int64_t index) {
    GuestText* text = Required(ut);
    if (index <= text->chunkNativeStart || index > text->chunkNativeLimit) {
        if (!Access(text, index, false)) return Sentinel;
    } else if (index - text->chunkNativeStart <= static_cast<std::int64_t>(text->nativeIndexingLimit)) {
        text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    } else {
        text->chunkOffset = text->pFuncs->mapNativeIndexToUTF16(text, index);
        if (text->chunkOffset == 0 && !Access(text, index, false)) return Sentinel;
    }
    text->chunkOffset--;
    std::int32_t c = text->chunkContents[text->chunkOffset];
    if (U16_IS_SURROGATE(c)) {
        SetNativeIndex(text, index);
        c = Previous(text);
    }
    return c;
}

std::int32_t APS5_VABI utext_extract_nid_postfix(UText* ut, const std::int64_t start, const std::int64_t limit, char16_t* destination, const std::int32_t capacity,
    std::int32_t* status) {
    GuestText* text = Required(ut);
    return text->pFuncs->extract(text, start, limit, destination, capacity, Status(status));
}

std::int8_t APS5_VABI utext_equals_nid_postfix(const UText* first, const UText* second) {
    const GuestText* a = Text(first);
    const GuestText* b = Text(second);
    if (a == nullptr || b == nullptr || a->magic != Magic || b->magic != Magic) return 0;
    if (a->pFuncs != b->pFuncs || a->context != b->context) return 0;
    return NativeIndex(a) == NativeIndex(b) ? 1 : 0;
}

std::int8_t APS5_VABI utext_isWritable_nid_postfix(const UText* ut) {
    return (Required(ut)->providerProperties & Writable) != 0 ? 1 : 0;
}

void APS5_VABI utext_freeze_nid_postfix(UText* ut) {
    Required(ut)->providerProperties &= ~Writable;
}

std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const UText* ut) {
    return (Required(ut)->providerProperties & HasMetaData) != 0 ? 1 : 0;
}

std::int32_t APS5_VABI utext_replace_nid_postfix(UText* ut, const std::int64_t nativeStart, const std::int64_t nativeLimit, const char16_t* replacement,
    const std::int32_t replacementLength, std::int32_t* status) {
    if (Failed(Status(status))) return 0;
    GuestText* text = Required(ut);
    if ((text->providerProperties & Writable) == 0) {
        *status = NoWritePermission;
        return 0;
    }
    return text->pFuncs->replace(text, nativeStart, nativeLimit, replacement, replacementLength, status);
}

void APS5_VABI utext_copy_nid_postfix(UText* ut, const std::int64_t nativeStart, const std::int64_t nativeLimit, const std::int64_t destinationIndex, const std::int8_t move,
    std::int32_t* status) {
    if (Failed(Status(status))) return;
    GuestText* text = Required(ut);
    if ((text->providerProperties & Writable) == 0) {
        *status = NoWritePermission;
        return;
    }
    text->pFuncs->copy(text, nativeStart, nativeLimit, destinationIndex, move, status);
}

UText* APS5_VABI utext_clone_nid_postfix(UText* destination, const UText* source, const std::int8_t deep, const std::int8_t readOnly, std::int32_t* status) {
    if (Failed(Status(status))) return destination;
    const GuestText* text = Required(source);
    GuestText* result = text->pFuncs->clone(Text(destination), text, deep, status);
    if (Failed(status)) return Public(result);
    if (result == nullptr) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    if (readOnly != 0) result->providerProperties &= ~Writable;
    return Public(result);
}

}
