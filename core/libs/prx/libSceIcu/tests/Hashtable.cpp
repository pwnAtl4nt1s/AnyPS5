#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "uhash.h"
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
    std::fprintf(stderr, "ICU hashtable check failed at line %u\n", location.line());
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

std::int32_t APS5_VABI CollidingHash(HashKey key) {
    return key.Integer & 3;
}

std::int32_t APS5_VABI IcuCollidingHash(UElement key) {
    return key.integer & 3;
}

std::map<void*, int> deletedKeys;
std::map<void*, int> deletedValues;

void APS5_VABI DeleteKey(void* key) { ++deletedKeys[key]; }
void APS5_VABI DeleteValue(void* value) { ++deletedValues[value]; }

void SameLayout(const Hashtable* ours, const UHashtable* icu, const std::string& step) {
    const auto fail = [&](const char* what) {
        std::fprintf(stderr, "ICU hashtable differs from ICU after %s: %s\n", step.c_str(), what);
        std::abort();
    };
    if (ours->Count != icu->count) fail("count");
    if (ours->Length != icu->length) fail("length");
    if (ours->HighWaterMark != icu->highWaterMark || ours->LowWaterMark != icu->lowWaterMark) fail("water marks");
    if (ours->PrimeIndex != icu->primeIndex) fail("prime index");
    for (std::int32_t index = 0; index < ours->Length; ++index) {
        const auto& left = ours->Elements[index];
        const auto& right = icu->elements[index];
        if (left.Hashcode != right.hashcode) fail("slot hashcode");
        if (left.Hashcode >= 0 && (left.Key.Integer != right.key.integer || left.Value.Integer != right.value.integer)) fail("slot content");
    }
}

void AgainstIcu(bool integers, bool colliding, std::int32_t policy, std::uint32_t seed) {
    std::int32_t status = 0;
    UErrorCode icuStatus = U_ZERO_ERROR;
    Hashtable* ours = nullptr;
    UHashtable* icu = nullptr;
    if (integers && colliding) {
        ours = uhash_open_nid_postfix(CollidingHash, uhash_compareLong_nid_postfix, uhash_compareLong_nid_postfix, &status);
        icu = uhash_open(IcuCollidingHash, uhash_compareLong, uhash_compareLong, &icuStatus);
    } else if (integers) {
        ours = uhash_openSize_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, uhash_compareLong_nid_postfix, 3, &status);
        icu = uhash_openSize(uhash_hashLong, uhash_compareLong, uhash_compareLong, 3, &icuStatus);
    } else {
        ours = uhash_open_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr, &status);
        icu = uhash_open(uhash_hashChars, uhash_compareChars, nullptr, &icuStatus);
    }
    Require(ours && icu && status == 0 && U_SUCCESS(icuStatus) && ours->Allocated == 1);
    uhash_setResizePolicy_nid_postfix(ours, policy);
    uhash_setResizePolicy(icu, static_cast<UHashResizePolicy>(policy));
    std::vector<std::string> words;
    for (int index = 0; index < 400; ++index) words.push_back("key-" + std::to_string(index * 7919 % 1000));
    std::mt19937 random(seed);
    const std::int32_t limit = policy == 2 ? 10 : 400;
    for (int step = 0; step < 3000; ++step) {
        const int operation = static_cast<int>(random() % 10);
        const std::int32_t number = static_cast<std::int32_t>(random() % static_cast<std::uint32_t>(limit)) - limit / 2;
        auto* word = words[random() % static_cast<std::uint32_t>(limit < 400 ? limit : 400)].data();
        const std::int32_t value = static_cast<std::int32_t>(random() % 100000) + 1;
        if (integers) {
            if (operation < 6) {
                Require(uhash_iputi_nid_postfix(ours, number, value, &status) == uhash_iputi(icu, number, value, &icuStatus));
            } else if (operation < 8) {
                Require(uhash_iremovei_nid_postfix(ours, number) == uhash_iremovei(icu, number));
            } else {
                Require(uhash_igeti_nid_postfix(ours, number) == uhash_igeti(icu, number));
            }
        } else {
            if (operation < 6) {
                Require(uhash_puti_nid_postfix(ours, word, value, &status) == uhash_puti(icu, word, value, &icuStatus));
            } else if (operation < 8) {
                Require(uhash_removei_nid_postfix(ours, word) == uhash_removei(icu, word));
            } else {
                std::string copy(word);
                Require(uhash_geti_nid_postfix(ours, copy.data()) == uhash_geti(icu, copy.data()));
                Require((uhash_find_nid_postfix(ours, copy.data()) == nullptr) == (uhash_find(icu, copy.data()) == nullptr));
            }
        }
        Require(status == static_cast<std::int32_t>(icuStatus));
        status = 0;
        icuStatus = U_ZERO_ERROR;
        SameLayout(ours, icu, "step " + std::to_string(step));
    }
    uhash_removeAll_nid_postfix(ours);
    uhash_removeAll(icu);
    SameLayout(ours, icu, "removeAll");
    uhash_close_nid_postfix(ours);
    uhash_close(icu);
}

void Pointers() {
    std::int32_t status = 0;
    Hashtable table{};
    Require(uhash_initSize_nid_postfix(&table, uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr, 100, &status) == &table);
    Require(table.Length == 127 && table.Allocated == 0 && table.HighWaterMark == 63 && table.LowWaterMark == 0);
    uhash_setKeyDeleter_nid_postfix(&table, DeleteKey);
    Require(uhash_setValueDeleter_nid_postfix(&table, DeleteValue) == nullptr);
    std::array<std::string, 3> keys{"alpha", "beta", "gamma"};
    std::array<int, 4> values{};
    Require(uhash_put_nid_postfix(&table, keys[0].data(), &values[0], &status) == nullptr);
    Require(uhash_put_nid_postfix(&table, keys[1].data(), &values[1], &status) == nullptr);
    Require(uhash_count_nid_postfix(&table) == 2 && uhash_get_nid_postfix(&table, "alpha") == &values[0]);
    Require(uhash_put_nid_postfix(&table, keys[0].data(), &values[2], &status) == nullptr);
    Require(deletedValues[&values[0]] == 1 && deletedKeys.empty() && uhash_get_nid_postfix(&table, "alpha") == &values[2]);
    std::string other = "alpha";
    Require(uhash_put_nid_postfix(&table, other.data(), &values[2], &status) == nullptr);
    Require(deletedKeys[keys[0].data()] == 1 && deletedValues[&values[2]] == 0);
    Require(uhash_put_nid_postfix(&table, keys[1].data(), nullptr, &status) == nullptr && uhash_count_nid_postfix(&table) == 1);
    Require(deletedKeys[keys[1].data()] == 1 && deletedValues[&values[1]] == 1);
    status = 1;
    Require(uhash_put_nid_postfix(&table, keys[2].data(), &values[3], &status) == nullptr && status == 1 && uhash_count_nid_postfix(&table) == 1);
    Require(deletedKeys[keys[2].data()] == 1 && deletedValues[&values[3]] == 1);
    status = 0;
    std::int32_t position = -1;
    const HashElement* element = uhash_nextElement_nid_postfix(&table, &position);
    Require(element && element->Key.Pointer == other.data() && element->Value.Pointer == &values[2] && position >= 0);
    Require(uhash_nextElement_nid_postfix(&table, &position) == nullptr);
    Require(uhash_find_nid_postfix(&table, "alpha") == element && uhash_find_nid_postfix(&table, "zeta") == nullptr);
    uhash_close_nid_postfix(&table);
    Require(deletedKeys[other.data()] == 1 && deletedValues[&values[2]] == 1 && table.Elements == nullptr);
}

void Integers() {
    std::int32_t status = 0;
    Hashtable* table = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, uhash_compareLong_nid_postfix, &status);
    Require(table && table->Length == 127);
    Require(uhash_iputi_nid_postfix(table, 5, 0, &status) == 0 && uhash_count_nid_postfix(table) == 1);
    Require(uhash_igeti_nid_postfix(table, 5) == 0 && uhash_iget_nid_postfix(table, 6) == nullptr);
    Require(uhash_iputi_nid_postfix(table, 5, 9, &status) == 0 && uhash_iputi_nid_postfix(table, 5, 10, &status) == 9);
    Require(uhash_iremovei_nid_postfix(table, 5) == 10 && uhash_count_nid_postfix(table) == 0);
    int marker = 0;
    Require(uhash_iput_nid_postfix(table, -3, &marker, &status) == nullptr && uhash_iget_nid_postfix(table, -3) == &marker);
    Require(uhash_iremove_nid_postfix(table, -3) == &marker);
    Hashtable* other = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, uhash_compareLong_nid_postfix, &status);
    for (std::int32_t key = 0; key < 50; ++key) {
        uhash_iputi_nid_postfix(table, key, key * 3 + 1, &status);
        uhash_iputi_nid_postfix(other, 49 - key, (49 - key) * 3 + 1, &status);
    }
    Require(table->Length == 127 && uhash_equals_nid_postfix(table, other) == 1 && uhash_equals_nid_postfix(table, table) == 1);
    uhash_iputi_nid_postfix(other, 7, 1, &status);
    Require(uhash_equals_nid_postfix(table, other) == 0 && uhash_equals_nid_postfix(table, nullptr) == 0);
    uhash_setValueComparator_nid_postfix(other, nullptr);
    uhash_setValueComparator_nid_postfix(table, nullptr);
    Require(uhash_equals_nid_postfix(table, other) == 0);
    uhash_setResizePolicy_nid_postfix(table, 1);
    for (std::int32_t key = 0; key < 50; ++key) uhash_iremovei_nid_postfix(table, key);
    Require(uhash_count_nid_postfix(table) == 0 && table->Length < 127 && table->LowWaterMark == static_cast<std::int32_t>(static_cast<float>(table->Length) * 0.1F));
    Require(Rejects([&] { uhash_setResizePolicy_nid_postfix(table, 3); }));
    uhash_close_nid_postfix(table);
    uhash_close_nid_postfix(other);
    uhash_close_nid_postfix(nullptr);
}

void Arguments() {
    std::int32_t status = 2;
    Require(uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr && status == 2);
    Hashtable table{};
    Require(uhash_init_nid_postfix(&table, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr && table.Elements == nullptr);
    Require(Rejects([&] { uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, nullptr); }));
    Require(Rejects([&] { uhash_count_nid_postfix(nullptr); }));
    Require(Rejects([&] { uhash_get_nid_postfix(nullptr, "a"); }));
    status = 0;
    Hashtable* small = uhash_openSize_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, 0, &status);
    Require(small && small->Length == 7);
    uhash_close_nid_postfix(small);
    Hashtable* huge = uhash_openSize_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, 3000, &status);
    Require(huge && huge->Length == 4093 && huge->PrimeIndex == 9);
    uhash_close_nid_postfix(huge);
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    for (const std::int32_t policy : {0, 1, 2}) {
        AgainstIcu(true, false, policy, 11u + static_cast<std::uint32_t>(policy));
        AgainstIcu(true, true, policy, 23u + static_cast<std::uint32_t>(policy));
        AgainstIcu(false, false, policy, 37u + static_cast<std::uint32_t>(policy));
    }
    Pointers();
    Integers();
    Arguments();
    std::puts("ICU hashtable checks passed");
    return 0;
}
