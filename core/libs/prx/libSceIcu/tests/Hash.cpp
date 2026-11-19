#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <type_traits>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU string hash check failed at line %u\n", location.line());
    std::abort();
}

template<class TUnit>
std::int32_t Reference(const std::vector<TUnit>& text, bool fold) {
    std::uint32_t hash = 0;
    const auto length = static_cast<std::int32_t>(text.size());
    const std::int32_t step = ((length - 32) / 32) + 1;
    for (std::int32_t index = 0; index < length; index += step) {
        std::uint32_t unit = static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<TUnit>>(text[static_cast<std::size_t>(index)]));
        if (fold && unit >= 'A' && unit <= 'Z') unit += 'a' - 'A';
        hash = hash * 37u + unit;
    }
    return static_cast<std::int32_t>(hash);
}

void Known() {
    const char16_t ab[] = u"ab";
    Require(ustr_hashUCharsN_nid_postfix(ab, 1) == 97);
    Require(ustr_hashUCharsN_nid_postfix(ab, 2) == 97 * 37 + 98);
    Require(ustr_hashCharsN_nid_postfix("ab", 2) == 97 * 37 + 98);
    Require(ustr_hashICharsN_nid_postfix("AB", 2) == 97 * 37 + 98);
    Require(ustr_hashCharsN_nid_postfix("AB", 2) == 65 * 37 + 66);
    Require(ustr_hashCharsN_nid_postfix("\xFF", 1) == 255);
    Require(ustr_hashICharsN_nid_postfix("\xC0", 1) == 0xC0);
    Require(ustr_hashUCharsN_nid_postfix(nullptr, 5) == 0);
    Require(ustr_hashCharsN_nid_postfix(nullptr, 5) == 0);
    Require(ustr_hashICharsN_nid_postfix(nullptr, 5) == 0);
    Require(ustr_hashUCharsN_nid_postfix(ab, 0) == 0);
    Require(ustr_hashCharsN_nid_postfix("ab", -3) == 0);
    const char16_t surrogate[] = {0xD83D, 0xDE00};
    Require(ustr_hashUCharsN_nid_postfix(surrogate, 2) == static_cast<std::int32_t>(0xD83Du * 37u + 0xDE00u));
}

void Strided() {
    for (const std::size_t length : {31u, 32u, 33u, 63u, 64u, 65u, 95u, 96u, 200u, 1000u, 4097u}) {
        std::vector<char16_t> wide(length);
        std::vector<char> narrow(length);
        for (std::size_t index = 0; index < length; ++index) {
            wide[index] = static_cast<char16_t>(0x41 + (index * 7919u) % 0xFF00u);
            narrow[index] = static_cast<char>(0x20 + (index * 31u) % 0xE0u);
        }
        const auto count = static_cast<std::int32_t>(length);
        Require(ustr_hashUCharsN_nid_postfix(wide.data(), count) == Reference(wide, false));
        Require(ustr_hashCharsN_nid_postfix(narrow.data(), count) == Reference(narrow, false));
        Require(ustr_hashICharsN_nid_postfix(narrow.data(), count) == Reference(narrow, true));
    }
    std::vector<char> text(64, 'x');
    const auto before = ustr_hashCharsN_nid_postfix(text.data(), 64);
    text[1] = 'y';
    Require(ustr_hashCharsN_nid_postfix(text.data(), 64) == before);
    text[2] = 'y';
    Require(ustr_hashCharsN_nid_postfix(text.data(), 64) != before);
}

}

int main() {
    Known();
    Strided();
    std::puts("ICU string hash checks passed");
    return 0;
}
