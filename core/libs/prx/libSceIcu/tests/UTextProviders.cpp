#include "unicode/utext.h"

#undef utext_setup
#undef utext_close
#undef utext_openUChars

#include "prx/libSceIcu/UText.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

std::string current;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UText check failed at line %u (%s)\n", location.line(), current.c_str());
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

void SameState(const UText* ours, const UText* icu) {
    Require(ours->chunkNativeStart == icu->chunkNativeStart && ours->chunkNativeLimit == icu->chunkNativeLimit);
    Require(ours->chunkOffset == icu->chunkOffset && ours->chunkLength == icu->chunkLength && ours->nativeIndexingLimit == icu->nativeIndexingLimit);
    Require(ours->providerProperties == icu->providerProperties && ours->a == icu->a && ours->b == icu->b && ours->c == icu->c);
    if (ours->chunkLength > 0) Require(std::memcmp(ours->chunkContents, icu->chunkContents, static_cast<std::size_t>(ours->chunkLength) * 2) == 0);
}

void Drive(UText* ours, UText* icu, std::int64_t span, std::uint32_t seed) {
    std::mt19937 random(seed);
    const auto index = [&] { return static_cast<std::int64_t>(random() % static_cast<std::uint32_t>(span + 6)) - 3; };
    for (int step = 0; step < 2500; ++step) {
        current = "step " + std::to_string(step);
        const int operation = static_cast<int>(random() % 15);
        switch (operation) {
        case 0:
        case 1:
            Require(utext_next32_nid_postfix(ours) == utext_next32(icu));
            break;
        case 2:
        case 3:
            Require(utext_previous32_nid_postfix(ours) == utext_previous32(icu));
            break;
        case 4: {
            const auto at = index();
            Require(utext_next32From_nid_postfix(ours, at) == utext_next32From(icu, at));
            break;
        }
        case 5: {
            const auto at = index();
            Require(utext_previous32From_nid_postfix(ours, at) == utext_previous32From(icu, at));
            break;
        }
        case 6: {
            const auto at = index();
            utext_setNativeIndex_nid_postfix(ours, at);
            utext_setNativeIndex(icu, at);
            break;
        }
        case 7:
            Require(utext_current32_nid_postfix(ours) == utext_current32(icu));
            break;
        case 8: {
            const auto at = index();
            Require(utext_char32At_nid_postfix(ours, at) == utext_char32At(icu, at));
            break;
        }
        case 9: {
            const auto delta = static_cast<std::int32_t>(random() % 21) - 10;
            Require(utext_moveIndex32_nid_postfix(ours, delta) == utext_moveIndex32(icu, delta));
            break;
        }
        case 10:
            Require(utext_getPreviousNativeIndex_nid_postfix(ours) == utext_getPreviousNativeIndex(icu));
            break;
        case 11: {
            auto start = index();
            auto limit = index();
            const auto capacity = static_cast<std::int32_t>(random() % 12);
            std::array<char16_t, 16> left{};
            std::array<char16_t, 16> right{};
            left.fill(0x7777);
            right.fill(0x7777);
            std::int32_t ourStatus = 0;
            UErrorCode icuStatus = U_ZERO_ERROR;
            Require(utext_extract_nid_postfix(ours, start, limit, left.data(), capacity, &ourStatus) == utext_extract(icu, start, limit, right.data(), capacity, &icuStatus));
            Require(ourStatus == static_cast<std::int32_t>(icuStatus) && left == right);
            break;
        }
        case 12:
            Require(utext_nativeLength_nid_postfix(ours) == utext_nativeLength(icu));
            break;
        case 13:
            Require(utext_isLengthExpensive_nid_postfix(ours) == (utext_isLengthExpensive(icu) ? 1 : 0));
            break;
        default:
            Require(utext_getNativeIndex_nid_postfix(ours) == utext_getNativeIndex(icu));
            break;
        }
        SameState(ours, icu);
    }
}

std::vector<std::string> Utf8Samples() {
    std::vector<std::string> samples{"", "a", "hello, world", "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 \xE4\xB8\xAD\xE6\x96\x87", "\xF0\x9F\x98", "\xE2\x82\x41\xC0\xAF\xED\xA0\x80\xFF\x80z"};
    std::string mixed;
    for (int i = 0; i < 300; ++i) {
        switch (i % 7) {
        case 0: mixed += "ab"; break;
        case 1: mixed += "\xC3\xA9"; break;
        case 2: mixed += "\xE2\x82\xAC"; break;
        case 3: mixed += "\xF0\x9F\x98\x80"; break;
        case 4: mixed += "\xE2\x82"; break;
        case 5: mixed += "\x80"; break;
        default: mixed += "xyz"; break;
        }
    }
    samples.push_back(mixed);
    samples.push_back(std::string(200, 'q') + "\xF0\x90\x80\x80" + std::string(100, 'r'));
    return samples;
}

std::vector<std::u16string> Utf16Samples() {
    std::u16string mixed;
    for (int i = 0; i < 300; ++i) {
        switch (i % 5) {
        case 0: mixed += u"ab"; break;
        case 1: mixed += u"\U0001F600"; break;
        case 2: mixed += static_cast<char16_t>(0xD800); break;
        case 3: mixed += static_cast<char16_t>(0xDC00); break;
        default: mixed += u"é"; break;
        }
    }
    return {u"", u"x", u"hello \U0001F600 world", std::u16string{0xDC00, 0x61, 0xD800}, mixed};
}

void Utf8() {
    std::uint32_t seed = 1;
    for (const auto& sample : Utf8Samples()) {
        for (const bool terminated : {false, true}) {
            const std::int64_t length = terminated ? -1 : static_cast<std::int64_t>(sample.size());
            std::int32_t status = 0;
            UErrorCode icuStatus = U_ZERO_ERROR;
            UText* ours = utext_openUTF8_nid_postfix(nullptr, sample.c_str(), length, &status);
            UText* icu = utext_openUTF8(nullptr, sample.c_str(), length, &icuStatus);
            Require(ours && icu && status == 0 && U_SUCCESS(icuStatus));
            current = "UTF-8 open";
            SameState(ours, icu);
            Drive(ours, icu, static_cast<std::int64_t>(sample.size()), seed++);
            std::int32_t cloneStatus = 0;
            UErrorCode icuCloneStatus = U_ZERO_ERROR;
            UText* ourClone = utext_clone_nid_postfix(nullptr, ours, 1, 1, &cloneStatus);
            UText* icuClone = utext_clone(nullptr, icu, true, true, &icuCloneStatus);
            Require(ourClone && icuClone && cloneStatus == 0 && ourClone->context != ours->context && utext_isWritable_nid_postfix(ourClone) == 0);
            Require(utext_equals_nid_postfix(ours, ours) == 1 && utext_equals_nid_postfix(ours, ourClone) == 0);
            Drive(ourClone, icuClone, static_cast<std::int64_t>(sample.size()), seed++);
            Require(utext_close(ourClone) == nullptr);
            utext_close_78(icuClone);
            Require(utext_close(ours) == nullptr);
            utext_close_78(icu);
        }
    }
}

void Utf16() {
    std::uint32_t seed = 100;
    for (const auto& sample : Utf16Samples()) {
        for (const bool terminated : {false, true}) {
            const std::int64_t length = terminated ? -1 : static_cast<std::int64_t>(sample.size());
            std::int32_t status = 0;
            UErrorCode icuStatus = U_ZERO_ERROR;
            UText* ours = utext_openUChars(nullptr, sample.c_str(), length, &status);
            UText* icu = utext_openUChars_78(nullptr, sample.c_str(), length, &icuStatus);
            Require(ours && icu && status == 0 && U_SUCCESS(icuStatus));
            current = "UTF-16 open";
            SameState(ours, icu);
            Drive(ours, icu, static_cast<std::int64_t>(sample.size()), seed++);
            std::int32_t cloneStatus = 0;
            UErrorCode icuCloneStatus = U_ZERO_ERROR;
            UText* ourClone = utext_clone_nid_postfix(nullptr, ours, 0, 0, &cloneStatus);
            UText* icuClone = utext_clone(nullptr, icu, false, false, &icuCloneStatus);
            Require(ourClone && cloneStatus == 0 && ourClone->context == ours->context && utext_equals_nid_postfix(ours, ourClone) == (utext_equals(icu, icuClone) ? 1 : 0));
            Drive(ourClone, icuClone, static_cast<std::int64_t>(sample.size()), seed++);
            utext_close(ourClone);
            utext_close_78(icuClone);
            utext_close(ours);
            utext_close_78(icu);
        }
    }
}

void Framework() {
    current = "framework";
    alignas(16) std::array<std::uint8_t, sizeof(UText)> storage{};
    UText* stack = reinterpret_cast<UText*>(storage.data());
    std::int32_t status = 0;
    Require(utext_setup(stack, 0, &status) == stack && status == 1);
    UText initialized = UTEXT_INITIALIZER;
    std::memcpy(storage.data(), &initialized, sizeof(initialized));
    status = 0;
    UText* opened = utext_openUTF8_nid_postfix(stack, "abc", 3, &status);
    Require(opened == stack && status == 0 && opened->extraSize == 480 && (opened->flags & 2) != 0);
    Require(utext_next32_nid_postfix(opened) == 'a' && utext_getNativeIndex_nid_postfix(opened) == 1);
    opened = utext_openUChars(stack, u"xyz", 3, &status);
    Require(opened == stack && status == 0 && opened->extraSize == 480 && utext_char32At_nid_postfix(opened, 2) == 'z');
    Require(utext_close(opened) == stack && opened->pFuncs == nullptr && opened->extraSize == 0);
    UText* heap = utext_setup(nullptr, 40, &status);
    Require(heap && status == 0 && heap->extraSize == 40 && heap->pExtra == reinterpret_cast<std::uint8_t*>(heap) + sizeof(UText) && (heap->flags & 5) == 5);
    status = 0;
    Require(utext_openUTF8_nid_postfix(nullptr, nullptr, 5, &status) == nullptr && status == 1);
    status = 0;
    Require(utext_openUChars(nullptr, u"a", -2, &status) == nullptr && status == 1);
    status = 7;
    Require(utext_openUTF8_nid_postfix(nullptr, "a", 1, &status) == nullptr && status == 7);
    status = 0;
    UText* empty = utext_openUTF8_nid_postfix(nullptr, nullptr, 0, &status);
    Require(empty && status == 0 && utext_nativeLength_nid_postfix(empty) == 0 && utext_next32_nid_postfix(empty) == -1);
    Require(utext_isWritable_nid_postfix(empty) == 0 && utext_hasMetaData_nid_postfix(empty) == 0);
    Require(utext_replace_nid_postfix(empty, 0, 0, u"x", 1, &status) == 0 && status == 30);
    status = 0;
    utext_copy_nid_postfix(empty, 0, 0, 0, 0, &status);
    Require(status == 30);
    Require(utext_equals_nid_postfix(empty, nullptr) == 0 && utext_equals_nid_postfix(empty, empty) == 1);
    Require(Rejects([&] { utext_next32_nid_postfix(nullptr); }));
    Require(Rejects([&] { utext_openUTF8_nid_postfix(nullptr, "a", 1, nullptr); }));
    heap->pFuncs = empty->pFuncs;
    Require(utext_close(heap) == nullptr);
    Require(utext_close(empty) == nullptr);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Utf8();
    Utf16();
    Framework();
    std::puts("ICU UText provider checks passed");
    return 0;
}
