# libSceIcu

## add Functional

- [Trie swapping](../../../core/libs/prx/libSceIcu/TrieSwap.cpp) is unverified on a console. `utrie2_swap`, `ucptrie_swap` and `utrie_swapAnyVersion` port ICU 68.2 `utrie_swap.cpp` onto the guest data swapper, including the version 1 `UTrie` swap that `utrie_swapAnyVersion` reaches; `utrie_swap` itself is not among the exports. Only the headers are validated, as in ICU. A null status throws where ICU dereferences it. The test compares UTrie, UTrie2 and UCPTrie images, valid and malformed headers and tries built by the bundled ICU 78 in every endianness pair. Closes when: compared with the console.
