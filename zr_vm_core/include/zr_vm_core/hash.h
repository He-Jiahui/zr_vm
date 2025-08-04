//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_HASH_H
#define ZR_VM_CORE_HASH_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"
struct SZrGlobalState;
struct SZrState;
struct SZrTypeValue;
// same type is basic condition for hash object
typedef TBool (*FZrHashCompare)(struct SZrState *state, const SZrRawObject *object1, const SZrRawObject *object2);

struct ZR_STRUCT_ALIGN SZrHashKeyValuePair {
    SZrTypeValue key;
    struct SZrHashKeyValuePair *next;
    // maybe a key value pair
    SZrRawObject *value;
};

typedef struct SZrHashKeyValuePair SZrHashKeyValuePair;

ZR_CORE_API TUInt64 ZrHashSeedCreate(struct SZrGlobalState *global, TUInt64 uniqueNumber);

ZR_CORE_API TUInt64 ZrHashCreate(struct SZrGlobalState *global, TNativeString string, TZrSize length);


#endif // ZR_VM_CORE_HASH_H
