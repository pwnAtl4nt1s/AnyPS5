# libSceIcu

## add Functional

- [ures_swap](../../../core/libs/prx/libSceIcu/ResourceSwap.cpp) is unverified on a console. It ports ICU 68.2 [uresdata.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uresdata.cpp) for resource bundle format versions 1.1 to 3 onto the guest data swapper, `uprv_sortArray` and `ucol_swap`: keys, 16-bit units, strings, aliases, binaries, int vectors, arrays and tables are swapped, format 1 tables are re-sorted for a charset family change, and collation binaries are swapped. Like ICU it trusts the offsets in the bundle; a table longer than its declared maximum grows the sorting buffers, where ICU writes past them. The test compares hand-built bundles of each format, local and pool keys, with the bundled ICU 78 for each charset family and endianness. Closes when: compared with the console.
