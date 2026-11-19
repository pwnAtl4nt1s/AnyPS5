#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/CommonHelpers.hpp"
#include "propname.h"
#include "putilimp.h"
#include "ustrfmt.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU common helper check failed at line %u\n", location.line());
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

void Integers() {
    std::mt19937 random(11);
    for (int round = 0; round < 20000; ++round) {
        const std::uint32_t value = round % 4 == 0 ? static_cast<std::uint32_t>(random() % 100) : static_cast<std::uint32_t>(random());
        const std::uint32_t radix = 2 + static_cast<std::uint32_t>(random() % 40);
        const std::int32_t capacity = static_cast<std::int32_t>(random() % 40);
        const std::int32_t width = static_cast<std::int32_t>(random() % 12) - 2;
        std::array<char16_t, 96> guest{};
        std::array<char16_t, 96> host{};
        guest.fill(u'#');
        host.fill(u'#');
        Require(uprv_itou_nid_postfix(guest.data(), capacity, value, radix, width) == uprv_itou(host.data(), capacity, value, radix, width));
        Require(guest == host);
    }
    std::array<char16_t, 4> buffer{};
    Require(Rejects([&] { uprv_itou_nid_postfix(buffer.data(), 4, 5, 0, 1); }));
    Require(Rejects([&] { uprv_itou_nid_postfix(nullptr, 4, 5, 10, 1); }));
}

std::string Name(std::mt19937& random, bool ebcdic) {
    static const std::string ascii = "aAbBzZ09-_ \t\n\v\f\r@[`{\x7f\x80\xc1\xe9";
    static const std::string extra = std::string("\x60\x6d\x40\x05\x15\x25\x0b\x0c\x0d\x81\xc1\xc9\xd1\xd9\xe2\xe9\x89", 17);
    const std::string& alphabet = ebcdic && random() % 2 == 0 ? extra : ascii;
    std::string name;
    const auto length = random() % 9;
    for (std::uint32_t index = 0; index < length; ++index) name.push_back(alphabet[random() % alphabet.size()]);
    return name;
}

void Names() {
    std::mt19937 random(23);
    for (int round = 0; round < 50000; ++round) {
        const bool ebcdic = round % 2 == 1;
        const std::string left = Name(random, ebcdic);
        const std::string right = round % 7 == 0 ? left : Name(random, ebcdic);
        Require(uprv_compareASCIIPropertyNames_nid_postfix(left.c_str(), right.c_str()) == uprv_compareASCIIPropertyNames(left.c_str(), right.c_str()));
        Require(uprv_compareEBCDICPropertyNames_nid_postfix(left.c_str(), right.c_str()) == uprv_compareEBCDICPropertyNames(left.c_str(), right.c_str()));
    }
    Require(uprv_compareASCIIPropertyNames_nid_postfix("General_Category", "general category") == 0);
    Require(uprv_compareASCIIPropertyNames_nid_postfix("gc", "gcb") < 0);
    Require(Rejects([] { uprv_compareASCIIPropertyNames_nid_postfix(nullptr, "a"); }));
    Require(Rejects([] { uprv_compareEBCDICPropertyNames_nid_postfix("a", nullptr); }));
}

void Paths() {
    for (const char* path : {"", "/", "/data/icu", "data/icu", "./x", "C:/x", "c:\\x", "\\x", "a:"}) {
        const bool expected = path[0] == '/';
        Require((uprv_pathIsAbsolute_nid_postfix(path) != 0) == expected);
#ifndef _WIN32
        Require((uprv_pathIsAbsolute_nid_postfix(path) != 0) == (uprv_pathIsAbsolute(path) != 0));
#endif
    }
    Require(uprv_pathIsAbsolute_nid_postfix(nullptr) == 0);
}

}

int main() {
    Integers();
    Names();
    Paths();
    std::puts("ICU common helper checks passed");
    return 0;
}
