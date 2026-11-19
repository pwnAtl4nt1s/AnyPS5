# libSceIcu

## add Functional

- [Data swapper and invariant-character helpers](../../../core/libs/prx/libSceIcu/DataSwapper.cpp) are unverified on a console. `udata_openSwapper`, `udata_openSwapperForInputData`, `udata_closeSwapper`, `udata_readInt16`, `udata_readInt32`, `udata_swapInvStringBlock`, `udata_swapDataHeader`, `udata_printError`, `uprv_ebcdicFromAscii`, `uprv_isInvariantString`, `uprv_isInvariantUString`, `uprv_compareInvEbcdicAsAscii`, `uprv_ebcdicToAscii`, `uprv_ebcdicToLowercaseAscii`, `uprv_aestrncpy` and `uprv_eastrncpy` port ICU 68.2 `udataswp.cpp` and `uinvchar.cpp` onto the guest `UDataSwapper` layout; the swapper is allocated through `uprv_malloc` and its function pointers are guest-callable. `udata_printError` hands the guest callback a System V `va_list`. The test compares every charset and endianness pair, data headers and the invariant helpers with the bundled ICU 78. Closes when: compared with the console.
