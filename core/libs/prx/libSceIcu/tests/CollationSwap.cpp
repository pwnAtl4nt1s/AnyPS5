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
U_CAPI int32_t U_EXPORT2 ucol_swap(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* pErrorCode);
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


struct Blob {
    std::vector<std::uint8_t> Bytes;

    std::uint32_t Offset() const {
        return static_cast<std::uint32_t>(Bytes.size());
    }
    void Align(std::size_t alignment) {
        while (Bytes.size() % alignment != 0) Bytes.push_back(0);
    }
    void Random(std::mt19937& random, std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) Bytes.push_back(static_cast<std::uint8_t>(random()));
    }
    void Put32(std::size_t at, std::uint32_t value) {
        std::memcpy(Bytes.data() + at, &value, 4);
    }
    void Put16(std::size_t at, std::uint16_t value) {
        std::memcpy(Bytes.data() + at, &value, 2);
    }
    void Add16(std::uint16_t value) {
        Bytes.resize(Bytes.size() + 2);
        Put16(Bytes.size() - 2, value);
    }
};

void AddTrie1(Blob& blob, std::mt19937& random) {
    const std::size_t start = blob.Offset();
    blob.Bytes.resize(start + 16);
    blob.Put32(start, 0x54726965);
    blob.Put32(start + 4, 0x25);
    blob.Put32(start + 8, 2048);
    blob.Put32(start + 12, 32);
    blob.Random(random, 2048 * 2 + 32 * 2);
}

void AddTrie2(Blob& blob, std::mt19937& random) {
    const std::size_t start = blob.Offset();
    blob.Bytes.resize(start + 16);
    blob.Put32(start, 0x54726932);
    blob.Put16(start + 4, 0);
    blob.Put16(start + 6, 2112);
    blob.Put16(start + 8, 48);
    blob.Random(random, 2112 * 2 + 192 * 2);
}

std::vector<std::uint8_t> Version3Body(std::mt19937& random, bool contractions, bool constants, bool scripts, std::uint8_t width) {
    Blob blob;
    blob.Bytes.resize(168, 0);
    const std::uint32_t options = blob.Offset();
    blob.Random(random, 16);
    const std::uint32_t expansion = blob.Offset();
    blob.Random(random, 24);
    std::uint32_t contractionIndex = 0;
    std::uint32_t contractionCEs = 0;
    const std::uint32_t contractionSize = contractions ? 5 : 0;
    if (contractions) {
        contractionIndex = blob.Offset();
        blob.Random(random, contractionSize * 2);
        blob.Align(4);
        contractionCEs = blob.Offset();
        blob.Random(random, contractionSize * 4);
    }
    const std::uint32_t mapping = blob.Offset();
    AddTrie1(blob, random);
    const std::uint32_t endExpansion = blob.Offset();
    blob.Random(random, 3 * 4);
    blob.Random(random, 7);
    blob.Align(4);
    std::uint32_t ucaConstants = 0;
    std::uint32_t combos = 0;
    std::int32_t combosSize = 0;
    if (constants) {
        ucaConstants = blob.Offset();
        blob.Random(random, 20);
        combos = blob.Offset();
        combosSize = 3;
        blob.Random(random, static_cast<std::size_t>(combosSize) * width * 2);
        blob.Align(4);
    }
    std::uint32_t scriptToLead = 0;
    std::uint32_t leadToScript = 0;
    if (scripts) {
        scriptToLead = blob.Offset();
        blob.Add16(2);
        blob.Add16(3);
        blob.Random(random, 2 * 4 + 3 * 2);
        blob.Align(4);
        leadToScript = blob.Offset();
        blob.Add16(4);
        blob.Add16(1);
        blob.Random(random, 4 * 2 + 1 * 2);
        blob.Align(4);
    }
    blob.Put32(0, blob.Offset());
    blob.Put32(4, options);
    blob.Put32(8, ucaConstants);
    blob.Put32(12, combos);
    blob.Put32(16, 0x20030618);
    blob.Put32(20, mapping);
    blob.Put32(24, expansion);
    blob.Put32(28, contractionIndex);
    blob.Put32(32, contractionCEs);
    blob.Put32(36, contractionSize);
    blob.Put32(40, endExpansion);
    blob.Put32(48, 3);
    blob.Put32(60, static_cast<std::uint32_t>(combosSize));
    blob.Bytes[64] = 1;
    blob.Bytes[65] = 0;
    blob.Bytes[66] = 0;
    blob.Bytes[67] = width;
    blob.Bytes[80] = 3;
    blob.Bytes[81] = 2;
    blob.Put32(84, scriptToLead);
    blob.Put32(88, leadToScript);
    return blob.Bytes;
}

std::vector<std::uint8_t> Version4Body(std::mt19937& random, std::int32_t indexesLength, std::int32_t reservedSection) {
    Blob blob;
    blob.Bytes.resize(static_cast<std::size_t>(indexesLength) * 4, 0);
    std::vector<std::uint32_t> offsets(20, 0);
    blob.Random(random, 8);
    const auto section = [&](std::int32_t index, std::size_t bytes, std::size_t alignment) {
        blob.Align(alignment);
        offsets[static_cast<std::size_t>(index)] = blob.Offset();
        blob.Random(random, bytes);
    };
    section(5, 12, 4);
    section(6, 256, 1);
    blob.Align(8);
    offsets[7] = blob.Offset();
    AddTrie2(blob, random);
    section(8, reservedSection == 8 ? 4 : 0, 8);
    section(9, 40, 8);
    section(10, reservedSection == 10 ? 4 : 0, 4);
    section(11, 24, 4);
    section(12, 16, 4);
    section(13, 10, 2);
    section(14, 6, 2);
    section(15, 8, 2);
    section(16, 14, 2);
    section(17, 256, 1);
    section(18, reservedSection == 18 ? 4 : 0, 4);
    blob.Align(4);
    offsets[19] = blob.Offset();
    blob.Put32(0, static_cast<std::uint32_t>(indexesLength));
    blob.Put32(4, 0x12345678);
    for (std::int32_t index = 2; index < indexesLength && index < 20; ++index) {
        blob.Put32(static_cast<std::size_t>(index) * 4, index < 5 ? static_cast<std::uint32_t>(random()) : offsets[static_cast<std::size_t>(index)]);
    }
    return blob.Bytes;
}

Words WithHeader(const std::vector<std::uint8_t>& body, std::uint8_t major, const char* format) {
    Words words((32 + body.size() + 3) / 4 + 2, 0);
    DataHeader(words, false, 0, format, major, 0);
    std::memcpy(reinterpret_cast<std::uint8_t*>(words.data()) + 32, body.data(), body.size());
    return words;
}

Words Bare(const std::vector<std::uint8_t>& body) {
    Words words((body.size() + 3) / 4 + 2, 0);
    std::memcpy(words.data(), body.data(), body.size());
    return words;
}

Words CollationBig(const Words& little) {
    UErrorCode status = U_ZERO_ERROR;
    UDataSwapper* swapper = udata_openSwapper(0, 0, 1, 0, &status);
    Words big(little.size(), 0);
    ucol_swap(swapper, little.data(), static_cast<int32_t>(little.size() * 4), big.data(), &status);
    udata_closeSwapper(swapper);
    if (U_FAILURE(status)) return {};
    return big;
}

int collationSwaps = 0;

void CompareCollation(const Words& input, std::uint8_t inBig, std::uint8_t outBig, std::uint8_t outCharset = 0) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, 0, outBig, outCharset, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, 0, outBig, outCharset, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact;
    {
        UErrorCode status = U_ZERO_ERROR;
        exact = ucol_swap(host, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-1, 0, 7, 8, 36, 39, 40, 100, 167, 168, 199, 200, exact - 1, exact, bytes}) {
        for (const bool inPlace : {false, true}) {
            Words guestOut(input.size(), 0x5a5a5a5au);
            Words hostOut(input.size(), 0x5a5a5a5au);
            if (inPlace) {
                guestOut = input;
                hostOut = input;
            }
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = ucol_swap_nid_postfix(guest, inPlace ? guestOut.data() : input.data(), length, guestOut.data(), &guestStatus);
            const std::int32_t hostResult = ucol_swap(host, inPlace ? hostOut.data() : input.data(), length, hostOut.data(), &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
            if (length == exact && U_SUCCESS(hostStatus)) ++collationSwaps;
        }
    }
    std::int32_t failed = 9;
    Require(ucol_swap_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    udata_closeSwapper_nid_postfix(guest);
    udata_closeSwapper(host);
}

void CompareBothWays(const Words& image) {
    CompareCollation(image, 0, 0);
    CompareCollation(image, 0, 1);
    CompareCollation(image, 0, 1, 1);
    const Words big = CollationBig(image);
    if (!big.empty()) {
        CompareCollation(big, 1, 0);
        CompareCollation(big, 1, 1);
    }
}

void Collations(std::mt19937& random) {
    for (const bool contractions : {false, true}) {
        for (const bool constants : {false, true}) {
            for (const bool scripts : {false, true}) {
                for (const std::uint8_t width : {std::uint8_t{2}, std::uint8_t{3}}) {
                    const auto body = Version3Body(random, contractions, constants, scripts, width);
                    CompareBothWays(Bare(body));
                    CompareBothWays(WithHeader(body, 3, "UCol"));
                    auto wrongMagic = body;
                    wrongMagic[16] ^= 1;
                    CompareBothWays(Bare(wrongMagic));
                    auto wrongEndian = body;
                    wrongEndian[65] = 1;
                    CompareBothWays(Bare(wrongEndian));
                    auto wrongVersion = body;
                    wrongVersion[80] = 2;
                    CompareBothWays(Bare(wrongVersion));
                }
            }
        }
    }
    for (const std::int32_t indexesLength : {1, 3, 5, 6, 12, 19, 20, 22}) {
        for (const std::int32_t reserved : {0, 8, 10, 18}) {
            for (const std::uint8_t major : {std::uint8_t{4}, std::uint8_t{5}, std::uint8_t{6}}) {
                CompareBothWays(WithHeader(Version4Body(random, indexesLength, reserved), major, "UCol"));
            }
        }
    }
    CompareBothWays(WithHeader(Version4Body(random, 20, 0), 4, "UCom"));
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
    Collations(random);
    Require(successes > 100 && positives > 20 && collationSwaps > 100);
    std::printf("ICU collation swap checks passed (%d inverse swaps, %d recognized, %d collation swaps)\n", successes, positives, collationSwaps);
    return 0;
}
