#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/BreakSwap.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "udataswp.h"
#include "unicode/ucptrie.h"
#include "unicode/umutablecptrie.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <source_location>
#include <stdexcept>
#include <vector>

U_CAPI int32_t U_EXPORT2 ubrk_swap(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* pErrorCode);

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
    std::fprintf(stderr, "ICU break swap check failed at line %u\n", location.line());
    std::abort();
}

using Bytes = std::vector<std::uint8_t>;

Bytes Trie() {
    UErrorCode status = U_ZERO_ERROR;
    UMutableCPTrie* builder = umutablecptrie_open(1, 0, &status);
    for (UChar32 c = 0x30; c < 0x3a; ++c) umutablecptrie_set(builder, c, 2, &status);
    umutablecptrie_setRange(builder, 0x41, 0x5a, 3, &status);
    UCPTrie* trie = umutablecptrie_buildImmutable(builder, UCPTRIE_TYPE_FAST, UCPTRIE_VALUE_BITS_16, &status);
    const std::int32_t size = ucptrie_toBinary(trie, nullptr, 0, &status);
    status = U_ZERO_ERROR;
    Bytes bytes(static_cast<std::size_t>(size));
    ucptrie_toBinary(trie, bytes.data(), size, &status);
    Require(U_SUCCESS(status));
    ucptrie_close(trie);
    umutablecptrie_close(builder);
    return bytes;
}

struct Options {
    bool ForwardEightBits;
    bool ReverseEightBits;
    bool Reverse;
    std::uint8_t InfoVersion;
    std::uint8_t HeaderVersion;
    std::uint32_t Magic;
    const char* Format;
};

std::vector<std::uint32_t> Image(std::mt19937& random, const Bytes& trie, const Options& options) {
    Bytes data(80, 0);
    const auto align = [&] {
        while (data.size() % 8 != 0) data.push_back(0);
    };
    const auto put32 = [&](std::size_t at, std::uint32_t value) { std::memcpy(data.data() + at, &value, 4); };
    const auto table = [&](bool eightBits, std::uint32_t rows) {
        const auto start = static_cast<std::uint32_t>(data.size());
        const std::uint32_t rowLength = eightBits ? 12 : 24;
        const std::uint32_t top[5] = {rows, rowLength, 5, 2, eightBits ? 4u : 1u};
        data.resize(data.size() + 20);
        std::memcpy(data.data() + start, top, 20);
        for (std::uint32_t index = 0; index < rows * rowLength; ++index) data.push_back(static_cast<std::uint8_t>(random()));
        const auto length = static_cast<std::uint32_t>(data.size()) - start;
        align();
        return std::pair<std::uint32_t, std::uint32_t>{start, length};
    };
    const auto forward = table(options.ForwardEightBits, 5);
    const auto reverse = options.Reverse ? table(options.ReverseEightBits, 3) : std::pair<std::uint32_t, std::uint32_t>{0, 0};
    const auto trieStart = static_cast<std::uint32_t>(data.size());
    data.insert(data.end(), trie.begin(), trie.end());
    align();
    const auto rulesStart = static_cast<std::uint32_t>(data.size());
    const char rules[] = "$x = [a-z];\n$x+;";
    data.insert(data.end(), rules, rules + sizeof(rules) - 1);
    align();
    const auto statusStart = static_cast<std::uint32_t>(data.size());
    for (int index = 0; index < 6; ++index) {
        data.resize(data.size() + 4);
        put32(data.size() - 4, static_cast<std::uint32_t>(random()));
    }
    align();
    put32(0, options.Magic);
    data[4] = options.HeaderVersion;
    put32(8, static_cast<std::uint32_t>(data.size()));
    put32(12, 9);
    put32(16, forward.first);
    put32(20, forward.second);
    put32(24, reverse.first);
    put32(28, reverse.second);
    put32(32, trieStart);
    put32(36, static_cast<std::uint32_t>(trie.size()));
    put32(40, rulesStart);
    put32(44, sizeof(rules) - 1);
    put32(48, statusStart);
    put32(52, 24);
    Bytes image(32, 0);
    const std::uint16_t headerSize = 32;
    const std::uint16_t infoSize = 20;
    std::memcpy(image.data(), &headerSize, 2);
    image[2] = 0xda;
    image[3] = 0x27;
    std::memcpy(image.data() + 4, &infoSize, 2);
    image[10] = 2;
    std::memcpy(image.data() + 12, options.Format, 4);
    image[16] = options.InfoVersion;
    std::memcpy(image.data() + 24, "Brk ab", 7);
    image.insert(image.end(), data.begin(), data.end());
    std::vector<std::uint32_t> words(image.size() / 4 + 2, 0x77777777u);
    std::memcpy(words.data(), image.data(), image.size());
    return words;
}

int swaps = 0;

void Compare(const std::vector<std::uint32_t>& input, std::uint8_t inBig, std::uint8_t outBig) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, 0, outBig, 0, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, 0, outBig, 0, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact = 0;
    {
        UErrorCode status = U_ZERO_ERROR;
        exact = ubrk_swap(host, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-2, -1, 0, 31, 32, exact - 1, exact, bytes}) {
        for (int mode = 0; mode < 3; ++mode) {
            std::vector<std::uint32_t> guestOut(input.size(), 0x5a5a5a5au);
            std::vector<std::uint32_t> hostOut(input.size(), 0x5a5a5a5au);
            if (mode == 1) {
                guestOut = input;
                hostOut = input;
            }
            void* guestTarget = mode == 2 ? nullptr : guestOut.data();
            void* hostTarget = mode == 2 ? nullptr : hostOut.data();
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = ubrk_swap_nid_postfix(guest, mode == 1 ? guestOut.data() : input.data(), length, guestTarget, &guestStatus);
            const std::int32_t hostResult = ubrk_swap(host, mode == 1 ? hostOut.data() : input.data(), length, hostTarget, &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
            if (length == exact && mode != 2 && U_SUCCESS(hostStatus)) ++swaps;
        }
    }
    std::int32_t failed = 9;
    Require(ubrk_swap_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    Require(ubrk_swap_nid_postfix(guest, input.data(), -1, nullptr, nullptr) == 0);
    std::int32_t status = 0;
    Require(ubrk_swap_nid_postfix(nullptr, input.data(), -1, nullptr, &status) == 0 && status == U_ILLEGAL_ARGUMENT_ERROR);
    udata_closeSwapper_nid_postfix(guest);
    udata_closeSwapper(host);
}

std::vector<std::uint32_t> BigEndian(const std::vector<std::uint32_t>& little) {
    UErrorCode status = U_ZERO_ERROR;
    UDataSwapper* swapper = udata_openSwapper(0, 0, 1, 0, &status);
    std::vector<std::uint32_t> big(little.size(), 0);
    ubrk_swap(swapper, little.data(), static_cast<int32_t>(little.size() * 4), big.data(), &status);
    udata_closeSwapper(swapper);
    if (U_FAILURE(status)) return {};
    return big;
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    std::mt19937 random(41);
    const Bytes trie = Trie();
    for (const bool forwardEight : {false, true}) {
        for (const bool reverseEight : {false, true}) {
            for (const bool reverse : {false, true}) {
                for (const std::uint8_t infoVersion : {std::uint8_t{6}, std::uint8_t{5}}) {
                    for (const std::uint8_t headerVersion : {std::uint8_t{6}, std::uint8_t{7}}) {
                        for (const std::uint32_t magic : {0xb1a0u, 0xb1a1u}) {
                            for (const char* format : {"Brk ", "Brkx"}) {
                                const auto image = Image(random, trie, {forwardEight, reverseEight, reverse, infoVersion, headerVersion, magic, format});
                                Compare(image, 0, 0);
                                Compare(image, 0, 1);
                                const auto big = BigEndian(image);
                                if (!big.empty()) {
                                    Compare(big, 1, 0);
                                    Compare(big, 1, 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (const std::uint32_t shortLength : {0u, 40u, 79u, 80u}) {
        auto image = Image(random, trie, {false, false, true, 6, 6, 0xb1a0u, "Brk "});
        std::memcpy(reinterpret_cast<std::uint8_t*>(image.data()) + 40, &shortLength, 4);
        Compare(image, 0, 0);
        Compare(image, 0, 1);
    }
    Require(swaps > 50);
    std::printf("ICU break swap checks passed (%d swaps)\n", swaps);
    return 0;
}
