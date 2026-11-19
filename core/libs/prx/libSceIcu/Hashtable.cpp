// Derived from icu4c/source/common/uhash.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2):
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uhash.cpp
//
// The upstream file's notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Configuration.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::array<std::int32_t, 29> Primes{7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521, 131071, 262139, 524287, 1048573,
    2097143, 4194301, 8388593, 16777213, 33554393, 67108859, 134217689, 268435399, 536870909, 1073741789, 2147483647};
constexpr std::int32_t DefaultPrimeIndex = 4;
constexpr std::array<float, 6> ResizeRatios{0.0F, 0.5F, 0.1F, 0.5F, 0.0F, 1.0F};
constexpr std::int32_t Deleted = static_cast<std::int32_t>(0x80000000u);
constexpr std::int32_t Empty = Deleted + 1;
constexpr std::int8_t KeyPointer = 1;
constexpr std::int8_t ValuePointer = 2;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t First = -1;

bool Vacant(const std::int32_t hashcode) {
    return hashcode < 0;
}

bool Failed(const std::int32_t* status) {
    if (status == nullptr) APS5_INVALID_ARG_EX;
    return *status > 0;
}

void Require(const void* table) {
    if (table == nullptr) APS5_INVALID_ARG_EX;
}

HashKey PointerKey(const void* pointer) {
    HashKey key{};
    key.Pointer = const_cast<void*>(pointer);
    return key;
}

HashKey IntegerKey(const std::int32_t integer) {
    HashKey key{};
    key.Integer = integer;
    return key;
}

std::int32_t Mark(const std::int32_t length, const float ratio) {
    const float mark = static_cast<float>(length) * ratio;
    if (mark >= 2147483648.0F) return static_cast<std::int32_t>(0x80000000u);
    return static_cast<std::int32_t>(mark);
}

void DeleteKeyValue(const Hashtable* table, void* key, void* value) {
    if (table->KeyDeleter != nullptr && key != nullptr) table->KeyDeleter(key);
    if (table->ValueDeleter != nullptr && value != nullptr) table->ValueDeleter(value);
}

HashKey SetElement(Hashtable* table, HashElement* element, const std::int32_t hashcode, const HashKey key, const HashKey value, const std::int8_t hint) {
    HashKey oldValue = element->Value;
    if (table->KeyDeleter != nullptr && element->Key.Pointer != nullptr && element->Key.Pointer != key.Pointer) table->KeyDeleter(element->Key.Pointer);
    if (table->ValueDeleter != nullptr) {
        if (oldValue.Pointer != nullptr && oldValue.Pointer != value.Pointer) table->ValueDeleter(oldValue.Pointer);
        oldValue.Pointer = nullptr;
    }
    if ((hint & KeyPointer) != 0)
        element->Key.Pointer = key.Pointer;
    else
        element->Key = key;
    if ((hint & ValuePointer) != 0)
        element->Value.Pointer = value.Pointer;
    else
        element->Value = value;
    element->Hashcode = hashcode;
    return oldValue;
}

HashKey RemoveElement(Hashtable* table, HashElement* element) {
    --table->Count;
    return SetElement(table, element, Deleted, HashKey{}, HashKey{}, 0);
}

void SetResizePolicy(Hashtable* table, const std::int32_t policy) {
    if (policy < 0 || policy > 2) APS5_INVALID_ARG_EX;
    table->LowWaterRatio = ResizeRatios[static_cast<std::size_t>(policy) * 2];
    table->HighWaterRatio = ResizeRatios[static_cast<std::size_t>(policy) * 2 + 1];
}

void Allocate(Hashtable* table, const std::int32_t primeIndex, std::int32_t* status) {
    if (*status > 0) return;
    table->PrimeIndex = static_cast<std::int8_t>(primeIndex);
    table->Length = Primes[static_cast<std::size_t>(primeIndex)];
    table->Elements = static_cast<HashElement*>(uprv_malloc_nid_postfix(sizeof(HashElement) * static_cast<std::size_t>(table->Length)));
    if (table->Elements == nullptr) {
        *status = MemoryAllocationError;
        return;
    }
    for (std::int32_t index = 0; index < table->Length; ++index) table->Elements[index] = HashElement{Empty, HashKey{}, HashKey{}};
    table->Count = 0;
    table->LowWaterMark = Mark(table->Length, table->LowWaterRatio);
    table->HighWaterMark = Mark(table->Length, table->HighWaterRatio);
}

Hashtable* Initialize(Hashtable* table, const HashKeyHasher keyHasher, const HashKeyComparator keyComparator, const HashKeyComparator valueComparator,
    const std::int32_t primeIndex, std::int32_t* status) {
    if (Failed(status)) return nullptr;
    Require(table);
    table->KeyHasher = keyHasher;
    table->KeyComparator = keyComparator;
    table->ValueComparator = valueComparator;
    table->KeyDeleter = nullptr;
    table->ValueDeleter = nullptr;
    table->Allocated = 0;
    SetResizePolicy(table, 0);
    Allocate(table, primeIndex, status);
    if (*status > 0) return nullptr;
    return table;
}

Hashtable* Create(const HashKeyHasher keyHasher, const HashKeyComparator keyComparator, const HashKeyComparator valueComparator, const std::int32_t primeIndex,
    std::int32_t* status) {
    if (Failed(status)) return nullptr;
    auto* table = static_cast<Hashtable*>(uprv_malloc_nid_postfix(sizeof(Hashtable)));
    if (table == nullptr) {
        *status = MemoryAllocationError;
        return nullptr;
    }
    Initialize(table, keyHasher, keyComparator, valueComparator, primeIndex, status);
    table->Allocated = 1;
    if (*status > 0) {
        IcuReleaseMemory_nid_no_patch(table);
        return nullptr;
    }
    return table;
}

std::int32_t PrimeIndexFor(const std::int32_t size) {
    std::size_t index = 0;
    while (index < Primes.size() - 1 && Primes[index] < size) ++index;
    return static_cast<std::int32_t>(index);
}

HashElement* Find(const Hashtable* table, const HashKey key, std::int32_t hashcode) {
    std::int32_t firstDeleted = -1;
    std::int32_t jump = 0;
    std::int32_t tableHash = 0;
    HashElement* elements = table->Elements;
    hashcode &= 0x7FFFFFFF;
    std::int32_t index = (hashcode ^ 0x4000000) % table->Length;
    const std::int32_t start = index;
    do {
        tableHash = elements[index].Hashcode;
        if (tableHash == hashcode) {
            if (table->KeyComparator(key, elements[index].Key) != 0) return &elements[index];
        } else if (!Vacant(tableHash)) {
        } else if (tableHash == Empty) {
            break;
        } else if (firstDeleted < 0) {
            firstDeleted = index;
        }
        if (jump == 0) jump = (hashcode % (table->Length - 1)) + 1;
        index = (index + jump) % table->Length;
    } while (index != start);
    if (firstDeleted >= 0)
        index = firstDeleted;
    else if (tableHash != Empty)
        throw std::logic_error("uhash table has no empty or deleted slot");
    return &elements[index];
}

HashElement* Lookup(const Hashtable* table, const HashKey key) {
    Require(table);
    return Find(table, key, table->KeyHasher(key));
}

void Rehash(Hashtable* table, std::int32_t* status) {
    HashElement* old = table->Elements;
    const std::int32_t oldLength = table->Length;
    std::int32_t primeIndex = table->PrimeIndex;
    if (table->Count > table->HighWaterMark) {
        if (++primeIndex >= static_cast<std::int32_t>(Primes.size())) return;
    } else if (table->Count < table->LowWaterMark) {
        if (--primeIndex < 0) return;
    } else {
        return;
    }
    Allocate(table, primeIndex, status);
    if (*status > 0) {
        table->Elements = old;
        table->Length = oldLength;
        return;
    }
    for (std::int32_t index = oldLength - 1; index >= 0; --index) {
        if (Vacant(old[index].Hashcode)) continue;
        HashElement* element = Find(table, old[index].Key, old[index].Hashcode);
        element->Key = old[index].Key;
        element->Value = old[index].Value;
        element->Hashcode = old[index].Hashcode;
        ++table->Count;
    }
    IcuReleaseMemory_nid_no_patch(old);
}

HashKey Remove(Hashtable* table, const HashKey key) {
    HashElement* element = Lookup(table, key);
    HashKey result{};
    if (!Vacant(element->Hashcode)) {
        result = RemoveElement(table, element);
        if (table->Count < table->LowWaterMark) {
            std::int32_t status = 0;
            Rehash(table, &status);
        }
    }
    return result;
}

HashKey Put(Hashtable* table, const HashKey key, const HashKey value, const std::int8_t hint, std::int32_t* status) {
    Require(table);
    if (!Failed(status)) {
        if ((hint & ValuePointer) != 0 && value.Pointer == nullptr) return Remove(table, key);
        if (table->Count > table->HighWaterMark) Rehash(table, status);
        if (*status <= 0) {
            const std::int32_t hashcode = table->KeyHasher(key);
            HashElement* element = Find(table, key, hashcode);
            bool full = false;
            if (Vacant(element->Hashcode)) {
                ++table->Count;
                if (table->Count == table->Length) {
                    --table->Count;
                    *status = MemoryAllocationError;
                    full = true;
                }
            }
            if (!full) return SetElement(table, element, hashcode & 0x7FFFFFFF, key, value, hint);
        }
    }
    DeleteKeyValue(table, key.Pointer, value.Pointer);
    return HashKey{};
}

}

extern "C" {

Hashtable* APS5_VABI uhash_open_nid_postfix(const HashKeyHasher keyHasher, const HashKeyComparator keyComparator, const HashKeyComparator valueComparator, std::int32_t* status) {
    return Create(keyHasher, keyComparator, valueComparator, DefaultPrimeIndex, status);
}

Hashtable* APS5_VABI uhash_openSize_nid_postfix(const HashKeyHasher keyHasher, const HashKeyComparator keyComparator, const HashKeyComparator valueComparator,
    const std::int32_t size, std::int32_t* status) {
    return Create(keyHasher, keyComparator, valueComparator, PrimeIndexFor(size), status);
}

Hashtable* APS5_VABI uhash_init_nid_postfix(Hashtable* table, const HashKeyHasher keyHasher, const HashKeyComparator keyComparator, const HashKeyComparator valueComparator,
    std::int32_t* status) {
    return Initialize(table, keyHasher, keyComparator, valueComparator, DefaultPrimeIndex, status);
}

Hashtable* APS5_VABI uhash_initSize_nid_postfix(Hashtable* table, const HashKeyHasher keyHasher, const HashKeyComparator keyComparator,
    const HashKeyComparator valueComparator, const std::int32_t size, std::int32_t* status) {
    return Initialize(table, keyHasher, keyComparator, valueComparator, PrimeIndexFor(size), status);
}

void APS5_VABI uhash_close_nid_postfix(Hashtable* table) {
    if (table == nullptr) return;
    if (table->Elements != nullptr) {
        if (table->KeyDeleter != nullptr || table->ValueDeleter != nullptr) {
            std::int32_t position = First;
            const HashElement* element = nullptr;
            while ((element = uhash_nextElement_nid_postfix(table, &position)) != nullptr) DeleteKeyValue(table, element->Key.Pointer, element->Value.Pointer);
        }
        IcuReleaseMemory_nid_no_patch(table->Elements);
        table->Elements = nullptr;
    }
    if (table->Allocated != 0) IcuReleaseMemory_nid_no_patch(table);
}

HashKeyHasher APS5_VABI uhash_setKeyHasher_nid_postfix(Hashtable* table, const HashKeyHasher function) {
    Require(table);
    const auto result = table->KeyHasher;
    table->KeyHasher = function;
    return result;
}

HashKeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(Hashtable* table, const HashKeyComparator function) {
    Require(table);
    const auto result = table->KeyComparator;
    table->KeyComparator = function;
    return result;
}

HashKeyComparator APS5_VABI uhash_setValueComparator_nid_postfix(Hashtable* table, const HashKeyComparator function) {
    Require(table);
    const auto result = table->ValueComparator;
    table->ValueComparator = function;
    return result;
}

HashDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(Hashtable* table, const HashDeleter function) {
    Require(table);
    const auto result = table->KeyDeleter;
    table->KeyDeleter = function;
    return result;
}

HashDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(Hashtable* table, const HashDeleter function) {
    Require(table);
    const auto result = table->ValueDeleter;
    table->ValueDeleter = function;
    return result;
}

void APS5_VABI uhash_setResizePolicy_nid_postfix(Hashtable* table, const std::int32_t policy) {
    Require(table);
    std::int32_t status = 0;
    SetResizePolicy(table, policy);
    table->LowWaterMark = Mark(table->Length, table->LowWaterRatio);
    table->HighWaterMark = Mark(table->Length, table->HighWaterRatio);
    Rehash(table, &status);
}

std::int32_t APS5_VABI uhash_count_nid_postfix(const Hashtable* table) {
    Require(table);
    return table->Count;
}

void* APS5_VABI uhash_get_nid_postfix(const Hashtable* table, const void* key) {
    return Lookup(table, PointerKey(key))->Value.Pointer;
}

void* APS5_VABI uhash_iget_nid_postfix(const Hashtable* table, const std::int32_t key) {
    return Lookup(table, IntegerKey(key))->Value.Pointer;
}

std::int32_t APS5_VABI uhash_geti_nid_postfix(const Hashtable* table, const void* key) {
    return Lookup(table, PointerKey(key))->Value.Integer;
}

std::int32_t APS5_VABI uhash_igeti_nid_postfix(const Hashtable* table, const std::int32_t key) {
    return Lookup(table, IntegerKey(key))->Value.Integer;
}

void* APS5_VABI uhash_put_nid_postfix(Hashtable* table, void* key, void* value, std::int32_t* status) {
    return Put(table, PointerKey(key), PointerKey(value), KeyPointer | ValuePointer, status).Pointer;
}

void* APS5_VABI uhash_iput_nid_postfix(Hashtable* table, const std::int32_t key, void* value, std::int32_t* status) {
    return Put(table, IntegerKey(key), PointerKey(value), ValuePointer, status).Pointer;
}

std::int32_t APS5_VABI uhash_puti_nid_postfix(Hashtable* table, void* key, const std::int32_t value, std::int32_t* status) {
    return Put(table, PointerKey(key), IntegerKey(value), KeyPointer, status).Integer;
}

std::int32_t APS5_VABI uhash_iputi_nid_postfix(Hashtable* table, const std::int32_t key, const std::int32_t value, std::int32_t* status) {
    return Put(table, IntegerKey(key), IntegerKey(value), 0, status).Integer;
}

void* APS5_VABI uhash_remove_nid_postfix(Hashtable* table, const void* key) {
    return Remove(table, PointerKey(key)).Pointer;
}

void* APS5_VABI uhash_iremove_nid_postfix(Hashtable* table, const std::int32_t key) {
    return Remove(table, IntegerKey(key)).Pointer;
}

std::int32_t APS5_VABI uhash_removei_nid_postfix(Hashtable* table, const void* key) {
    return Remove(table, PointerKey(key)).Integer;
}

std::int32_t APS5_VABI uhash_iremovei_nid_postfix(Hashtable* table, const std::int32_t key) {
    return Remove(table, IntegerKey(key)).Integer;
}

void APS5_VABI uhash_removeAll_nid_postfix(Hashtable* table) {
    Require(table);
    std::int32_t position = First;
    const HashElement* element = nullptr;
    if (table->Count != 0)
        while ((element = uhash_nextElement_nid_postfix(table, &position)) != nullptr) uhash_removeElement_nid_postfix(table, element);
}

const HashElement* APS5_VABI uhash_find_nid_postfix(const Hashtable* table, const void* key) {
    const HashElement* element = Lookup(table, PointerKey(key));
    return Vacant(element->Hashcode) ? nullptr : element;
}

const HashElement* APS5_VABI uhash_nextElement_nid_postfix(const Hashtable* table, std::int32_t* position) {
    Require(table);
    Require(position);
    for (std::int32_t index = *position + 1; index < table->Length; ++index) {
        if (!Vacant(table->Elements[index].Hashcode)) {
            *position = index;
            return &table->Elements[index];
        }
    }
    return nullptr;
}

void* APS5_VABI uhash_removeElement_nid_postfix(Hashtable* table, const HashElement* element) {
    Require(table);
    Require(element);
    if (!Vacant(element->Hashcode)) return RemoveElement(table, const_cast<HashElement*>(element)).Pointer;
    return nullptr;
}

std::int8_t APS5_VABI uhash_equals_nid_postfix(const Hashtable* first, const Hashtable* second) {
    if (first == second) return 1;
    if (first == nullptr || second == nullptr || first->KeyComparator != second->KeyComparator || first->ValueComparator != second->ValueComparator ||
        first->ValueComparator == nullptr)
        return 0;
    if (first->Count != second->Count) return 0;
    std::int32_t position = First;
    for (std::int32_t index = 0; index < first->Count; ++index) {
        const HashElement* element = uhash_nextElement_nid_postfix(first, &position);
        const HashElement* other = Find(second, element->Key, second->KeyHasher(element->Key));
        if (first->ValueComparator(element->Value, other->Value) == 0) return 0;
    }
    return 1;
}

}
