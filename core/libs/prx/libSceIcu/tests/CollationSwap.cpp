#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/CollationSwap.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
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

U_CAPI UBool U_EXPORT2 ucol_looksLikeCollationBinary(const UDataSwapper* ds, const void* inData, int32_t length);
U_CAPI int32_t U_EXPORT2 ucol_swapInverseUCA(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* pErrorCode);

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
    std::fprintf(stderr, "ICU collation swap check failed at line %u\n", location.line());
    std::abort();
}

using Words = std::vector<std::uint32_t>;

void Put32(Words& words, std::size_t offset, std::uint32_t value, bool big) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(words.data());
    for (std::size_t index = 0; index < 4; ++index) bytes[offset + index] = static_cast<std::uint8_t>(value >> (big ? 24 - 8 * index : 8 * index));
}

void DataHeader(Words& words, bool big, std::uint8_t charset, const char* format, std::uint8_t major, std::uint8_t minor) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(words.data());
    bytes[0] = big ? 0 : 32;
    bytes[1] = big ? 32 : 0;
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    bytes[4] = big ? 0 : 20;
    bytes[5] = big ? 20 : 0;
    bytes[6] = 0;
    bytes[7] = 0;
    bytes[8] = big;
    bytes[9] = charset;
    bytes[10] = 2;
    bytes[11] = 0;
    std::memcpy(bytes + 12, format, 4);
    bytes[16] = major;
    bytes[17] = minor;
    std::memset(bytes + 18, 0, 6);
    std::memcpy(bytes + 24, "InvC ab", 8);
    bytes[31] = 0;
}

Words Inverse(std::mt19937& random, bool big, const char* format, std::uint8_t major, std::uint8_t minor, std::uint32_t rows, std::uint32_t units,
    std::int32_t sizeDelta) {
    const std::uint32_t table = 32;
    const std::uint32_t conts = table + rows * 12;
    const std::uint32_t byteSize = conts + units * 2;
    Words words((32 + byteSize + 3) / 4 + 4);
    for (auto& word : words) word = static_cast<std::uint32_t>(random());
    DataHeader(words, big, 0, format, major, minor);
    Put32(words, 32, static_cast<std::uint32_t>(static_cast<std::int32_t>(byteSize) + sizeDelta), big);
    Put32(words, 36, rows, big);
    Put32(words, 40, units, big);
    Put32(words, 44, table, big);
    Put32(words, 48, conts, big);
    return words;
}

Words Version3(std::mt19937& random, bool big, std::uint8_t charset, std::uint32_t magic, std::uint8_t formatVersion, std::int32_t size) {
    Words words(64);
    for (auto& word : words) word = static_cast<std::uint32_t>(random());
    Put32(words, 0, static_cast<std::uint32_t>(size), big);
    Put32(words, 16, magic, big);
    auto* bytes = reinterpret_cast<std::uint8_t*>(words.data());
    bytes[65] = big;
    bytes[66] = charset;
    bytes[80] = formatVersion;
    return words;
}

void Compare(const Words& input, std::uint8_t inBig, std::uint8_t outBig, int& successes) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, 0, outBig, 0, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, 0, outBig, 0, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact;
    {
        UErrorCode status = U_ZERO_ERROR;
        exact = ucol_swapInverseUCA(host, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-1, 0, 32, 63, 64, 95, 96, exact - 1, exact, bytes}) {
        Require((ucol_looksLikeCollationBinary_nid_postfix(guest, input.data(), length) != 0) == (ucol_looksLikeCollationBinary(host, input.data(), length) != 0));
        for (const bool inPlace : {false, true}) {
            Words guestOut(input.size(), 0x5a5a5a5au);
            Words hostOut(input.size(), 0x5a5a5a5au);
            if (inPlace) {
                guestOut = input;
                hostOut = input;
            }
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = ucol_swapInverseUCA_nid_postfix(guest, inPlace ? guestOut.data() : input.data(), length, guestOut.data(), &guestStatus);
            const std::int32_t hostResult = ucol_swapInverseUCA(host, inPlace ? hostOut.data() : input.data(), length, hostOut.data(), &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
            if (length == exact && U_SUCCESS(hostStatus)) ++successes;
        }
    }
    std::int32_t failed = 9;
    Require(ucol_swapInverseUCA_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    Require(ucol_swapInverseUCA_nid_postfix(guest, input.data(), -1, nullptr, nullptr) == 0);
    udata_closeSwapper_nid_postfix(guest);
    udata_closeSwapper(host);
}

void LooksLike(const Words& input, int& positives) {
    for (std::uint8_t inBig = 0; inBig < 2; ++inBig) {
        for (std::uint8_t charset = 0; charset < 2; ++charset) {
            std::int32_t guestStatus = 0;
            UErrorCode hostStatus = U_ZERO_ERROR;
            DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, charset, 0, 0, &guestStatus);
            UDataSwapper* host = udata_openSwapper(inBig, charset, 0, 0, &hostStatus);
            for (const std::int32_t length : {-2, -1, 0, 100, 167, 168, 200, 255, 256}) {
                const bool guestResult = ucol_looksLikeCollationBinary_nid_postfix(guest, input.data(), length) != 0;
                Require(guestResult == (ucol_looksLikeCollationBinary(host, input.data(), length) != 0));
                if (guestResult) ++positives;
            }
            Require(ucol_looksLikeCollationBinary_nid_postfix(nullptr, input.data(), -1) == 0);
            Require(ucol_looksLikeCollationBinary_nid_postfix(guest, nullptr, -1) == 0);
            udata_closeSwapper_nid_postfix(guest);
            udata_closeSwapper(host);
        }
    }
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    std::mt19937 random(43);
    int successes = 0;
    int positives = 0;
    for (const bool big : {false, true}) {
        for (const char* format : {"InvC", "InvD", "UCol"}) {
            for (const std::uint8_t major : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}, std::uint8_t{4}}) {
                for (const std::uint8_t minor : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{5}}) {
                    for (const std::uint32_t rows : {0u, 1u, 7u}) {
                        for (const std::uint32_t units : {0u, 1u, 9u}) {
                            for (const std::int32_t delta : {0, -4, 4}) {
                                const Words image = Inverse(random, big, format, major, minor, rows, units, delta);
                                Compare(image, big, 0, successes);
                                Compare(image, big, 1, successes);
                                if (rows == 1 && units == 1 && delta == 0) LooksLike(image, positives);
                            }
                        }
                    }
                }
            }
        }
    }
    for (const bool big : {false, true}) {
        for (const std::uint8_t charset : {std::uint8_t{0}, std::uint8_t{1}}) {
            for (const std::uint32_t magic : {0x20030618u, 0x20030619u}) {
                for (const std::uint8_t formatVersion : {std::uint8_t{2}, std::uint8_t{3}}) {
                    for (const std::int32_t size : {100, 168, 200, 256, 300}) {
                        LooksLike(Version3(random, big, charset, magic, formatVersion, size), positives);
                        auto flipped = Version3(random, big, charset, magic, formatVersion, size);
                        reinterpret_cast<std::uint8_t*>(flipped.data())[65] = !big;
                        LooksLike(flipped, positives);
                    }
                }
            }
        }
    }
    Require(successes > 100 && positives > 20);
    std::printf("ICU collation swap checks passed (%d swaps, %d recognized)\n", successes, positives);
    return 0;
}
