//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_OBJECT_H
#define ZR_VM_CORE_OBJECT_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/hash.h"
struct SZrState;

struct ZR_STRUCT_ALIGN SZrObjectPrototype {
    SZrRawObject super;
    struct SZrObjectPrototype *superPrototype;
};

typedef struct SZrObjectPrototype SZrObjectPrototype;

struct ZR_STRUCT_ALIGN SZrObject {
    SZrRawObject super;
    TUInt64 nodeCapacity;

    SZrObjectPrototype *prototype;

    SZrHashNode *nodeList;
    SZrHashNode *nodeListEnd;

    SZrRawObject *gcList;
};

typedef struct SZrObject SZrObject;

ZR_CORE_API SZrObject *ZrObjectNew(struct SZrState *state, SZrObjectPrototype *prototype);

ZR_CORE_API TNativeString ZrObjectToString(struct SZrState *state, SZrObject *object);

#endif //ZR_VM_CORE_OBJECT_H
