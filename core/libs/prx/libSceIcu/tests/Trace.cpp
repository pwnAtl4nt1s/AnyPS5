#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Trace.hpp"
#include "unicode/utrace.h"
#include "utracimp.h"
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU trace check failed at line %u\n", location.line());
    std::abort();
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

std::int32_t APS5_VABI GuestVFormat(char* output, std::int32_t capacity, std::int32_t indent, const char* format, ...) {
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

template<class... Arguments>
void Compare(const char* format, Arguments... arguments) {
    for (const std::int32_t capacity : {0, 1, 5, 17, 40, 400}) {
        for (const std::int32_t indent : {0, 2}) {
            std::array<char, 512> guest{};
            std::array<char, 512> host{};
            std::array<char, 512> viaList{};
            guest.fill('#');
            host.fill('#');
            viaList.fill('#');
            const std::int32_t guestLength = utrace_format_nid_postfix(capacity ? guest.data() : nullptr, capacity, indent, format, arguments...);
            const std::int32_t hostLength = utrace_format(capacity ? host.data() : nullptr, capacity, indent, format, arguments...);
            const std::int32_t listLength = GuestVFormat(capacity ? viaList.data() : nullptr, capacity, indent, format, arguments...);
            Require(guestLength == hostLength && listLength == hostLength);
            Require(guest == host && viaList == host);
        }
    }
}

void Formats() {
    const char16_t units[] = u"Abé";
    const char* strings[] = {"one", "two", nullptr};
    const char16_t* unitStrings[] = {u"x", u"yz", nullptr};
    const std::int8_t bytes[] = {1, -2, 3, 0};
    const std::int16_t halves[] = {0x1234, -1, 0};
    const std::int32_t words[] = {0x12345678, -5, 0};
    const std::int64_t longs[] = {0x123456789abcdefLL, -7, 0};
    const void* pointers[] = {&units, nullptr};
    const char text[] = "hello";
    const char16_t embedded[] = {u'a', 0, u'b', 0};
    Compare("%S", embedded, 3);
    Compare("plain text");
    Compare("");
    Compare("%c%c", 'x', 0x141);
    Compare("%s and %s", "abc", static_cast<const char*>(nullptr));
    Compare("%S|%S|%S", units, 2, units, -1, static_cast<const char16_t*>(nullptr), 3);
    Compare("%b %h %d %l", 0x1ff, 0x12345, -1, static_cast<std::int64_t>(-2));
    Compare("%p %p", static_cast<const void*>(&units), static_cast<const void*>(nullptr));
    Compare("%vb %vh %vd %vl", bytes, 3, halves, -1, words, 2, longs, -1);
    Compare("%vs%vS%vp%vc", strings, -1, unitStrings, 2, pointers, -1, text, -1);
    Compare("%vd", static_cast<const void*>(nullptr), 4);
    Compare("%v", bytes, 2);
    Compare("100%% %q trailing %");
    Compare("line\nnext\n\nlast %s\n", "x\ny");
    Compare("%d %d %d %d %d %d %d %d %d %l %s", 1, 2, 3, 4, 5, 6, 7, 8, 9, static_cast<std::int64_t>(10), "eleven");
}

std::string guestTrace;
std::string hostTrace;
const int contextValue = 7;

void APS5_VABI GuestEntry(const void* context, std::int32_t function) {
    Require(context == &contextValue);
    guestTrace += "entry " + std::to_string(function) + ";";
}

void APS5_VABI GuestExit(const void* context, std::int32_t function, const char* format, VaList* arguments) {
    Require(context == &contextValue);
    std::array<char, 256> buffer{};
    utrace_vformat_nid_postfix(buffer.data(), static_cast<std::int32_t>(buffer.size()), 0, format, arguments);
    guestTrace += "exit " + std::to_string(function) + " " + buffer.data() + ";";
}

void APS5_VABI GuestData(const void* context, std::int32_t function, std::int32_t level, const char* format, VaList* arguments) {
    Require(context == &contextValue);
    std::array<char, 256> buffer{};
    utrace_vformat_nid_postfix(buffer.data(), static_cast<std::int32_t>(buffer.size()), 1, format, arguments);
    guestTrace += "data " + std::to_string(function) + " " + std::to_string(level) + " " + buffer.data() + ";";
}

void U_CALLCONV HostEntry(const void*, int32_t function) {
    hostTrace += "entry " + std::to_string(function) + ";";
}

void U_CALLCONV HostExit(const void*, int32_t function, const char* format, va_list arguments) {
    std::array<char, 256> buffer{};
    utrace_vformat(buffer.data(), static_cast<int32_t>(buffer.size()), 0, format, arguments);
    hostTrace += "exit " + std::to_string(function) + " " + buffer.data() + ";";
}

void U_CALLCONV HostData(const void*, int32_t function, int32_t level, const char* format, va_list arguments) {
    std::array<char, 256> buffer{};
    utrace_vformat(buffer.data(), static_cast<int32_t>(buffer.size()), 1, format, arguments);
    hostTrace += "data " + std::to_string(function) + " " + std::to_string(level) + " " + buffer.data() + ";";
}

void Tracing() {
    Require(utrace_getLevel_nid_postfix() == UTRACE_ERROR);
    utrace_entry_nid_postfix(1);
    utrace_exit_nid_postfix(1, 99);
    utrace_data_nid_postfix(1, 2, "%d", 3);
    utrace_setFunctions_nid_postfix(&contextValue, GuestEntry, GuestExit, GuestData);
    utrace_setFunctions(&contextValue, HostEntry, HostExit, HostData);
    const void* context = nullptr;
    TraceEntry entry = nullptr;
    TraceExit exit = nullptr;
    TraceData data = nullptr;
    utrace_getFunctions_nid_postfix(&context, &entry, &exit, &data);
    Require(context == &contextValue && entry == GuestEntry && exit == GuestExit && data == GuestData);
    utrace_entry_nid_postfix(0x1003);
    utrace_entry(0x1003);
    const UErrorCode status = U_BUFFER_OVERFLOW_ERROR;
    int value = 0;
    utrace_exit_nid_postfix(0x2001, 0);
    utrace_exit(0x2001, 0);
    utrace_exit_nid_postfix(0x2002, 1, 77);
    utrace_exit(0x2002, 1, 77);
    utrace_exit_nid_postfix(0x2003, 0x10, status);
    utrace_exit(0x2003, 0x10, status);
    utrace_exit_nid_postfix(0x2004, 0x11, -3, status);
    utrace_exit(0x2004, 0x11, -3, status);
    utrace_exit_nid_postfix(0x2005, 0x12, &value, status);
    utrace_exit(0x2005, 0x12, &value, status);
    utrace_data_nid_postfix(0x3001, 5, "%s %d\n%vh", "name", 12, u"ab", -1);
    utrace_data(0x3001, 5, "%s %d\n%vh", "name", 12, u"ab", -1);
    Require(guestTrace == hostTrace);
    Require(Rejects([] { utrace_exit_nid_postfix(1, 3); }));
    utrace_setFunctions_nid_postfix(nullptr, nullptr, nullptr, nullptr);
    utrace_setFunctions(nullptr, nullptr, nullptr, nullptr);
    for (const std::int32_t level : {-5, -1, 0, 4, 9, 10, 100}) {
        utrace_setLevel_nid_postfix(level);
        utrace_setLevel(level);
        Require(utrace_getLevel_nid_postfix() == utrace_getLevel());
    }
    Require(Rejects([] { utrace_getFunctions_nid_postfix(nullptr, nullptr, nullptr, nullptr); }));
}

void Names() {
    for (std::int32_t function = -2; function < 0x3010; ++function) {
        const char* guest = utrace_functionName_nid_postfix(function);
        const char* host = utrace_functionName(function);
        if (function == 0x2008) {
            Require(std::strcmp(guest, "[BOGUS Trace Function Number]") == 0);
            continue;
        }
        Require(std::strcmp(guest, host) == 0);
    }
}

}

int main() {
    Formats();
    Tracing();
    Names();
    std::puts("ICU trace checks passed");
    return 0;
}
