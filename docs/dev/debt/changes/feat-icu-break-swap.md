# libSceIcu

## add Functional

- [ubrk_swap](../../../core/libs/prx/libSceIcu/BreakSwap.cpp) is unverified on a console. It ports ICU 68.2 [rbbidata.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/rbbidata.cpp) onto the guest data swapper and the guest `ucptrie_swap`: the break data header, the 8- and 16-bit state tables, the character category trie and the status table are swapped and the rule source copied, for format version 6. Like ICU it trusts the section offsets in the break data header. The test compiles the bundled ICU 78 `rbbidata.cpp` and compares break images with each table width, with and without a reverse table, and malformed headers in every endianness pair. Closes when: compared with the console.
