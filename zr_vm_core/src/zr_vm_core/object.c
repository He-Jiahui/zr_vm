//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"

#include "zr_vm_core/conversion.h"
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
    ZrHashSetInit(state, &object->nodeMap, ZR_OBJECT_TABLE_INIT_SIZE_LOG2);

    // CONSTRUCT OBJECT FROM PROTOTYPE
    SZrMeta *constructor = ZrObjectGetMetaRecursively(state->global, object, ZR_META_CONSTRUCTOR);
    // todo: call constructor
}

TBool ZrObjectCompare(struct SZrState *state, SZrObject *object1, SZrObject *object2) {
    if (object1 == object2) {
        return ZR_TRUE;
    }
    if (object1->prototype != object2->prototype) {
        return ZR_FALSE;
    }
    SZrGlobalState *global = state->global;
    SZrMeta *equal = ZrObjectGetMetaRecursively(global, object1, ZR_META_COMPARE);
    if (equal == ZR_NULL) {
        return ZR_FALSE;
    }
    // todo: call compare function
    return ZR_FALSE;
}


void ZrObjectSetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key, const SZrTypeValue *value) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrLogError(state, "attempt to set value with null key");
        return;
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, nodeMap, key);
    if (pair != ZR_NULL) {
        // todo: we create a box wrapper for the value
        // pair->value = *value;
        return;
    }
    // todo: we create a box wrapper for the value
    return;
}
