# libSceIcu

## add Functional

- [String hashes](../../../core/libs/prx/libSceIcu/Hash.cpp) are unverified on a console. `ustr_hashUCharsN`, `ustr_hashCharsN` and `ustr_hashICharsN` follow `STRING_HASH` in ICU 68.2 [ustring.cpp](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/ustring.cpp), unchanged in ICU 78.1: from the first unit, every `((length - 32) / 32) + 1`th unit is folded in as `hash * 37 + unit`, with bytes read unsigned and `ustr_hashICharsN` lowering ASCII `A`-`Z` only; a null string or a length of zero or less hashes to 0. The signatures are ICU's internal `ustr_imp.h` ones; no title call site was read.
