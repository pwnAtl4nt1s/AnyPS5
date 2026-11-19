#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/NormalizerSwap.hpp"
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

U_CAPI int32_t U_EXPORT2 unorm2_swap(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* pErrorCode);

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
    std::fprintf(stderr, "ICU normalizer swap check failed at line %u\n", location.line());
    std::abort();
}

using Words = std::vector<std::uint32_t>;

Words Trie(UCPTrieType type) {
    UErrorCode status = U_ZERO_ERROR;
    UMutableCPTrie* builder = umutablecptrie_open(0, 0xffff, &status);
    for (UChar32 c = 0x300; c < 0x370; ++c) umutablecptrie_set(builder, c, static_cast<std::uint32_t>(c * 3), &status);
    umutablecptrie_setRange(builder, 0xac00, 0xd7a3, 0x8000, &status);
    UCPTrie* trie = umutablecptrie_buildImmutable(builder, type, UCPTRIE_VALUE_BITS_16, &status);
    const std::int32_t size = ucptrie_toBinary(trie, nullptr, 0, &status);
    status = U_ZERO_ERROR;
    Words words(static_cast<std::size_t>(size + 3) / 4, 0);
    ucptrie_toBinary(trie, words.data(), size, &status);
    Require(U_SUCCESS(status));
    ucptrie_close(trie);
    umutablecptrie_close(builder);
    return words;
}

Words Image(std::mt19937& random, const Words& trie, std::uint8_t version, const char* format, std::int32_t extraUnits, std::int32_t indexCount) {
    const std::int32_t trieOffset = indexCount * 4;
    const std::int32_t extraOffset = trieOffset + static_cast<std::int32_t>(trie.size() * 4);
    const std::int32_t fcdOffset = extraOffset + extraUnits * 2;
    const std::int32_t total = fcdOffset + 256;
    Words words(static_cast<std::size_t>(32 + total + 3) / 4 + 2);
    for (auto& word : words) word = static_cast<std::uint32_t>(random());
    auto* bytes = reinterpret_cast<std::uint8_t*>(words.data());
    const std::uint16_t headerSize = 32;
    const std::uint16_t infoSize = 20;
    std::memcpy(bytes, &headerSize, 2);
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    std::memcpy(bytes + 4, &infoSize, 2);
    bytes[8] = 0;
    bytes[9] = 0;
    bytes[10] = 2;
    std::memcpy(bytes + 12, format, 4);
    bytes[16] = version;
    std::memcpy(bytes + 24, "Nrm2 ab", 8);
    bytes[31] = 0;
    std::vector<std::int32_t> indexes(static_cast<std::size_t>(indexCount), 0);
    for (auto& value : indexes) value = static_cast<std::int32_t>(random() % 0x10000);
    indexes[0] = trieOffset;
    indexes[1] = extraOffset;
    indexes[2] = fcdOffset;
    for (std::size_t index = 3; index < 8; ++index) indexes[index] = total;
    std::memcpy(bytes + 32, indexes.data(), indexes.size() * 4);
    std::memcpy(bytes + 32 + trieOffset, trie.data(), trie.size() * 4);
    return words;
}

Words BigEndian(const Words& little) {
    UErrorCode status = U_ZERO_ERROR;
    UDataSwapper* swapper = udata_openSwapper(0, 0, 1, 0, &status);
    Words big(little.size(), 0);
    unorm2_swap(swapper, little.data(), static_cast<std::int32_t>(little.size() * 4), big.data(), &status);
    udata_closeSwapper(swapper);
    if (U_FAILURE(status)) return {};
    return big;
}

void Compare(const Words& input, std::uint8_t inBig, std::uint8_t outBig) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, 0, outBig, 0, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, 0, outBig, 0, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact = 0;
    {
        UErrorCode status = U_ZERO_ERROR;
        exact = unorm2_swap(host, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-1, 0, 32, 32 + 55, 32 + 56, 32 + 59, 32 + 60, 32 + 75, 32 + 76, exact - 1, exact, bytes}) {
        for (const bool inPlace : {false, true}) {
            Words guestOut(input.size(), 0x5a5a5a5au);
            Words hostOut(input.size(), 0x5a5a5a5au);
            if (inPlace) {
                guestOut = input;
                hostOut = input;
            }
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = unorm2_swap_nid_postfix(guest, inPlace ? guestOut.data() : input.data(), length, guestOut.data(), &guestStatus);
            const std::int32_t hostResult = unorm2_swap(host, inPlace ? hostOut.data() : input.data(), length, hostOut.data(), &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
        }
    }
    std::int32_t failed = 9;
    Require(unorm2_swap_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    Require(unorm2_swap_nid_postfix(guest, input.data(), -1, nullptr, nullptr) == 0);
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
    std::mt19937 random(31);
    for (const UCPTrieType type : {UCPTRIE_TYPE_FAST, UCPTRIE_TYPE_SMALL}) {
        const Words trie = Trie(type);
        for (const std::uint8_t version : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}, std::uint8_t{4}}) {
            for (const char* format : {"Nrm2", "Nrm3", "nrm2"}) {
                for (const std::int32_t extraUnits : {0, 1, 2, 50}) {
                    for (const std::int32_t indexCount : {16, 20}) {
                        const Words image = Image(random, trie, version, format, extraUnits, indexCount);
                        Compare(image, 0, 0);
                        Compare(image, 0, 1);
                        const Words big = BigEndian(image);
                        if (!big.empty()) {
                            Compare(big, 1, 0);
                            Compare(big, 1, 1);
                        }
                    }
                }
            }
        }
        const Words image = Image(random, trie, 5, "Nrm2", 4, 24);
        Words output(image.size());
        std::int32_t status = 0;
        DataSwapper* swapper = udata_openSwapper_nid_postfix(0, 0, 1, 0, &status);
        Require(unorm2_swap_nid_postfix(swapper, image.data(), -1, nullptr, &status) == 0 && status == U_UNSUPPORTED_ERROR);
        udata_closeSwapper_nid_postfix(swapper);
    }
    for (std::uint8_t version = 1; version <= 4; ++version) {
        Words image = Image(random, Trie(UCPTRIE_TYPE_SMALL), version, "Nrm2", 0, 20);
        std::memset(reinterpret_cast<std::uint8_t*>(image.data()) + 32, 0, 32);
        for (std::int32_t length = 32 + 48; length <= 32 + 80; ++length) {
            for (const std::uint8_t outBig : {std::uint8_t{0}, std::uint8_t{1}}) {
                std::int32_t guestStatus = 0;
                UErrorCode hostStatus = U_ZERO_ERROR;
                DataSwapper* guest = udata_openSwapper_nid_postfix(0, 0, outBig, 0, &guestStatus);
                UDataSwapper* host = udata_openSwapper(0, 0, outBig, 0, &hostStatus);
                Words guestOut(image.size(), 1);
                Words hostOut(image.size(), 1);
                Require(unorm2_swap_nid_postfix(guest, image.data(), length, guestOut.data(), &guestStatus) ==
                    unorm2_swap(host, image.data(), length, hostOut.data(), &hostStatus));
                Require(guestStatus == hostStatus && guestOut == hostOut);
                udata_closeSwapper_nid_postfix(guest);
                udata_closeSwapper(host);
            }
        }
    }
    std::puts("ICU normalizer swap checks passed");
    return 0;
}
