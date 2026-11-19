# libSceIcu

## add Functional

- [uprv_sortArray and uprv_stableBinarySearch](../../../core/libs/prx/libSceIcu/ArrayComparators.cpp) have no console ABI or behavior verification. They port ICU 68.2 [uarrsort.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp) and call the guest comparator with the same arguments in the same order, so an unstable quicksort leaves equal items where ICU does. The item scratch space is host memory, not `uprv_malloc`, so a failed allocation throws instead of setting `U_MEMORY_ALLOCATION_ERROR`. The test compares the sorted bytes and every comparator call with the bundled ICU 78 for item sizes 1 to 300. Closes when: compared with the console.
