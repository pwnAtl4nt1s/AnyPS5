# libSceIcu

## add Functional

- [uchar_swapNames](../../../core/libs/prx/libSceIcu/NamesSwap.cpp) is unverified on a console. It ports ICU 68.2 [unames.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/unames.cpp) onto the guest data swapper, including the token permutation and the group string rewrite for a charset family change. Like ICU it trusts the offsets, token count and group lengths inside the data, so malformed data, a token count above 512 or a permutation outside the token table can read or write outside the buffers. The test compares synthetic `unam` images with one- and two-byte tokens, both length encodings and all algorithmic range types with the bundled ICU 78 for each charset family and endianness. Closes when: compared with the console.
