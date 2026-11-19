#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/ArraySort.hpp"
#include "uarrsort.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <source_location>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU array sort check failed at line %u\n", location.line());
    std::abort();
}

struct Context {
    std::int32_t KeyBytes;
    std::vector<std::pair<std::int32_t, std::int32_t>> Calls;
};

std::int32_t Key(const Context* context, const void* item) {
    std::int32_t key = 0;
    std::memcpy(&key, item, static_cast<std::size_t>(context->KeyBytes));
    return key;
}

std::int32_t Record(const void* context, const void* left, const void* right) {
    auto* state = const_cast<Context*>(static_cast<const Context*>(context));
    const std::int32_t a = Key(state, left);
    const std::int32_t b = Key(state, right);
    state->Calls.emplace_back(a, b);
    return a < b ? -1 : a == b ? 0 : 1;
}

std::int32_t APS5_VABI GuestCompare(const void* context, const void* left, const void* right) {
    return Record(context, left, right);
}

std::int32_t U_CALLCONV HostCompare(const void* context, const void* left, const void* right) {
    return Record(context, left, right);
}

std::vector<std::uint8_t> Items(std::mt19937& random, std::int32_t count, std::int32_t itemSize, std::int32_t keyBytes, std::int32_t range) {
    std::vector<std::uint8_t> items(static_cast<std::size_t>(count) * static_cast<std::size_t>(itemSize) + 1);
    for (auto& byte : items) byte = static_cast<std::uint8_t>(random());
    for (std::int32_t index = 0; index < count; ++index) {
        const std::int32_t key = static_cast<std::int32_t>(random() % static_cast<std::uint32_t>(range));
        std::memcpy(items.data() + static_cast<std::size_t>(index) * static_cast<std::size_t>(itemSize), &key, static_cast<std::size_t>(keyBytes));
    }
    return items;
}

void Sorts() {
    std::mt19937 random(3);
    for (const std::int32_t itemSize : {1, 3, 4, 8, 13, 200, 300}) {
        const std::int32_t keyBytes = itemSize < 4 ? itemSize : 4;
        for (const std::int32_t count : {0, 1, 2, 5, 8, 9, 10, 17, 64, 257}) {
            for (const std::int32_t range : {1, 3, 1000}) {
                for (const std::int8_t stable : {std::int8_t{0}, std::int8_t{1}}) {
                    auto guestItems = Items(random, count, itemSize, keyBytes, range);
                    auto hostItems = guestItems;
                    Context guestContext{keyBytes, {}};
                    Context hostContext{keyBytes, {}};
                    std::int32_t guestStatus = 0;
                    UErrorCode hostStatus = U_ZERO_ERROR;
                    uprv_sortArray_nid_postfix(guestItems.data(), count, itemSize, GuestCompare, &guestContext, stable, &guestStatus);
                    uprv_sortArray(hostItems.data(), count, itemSize, HostCompare, &hostContext, stable, &hostStatus);
                    Require(guestStatus == hostStatus);
                    Require(guestItems == hostItems);
                    Require(guestContext.Calls == hostContext.Calls);
                    for (std::int32_t limit = 0; limit <= count; limit += count / 7 + 1) {
                        for (int probe = 0; probe < 4; ++probe) {
                            auto item = Items(random, 1, itemSize, keyBytes, range + 1);
                            guestContext.Calls.clear();
                            hostContext.Calls.clear();
                            const std::int32_t guest = uprv_stableBinarySearch_nid_postfix(reinterpret_cast<char*>(guestItems.data()), limit, item.data(), itemSize,
                                GuestCompare, &guestContext);
                            const std::int32_t host = uprv_stableBinarySearch(reinterpret_cast<char*>(hostItems.data()), limit, item.data(), itemSize, HostCompare,
                                &hostContext);
                            Require(guest == host);
                            Require(guestContext.Calls == hostContext.Calls);
                        }
                    }
                }
            }
        }
    }
}

void Arguments() {
    std::int32_t items[3] = {3, 1, 2};
    Context context{4, {}};
    const auto check = [&](void* array, std::int32_t length, std::int32_t itemSize, bool withComparator, std::int32_t initial) {
        std::int32_t guestStatus = initial;
        UErrorCode hostStatus = static_cast<UErrorCode>(initial);
        uprv_sortArray_nid_postfix(array, length, itemSize, withComparator ? GuestCompare : nullptr, &context, 0, &guestStatus);
        uprv_sortArray(array, length, itemSize, withComparator ? HostCompare : nullptr, &context, 0, &hostStatus);
        Require(guestStatus == hostStatus);
    };
    check(nullptr, 3, 4, true, 0);
    check(nullptr, 0, 4, true, 0);
    check(items, -1, 4, true, 0);
    check(items, 3, 0, true, 0);
    check(items, 3, -4, true, 0);
    check(items, 3, 4, false, 0);
    check(items, 3, 4, true, 2);
    check(items, 3, 4, true, -128);
    Require(items[0] == 1 && items[1] == 2 && items[2] == 3);
    uprv_sortArray_nid_postfix(items, 3, 4, GuestCompare, &context, 0, nullptr);
}

}

int main() {
    Sorts();
    Arguments();
    std::puts("ICU array sort checks passed");
    return 0;
}
