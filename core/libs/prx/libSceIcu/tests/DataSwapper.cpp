#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/DataSwapper.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "udataswp.h"
#include "uinvchar.h"
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
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
    std::fprintf(stderr, "ICU data swapper check failed at line %u\n", location.line());
    std::abort();
}

std::string guestMessage;
std::string hostMessage;

void APS5_VABI GuestPrinter(void* context, const char* format, VaList* arguments) {
    Require(context == &guestMessage);
#ifdef _WIN32
    static_cast<void>(arguments);
    guestMessage = format;
#else
    std::array<char, 512> buffer{};
    std::vsnprintf(buffer.data(), buffer.size(), format, reinterpret_cast<std::decay_t<std::va_list>>(arguments));
    guestMessage = buffer.data();
#endif
}

void U_CALLCONV HostPrinter(void* context, const char* format, va_list arguments) {
    Require(context == &hostMessage);
#ifdef _WIN32
    static_cast<void>(arguments);
    hostMessage = format;
#else
    std::array<char, 512> buffer{};
    std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    hostMessage = buffer.data();
#endif
}

struct Pair {
    DataSwapper* Guest = nullptr;
    UDataSwapper* Host = nullptr;
};

Pair Open(std::uint8_t inBig, std::uint8_t inCharset, std::uint8_t outBig, std::uint8_t outCharset) {
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    Pair pair{udata_openSwapper_nid_postfix(inBig, inCharset, outBig, outCharset, &guestStatus),
        udata_openSwapper(inBig, inCharset, outBig, outCharset, &hostStatus)};
    Require(guestStatus == hostStatus);
    Require((pair.Guest == nullptr) == (pair.Host == nullptr));
    if (pair.Guest) {
        Require(pair.Guest->InIsBigEndian == pair.Host->inIsBigEndian && pair.Guest->InCharset == pair.Host->inCharset);
        Require(pair.Guest->OutIsBigEndian == pair.Host->outIsBigEndian && pair.Guest->OutCharset == pair.Host->outCharset);
        Require(pair.Guest->PrintError == nullptr && pair.Guest->PrintErrorContext == nullptr);
        pair.Guest->PrintError = GuestPrinter;
        pair.Guest->PrintErrorContext = &guestMessage;
        pair.Host->printError = HostPrinter;
        pair.Host->printErrorContext = &hostMessage;
    }
    return pair;
}

void Close(Pair pair) {
    udata_closeSwapper_nid_postfix(pair.Guest);
    udata_closeSwapper(pair.Host);
}

template<class TGuest, class THost>
void CompareSwap(TGuest guest, THost host, const std::vector<std::uint8_t>& input, std::int32_t length, bool inPlace, bool withOutput = true) {
    std::vector<std::uint8_t> guestOut(input.size() + 8, 0xcc);
    std::vector<std::uint8_t> hostOut(input.size() + 8, 0xcc);
    if (inPlace) {
        std::copy(input.begin(), input.end(), guestOut.begin());
        std::copy(input.begin(), input.end(), hostOut.begin());
    }
    std::int32_t guestStatus = 0;
    UErrorCode hostStatus = U_ZERO_ERROR;
    guestMessage.clear();
    hostMessage.clear();
    const void* guestIn = inPlace ? guestOut.data() : input.data();
    const void* hostIn = inPlace ? hostOut.data() : input.data();
    const std::int32_t guestResult = guest(guestIn, length, withOutput ? guestOut.data() : nullptr, &guestStatus);
    const std::int32_t hostResult = host(hostIn, length, withOutput ? hostOut.data() : nullptr, &hostStatus);
    Require(guestResult == hostResult);
    Require(guestStatus == hostStatus);
    Require(guestOut == hostOut);
    Require(guestMessage == hostMessage);
}

std::vector<std::uint8_t> RandomBytes(std::mt19937& random, std::size_t size, bool printable) {
    std::vector<std::uint8_t> bytes(size);
    for (auto& byte : bytes) {
        const auto value = static_cast<std::uint8_t>(random());
        byte = printable && (random() % 8) != 0 ? static_cast<std::uint8_t>(0x20 + value % 0x5f) : value;
    }
    return bytes;
}

void Swappers() {
    std::mt19937 random(5);
    for (std::uint8_t inBig = 0; inBig < 2; ++inBig) {
        for (std::uint8_t inCharset = 0; inCharset < 3; ++inCharset) {
            for (std::uint8_t outBig = 0; outBig < 2; ++outBig) {
                for (std::uint8_t outCharset = 0; outCharset < 3; ++outCharset) {
                    Pair pair = Open(inBig, inCharset, outBig, outCharset);
                    if (!pair.Guest) continue;
                    for (const std::uint32_t value : {0u, 1u, 0x1234u, 0xfedcba98u, 0x80000000u}) {
                        Require(pair.Guest->ReadUInt16(static_cast<std::uint16_t>(value)) == pair.Host->readUInt16(static_cast<std::uint16_t>(value)));
                        Require(pair.Guest->ReadUInt32(value) == pair.Host->readUInt32(value));
                        Require(udata_readInt16_nid_postfix(pair.Guest, static_cast<std::int16_t>(value)) == udata_readInt16(pair.Host, static_cast<std::int16_t>(value)));
                        Require(udata_readInt32_nid_postfix(pair.Guest, static_cast<std::int32_t>(value)) == udata_readInt32(pair.Host, static_cast<std::int32_t>(value)));
                        std::uint16_t guest16 = 0;
                        std::uint16_t host16 = 0;
                        std::uint32_t guest32 = 0;
                        std::uint32_t host32 = 0;
                        pair.Guest->WriteUInt16(&guest16, static_cast<std::uint16_t>(value));
                        pair.Host->writeUInt16(&host16, static_cast<std::uint16_t>(value));
                        pair.Guest->WriteUInt32(&guest32, value);
                        pair.Host->writeUInt32(&host32, value);
                        Require(guest16 == host16 && guest32 == host32);
                    }
                    const std::array<DataSwapFunction, 4> guestSwaps{pair.Guest->SwapArray16, pair.Guest->SwapArray32, pair.Guest->SwapArray64, pair.Guest->SwapInvChars};
                    const std::array<UDataSwapFn*, 4> hostSwaps{pair.Host->swapArray16, pair.Host->swapArray32, pair.Host->swapArray64, pair.Host->swapInvChars};
                    for (std::size_t kind = 0; kind < guestSwaps.size(); ++kind) {
                        for (const std::int32_t length : {-1, 0, 1, 2, 3, 4, 6, 8, 12, 16, 40}) {
                            for (int round = 0; round < 6; ++round) {
                                const auto input = RandomBytes(random, 48, kind == 3 && round < 4);
                                for (const bool inPlace : {false, true}) {
                                    CompareSwap([&](const void* in, std::int32_t n, void* out, std::int32_t* s) { return guestSwaps[kind](pair.Guest, in, n, out, s); },
                                        [&](const void* in, std::int32_t n, void* out, UErrorCode* s) { return hostSwaps[kind](pair.Host, in, n, out, s); }, input, length, inPlace);
                                }
                                CompareSwap([&](const void* in, std::int32_t n, void* out, std::int32_t* s) { return guestSwaps[kind](pair.Guest, in, n, out, s); },
                                    [&](const void* in, std::int32_t n, void* out, UErrorCode* s) { return hostSwaps[kind](pair.Host, in, n, out, s); }, input, length, false, false);
                                CompareSwap([&](const void* in, std::int32_t n, void* out, std::int32_t* s) { return udata_swapInvStringBlock_nid_postfix(pair.Guest, in, n, out, s); },
                                    [&](const void* in, std::int32_t n, void* out, UErrorCode* s) { return udata_swapInvStringBlock(pair.Host, in, n, out, s); }, input, length, round % 2 == 0);
                            }
                        }
                        std::int32_t failed = 3;
                        UErrorCode hostFailed = U_INTERNAL_PROGRAM_ERROR;
                        const std::uint64_t word = 7;
                        Require(guestSwaps[kind](pair.Guest, &word, 8, nullptr, &failed) == hostSwaps[kind](pair.Host, &word, 8, nullptr, &hostFailed));
                        Require(failed == 3);
                    }
                    const char16_t* local[] = {u"", u"abc", u"ab", u"abd", u"a[b", u"AZaz09", u"xé", u"abc!"};
                    const char* asciiOut[] = {"", "abc", "ab", "abd", "a[b", "AZaz09", "x\xe9", "abc!", "a#c"};
                    for (const char* outText : asciiOut) {
                        std::array<char, 16> encoded{};
                        std::strncpy(encoded.data(), outText, encoded.size() - 1);
                        if (outCharset == 1) {
                            std::int32_t status = 0;
                            UErrorCode hostStatus = U_ZERO_ERROR;
                            std::array<char, 16> host{};
                            const auto n = static_cast<std::int32_t>(std::strlen(outText));
                            encoded.fill(0);
                            uprv_ebcdicFromAscii_nid_postfix(pair.Guest, outText, n, encoded.data(), &status);
                            uprv_ebcdicFromAscii(pair.Host, outText, n, host.data(), &hostStatus);
                            Require(status == hostStatus && encoded == host);
                        }
                        for (const char16_t* localText : local) {
                            for (const std::int32_t outLength : {-1, 0, 1, 2, 3}) {
                                for (const std::int32_t localLength : {-1, 0, 1, 2, 3}) {
                                    Require(pair.Guest->CompareInvChars(pair.Guest, encoded.data(), outLength, localText, localLength) ==
                                        pair.Host->compareInvChars(pair.Host, encoded.data(), outLength, localText, localLength));
                                }
                            }
                        }
                    }
                    Require(pair.Guest->CompareInvChars(pair.Guest, nullptr, -1, u"a", -1) == 0);
                    Require(pair.Guest->CompareInvChars(pair.Guest, "a", -2, u"a", -1) == 0);
                    Close(pair);
                }
            }
        }
    }
}

std::vector<std::uint8_t> Header(bool bigEndian, std::uint8_t charset, std::uint16_t headerSize, std::uint16_t infoSize, const char* copyright) {
    std::vector<std::uint8_t> bytes(96, 0);
    const auto put16 = [&](std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(bigEndian ? value >> 8 : value);
        bytes[offset + 1] = static_cast<std::uint8_t>(bigEndian ? value : value >> 8);
    };
    put16(0, headerSize);
    bytes[2] = 0xda;
    bytes[3] = 0x27;
    put16(4, infoSize);
    put16(6, 0x1357);
    bytes[8] = bigEndian;
    bytes[9] = charset;
    bytes[10] = 2;
    std::memcpy(&bytes[12], "Test", 4);
    bytes[16] = 1;
    bytes[20] = 2;
    std::memcpy(&bytes[4 + infoSize], copyright, std::strlen(copyright));
    return bytes;
}

void Headers() {
    const char* copyrights[] = {"Copyright (C) Unicode", "", "a@b", "long copyright text that fills the rest of the header area!!!!!!"};
    for (const bool bigEndian : {false, true}) {
        for (std::uint8_t charset = 0; charset < 3; ++charset) {
            for (const std::uint16_t headerSize : {std::uint16_t{16}, std::uint16_t{24}, std::uint16_t{32}, std::uint16_t{64}, std::uint16_t{96}, std::uint16_t{200}}) {
                for (const std::uint16_t infoSize : {std::uint16_t{18}, std::uint16_t{20}, std::uint16_t{24}, std::uint16_t{60}}) {
                    for (const char* copyright : copyrights) {
                        if (4u + infoSize + std::strlen(copyright) > 96) continue;
                        auto bytes = Header(bigEndian, charset, headerSize, infoSize, copyright);
                        for (const std::int32_t length : {-1, 0, 10, 24, 32, 64, 96}) {
                            std::int32_t guestStatus = 0;
                            UErrorCode hostStatus = U_ZERO_ERROR;
                            DataSwapper* guest = udata_openSwapperForInputData_nid_postfix(bytes.data(), length, !bigEndian, charset == 0 ? 1 : 0, &guestStatus);
                            UDataSwapper* host = udata_openSwapperForInputData(bytes.data(), length, !bigEndian, charset == 0 ? 1 : 0, &hostStatus);
                            Require(guestStatus == hostStatus && (guest == nullptr) == (host == nullptr));
                            if (guest) {
                                Require(guest->InIsBigEndian == host->inIsBigEndian && guest->InCharset == host->inCharset);
                                guest->PrintError = GuestPrinter;
                                guest->PrintErrorContext = &guestMessage;
                                host->printError = HostPrinter;
                                host->printErrorContext = &hostMessage;
                                for (const bool inPlace : {false, true}) {
                                    CompareSwap([&](const void* in, std::int32_t n, void* out, std::int32_t* s) { return udata_swapDataHeader_nid_postfix(guest, in, n, out, s); },
                                        [&](const void* in, std::int32_t n, void* out, UErrorCode* s) { return udata_swapDataHeader(host, in, n, out, s); }, bytes, length, inPlace);
                                }
                                udata_closeSwapper_nid_postfix(guest);
                                udata_closeSwapper(host);
                            }
                        }
                    }
                }
            }
        }
    }
    auto bytes = Header(false, 0, 64, 20, "x");
    bytes[2] = 0xdb;
    Pair pair = Open(0, 0, 1, 1);
    CompareSwap([&](const void* in, std::int32_t n, void* out, std::int32_t* s) { return udata_swapDataHeader_nid_postfix(pair.Guest, in, n, out, s); },
        [&](const void* in, std::int32_t n, void* out, UErrorCode* s) { return udata_swapDataHeader(pair.Host, in, n, out, s); }, bytes, 64, false);
    Require(!guestMessage.empty());
    const auto valid = Header(false, 0, 64, 20, "x");
    for (const std::int32_t length : {-2, -1, 0, 10, 30, 64}) {
        CompareSwap([&](const void* in, std::int32_t n, void* out, std::int32_t* s) { return udata_swapDataHeader_nid_postfix(pair.Guest, in, n, out, s); },
            [&](const void* in, std::int32_t n, void* out, UErrorCode* s) { return udata_swapDataHeader(pair.Host, in, n, out, s); }, valid, length, false);
    }
    std::int32_t status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(nullptr, -1, 0, 0, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR);
    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(bytes.data(), -1, 0, 2, &status) == nullptr && status == U_ILLEGAL_ARGUMENT_ERROR);
    status = 0;
    Require(udata_openSwapperForInputData_nid_postfix(bytes.data(), -1, 0, 0, &status) == nullptr && status == U_UNSUPPORTED_ERROR);
    Require(udata_openSwapperForInputData_nid_postfix(bytes.data(), -1, 0, 0, nullptr) == nullptr);
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, nullptr) == nullptr);
    status = 5;
    Require(udata_openSwapper_nid_postfix(0, 0, 0, 0, &status) == nullptr && status == 5);
    pair.Guest->PrintError = nullptr;
    udata_printError_nid_postfix(pair.Guest, "%d", 1);
    Close(pair);
}

void Invariants() {
    std::mt19937 random(9);
    for (int round = 0; round < 2000; ++round) {
        const auto bytes = RandomBytes(random, 12, round % 2 == 0);
        std::vector<char> text(bytes.begin(), bytes.end());
        text.push_back(0);
        std::vector<char16_t> units;
        for (std::size_t index = 0; index < 12; ++index) units.push_back(static_cast<char16_t>(round % 3 == 0 ? random() : random() % 0x90));
        units.push_back(0);
        for (const std::int32_t length : {-1, 0, 1, 5, 12}) {
            Require(uprv_isInvariantString_nid_postfix(text.data(), length) == uprv_isInvariantString(text.data(), length));
            Require(uprv_isInvariantUString_nid_postfix(units.data(), length) == uprv_isInvariantUString(units.data(), length));
        }
        const auto other = RandomBytes(random, 12, round % 2 == 0);
        std::vector<char> second(other.begin(), other.end());
        second.push_back(0);
        if (round % 5 == 0) second = text;
        if (round % 7 == 0) second[static_cast<std::size_t>(random() % 12)] = 0;
        Require(uprv_compareInvEbcdicAsAscii_nid_postfix(text.data(), second.data()) == uprv_compareInvEbcdicAsAscii(text.data(), second.data()));
        for (const std::int32_t length : {-1, 0, 3, 12, 16}) {
            std::array<std::uint8_t, 20> guest{};
            std::array<std::uint8_t, 20> host{};
            guest.fill(0x55);
            host.fill(0x55);
            const auto* source = reinterpret_cast<const std::uint8_t*>(text.data());
            Require(uprv_aestrncpy_nid_postfix(guest.data(), source, length) == guest.data());
            uprv_aestrncpy(host.data(), source, length);
            Require(guest == host);
            Require(uprv_eastrncpy_nid_postfix(guest.data(), source, length) == guest.data());
            uprv_eastrncpy(host.data(), source, length);
            Require(guest == host);
        }
    }
    for (int value = 0; value < 256; ++value) {
        Require(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(value)) == uprv_ebcdicToAscii(static_cast<char>(value)));
        Require(uprv_ebcdicToLowercaseAscii_nid_postfix(static_cast<char>(value)) == uprv_ebcdicToLowercaseAscii(static_cast<char>(value)));
    }
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Swappers();
    Headers();
    Invariants();
    std::puts("ICU data swapper checks passed");
    return 0;
}
