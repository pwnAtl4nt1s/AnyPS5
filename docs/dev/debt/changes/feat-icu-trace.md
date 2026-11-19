# libSceIcu

## add Functional

- [ICU tracing](../../../core/libs/prx/libSceIcu/Trace.cpp) is unverified on a console. The ten `utrace_*` exports port ICU 68.2 [utrace.cpp](https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/utrace.cpp): the guest exit and data callbacks receive a System V `va_list`, and `utrace_vformat` reads its arguments from the guest's `va_list` as eight-byte integer slots, which covers every format it accepts. Function names follow the ICU 68.2 tables, so `0x2008` is bogus where ICU 78 names `ucol_strcollUTF8`. This module's ICU code never calls the trace hooks itself. An unknown exit return type, a null format or null `utrace_getFunctions` outputs throw where ICU aborts or dereferences. The test compares formatting, callbacks and levels with the bundled ICU 78. Closes when: compared with the console.
