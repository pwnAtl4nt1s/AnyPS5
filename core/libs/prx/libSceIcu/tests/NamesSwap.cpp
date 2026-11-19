#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/NamesSwap.hpp"
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
#include <string>
#include <vector>

U_CAPI int32_t U_EXPORT2 uchar_swapNames(const UDataSwapper* ds, const void* inData, int32_t length, void* outData, UErrorCode* pErrorCode);

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
    std::fprintf(stderr, "ICU names swap check failed at line %u\n", location.line());
    std::abort();
}

struct Options {
    std::uint16_t TokenCount = 280;
    bool VariantDirect = false;
    std::uint8_t RangeType = 1;
    bool LongLines = true;
    std::uint8_t Version = 1;
};

struct Builder {
    std::vector<std::uint8_t> Bytes;

    void Align() {
        while (Bytes.size() % 4 != 0) Bytes.push_back(0);
    }
    void U16(std::uint16_t value) {
        Bytes.push_back(static_cast<std::uint8_t>(value));
        Bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    void U32(std::uint32_t value, std::size_t at) {
        for (std::size_t index = 0; index < 4; ++index) Bytes[at + index] = static_cast<std::uint8_t>(value >> (8 * index));
    }
    void U32(std::uint32_t value) {
        Bytes.resize(Bytes.size() + 4);
        U32(value, Bytes.size() - 4);
    }
    void Text(const char* text) {
        for (std::size_t index = 0; index <= std::strlen(text); ++index) Bytes.push_back(static_cast<std::uint8_t>(text[index]));
    }
};

std::vector<std::uint32_t> Image(std::mt19937& random, const Options& options) {
    Builder data;
    data.Bytes.resize(16);
    data.U16(options.TokenCount);
    std::vector<std::int16_t> tokens(options.TokenCount, 0);
    const std::string direct = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
    std::vector<std::uint8_t> singles;
    std::vector<std::uint8_t> leads;
    for (std::uint16_t index = 0; index < options.TokenCount; ++index) {
        if (index < 256 && direct.find(static_cast<char>(index)) != std::string::npos) {
            tokens[index] = -1;
        } else if (index == 3 || index == 4) {
            tokens[index] = -2;
        } else {
            tokens[index] = static_cast<std::int16_t>((index % 5) * 4);
        }
        if (index > 0 && index < 256 && tokens[index] != -2) singles.push_back(static_cast<std::uint8_t>(index));
        if (index < 256 && tokens[index] == -2) leads.push_back(static_cast<std::uint8_t>(index));
    }
    if (options.VariantDirect && options.TokenCount > '#') tokens['#'] = -1;
    for (const std::uint16_t index : {std::uint16_t{300}, std::uint16_t{304}, std::uint16_t{321}}) {
        if (index < options.TokenCount) tokens[index] = -1;
    }
    for (const std::int16_t token : tokens) data.U16(static_cast<std::uint16_t>(token));
    data.Align();
    const auto tokenStringOffset = static_cast<std::uint32_t>(data.Bytes.size());
    data.Text("LATIN");
    data.Text("SMALL");
    data.Text("CAPITAL");
    data.Text("LETTER");
    data.Bytes.push_back(7);
    data.Align();
    const auto groupsOffset = static_cast<std::uint32_t>(data.Bytes.size());
    const std::uint16_t groupCount = 3;
    data.U16(groupCount);
    std::vector<std::uint8_t> strings;
    for (std::uint16_t group = 0; group < groupCount; ++group) {
        data.U16(static_cast<std::uint16_t>(group * 2));
        data.U16(0);
        data.U16(static_cast<std::uint16_t>(strings.size()));
        std::vector<std::uint8_t> nibbles;
        std::vector<std::uint8_t> body;
        for (std::uint16_t line = 0; line < 32; ++line) {
            std::uint16_t length = static_cast<std::uint16_t>(random() % 12);
            if (options.LongLines && line % 7 == 3) length = static_cast<std::uint16_t>(12 + random() % 64);
            if (options.LongLines && line == 2) length = 12;
            if (options.LongLines && line == 5) length = 13;
            if (length < 12) {
                nibbles.push_back(static_cast<std::uint8_t>(length));
            } else {
                nibbles.push_back(static_cast<std::uint8_t>(0xc | ((length - 12) >> 4)));
                nibbles.push_back(static_cast<std::uint8_t>((length - 12) & 0xf));
            }
            std::uint16_t written = 0;
            while (written < length) {
                const bool pair = options.TokenCount > 256 && !leads.empty() && length - written >= 2 && random() % 3 == 0;
                if (pair) {
                    body.push_back(leads[random() % leads.size()]);
                    body.push_back(static_cast<std::uint8_t>(random() % (options.TokenCount - 256)));
                    written = static_cast<std::uint16_t>(written + 2);
                } else {
                    body.push_back(singles[random() % singles.size()]);
                    ++written;
                }
            }
        }
        if (nibbles.size() % 2 != 0) nibbles.push_back(0);
        for (std::size_t index = 0; index < nibbles.size(); index += 2) strings.push_back(static_cast<std::uint8_t>(nibbles[index] << 4 | nibbles[index + 1]));
        strings.insert(strings.end(), body.begin(), body.end());
    }
    data.Align();
    const auto groupStringOffset = static_cast<std::uint32_t>(data.Bytes.size());
    data.Bytes.insert(data.Bytes.end(), strings.begin(), strings.end());
    data.Align();
    const auto algorithmicOffset = static_cast<std::uint32_t>(data.Bytes.size());
    data.U32(3);
    const auto range = [&](std::uint8_t type, std::uint8_t variant, const std::vector<std::uint16_t>& factors, const std::vector<const char*>& texts) {
        const std::size_t start = data.Bytes.size();
        data.U32(0x4e00);
        data.U32(0x9fff);
        data.Bytes.push_back(type);
        data.Bytes.push_back(variant);
        data.U16(0);
        for (const std::uint16_t factor : factors) data.U16(factor);
        for (const char* text : texts) data.Text(text);
        while (data.Bytes.size() % 4 != 0) data.Bytes.push_back(type == 1 ? 'Q' : 0);
        const auto size = static_cast<std::uint16_t>(data.Bytes.size() - start);
        data.Bytes[start + 10] = static_cast<std::uint8_t>(size);
        data.Bytes[start + 11] = static_cast<std::uint8_t>(size >> 8);
    };
    range(0, 4, {}, {"CJK UNIFIED IDEOGRAPH-"});
    range(1, 3, {19, 21, 28}, {"HANGUL SYLLABLE ", "G", "GG", "N", "A", "AE", "", "K"});
    range(options.RangeType, 2, {5, 6}, {"PREFIX ", "X", "Y"});
    data.U32(tokenStringOffset, 0);
    data.U32(groupsOffset, 4);
    data.U32(groupStringOffset, 8);
    data.U32(algorithmicOffset, 12);
    std::vector<std::uint8_t> image(32, 0);
    const std::uint16_t headerSize = 32;
    const std::uint16_t infoSize = 20;
    std::memcpy(image.data(), &headerSize, 2);
    image[2] = 0xda;
    image[3] = 0x27;
    std::memcpy(image.data() + 4, &infoSize, 2);
    image[10] = 2;
    std::memcpy(image.data() + 12, "unam", 4);
    image[16] = options.Version;
    std::memcpy(image.data() + 24, "Names", 6);
    image.insert(image.end(), data.Bytes.begin(), data.Bytes.end());
    std::vector<std::uint32_t> words((image.size() + 3) / 4 + 2, 0);
    std::memcpy(words.data(), image.data(), image.size());
    return words;
}

std::vector<std::uint32_t> Convert(const std::vector<std::uint32_t>& input, std::uint8_t inBig, std::uint8_t inCharset, std::uint8_t outBig, std::uint8_t outCharset) {
    UErrorCode status = U_ZERO_ERROR;
    UDataSwapper* swapper = udata_openSwapper(inBig, inCharset, outBig, outCharset, &status);
    std::vector<std::uint32_t> output(input.size(), 0);
    uchar_swapNames(swapper, input.data(), static_cast<int32_t>(input.size() * 4), output.data(), &status);
    udata_closeSwapper(swapper);
    if (U_FAILURE(status)) return {};
    return output;
}

int compared = 0;

void Compare(const std::vector<std::uint32_t>& input, std::uint8_t inBig, std::uint8_t inCharset, std::uint8_t outBig, std::uint8_t outCharset) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    DataSwapper* guest = udata_openSwapper_nid_postfix(inBig, inCharset, outBig, outCharset, &guestStatus);
    UDataSwapper* host = udata_openSwapper(inBig, inCharset, outBig, outCharset, &hostStatus);
    const auto bytes = static_cast<std::int32_t>(input.size() * 4);
    std::vector<std::int32_t> lengths{-1, 0, 40, 51, 52, 60, bytes / 2};
    for (std::int32_t length = bytes - 160; length <= bytes; length += 4) lengths.push_back(length);
    for (const std::int32_t length : lengths) {
        for (const bool inPlace : {false, true}) {
            std::vector<std::uint32_t> guestOut(input.size(), 0x5a5a5a5au);
            std::vector<std::uint32_t> hostOut(input.size(), 0x5a5a5a5au);
            if (inPlace) {
                guestOut = input;
                hostOut = input;
            }
            guestStatus = 0;
            hostStatus = U_ZERO_ERROR;
            const std::int32_t guestResult = uchar_swapNames_nid_postfix(guest, inPlace ? guestOut.data() : input.data(), length, guestOut.data(), &guestStatus);
            const std::int32_t hostResult = uchar_swapNames(host, inPlace ? hostOut.data() : input.data(), length, hostOut.data(), &hostStatus);
            Require(guestResult == hostResult);
            Require(guestStatus == hostStatus);
            Require(guestOut == hostOut);
            if (length == bytes && U_SUCCESS(hostStatus)) ++compared;
        }
    }
    std::int32_t failed = 9;
    Require(uchar_swapNames_nid_postfix(guest, input.data(), -1, nullptr, &failed) == 0 && failed == 9);
    Require(uchar_swapNames_nid_postfix(guest, input.data(), -1, nullptr, nullptr) == 0);
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
    std::mt19937 random(37);
    for (const std::uint16_t tokenCount : {std::uint16_t{256}, std::uint16_t{280}, std::uint16_t{512}}) {
        for (const bool variant : {false, true}) {
            for (const std::uint8_t rangeType : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}}) {
                for (const bool longLines : {false, true}) {
                    for (const std::uint8_t version : {std::uint8_t{1}, std::uint8_t{2}}) {
                        Options options{tokenCount, variant, rangeType, longLines, version};
                        const auto image = Image(random, options);
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
    }
    Require(compared > 100);
    std::printf("ICU names swap checks passed (%d full swaps)\n", compared);
    return 0;
}
