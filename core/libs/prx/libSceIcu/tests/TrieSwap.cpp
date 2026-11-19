#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/TrieSwap.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "udataswp.h"
#include "unicode/ucptrie.h"
#include "unicode/umutablecptrie.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
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
    std::fprintf(stderr, "ICU trie swap check failed at line %u\n", location.line());
    std::abort();
}

using Words = std::vector<std::uint32_t>;
using GuestSwap = std::int32_t (APS5_VABI*)(const DataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
using HostSwap = std::int32_t (*)(const UDataSwapper*, const void*, std::int32_t, void*, UErrorCode*);

struct Pair {
    DataSwapper* Guest;
    UDataSwapper* Host;
};

Pair Open(std::uint8_t inBig, std::uint8_t outBig) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Pair pair{udata_openSwapper_nid_postfix(inBig, 0, outBig, 0, &guestStatus), udata_openSwapper(inBig, 0, outBig, 0, &hostStatus)};
    Require(guestStatus == 0 && hostStatus == U_ZERO_ERROR);
    return pair;
}

void Close(Pair pair) {
    udata_closeSwapper_nid_postfix(pair.Guest);
    udata_closeSwapper(pair.Host);
}

void Put16(Words& words, std::size_t byteOffset, std::uint16_t value) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(words.data()) + byteOffset, &value, 2);
}

Words Random(std::mt19937& random, std::size_t bytes) {
    Words words((bytes + 3) / 4 + 4);
    for (auto& word : words) word = static_cast<std::uint32_t>(random());
    return words;
}

Words TrieV1(std::mt19937& random, std::uint32_t options, std::int32_t indexLength, std::int32_t dataLength) {
    const bool is32 = (options & 0x100) != 0;
    Words words = Random(random, 16 + static_cast<std::size_t>(indexLength) * 2 + static_cast<std::size_t>(dataLength) * (is32 ? 4 : 2));
    words[0] = 0x54726965;
    words[1] = options;
    words[2] = static_cast<std::uint32_t>(indexLength);
    words[3] = static_cast<std::uint32_t>(dataLength);
    return words;
}

Words TrieV2(std::mt19937& random, std::uint16_t options, std::uint16_t indexLength, std::uint16_t shiftedDataLength) {
    const std::size_t dataLength = static_cast<std::size_t>(shiftedDataLength) << 2;
    Words words = Random(random, 16 + indexLength * 2u + dataLength * ((options & 0xf) == 0 ? 2 : 4));
    words[0] = 0x54726932;
    Put16(words, 4, options);
    Put16(words, 6, indexLength);
    Put16(words, 8, shiftedDataLength);
    return words;
}

Words TrieV3(std::mt19937& random, std::uint16_t options, std::uint16_t indexLength, std::uint16_t dataLength) {
    const std::size_t total = (static_cast<std::size_t>(options & 0xf000) << 4) | dataLength;
    const std::size_t width = (options & 7) == 0 ? 2 : (options & 7) == 1 ? 4 : 1;
    Words words = Random(random, 16 + indexLength * 2u + total * width);
    words[0] = 0x54726933;
    Put16(words, 4, options);
    Put16(words, 6, indexLength);
    Put16(words, 8, dataLength);
    return words;
}

Words BigEndian(const Words& little) {
    Pair pair = Open(0, 1);
    Words big(little.size());
    UErrorCode status = U_ZERO_ERROR;
    const std::int32_t size = utrie_swapAnyVersion(pair.Host, little.data(), static_cast<std::int32_t>(little.size() * 4), big.data(), &status);
    Close(pair);
    if (U_FAILURE(status)) return {};
    Require(size > 0);
    return big;
}

void Compare(GuestSwap guest, HostSwap host, const Words& input, std::uint8_t inBig, std::uint8_t outBig) {
    Pair pair = Open(inBig, outBig);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact = 0;
    {
        UErrorCode status = U_ZERO_ERROR;
        exact = host(pair.Host, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-1, 0, 4, 15, 16, 17, exact - 1, exact, exact + 1, bytes}) {
        for (int mode = 0; mode < 3; ++mode) {
            Words guestOut(input.size() + 2, 0xa5a5a5a5u);
            Words hostOut(input.size() + 2, 0xa5a5a5a5u);
            if (mode == 1) {
                std::copy(input.begin(), input.end(), guestOut.begin());
                std::copy(input.begin(), input.end(), hostOut.begin());
            }
            const void* guestIn = mode == 1 ? guestOut.data() : input.data();
            const void* hostIn = mode == 1 ? hostOut.data() : input.data();
            void* guestTarget = mode == 2 ? nullptr : guestOut.data();
            void* hostTarget = mode == 2 ? nullptr : hostOut.data();
            std::int32_t guestStatus = 0;
            UErrorCode hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = guest(pair.Guest, guestIn, length, guestTarget, &guestStatus);
            const std::int32_t hostResult = host(pair.Host, hostIn, length, hostTarget, &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
        }
    }
    std::int32_t failed = 4;
    Require(guest(pair.Guest, input.data(), -1, nullptr, &failed) == 0 && failed == 4);
    for (const bool nullSwapper : {false, true}) {
        std::int32_t guestStatus = 0;
        UErrorCode hostStatus = U_ZERO_ERROR;
        const std::int32_t guestResult = nullSwapper ? guest(nullptr, input.data(), -1, nullptr, &guestStatus) : guest(pair.Guest, nullptr, -1, nullptr, &guestStatus);
        const std::int32_t hostResult = nullSwapper ? host(nullptr, input.data(), -1, nullptr, &hostStatus) : host(pair.Host, nullptr, -1, nullptr, &hostStatus);
        Require(guestResult == hostResult && guestStatus == hostStatus);
    }
    Close(pair);
}

void CompareAll(const Words& little, HostSwap host, GuestSwap guest) {
    for (const bool any : {false, true}) {
        const GuestSwap g = any ? utrie_swapAnyVersion_nid_postfix : guest;
        const HostSwap h = any ? utrie_swapAnyVersion : host;
        if (g == nullptr) continue;
        Compare(g, h, little, 0, 0);
        Compare(g, h, little, 0, 1);
        const Words big = BigEndian(little);
        if (!big.empty()) {
            Compare(g, h, big, 1, 0);
            Compare(g, h, big, 1, 1);
        }
        Compare(g, h, little, 1, 0);
    }
}

void Version1(std::mt19937& random) {
    for (const std::uint32_t options : {0x25u, 0x125u, 0x225u, 0x325u, 0x24u, 0x35u, 0x15u}) {
        for (const std::int32_t indexLength : {2048, 2080, 2049, 2016}) {
            for (const std::int32_t dataLength : {32, 36, 34, 28, 288, 300}) {
                CompareAll(TrieV1(random, options, indexLength, dataLength), utrie_swapAnyVersion, nullptr);
            }
        }
    }
}

void Version2(std::mt19937& random) {
    for (const std::uint16_t options : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{0x10}, std::uint16_t{0xff1}}) {
        for (const std::uint16_t indexLength : {std::uint16_t{2112}, std::uint16_t{2113}, std::uint16_t{2111}, std::uint16_t{2400}}) {
            for (const std::uint16_t shifted : {std::uint16_t{48}, std::uint16_t{47}, std::uint16_t{60}}) {
                CompareAll(TrieV2(random, options, indexLength, shifted), utrie2_swap, utrie2_swap_nid_postfix);
            }
        }
    }
}

void Version3(std::mt19937& random) {
    for (const std::uint16_t options : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{3}, std::uint16_t{0x40}, std::uint16_t{0x41},
             std::uint16_t{0x42}, std::uint16_t{0x80}, std::uint16_t{0xc0}, std::uint16_t{0x08}, std::uint16_t{0x1000}, std::uint16_t{0x0f02}}) {
        for (const std::uint16_t indexLength : {std::uint16_t{64}, std::uint16_t{63}, std::uint16_t{1024}, std::uint16_t{1023}}) {
            for (const std::uint16_t dataLength : {std::uint16_t{0x80}, std::uint16_t{0x7f}, std::uint16_t{0x81}, std::uint16_t{0x10}}) {
                CompareAll(TrieV3(random, options, indexLength, dataLength), ucptrie_swap, ucptrie_swap_nid_postfix);
            }
        }
    }
}

void Built() {
    for (const UCPTrieType type : {UCPTRIE_TYPE_FAST, UCPTRIE_TYPE_SMALL}) {
        for (const UCPTrieValueWidth width : {UCPTRIE_VALUE_BITS_16, UCPTRIE_VALUE_BITS_32, UCPTRIE_VALUE_BITS_8}) {
            UErrorCode status = U_ZERO_ERROR;
            UMutableCPTrie* mutableTrie = umutablecptrie_open(1, 0xad, &status);
            for (UChar32 c = 0; c < 0x110000; c += 0x1777) umutablecptrie_set(mutableTrie, c, static_cast<std::uint32_t>(c % 200), &status);
            umutablecptrie_setRange(mutableTrie, 0x4e00, 0x9fff, 7, &status);
            UCPTrie* trie = umutablecptrie_buildImmutable(mutableTrie, type, width, &status);
            Require(U_SUCCESS(status));
            const std::int32_t size = ucptrie_toBinary(trie, nullptr, 0, &status);
            status = U_ZERO_ERROR;
            Words words(static_cast<std::size_t>(size + 3) / 4);
            ucptrie_toBinary(trie, words.data(), size, &status);
            Require(U_SUCCESS(status));
            CompareAll(words, ucptrie_swap, ucptrie_swap_nid_postfix);
            ucptrie_close(trie);
            umutablecptrie_close(mutableTrie);
        }
    }
}

void Unrecognized() {
    Pair pair = Open(0, 1);
    Words words(8, 0x12345678u);
    for (const std::int32_t length : {-1, 0, 15, 32}) {
        std::int32_t guestStatus = 0;
        UErrorCode hostStatus = U_ZERO_ERROR;
        Require(utrie_swapAnyVersion_nid_postfix(pair.Guest, words.data(), length, words.data(), &guestStatus) ==
            utrie_swapAnyVersion(pair.Host, words.data(), length, words.data(), &hostStatus));
        Require(guestStatus == hostStatus);
    }
    std::vector<std::uint8_t> unaligned(40, 0);
    const std::uint32_t signature = 0x54726933;
    std::memcpy(unaligned.data() + 1, &signature, 4);
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Require(utrie_swapAnyVersion_nid_postfix(pair.Guest, unaligned.data() + 1, 32, nullptr, &guestStatus) ==
        utrie_swapAnyVersion(pair.Host, unaligned.data() + 1, 32, nullptr, &hostStatus));
    Require(guestStatus == hostStatus && guestStatus == U_INVALID_FORMAT_ERROR);
    Close(pair);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    std::mt19937 random(17);
    Version1(random);
    Version2(random);
    Version3(random);
    Built();
    Unrecognized();
    std::puts("ICU trie swap checks passed");
    return 0;
}
