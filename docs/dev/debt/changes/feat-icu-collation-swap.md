# libSceIcu

## add Functional

- [Collation data checks and inverse UCA swapping](../../../core/libs/prx/libSceIcu/CollationSwap.cpp) are unverified on a console, and `ucol_swap` still throws. `ucol_looksLikeCollationBinary` and `ucol_swapInverseUCA` port ICU 68.2 [ucol_swp.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/ucol_swp.cpp) onto the guest data swapper; the format-version 3 `UCATableHeader` fields are read at their ICU 68.2 offsets. Like ICU, `ucol_swapInverseUCA` trusts the table offsets in the data. The test compares inverse UCA images and version 3 and 4 headers with the bundled ICU 78 in every endianness pair. Closes when: compared with the console.
