#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UText;

extern "C" {
UText* APS5_VABI utext_setup(UText* ut, std::int32_t extraSpace, std::int32_t* status);
UText* APS5_VABI utext_close(UText* ut);
UText* APS5_VABI utext_openUChars(UText* ut, const char16_t* s, std::int64_t length, std::int32_t* status);
UText* APS5_VABI utext_openUTF8_nid_postfix(UText* ut, const char* s, std::int64_t length, std::int32_t* status);
std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(UText* ut, std::int32_t delta);
std::int64_t APS5_VABI utext_nativeLength_nid_postfix(UText* ut);
std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const UText* ut);
std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const UText* ut);
void APS5_VABI utext_setNativeIndex_nid_postfix(UText* ut, std::int64_t index);
std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(UText* ut);
std::int32_t APS5_VABI utext_current32_nid_postfix(UText* ut);
std::int32_t APS5_VABI utext_char32At_nid_postfix(UText* ut, std::int64_t nativeIndex);
std::int32_t APS5_VABI utext_next32_nid_postfix(UText* ut);
std::int32_t APS5_VABI utext_previous32_nid_postfix(UText* ut);
std::int32_t APS5_VABI utext_next32From_nid_postfix(UText* ut, std::int64_t index);
std::int32_t APS5_VABI utext_previous32From_nid_postfix(UText* ut, std::int64_t index);
std::int32_t APS5_VABI utext_extract_nid_postfix(UText* ut, std::int64_t start, std::int64_t limit, char16_t* destination, std::int32_t capacity, std::int32_t* status);
std::int8_t APS5_VABI utext_equals_nid_postfix(const UText* first, const UText* second);
std::int8_t APS5_VABI utext_isWritable_nid_postfix(const UText* ut);
void APS5_VABI utext_freeze_nid_postfix(UText* ut);
std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const UText* ut);
std::int32_t APS5_VABI utext_replace_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, const char16_t* replacement, std::int32_t replacementLength, std::int32_t* status);
void APS5_VABI utext_copy_nid_postfix(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, std::int64_t destinationIndex, std::int8_t move, std::int32_t* status);
UText* APS5_VABI utext_clone_nid_postfix(UText* destination, const UText* source, std::int8_t deep, std::int8_t readOnly, std::int32_t* status);
}
