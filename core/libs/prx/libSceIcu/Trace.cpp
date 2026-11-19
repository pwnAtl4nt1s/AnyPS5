// Derived from icu4c/source/common/utrace.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utrace.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2014, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/Trace.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::int32_t LevelOff = -1;
constexpr std::int32_t LevelError = 0;
constexpr std::int32_t LevelVerbose = 9;
constexpr std::int32_t ExitInt32 = 1;
constexpr std::int32_t ExitPointer = 2;
constexpr std::int32_t ExitStatus = 0x10;

std::atomic<TraceEntry> entryFunction{nullptr};
std::atomic<TraceExit> exitFunction{nullptr};
std::atomic<TraceData> dataFunction{nullptr};
std::atomic<const void*> traceContext{nullptr};
std::atomic<std::int32_t> traceLevel{LevelError};

constexpr std::array<const char*, 2> FunctionNames{"u_init", "u_cleanup"};
constexpr std::array<const char*, 8> ConverterNames{"ucnv_open", "ucnv_openPackage", "ucnv_openAlgorithmic", "ucnv_clone", "ucnv_close", "ucnv_flushCache",
    "ucnv_load", "ucnv_unload"};
constexpr std::array<const char*, 8> CollatorNames{"ucol_open", "ucol_close", "ucol_strcoll", "ucol_getSortKey", "ucol_getLocale", "ucol_nextSortKeyPart",
    "ucol_strcollIter", "ucol_openFromShortString"};
constexpr std::array<const char*, 4> ResourceNames{"resc", "bundle-open", "file-open", "res-open"};

std::uint64_t NextArgument(VaList* arguments) {
    std::uint64_t value;
    if (arguments->gp_offset < 48) {
        std::memcpy(&value, static_cast<const std::uint8_t*>(arguments->reg_save_area) + arguments->gp_offset, sizeof(value));
        arguments->gp_offset += 8;
    } else {
        std::memcpy(&value, arguments->overflow_arg_area, sizeof(value));
        arguments->overflow_arg_area = static_cast<std::uint8_t*>(arguments->overflow_arg_area) + 8;
    }
    return value;
}

std::int32_t NextInt(VaList* arguments) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(NextArgument(arguments)));
}

const void* NextPointer(VaList* arguments) {
    return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(NextArgument(arguments)));
}

struct Output {
    char* Buffer;
    std::int32_t Index;
    std::int32_t Capacity;

    void Put(const char c, const std::int32_t indent) {
        if (Index == 0 || (c != '\n' && c != 0 && Index < Capacity && Buffer[Index - 1] == '\n') || (c == '\n' && Index >= Capacity)) {
            for (std::int32_t count = 0; count < indent; ++count) {
                if (Index < Capacity) Buffer[Index] = ' ';
                ++Index;
            }
        }
        if (Index < Capacity) Buffer[Index] = c;
        if (c != 0) ++Index;
    }

    void Hex(const std::int64_t value, const std::int32_t digits) {
        for (std::int32_t shift = (digits - 1) * 4; shift >= 0; shift -= 4) Put("0123456789abcdef"[(value >> shift) & 0xf], 0);
    }

    void Pointer(const void* value) {
        const auto bits = reinterpret_cast<std::uintptr_t>(value);
        for (std::int32_t byte = static_cast<std::int32_t>(sizeof(void*)) - 1; byte >= 0; --byte) {
            Hex(static_cast<std::int8_t>(static_cast<std::uint8_t>(bits >> (byte * 8))), 2);
        }
    }

    void String(const char* text, const std::int32_t indent) {
        if (text == nullptr) text = "*NULL*";
        std::int32_t index = 0;
        char c;
        do {
            c = text[index++];
            Put(c, indent);
        } while (c != 0);
    }

    void Units(const char16_t* text, const std::int32_t length, const std::int32_t indent) {
        if (text == nullptr) {
            String(nullptr, indent);
            return;
        }
        for (std::int32_t index = 0; index < length || length == -1; ++index) {
            const char16_t c = text[index];
            Hex(c, 4);
            Put(' ', indent);
            if (length == -1 && c == 0) break;
        }
    }
};

void Vector(Output& out, const char type, const void* data, const std::int32_t length, const std::int32_t indent) {
    if (data == nullptr) {
        out.String("*NULL* ", indent);
    } else {
        const auto* bytes = static_cast<const char*>(data);
        const auto* halves = static_cast<const std::int16_t*>(data);
        const auto* words = static_cast<const std::int32_t*>(data);
        const auto* longs = static_cast<const std::int64_t*>(data);
        const auto* pointers = static_cast<const void* const*>(data);
        std::int64_t value = 0;
        for (std::int32_t index = 0; index < length || length == -1; ++index) {
            std::int32_t digits = 0;
            switch (type) {
            case 'b':
                digits = 2;
                value = *bytes++;
                break;
            case 'h':
                digits = 4;
                value = *halves++;
                break;
            case 'd':
                digits = 8;
                value = *words++;
                break;
            case 'l':
                digits = 16;
                value = *longs++;
                break;
            case 'p':
                out.Pointer(*pointers);
                value = *pointers == nullptr ? 0 : 1;
                ++pointers;
                break;
            case 'c':
                out.Put(*bytes, indent);
                value = *bytes;
                ++bytes;
                break;
            case 's':
                out.String(static_cast<const char*>(*pointers), indent);
                out.Put('\n', indent);
                value = *pointers == nullptr ? 0 : 1;
                ++pointers;
                break;
            case 'S':
                out.Units(static_cast<const char16_t*>(*pointers), -1, indent);
                out.Put('\n', indent);
                value = *pointers == nullptr ? 0 : 1;
                ++pointers;
                break;
            default:
                break;
            }
            if (digits > 0) {
                out.Hex(value, digits);
                out.Put(' ', indent);
            }
            if (length == -1 && value == 0) break;
        }
    }
    out.Put('[', indent);
    out.Hex(length, 8);
    out.Put(']', indent);
}

}

extern "C" {

void APS5_VABI utrace_setFunctions_nid_postfix(const void* context, TraceEntry entry, TraceExit exit, TraceData data) {
    entryFunction.store(entry);
    exitFunction.store(exit);
    dataFunction.store(data);
    traceContext.store(context);
}

void APS5_VABI utrace_getFunctions_nid_postfix(const void** context, TraceEntry* entry, TraceExit* exit, TraceData* data) {
    if (context == nullptr || entry == nullptr || exit == nullptr || data == nullptr) APS5_INVALID_ARG_EX;
    *entry = entryFunction.load();
    *exit = exitFunction.load();
    *data = dataFunction.load();
    *context = traceContext.load();
}

void APS5_VABI utrace_setLevel_nid_postfix(std::int32_t level) {
    if (level < LevelOff) level = LevelOff;
    if (level > LevelVerbose) level = LevelVerbose;
    traceLevel.store(level);
}

std::int32_t APS5_VABI utrace_getLevel_nid_postfix() {
    return traceLevel.load();
}

void APS5_VABI utrace_entry_nid_postfix(const std::int32_t function) {
    if (const TraceEntry entry = entryFunction.load()) entry(traceContext.load(), function);
}

void APS5_VABI utrace_exit_nid_postfix(const std::int32_t function, const std::int32_t returnType, ...) {
    const TraceExit exit = exitFunction.load();
    if (exit == nullptr) return;
    const char* format;
    switch (returnType) {
    case 0:
        format = "Returns.";
        break;
    case ExitInt32:
        format = "Returns %d.";
        break;
    case ExitStatus:
        format = "Returns.  Status = %d.";
        break;
    case ExitInt32 | ExitStatus:
        format = "Returns %d.  Status = %d.";
        break;
    case ExitPointer | ExitStatus:
        format = "Returns %d.  Status = %p.";
        break;
    default:
        APS5_INVALID_ARG_EX;
    }
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, returnType);
#else
    std::va_list arguments;
    va_start(arguments, returnType);
#endif
    exit(traceContext.load(), function, format, reinterpret_cast<VaList*>(arguments));
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
}

void APS5_VABI utrace_data_nid_postfix(const std::int32_t function, const std::int32_t level, const char* format, ...) {
    const TraceData data = dataFunction.load();
    if (data == nullptr) return;
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
#else
    std::va_list arguments;
    va_start(arguments, format);
#endif
    data(traceContext.load(), function, level, format, reinterpret_cast<VaList*>(arguments));
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
}

std::int32_t APS5_VABI utrace_vformat_nid_postfix(char* output, const std::int32_t capacity, const std::int32_t indent, const char* format, VaList* arguments) {
    if (format == nullptr || arguments == nullptr) APS5_INVALID_ARG_EX;
    Output out{output, 0, capacity};
    std::int32_t index = 0;
    for (;;) {
        char c = format[index++];
        if (c != '%') {
            out.Put(c, indent);
            if (c == 0) break;
            continue;
        }
        c = format[index++];
        switch (c) {
        case 'c':
            out.Put(static_cast<char>(NextInt(arguments)), indent);
            break;
        case 's':
            out.String(static_cast<const char*>(NextPointer(arguments)), indent);
            break;
        case 'S': {
            const auto* text = static_cast<const char16_t*>(NextPointer(arguments));
            const std::int32_t length = NextInt(arguments);
            out.Units(text, length, indent);
            break;
        }
        case 'b':
            out.Hex(NextInt(arguments), 2);
            break;
        case 'h':
            out.Hex(NextInt(arguments), 4);
            break;
        case 'd':
            out.Hex(NextInt(arguments), 8);
            break;
        case 'l':
            out.Hex(static_cast<std::int64_t>(NextArgument(arguments)), 16);
            break;
        case 'p':
            out.Pointer(NextPointer(arguments));
            break;
        case 0:
            out.Put('%', indent);
            --index;
            break;
        case 'v': {
            const char type = format[index];
            if (type != 0) ++index;
            const void* data = NextPointer(arguments);
            const std::int32_t length = NextInt(arguments);
            Vector(out, type, data, length, indent);
            break;
        }
        default:
            out.Put(c, indent);
            break;
        }
    }
    out.Put(0, indent);
    return out.Index + 1;
}

std::int32_t APS5_VABI utrace_format_nid_postfix(char* output, const std::int32_t capacity, const std::int32_t indent, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
#else
    std::va_list arguments;
    va_start(arguments, format);
#endif
    const std::int32_t result = utrace_vformat_nid_postfix(output, capacity, indent, format, reinterpret_cast<VaList*>(arguments));
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
    return result;
}

const char* APS5_VABI utrace_functionName_nid_postfix(const std::int32_t function) {
    if (function >= 0 && function < 2) return FunctionNames[static_cast<std::size_t>(function)];
    if (function >= 0x1000 && function < 0x1008) return ConverterNames[static_cast<std::size_t>(function - 0x1000)];
    if (function >= 0x2000 && function < 0x2008) return CollatorNames[static_cast<std::size_t>(function - 0x2000)];
    if (function >= 0x3000 && function < 0x3004) return ResourceNames[static_cast<std::size_t>(function - 0x3000)];
    return "[BOGUS Trace Function Number]";
}

}
