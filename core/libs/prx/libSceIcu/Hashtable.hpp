#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIcu/Hash.hpp"
#include <cstdint>

using HashKeyHasher = std::int32_t (APS5_VABI*)(HashKey);
using HashKeyComparator = std::int8_t (APS5_VABI*)(HashKey, HashKey);
using HashDeleter = void (APS5_VABI*)(void*);

struct HashElement {
    std::int32_t Hashcode;
    HashKey Value;
    HashKey Key;
};

struct Hashtable {
    HashElement* Elements;
    HashKeyHasher KeyHasher;
    HashKeyComparator KeyComparator;
    HashKeyComparator ValueComparator;
    HashDeleter KeyDeleter;
    HashDeleter ValueDeleter;
    std::int32_t Count;
    std::int32_t Length;
    std::int32_t HighWaterMark;
    std::int32_t LowWaterMark;
    float HighWaterRatio;
    float LowWaterRatio;
    std::int8_t PrimeIndex;
    std::int8_t Allocated;
};

static_assert(sizeof(HashElement) == 24);
static_assert(sizeof(Hashtable) == 80);

extern "C" {
Hashtable* APS5_VABI uhash_open_nid_postfix(HashKeyHasher keyHasher, HashKeyComparator keyComparator, HashKeyComparator valueComparator, std::int32_t* status);
Hashtable* APS5_VABI uhash_openSize_nid_postfix(HashKeyHasher keyHasher, HashKeyComparator keyComparator, HashKeyComparator valueComparator, std::int32_t size, std::int32_t* status);
Hashtable* APS5_VABI uhash_init_nid_postfix(Hashtable* table, HashKeyHasher keyHasher, HashKeyComparator keyComparator, HashKeyComparator valueComparator, std::int32_t* status);
Hashtable* APS5_VABI uhash_initSize_nid_postfix(Hashtable* table, HashKeyHasher keyHasher, HashKeyComparator keyComparator, HashKeyComparator valueComparator, std::int32_t size, std::int32_t* status);
void APS5_VABI uhash_close_nid_postfix(Hashtable* table);
HashKeyHasher APS5_VABI uhash_setKeyHasher_nid_postfix(Hashtable* table, HashKeyHasher function);
HashKeyComparator APS5_VABI uhash_setKeyComparator_nid_postfix(Hashtable* table, HashKeyComparator function);
HashKeyComparator APS5_VABI uhash_setValueComparator_nid_postfix(Hashtable* table, HashKeyComparator function);
HashDeleter APS5_VABI uhash_setKeyDeleter_nid_postfix(Hashtable* table, HashDeleter function);
HashDeleter APS5_VABI uhash_setValueDeleter_nid_postfix(Hashtable* table, HashDeleter function);
void APS5_VABI uhash_setResizePolicy_nid_postfix(Hashtable* table, std::int32_t policy);
std::int32_t APS5_VABI uhash_count_nid_postfix(const Hashtable* table);
void* APS5_VABI uhash_get_nid_postfix(const Hashtable* table, const void* key);
void* APS5_VABI uhash_iget_nid_postfix(const Hashtable* table, std::int32_t key);
std::int32_t APS5_VABI uhash_geti_nid_postfix(const Hashtable* table, const void* key);
std::int32_t APS5_VABI uhash_igeti_nid_postfix(const Hashtable* table, std::int32_t key);
void* APS5_VABI uhash_put_nid_postfix(Hashtable* table, void* key, void* value, std::int32_t* status);
void* APS5_VABI uhash_iput_nid_postfix(Hashtable* table, std::int32_t key, void* value, std::int32_t* status);
std::int32_t APS5_VABI uhash_puti_nid_postfix(Hashtable* table, void* key, std::int32_t value, std::int32_t* status);
std::int32_t APS5_VABI uhash_iputi_nid_postfix(Hashtable* table, std::int32_t key, std::int32_t value, std::int32_t* status);
void* APS5_VABI uhash_remove_nid_postfix(Hashtable* table, const void* key);
void* APS5_VABI uhash_iremove_nid_postfix(Hashtable* table, std::int32_t key);
std::int32_t APS5_VABI uhash_removei_nid_postfix(Hashtable* table, const void* key);
std::int32_t APS5_VABI uhash_iremovei_nid_postfix(Hashtable* table, std::int32_t key);
void APS5_VABI uhash_removeAll_nid_postfix(Hashtable* table);
const HashElement* APS5_VABI uhash_find_nid_postfix(const Hashtable* table, const void* key);
const HashElement* APS5_VABI uhash_nextElement_nid_postfix(const Hashtable* table, std::int32_t* position);
void* APS5_VABI uhash_removeElement_nid_postfix(Hashtable* table, const HashElement* element);
std::int8_t APS5_VABI uhash_equals_nid_postfix(const Hashtable* first, const Hashtable* second);
}
