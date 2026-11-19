#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/ResourceSwap.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "udataswp.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

U_CAPI int32_t U_EXPORT2 ures_swap(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* pErrorCode);

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
    std::fprintf(stderr, "ICU resource swap check failed at line %u\n", location.line());
    std::abort();
}

using Words = std::vector<std::uint32_t>;

struct Options {
    std::uint8_t Major;
    std::uint8_t Minor;
    bool Collation;
    std::int32_t IndexLength;
    bool SixteenBit;
    std::int32_t MaxTableOverride;
    bool PoolKeys = false;
};

struct Bundle {
    Words Data;
    std::map<std::string, std::uint32_t> Keys;

    std::uint32_t Key(const std::string& name) const {
        if (!name.empty() && name[0] == '#') return static_cast<std::uint32_t>(std::stoul(name.substr(1)));
        return Keys.at(name);
    }
    std::uint32_t Collation() {
        std::vector<std::uint8_t> bytes(44, 0);
        bytes[0] = 32;
        bytes[2] = 0xda;
        bytes[3] = 0x27;
        bytes[4] = 20;
        bytes[10] = 2;
        std::memcpy(bytes.data() + 12, "UCol", 4);
        bytes[16] = 4;
        std::memcpy(bytes.data() + 24, "Coll ab", 8);
        bytes[31] = 0;
        const std::uint32_t indexes[3] = {3, 0x1234, 0x5678};
        std::memcpy(bytes.data() + 32, indexes, 12);
        const std::uint32_t at = Push(44);
        for (std::size_t index = 0; index < 44; index += 4) {
            std::uint32_t word;
            std::memcpy(&word, bytes.data() + index, 4);
            Push(word);
        }
        return 1u << 28 | at;
    }
    std::uint32_t Top() const {
        return static_cast<std::uint32_t>(Data.size());
    }
    std::uint32_t Push(std::uint32_t word) {
        Data.push_back(word);
        return Top() - 1;
    }
    std::uint32_t String(const std::u16string& text, std::uint32_t type = 0) {
        const std::uint32_t at = Push(static_cast<std::uint32_t>(text.size()));
        std::vector<char16_t> units(text.begin(), text.end());
        units.push_back(0);
        if (units.size() % 2 != 0) units.push_back(0);
        for (std::size_t index = 0; index < units.size(); index += 2) Push(static_cast<std::uint32_t>(units[index]) | static_cast<std::uint32_t>(units[index + 1]) << 16);
        return type << 28 | at;
    }
    std::uint32_t Binary(std::mt19937& random, std::uint32_t bytes) {
        const std::uint32_t at = Push(bytes);
        for (std::uint32_t index = 0; index < (bytes + 3) / 4; ++index) Push(static_cast<std::uint32_t>(random()));
        return 1u << 28 | at;
    }
    std::uint32_t Vector(const std::vector<std::int32_t>& values) {
        const std::uint32_t at = Push(static_cast<std::uint32_t>(values.size()));
        for (const std::int32_t value : values) Push(static_cast<std::uint32_t>(value));
        return 14u << 28 | at;
    }
    std::uint32_t Array(const std::vector<std::uint32_t>& items) {
        const std::uint32_t at = Push(static_cast<std::uint32_t>(items.size()));
        for (const std::uint32_t item : items) Push(item);
        return 8u << 28 | at;
    }
    std::uint32_t Table16(const std::vector<std::pair<std::string, std::uint32_t>>& items) {
        const std::uint32_t at = Top();
        std::vector<std::uint16_t> units{static_cast<std::uint16_t>(items.size())};
        for (const auto& item : items) units.push_back(static_cast<std::uint16_t>(Key(item.first)));
        if (units.size() % 2 != 0) units.push_back(0);
        for (std::size_t index = 0; index < units.size(); index += 2) Push(static_cast<std::uint32_t>(units[index]) | static_cast<std::uint32_t>(units[index + 1]) << 16);
        for (const auto& item : items) Push(item.second);
        return 2u << 28 | at;
    }
    std::uint32_t Table32(const std::vector<std::pair<std::string, std::uint32_t>>& items) {
        const std::uint32_t at = Push(static_cast<std::uint32_t>(items.size()));
        for (const auto& item : items) Push(Key(item.first));
        for (const auto& item : items) Push(item.second);
        return 4u << 28 | at;
    }
};

Words Image(std::mt19937& random, const Options& options) {
    Bundle bundle;
    bundle.Data.assign(1 + static_cast<std::size_t>(options.IndexLength), 0);
    const std::vector<std::string> keys{"%%CollationBin", "%%Other", "9lives", "Alpha", "Beta", "alpha", "beta", "gamma", "zeta"};
    std::string keyBytes;
    const auto keysStart = static_cast<std::uint32_t>(bundle.Data.size() * 4);
    for (const std::string& key : keys) {
        bundle.Keys[key] = options.PoolKeys ? static_cast<std::uint32_t>(random() % keysStart) : keysStart + static_cast<std::uint32_t>(keyBytes.size());
        if (options.PoolKeys) continue;
        keyBytes += key;
        keyBytes.push_back('\0');
    }
    while (keyBytes.size() % 4 != 0) keyBytes.push_back(static_cast<char>(0xaa));
    for (std::size_t index = 0; index < keyBytes.size(); index += 4) {
        std::uint32_t word;
        std::memcpy(&word, keyBytes.data() + index, 4);
        bundle.Push(word);
    }
    const std::uint32_t keysTop = bundle.Top();
    if (options.SixteenBit) {
        for (int index = 0; index < 6; ++index) bundle.Push(static_cast<std::uint32_t>(random()));
    }
    const std::uint32_t sixteenTop = bundle.Top();
    const std::uint32_t text = bundle.String(u"Hello world");
    const std::uint32_t alias = bundle.String(u"de/foo", 3);
    const std::uint32_t vector = bundle.Vector({1, -2, 300000, 0x7fffffff});
    const std::uint32_t binary = bundle.Binary(random, 10);
    const std::uint32_t inner = bundle.Table32({{"9lives", 7u << 28 | 12345}, {"Alpha", text}, {"beta", vector}, {"zeta", 0}});
    const std::uint32_t array = bundle.Array({text, 7u << 28 | 5, inner, binary, 6u << 28 | 3, 9u << 28 | 1, 5u << 28 | 2, 0});
    std::vector<std::pair<std::string, std::uint32_t>> rootItems{
        {"%%Other", bundle.Binary(random, 6)}, {"9lives", array}, {"Alpha", alias}, {"Beta", 7u << 28 | 99}, {"alpha", text}, {"beta", vector}, {"gamma", inner},
        {"zeta", bundle.String(u"z")}};
    if (options.Collation) rootItems.insert(rootItems.begin(), {"%%CollationBin", bundle.Binary(random, 40)});
    const std::uint32_t looksLike = bundle.Collation();
    rootItems.push_back({"#" + std::to_string(options.PoolKeys ? 4 : (keysTop + 2) * 4), looksLike});
    const std::uint32_t root = bundle.Table16(rootItems);
    bundle.Data[0] = root;
    const std::int32_t maxTable = options.MaxTableOverride >= 0 ? options.MaxTableOverride : static_cast<std::int32_t>(rootItems.size());
    const std::array<std::uint32_t, 8> indexes{static_cast<std::uint32_t>(options.IndexLength), keysTop, bundle.Top(), bundle.Top(),
        static_cast<std::uint32_t>(maxTable), 0, sixteenTop, 0};
    for (std::int32_t index = 0; index < options.IndexLength && index < 8; ++index) bundle.Data[1 + static_cast<std::size_t>(index)] = indexes[static_cast<std::size_t>(index)];
    Words image(8, 0);
    auto* bytes = reinterpret_cast<std::uint8_t*>(image.data());
    bytes[0] = 32;
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    bytes[4] = 20;
    bytes[10] = 2;
    std::memcpy(bytes + 12, "ResB", 4);
    bytes[16] = options.Major;
    bytes[17] = options.Minor;
    std::memcpy(bytes + 24, "ResB ab", 8);
    bytes[31] = 0;
    image.insert(image.end(), bundle.Data.begin(), bundle.Data.end());
    image.push_back(0x99999999u);
    return image;
}

Words Convert(const Words& input, std::uint8_t inBig, std::uint8_t inCharset, std::uint8_t outBig, std::uint8_t outCharset) {
    UErrorCode status = U_ZERO_ERROR;
    UDataSwapper* swapper = udata_openSwapper(inBig, inCharset, outBig, outCharset, &status);
    Words output(input.size(), 0);
    ures_swap(swapper, input.data(), static_cast<int32_t>(input.size() * 4), output.data(), &status);
    udata_closeSwapper(swapper);
    if (U_FAILURE(status)) return {};
    return output;
}

int swaps = 0;

void Compare(const Words& input, std::uint8_t inBig, std::uint8_t inCharset, std::uint8_t outBig, std::uint8_t outCharset) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, inCharset, outBig, outCharset, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, inCharset, outBig, outCharset, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::int32_t exact;
    {
        UErrorCode status = U_ZERO_ERROR;
        exact = ures_swap(host, input.data(), -1, nullptr, &status);
    }
    for (const std::int32_t length : {-1, 0, 32, 55, 56, 60, 100, exact - 4, exact - 1, exact, bytes}) {
        for (const bool inPlace : {false, true}) {
            Words guestOut(input.size(), 0x5a5a5a5au);
            Words hostOut(input.size(), 0x5a5a5a5au);
            if (inPlace) {
                guestOut = input;
                hostOut = input;
            }
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = ures_swap_nid_postfix(guest, inPlace ? guestOut.data() : input.data(), length, guestOut.data(), &guestStatus);
            const std::int32_t hostResult = ures_swap(host, inPlace ? hostOut.data() : input.data(), length, hostOut.data(), &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
            if (length == exact && U_SUCCESS(hostStatus) && hostResult > 0) ++swaps;
        }
    }
    std::int32_t failed = 9;
    Require(ures_swap_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    Require(ures_swap_nid_postfix(guest, input.data(), -1, nullptr, nullptr) == 0);
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
    std::mt19937 random(47);
    const std::vector<std::pair<std::uint8_t, std::uint8_t>> versions{{1, 0}, {1, 1}, {1, 2}, {2, 0}, {3, 0}, {4, 0}};
    for (const auto& version : versions) {
        for (const bool collation : {false, true}) {
            for (const std::int32_t indexLength : {4, 5, 6, 7, 8}) {
                for (const std::int32_t maxTable : {-1, 300, -2}) {
                    const auto image = Image(random, {version.first, version.second, collation, indexLength, indexLength > 6, maxTable == 300 ? 300 : -1, maxTable == -2});
                    for (const std::uint8_t outBig : {std::uint8_t{0}, std::uint8_t{1}}) {
                        for (const std::uint8_t outCharset : {std::uint8_t{0}, std::uint8_t{1}}) Compare(image, 0, 0, outBig, outCharset);
                    }
                    const auto ebcdic = Convert(image, 0, 0, 1, 1);
                    if (!ebcdic.empty()) {
                        Compare(ebcdic, 1, 1, 0, 0);
                        Compare(ebcdic, 1, 1, 1, 1);
                    }
                }
            }
        }
    }
    Require(swaps > 100);
    std::printf("ICU resource swap checks passed (%d swaps)\n", swaps);
    return 0;
}
