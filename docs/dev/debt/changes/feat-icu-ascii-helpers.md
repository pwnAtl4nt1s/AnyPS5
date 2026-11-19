# libSceIcu

## add Functional

- [ASCII character helpers](../../../core/libs/prx/libSceIcu/CString.cpp) are unverified on a console. `uprv_isASCIILetter`, `uprv_toupper`, `uprv_asciitolower`, `uprv_ebcdictolower`, `uprv_stricmp` and `uprv_strnicmp` follow the ASCII-family build of ICU 68.2 [cstring.cpp](https://github.com/unicode-org/icu/blob/release-68-2/icu4c/source/common/cstring.cpp): only ASCII letters change case, `uprv_ebcdictolower` maps the EBCDIC capitals 0xC1-0xC9, 0xD1-0xD9 and 0xE2-0xE9 down by 0x40, and the comparisons order a null string first, a prefix before a longer string, and otherwise return the difference of the lowered unsigned bytes, `uprv_strnicmp` comparing at most its count. `UBool` is returned as an 8-bit integer, as in ICU 68.
