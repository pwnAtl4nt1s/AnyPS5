# libSceIcu

## add Functional

- [Hashtable](../../../core/libs/prx/libSceIcu/Hashtable.cpp) is unverified on a console; `uhash_hashUnicodeString`, `uhash_compareUnicodeString`, their caseless forms and `uhash_deleteHashtable`, which take guest C++ objects, still throw, and a null table, position or status throws where ICU dereferences it. The rest of ICU 68.2 `uhash.cpp` is ported onto the guest `UHashtable` and `UHashElement` layouts (80 and 24 bytes): open, init, close, the setters and resize policy, count, the `get`, `put` and `remove` families, `removeAll`, `find`, `nextElement`, `removeElement` and `equals`. Callbacks are called as guest functions and memory comes from the ICU allocator. As in ICU 68, an integer value of 0 is stored, where ICU 78 removes the key. The test compares every slot with ICU 78's `uhash` after each step of random integer, colliding and string workloads under the three resize policies.
