//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"


SZrObject *ZrObjectNew(SZrState *state, SZrObjectPrototype *prototype) {
    SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrObject), ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = prototype;
    // object->gcList = ZR_NULL;
    ZrHashSetConstruct(&object->nodeMap);
    return object;
}

void ZrObjectInit(struct SZrState *state, SZrObject *object) {
    SZrGlobalState *global = state->global;
    // todo:
    ZrHashSetInit(global, &object->nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);

    // CONSTRUCT OBJECT FROM PROTOTYPE
    SZrMeta* constructor = ZrObjectGetMetaRecursively(state, object, ZR_META_CONSTRUCTOR);
    // todo: call constructor
}
