#include "prx/libSceIcu/Trie2.hpp"
#include "prx/libc/include/General.hpp"
#include "utrie2.h"
#include <cstdint>
#include <string>

namespace {

struct Enumeration {
    Icu::Trie2EnumValue value;
    Icu::Trie2EnumRange range;
    const void* context;
};

uint32_t U_CALLCONV ForwardValue(const void* context, uint32_t value) {
    const auto& forward = *static_cast<const Enumeration*>(context);
    return forward.value(forward.context, value);
}

UBool U_CALLCONV ForwardRange(const void* context, UChar32 start, UChar32 end, uint32_t value) {
    const auto& forward = *static_cast<const Enumeration*>(context);
    return forward.range(forward.context, start, end, value) != 0;
}

template <typename T>
T* Required(T* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
    return pointer;
}

UErrorCode* Status(std::int32_t* error) {
    return reinterpret_cast<UErrorCode*>(Required(error));
}

void Enumerate(const UTrie2* trie, const UChar32 start, const Icu::Trie2EnumValue enumValue, const Icu::Trie2EnumRange enumRange, const void* context, const bool lead) {
    Required(trie);
    const Enumeration forward{enumValue, enumRange, context};
    UTrie2EnumValue* value = enumValue ? ForwardValue : nullptr;
    UTrie2EnumRange* range = enumRange ? ForwardRange : nullptr;
    if (lead)
        utrie2_enumForLeadSurrogate(trie, start, value, range, &forward);
    else
        utrie2_enum(trie, value, range, &forward);
}

}

extern "C" {

UTrie2* APS5_VABI utrie2_openFromSerialized_nid_postfix(const std::int32_t valueBits, const void* data, const std::int32_t length, std::int32_t* actualLength,
    std::int32_t* status) {
    return utrie2_openFromSerialized(static_cast<UTrie2ValueBits>(valueBits), data, length, actualLength, Status(status));
}

UTrie2* APS5_VABI utrie2_openDummy_nid_postfix(const std::int32_t valueBits, const std::uint32_t initialValue, const std::uint32_t errorValue, std::int32_t* status) {
    return utrie2_openDummy(static_cast<UTrie2ValueBits>(valueBits), initialValue, errorValue, Status(status));
}

void APS5_VABI utrie2_close_nid_postfix(UTrie2* trie) {
    utrie2_close(trie);
}

std::int8_t APS5_VABI utrie2_isFrozen_nid_postfix(const UTrie2* trie) {
    return utrie2_isFrozen(Required(trie)) ? 1 : 0;
}

std::int32_t APS5_VABI utrie2_serialize_nid_postfix(const UTrie2* trie, void* data, const std::int32_t capacity, std::int32_t* status) {
    return utrie2_serialize(trie, data, capacity, Status(status));
}

std::uint32_t APS5_VABI utrie2_get32_nid_postfix(const UTrie2* trie, const std::int32_t c) {
    return utrie2_get32(Required(trie), c);
}

std::uint32_t APS5_VABI utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(const UTrie2* trie, const std::int32_t c) {
    return utrie2_get32FromLeadSurrogateCodeUnit(Required(trie), c);
}

std::int32_t APS5_VABI utrie2_internalU8NextIndex_nid_postfix(const UTrie2* trie, const std::int32_t c, const std::uint8_t* source, const std::uint8_t* limit) {
    return utrie2_internalU8NextIndex(Required(trie), c, Required(source), Required(limit));
}

std::int32_t APS5_VABI utrie2_internalU8PrevIndex_nid_postfix(const UTrie2* trie, const std::int32_t c, const std::uint8_t* start, const std::uint8_t* source) {
    return utrie2_internalU8PrevIndex(Required(trie), c, Required(start), Required(source));
}

void APS5_VABI utrie2_enum_nid_postfix(const UTrie2* trie, const Icu::Trie2EnumValue enumValue, const Icu::Trie2EnumRange enumRange, const void* context) {
    Enumerate(trie, 0, enumValue, enumRange, context, false);
}

void APS5_VABI utrie2_enumForLeadSurrogate_nid_postfix(const UTrie2* trie, const std::int32_t lead, const Icu::Trie2EnumValue enumValue, const Icu::Trie2EnumRange enumRange,
    const void* context) {
    Enumerate(trie, lead, enumValue, enumRange, context, true);
}

}
