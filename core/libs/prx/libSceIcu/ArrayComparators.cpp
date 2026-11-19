// Derived from icu4c/source/common/uarrsort.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 2003-2013, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/LegacyExports.hpp"
#include "prx/libSceIcu/ArraySort.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr std::int32_t MinimumQuickSort = 9;
constexpr std::int32_t IllegalArgument = 1;

void InsertionSort(char* array, const std::int32_t length, const std::int32_t itemSize, ArrayComparator compare, const void* context, void* value) {
    for (std::int32_t index = 1; index < length; ++index) {
        char* item = array + static_cast<std::ptrdiff_t>(index) * itemSize;
        std::int32_t insertion = uprv_stableBinarySearch_nid_postfix(array, index, item, itemSize, compare, context);
        insertion = insertion < 0 ? ~insertion : insertion + 1;
        if (insertion < index) {
            char* destination = array + static_cast<std::ptrdiff_t>(insertion) * itemSize;
            std::memcpy(value, item, static_cast<std::size_t>(itemSize));
            std::memmove(destination + itemSize, destination, static_cast<std::size_t>(index - insertion) * static_cast<std::size_t>(itemSize));
            std::memcpy(destination, value, static_cast<std::size_t>(itemSize));
        }
    }
}

char* At(char* array, const std::int32_t index, const std::int32_t itemSize) {
    return array + static_cast<std::ptrdiff_t>(index) * itemSize;
}

void QuickSort(char* array, std::int32_t start, std::int32_t limit, const std::int32_t itemSize, ArrayComparator compare, const void* context, void* pivot,
    void* swap) {
    const auto size = static_cast<std::size_t>(itemSize);
    do {
        if (start + MinimumQuickSort >= limit) {
            InsertionSort(At(array, start, itemSize), limit - start, itemSize, compare, context, pivot);
            break;
        }
        std::int32_t left = start;
        std::int32_t right = limit;
        std::memcpy(pivot, At(array, (start + limit) / 2, itemSize), size);
        do {
            while (compare(context, At(array, left, itemSize), pivot) < 0) ++left;
            while (compare(context, pivot, At(array, right - 1, itemSize)) < 0) --right;
            if (left < right) {
                --right;
                if (left < right) {
                    std::memcpy(swap, At(array, left, itemSize), size);
                    std::memcpy(At(array, left, itemSize), At(array, right, itemSize), size);
                    std::memcpy(At(array, right, itemSize), swap, size);
                }
                ++left;
            }
        } while (left < right);
        if (right - start < limit - left) {
            if (start < right - 1) QuickSort(array, start, right, itemSize, compare, context, pivot, swap);
            start = left;
        } else {
            if (left < limit - 1) QuickSort(array, left, limit, itemSize, compare, context, pivot, swap);
            limit = right;
        }
    } while (start < limit - 1);
}

std::size_t Units(const std::int32_t itemSize) {
    return (static_cast<std::size_t>(itemSize) + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t);
}

}

extern "C" {

std::int32_t APS5_VABI uprv_int32Comparator_nid_postfix(const void* context, const void* left, const void* right) {
    (void)context;
    const auto l = static_cast<std::uint32_t>(*static_cast<const std::int32_t*>(left));
    const auto r = static_cast<std::uint32_t>(*static_cast<const std::int32_t*>(right));
    return static_cast<std::int32_t>(l - r);
}

std::int32_t APS5_VABI uprv_uint32Comparator_nid_postfix(const void* context, const void* left, const void* right) {
    (void)context;
    const auto l = *static_cast<const std::uint32_t*>(left);
    const auto r = *static_cast<const std::uint32_t*>(right);
    if (l < r) return -1;
    if (l == r) return 0;
    return 1;
}

std::int32_t APS5_VABI uprv_uint16Comparator_nid_postfix(const void* context, const void* left, const void* right) {
    (void)context;
    return static_cast<std::int32_t>(*static_cast<const std::uint16_t*>(left)) - static_cast<std::int32_t>(*static_cast<const std::uint16_t*>(right));
}

std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char* array, std::int32_t limit, void* item, const std::int32_t itemSize, ArrayComparator compare,
    const void* context) {
    std::int32_t start = 0;
    bool found = false;
    while (limit - start >= MinimumQuickSort) {
        const std::int32_t middle = (start + limit) / 2;
        const std::int32_t difference = compare(context, item, At(array, middle, itemSize));
        if (difference == 0) {
            found = true;
            start = middle + 1;
        } else if (difference < 0) {
            limit = middle;
        } else {
            start = middle;
        }
    }
    while (start < limit) {
        const std::int32_t difference = compare(context, item, At(array, start, itemSize));
        if (difference == 0) {
            found = true;
        } else if (difference < 0) {
            break;
        }
        ++start;
    }
    return found ? start - 1 : ~start;
}

void APS5_VABI uprv_sortArray_nid_postfix(void* array, const std::int32_t length, const std::int32_t itemSize, ArrayComparator compare, const void* context,
    const std::int8_t sortStable, std::int32_t* status) {
    if (status == nullptr || *status > 0) return;
    if ((length > 0 && array == nullptr) || length < 0 || itemSize <= 0 || compare == nullptr) {
        *status = IllegalArgument;
        return;
    }
    if (length <= 1) return;
    auto* items = static_cast<char*>(array);
    if (length < MinimumQuickSort || sortStable != 0) {
        std::vector<std::max_align_t> value(Units(itemSize));
        InsertionSort(items, length, itemSize, compare, context, value.data());
    } else {
        std::vector<std::max_align_t> scratch(Units(itemSize) * 2);
        QuickSort(items, 0, length, itemSize, compare, context, scratch.data(), scratch.data() + Units(itemSize));
    }
}

}
