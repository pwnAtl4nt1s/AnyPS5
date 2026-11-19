#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t Bits16 = 0;
constexpr std::int32_t Bits32 = 1;
constexpr std::uint32_t Initial = 7;
constexpr std::uint32_t Error = 0xBAD;
constexpr std::uint32_t High = 0x55;
constexpr std::int32_t Blocks = 5;
constexpr std::int32_t IndexLength = 2112;
constexpr std::int32_t NullBlock = 0xC0;
constexpr std::int32_t FirstBlock = 0xE0;
constexpr std::int32_t DataLength = FirstBlock + Blocks * 32 + 4;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTrie2 check failed at line %u\n", location.line());
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

std::int32_t BlockFor(std::int32_t index2) {
    if (index2 >= 2048) return (index2 * 7) % Blocks;
    if (index2 % 3 == 0) return -1;
    return index2 % Blocks;
}

std::uint32_t BlockValue(std::int32_t block, std::int32_t offset) {
    return block < 0 ? Initial : static_cast<std::uint32_t>(1000 * (block + 1) + offset);
}

std::uint32_t Expected(std::int32_t c) {
    if (c < 0 || c > 0x10FFFF) return Error;
    if (c >= 0x10000) return High;
    if (c < 0x80) return static_cast<std::uint32_t>(c * 3 + 1);
    std::int32_t index2 = c >> 5;
    if (c >= 0xD800 && c <= 0xDBFF) index2 = 2048 + ((c - 0xD800) >> 5);
    return BlockValue(BlockFor(index2), c & 31);
}

std::vector<std::uint32_t> Serialized() {
    std::vector<std::uint32_t> words(4 + IndexLength / 2 + DataLength, 0);
    auto* bytes = reinterpret_cast<std::uint8_t*>(words.data());
    const std::uint32_t signature = 0x54726932;
    std::memcpy(bytes, &signature, 4);
    const std::uint16_t header[6]{static_cast<std::uint16_t>(Bits32), static_cast<std::uint16_t>(IndexLength), static_cast<std::uint16_t>(DataLength >> 2), 0xFFFF,
        static_cast<std::uint16_t>(NullBlock), static_cast<std::uint16_t>(0x10000 >> 11)};
    std::memcpy(bytes + 4, header, sizeof(header));
    auto* index = reinterpret_cast<std::uint16_t*>(bytes + 16);
    for (std::int32_t i = 0; i < 2080; ++i) {
        std::int32_t offset = 0;
        if (i < 4)
            offset = i * 32;
        else
            offset = BlockFor(i) < 0 ? NullBlock : FirstBlock + BlockFor(i) * 32;
        index[i] = static_cast<std::uint16_t>(offset >> 2);
    }
    for (std::int32_t i = 2080; i < IndexLength; ++i) index[i] = 0x80;
    auto* data = reinterpret_cast<std::uint32_t*>(bytes + 16 + IndexLength * 2);
    for (std::int32_t c = 0; c < 0x80; ++c) data[c] = static_cast<std::uint32_t>(c * 3 + 1);
    for (std::int32_t i = 0x80; i < 0xC0; ++i) data[i] = Error;
    for (std::int32_t i = 0; i < 32; ++i) data[NullBlock + i] = Initial;
    for (std::int32_t block = 0; block < Blocks; ++block)
        for (std::int32_t i = 0; i < 32; ++i) data[FirstBlock + block * 32 + i] = BlockValue(block, i);
    for (std::int32_t i = 0; i < 4; ++i) data[DataLength - 4 + i] = High;
    return words;
}

struct Range {
    std::int32_t Start;
    std::int32_t End;
    std::uint32_t Value;
};

struct Collector {
    std::vector<Range> Ranges;
    std::int32_t StopAfter = -1;
};

std::uint32_t APS5_VABI Flip(const void*, std::uint32_t value) {
    return value ^ 1u;
}

std::int8_t APS5_VABI Collect(const void* context, std::int32_t start, std::int32_t end, std::uint32_t value) {
    auto* collector = static_cast<Collector*>(const_cast<void*>(context));
    collector->Ranges.push_back({start, end, value});
    return collector->StopAfter < 0 || static_cast<std::int32_t>(collector->Ranges.size()) < collector->StopAfter ? 1 : 0;
}

void Lookups(const UTrie2* trie) {
    for (std::int32_t c = -2; c <= 0x110001; ++c) Require(utrie2_get32_nid_postfix(trie, c) == Expected(c));
    for (std::int32_t unit = 0xD800; unit <= 0xDBFF; ++unit) Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, unit) == BlockValue(BlockFor(unit >> 5), unit & 31));
    Require(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0x41) == Error);
    Require(utrie2_isFrozen_nid_postfix(trie) == 1);
}

void Utf8(const UTrie2* trie) {
    const std::vector<std::uint32_t> words = Serialized();
    const auto* values = reinterpret_cast<const std::uint32_t*>(reinterpret_cast<const std::uint8_t*>(words.data()) + 16 + IndexLength * 2);
    const std::uint8_t euro[]{0xE2, 0x82, 0xAC};
    std::int32_t result = utrie2_internalU8NextIndex_nid_postfix(trie, euro[0], euro + 1, euro + 3);
    Require((result & 7) == 2 && values[result >> 3] == Expected(0x20AC));
    result = utrie2_internalU8PrevIndex_nid_postfix(trie, euro[2], euro, euro + 2);
    Require((result & 7) == 2 && values[result >> 3] == Expected(0x20AC));
    const std::uint8_t smile[]{0xF0, 0x9F, 0x98, 0x80};
    result = utrie2_internalU8NextIndex_nid_postfix(trie, smile[0], smile + 1, smile + 4);
    Require((result & 7) == 3 && values[result >> 3] == High);
    const std::uint8_t broken[]{0xE2, 0x41};
    result = utrie2_internalU8NextIndex_nid_postfix(trie, broken[0], broken + 1, broken + 2);
    Require(values[result >> 3] == Error);
}

void Enumeration(const UTrie2* trie) {
    Collector all;
    utrie2_enum_nid_postfix(trie, nullptr, Collect, &all);
    Require(!all.Ranges.empty() && all.Ranges.front().Start == 0 && all.Ranges.back().End == 0x10FFFF);
    std::int32_t next = 0;
    for (const auto& range : all.Ranges) {
        Require(range.Start == next && range.End >= range.Start);
        for (std::int32_t c = range.Start; c <= range.End; ++c) Require(Expected(c) == range.Value);
        next = range.End + 1;
    }
    for (std::size_t i = 1; i < all.Ranges.size(); ++i) Require(all.Ranges[i].Value != all.Ranges[i - 1].Value);
    Collector flipped;
    utrie2_enum_nid_postfix(trie, Flip, Collect, &flipped);
    Require(flipped.Ranges.size() == all.Ranges.size() && flipped.Ranges[5].Value == (all.Ranges[5].Value ^ 1u));
    Collector stopped;
    stopped.StopAfter = 3;
    utrie2_enum_nid_postfix(trie, nullptr, Collect, &stopped);
    Require(stopped.Ranges.size() == 3);
    utrie2_enum_nid_postfix(trie, Flip, nullptr, &stopped);
    Require(stopped.Ranges.size() == 3);
    Collector lead;
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xD83D, nullptr, Collect, &lead);
    Require(lead.Ranges.size() == 1 && lead.Ranges[0].Start == 0x1F400 && lead.Ranges[0].End == 0x1F7FF && lead.Ranges[0].Value == High);
    Collector none;
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xDC00, nullptr, Collect, &none);
    Require(none.Ranges.empty());
}

void Serialization() {
    std::vector<std::uint32_t> words = Serialized();
    const auto bytes = static_cast<std::int32_t>(words.size() * 4);
    std::int32_t status = Success;
    std::int32_t actual = 0;
    UTrie2* trie = utrie2_openFromSerialized_nid_postfix(Bits32, words.data(), bytes + 64, &actual, &status);
    Require(trie && status == Success && actual == bytes);
    Lookups(trie);
    Utf8(trie);
    Enumeration(trie);
    std::vector<std::uint32_t> copy(words.size() + 4, 0);
    Require(utrie2_serialize_nid_postfix(trie, copy.data(), static_cast<std::int32_t>(copy.size() * 4), &status) == bytes && status == Success);
    Require(std::memcmp(copy.data(), words.data(), static_cast<std::size_t>(bytes)) == 0);
    Require(utrie2_serialize_nid_postfix(trie, copy.data(), 16, &status) == bytes && status == BufferOverflow);
    status = Success;
    Require(utrie2_serialize_nid_postfix(trie, reinterpret_cast<std::uint8_t*>(copy.data()) + 2, 1000000, &status) == 0 && status == IllegalArgument);
    status = Success;
    Require(utrie2_serialize_nid_postfix(trie, nullptr, 0, &status) == bytes && status == BufferOverflow);
    utrie2_close_nid_postfix(trie);
    utrie2_close_nid_postfix(nullptr);
}

void Errors() {
    std::vector<std::uint32_t> words = Serialized();
    const auto bytes = static_cast<std::int32_t>(words.size() * 4);
    std::int32_t status = IllegalArgument;
    Require(utrie2_openFromSerialized_nid_postfix(Bits32, words.data(), bytes, nullptr, &status) == nullptr && status == IllegalArgument);
    status = Success;
    Require(utrie2_openFromSerialized_nid_postfix(Bits16, words.data(), bytes, nullptr, &status) == nullptr && status == InvalidFormat);
    status = Success;
    Require(utrie2_openFromSerialized_nid_postfix(2, words.data(), bytes, nullptr, &status) == nullptr && status == IllegalArgument);
    status = Success;
    Require(utrie2_openFromSerialized_nid_postfix(Bits32, reinterpret_cast<std::uint8_t*>(words.data()) + 1, bytes - 1, nullptr, &status) == nullptr && status == IllegalArgument);
    status = Success;
    Require(utrie2_openFromSerialized_nid_postfix(Bits32, words.data(), 12, nullptr, &status) == nullptr && status == InvalidFormat);
    status = Success;
    Require(utrie2_openFromSerialized_nid_postfix(Bits32, words.data(), bytes - 4, nullptr, &status) == nullptr && status == InvalidFormat);
    words[0] = 0x54726931;
    status = Success;
    Require(utrie2_openFromSerialized_nid_postfix(Bits32, words.data(), bytes, nullptr, &status) == nullptr && status == InvalidFormat);
    Require(Rejects([&] { utrie2_openFromSerialized_nid_postfix(Bits32, words.data(), bytes, nullptr, nullptr); }));
    Require(Rejects([&] { utrie2_get32_nid_postfix(nullptr, 0x41); }));
    Require(Rejects([&] { utrie2_isFrozen_nid_postfix(nullptr); }));
}

void Dummy() {
    for (const std::int32_t bits : {Bits16, Bits32}) {
        std::int32_t status = Success;
        UTrie2* trie = utrie2_openDummy_nid_postfix(bits, 0x1234, 0xEEEE, &status);
        Require(trie && status == Success && utrie2_isFrozen_nid_postfix(trie) == 1);
        for (const std::int32_t c : {0, 0x41, 0x7FF, 0xD800, 0xFFFF, 0x10000, 0x10FFFF}) Require(utrie2_get32_nid_postfix(trie, c) == 0x1234);
        Require(utrie2_get32_nid_postfix(trie, 0x110000) == 0xEEEE && utrie2_get32_nid_postfix(trie, -1) == 0xEEEE);
        Collector ranges;
        utrie2_enum_nid_postfix(trie, nullptr, Collect, &ranges);
        Require(ranges.Ranges.size() == 1 && ranges.Ranges[0].Start == 0 && ranges.Ranges[0].End == 0x10FFFF && ranges.Ranges[0].Value == 0x1234);
        std::vector<std::uint32_t> copy(2000, 0);
        const std::int32_t length = utrie2_serialize_nid_postfix(trie, copy.data(), 8000, &status);
        Require(status == Success && length == 16 + 2112 * 2 + 196 * (bits == Bits16 ? 2 : 4));
        UTrie2* reopened = utrie2_openFromSerialized_nid_postfix(bits, copy.data(), length, nullptr, &status);
        Require(reopened && status == Success && utrie2_get32_nid_postfix(reopened, 0x2603) == 0x1234);
        utrie2_close_nid_postfix(reopened);
        utrie2_close_nid_postfix(trie);
    }
    std::int32_t status = Success;
    Require(utrie2_openDummy_nid_postfix(-1, 0, 0, &status) == nullptr && status == IllegalArgument);
}

}

int main() {
    Serialization();
    Errors();
    Dummy();
    std::puts("ICU UTrie2 checks passed");
    return 0;
}
