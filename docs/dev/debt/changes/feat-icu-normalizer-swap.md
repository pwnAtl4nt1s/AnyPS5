# libSceIcu

## add Functional

- [unorm2_swap](../../../core/libs/prx/libSceIcu/NormalizerSwap.cpp) is unverified on a console. It ports ICU 68.2 [normalizer2impl.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/normalizer2impl.cpp) onto the guest data swapper and the guest trie swapper, accepting `Nrm2` format versions 1 to 4 as ICU 68.2 does; ICU 78 also accepts version 5, which is rejected here. The test compares Normalizer2 images with fast and small code point tries, each format version and the index-length limits with the bundled ICU 78 in every endianness pair. Closes when: compared with the console.
