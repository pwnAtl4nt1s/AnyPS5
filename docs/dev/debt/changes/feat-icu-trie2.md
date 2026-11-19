# libSceIcu

## add Functional

- [UTrie2](../../../core/libs/prx/libSceIcu/Trie2.cpp) is unverified on a console. `utrie2_openFromSerialized`, `utrie2_openDummy`, `utrie2_close`, `utrie2_isFrozen`, `utrie2_serialize`, `utrie2_get32`, `utrie2_get32FromLeadSurrogateCodeUnit`, `utrie2_internalU8NextIndex`, `utrie2_internalU8PrevIndex`, `utrie2_enum` and `utrie2_enumForLeadSurrogate` call the bundled ICU 78 `utrie2.cpp`, whose `UTrie2` layout and runtime code match ICU 68.2 apart from `nullptr` spellings, so the guest's `UTRIE2_GET16`/`GET32` macros read the returned struct directly. Tries are allocated by the host ICU allocator, not through `u_setMemoryFunctions`; the guest enumeration callbacks are forwarded through a context. A null trie or status throws where ICU dereferences it. The test builds a 32-bit trie by hand and checks every code point, the lead-surrogate code units, UTF-8 index lookups, enumeration, serialization and the format errors, and the 16- and 32-bit dummy tries.
