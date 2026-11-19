#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t Unsupported = 16;

std::int32_t reallocations = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    ++reallocations;
    return std::realloc(pointer, size);
}
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU string enumeration check failed at line %u\n", location.line());
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

void CharStrings() {
    const std::string longWord(300, 'q');
    const std::array<const char*, 4> words{"alpha", "be", "", longWord.c_str()};
    std::int32_t status = Success;
    UEnumeration* enumeration = uenum_openCharStringsEnumeration_nid_postfix(words.data(), 4, &status);
    Require(enumeration && status == Success);
    Require(uenum_count_nid_postfix(enumeration, &status) == 4 && status == Success);
    std::int32_t length = -1;
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == words[0] && length == 5);
    const char16_t* unit = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(unit && length == 2 && std::u16string(unit) == u"be" && status == Success);
    unit = uenum_unext_nid_postfix(enumeration, &length, &status);
    Require(unit && length == 0 && unit[0] == 0);
    const auto before = reallocations;
    unit = uenum_unext_nid_postfix(enumeration, nullptr, &status);
    Require(unit && std::u16string(unit) == std::u16string(300, u'q') && reallocations == before + 1);
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr && status == Success);
    Require(uenum_unext_nid_postfix(enumeration, &length, &status) == nullptr && length == 0 && status == Success);
    uenum_reset_nid_postfix(enumeration, &status);
    Require(status == Success && uenum_next_nid_postfix(enumeration, &length, &status) == words[0]);
    uenum_close_nid_postfix(enumeration);
}

void UnicodeStrings() {
    const std::array<const char16_t*, 3> words{u"x-y", u"café", u""};
    std::int32_t status = Success;
    UEnumeration* enumeration = uenum_openUCharStringsEnumeration_nid_postfix(words.data(), 3, &status);
    Require(enumeration && status == Success && uenum_count_nid_postfix(enumeration, &status) == 3);
    std::int32_t length = -1;
    Require(uenum_unext_nid_postfix(enumeration, &length, &status) == words[0] && length == 3);
    const char* text = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(text && length == 4 && std::memcmp(text, "caf\0", 5) == 0 && status == Success);
    text = uenum_next_nid_postfix(enumeration, nullptr, &status);
    Require(text && text[0] == 0);
    Require(uenum_next_nid_postfix(enumeration, &length, &status) == nullptr);
    uenum_reset_nid_postfix(enumeration, &status);
    text = uenum_next_nid_postfix(enumeration, &length, &status);
    Require(text && std::string(text) == "x-y" && length == 3);
    uenum_close_nid_postfix(enumeration);
}

void Arguments() {
    std::int32_t status = Success;
    UEnumeration* empty = uenum_openCharStringsEnumeration_nid_postfix(nullptr, 0, &status);
    Require(empty && status == Success && uenum_count_nid_postfix(empty, &status) == 0);
    std::int32_t length = 7;
    Require(uenum_next_nid_postfix(empty, &length, &status) == nullptr && length == 7);
    uenum_close_nid_postfix(empty);
    const char* one[] = {"a"};
    Require(uenum_openCharStringsEnumeration_nid_postfix(one, -1, &status) == nullptr && status == Success);
    Require(uenum_openCharStringsEnumeration_nid_postfix(nullptr, 1, &status) == nullptr && status == Success);
    Require(uenum_openUCharStringsEnumeration_nid_postfix(nullptr, 2, &status) == nullptr && status == Success);
    status = IllegalArgument;
    Require(uenum_openCharStringsEnumeration_nid_postfix(one, 1, &status) == nullptr && status == IllegalArgument);
    Require(uenum_openUCharStringsEnumeration_nid_postfix(nullptr, 0, &status) == nullptr && status == IllegalArgument);
    Require(Rejects([&] { uenum_openCharStringsEnumeration_nid_postfix(one, 1, nullptr); }));
}

void Defaults() {
    UEnumeration bare{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    std::int32_t status = Success;
    uenum_reset_nid_postfix(&bare, &status);
    Require(status == Unsupported);
    status = Success;
    std::int32_t length = 5;
    Require(uenum_unextDefault_nid_postfix(&bare, &length, &status) == nullptr && status == Unsupported && length == 0);
    status = Success;
    Require(uenum_nextDefault_nid_postfix(&bare, &length, &status) == nullptr && status == Unsupported);
    status = IllegalArgument;
    uenum_reset_nid_postfix(&bare, &status);
    Require(status == IllegalArgument);
    status = Success;
    uenum_reset_nid_postfix(nullptr, &status);
    Require(status == Success);
    Require(Rejects([&] { uenum_nextDefault_nid_postfix(&bare, nullptr, &status); }));
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CharStrings();
    UnicodeStrings();
    Arguments();
    Defaults();
    std::puts("ICU string enumeration checks passed");
    return 0;
}
