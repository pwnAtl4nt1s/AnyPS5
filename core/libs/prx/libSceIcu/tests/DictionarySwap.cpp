#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/DictionarySwap.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "dictionarydata.h"
#include "udataswp.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU dictionary swap check failed at line %u\n", location.line());
    std::abort();
}

std::vector<std::uint32_t> Image(std::mt19937& random, bool big, std::int32_t trieType, std::int32_t trieBytes, std::int32_t extra, const char* format,
    std::uint8_t version) {
    const std::int32_t headerSize = 32;
    const std::int32_t size = 32 + trieBytes + extra;
    std::vector<std::uint32_t> words(static_cast<std::size_t>(headerSize + size) / 4 + 4);
    for (auto& word : words) word = static_cast<std::uint32_t>(random());
    auto* bytes = reinterpret_cast<std::uint8_t*>(words.data());
    const auto put16 = [&](std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(big ? value >> 8 : value);
        bytes[offset + 1] = static_cast<std::uint8_t>(big ? value : value >> 8);
    };
    const auto put32 = [&](std::size_t offset, std::int32_t value) {
        const auto raw = static_cast<std::uint32_t>(value);
        for (std::size_t index = 0; index < 4; ++index) bytes[offset + index] = static_cast<std::uint8_t>(raw >> (big ? 24 - 8 * index : 8 * index));
    };
    put16(0, headerSize);
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    put16(4, 20);
    bytes[8] = big;
    bytes[9] = 0;
    bytes[10] = 2;
    std::memcpy(bytes + 12, format, 4);
    bytes[16] = version;
    std::memcpy(bytes + 24, "Dict ab", 8);
    bytes[31] = 0;
    const std::int32_t indexes[8] = {32, 32 + trieBytes, 32 + trieBytes, size, trieType, 0x1000e00, 0, 0};
    for (std::size_t index = 0; index < 8; ++index) put32(headerSize + index * 4, indexes[index]);
    return words;
}

void Compare(const std::vector<std::uint32_t>& input, std::uint8_t inBig, std::uint8_t outBig) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, 0, outBig, 0, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, 0, outBig, 0, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact = 0;
    {
        std::int32_t status = 0;
        exact = udict_swap_nid_postfix(guest, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-1, 0, 20, 32, 40, 63, 64, exact - 1, exact, bytes}) {
        for (const bool inPlace : {false, true}) {
            std::vector<std::uint32_t> guestOut(input.size(), 0x5a5a5a5au);
            std::vector<std::uint32_t> hostOut(input.size(), 0x5a5a5a5au);
            if (inPlace) {
                guestOut = input;
                hostOut = input;
            }
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = udict_swap_nid_postfix(guest, inPlace ? guestOut.data() : input.data(), length, guestOut.data(), &guestStatus);
            const std::int32_t hostResult = udict_swap(host, inPlace ? hostOut.data() : input.data(), length, hostOut.data(), &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
        }
    }
    std::int32_t failed = 9;
    Require(udict_swap_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    Require(udict_swap_nid_postfix(guest, input.data(), -1, nullptr, nullptr) == 0);
    udata_closeSwapper_nid_postfix(guest);
    udata_closeSwapper(host);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    std::mt19937 random(29);
    for (const bool big : {false, true}) {
        for (const std::int32_t trieType : {0, 1, 2, 7, 8, 9, 0x20}) {
            for (const std::int32_t trieBytes : {0, 2, 6, 64, 66}) {
                for (const std::int32_t extra : {0, 4}) {
                    for (const char* format : {"Dict", "Dicu", "dict"}) {
                        for (const std::uint8_t version : {std::uint8_t{1}, std::uint8_t{2}}) {
                            const auto image = Image(random, big, trieType, trieBytes, extra, format, version);
                            for (const std::uint8_t outBig : {std::uint8_t{0}, std::uint8_t{1}}) Compare(image, big, outBig);
                        }
                    }
                }
            }
        }
    }
    std::puts("ICU dictionary swap checks passed");
    return 0;
}
