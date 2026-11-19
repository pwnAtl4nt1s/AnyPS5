# libSceIcu

## add Functional

- [udict_swap](../../../core/libs/prx/libSceIcu/DictionarySwap.cpp) is unverified on a console. It ports ICU 68.2 [dictionarydata.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/dictionarydata.cpp) onto the guest data swapper: the data header, the eight indexes and a UChar trie are swapped and a byte trie is copied. The test compiles the bundled ICU 78 `dictionarydata.cpp` and compares dictionary images of each trie type, format and length in every endianness pair. Closes when: compared with the console.
