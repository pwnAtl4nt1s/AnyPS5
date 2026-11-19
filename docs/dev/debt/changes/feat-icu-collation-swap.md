# libSceIcu

## add Functional

- [Collation data swapping](../../../core/libs/prx/libSceIcu/CollationSwap.cpp) is unverified on a console. `ucol_swap`, `ucol_looksLikeCollationBinary` and `ucol_swapInverseUCA` port ICU 68.2 [ucol_swp.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucol_swp.cpp) onto the guest data and trie swappers, for the header-less format version 3 `UCATableHeader` and format versions 4 and 5. Like ICU they trust the section offsets in the data; an index count below 1 is treated as 1 where ICU writes before its index array. The test compares version 3, 4 and 5 collation images, inverse UCA images and malformed headers with the bundled ICU 78 in every endianness pair. Closes when: compared with the console.
