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
    object->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
    ZrHashSetConstruct(&object->nodeMap);
    return object;
}

SZrObject *ZrObjectNewCustomized(struct SZrState *state, TZrSize size, EZrObjectInternalType internalType) {
    // 根据 internalType 选择正确的值类型
    EZrValueType valueType = ZR_VALUE_TYPE_OBJECT;
    if (internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        valueType = ZR_VALUE_TYPE_ARRAY;
    }
    SZrRawObject *rawObject = ZrRawObjectNew(state, valueType, size, ZR_FALSE);
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    object->prototype = ZR_NULL;
    object->internalType = internalType;
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

TBool ZrObjectCompareWithAddress(struct SZrState *state, SZrObject *object1, SZrObject *object2) {
    ZR_UNUSED_PARAMETER(state);
    return object1 == object2;
}


void ZrObjectSetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key, const SZrTypeValue *value) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrLogError(state, "attempt to set value with null key");
        return;
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, nodeMap, key);
    if (pair == ZR_NULL) {
        pair = ZrHashSetAdd(state, nodeMap, key);
    }
    ZrValueCopy(state, &pair->value, value);
}

const SZrTypeValue *ZrObjectGetValue(struct SZrState *state, SZrObject *object, const SZrTypeValue *key) {
    ZR_ASSERT(object != ZR_NULL);
    if (key == ZR_NULL) {
        ZrLogError(state, "attempt to get value with null key");
        return ZR_NULL;
    }
    SZrHashSet *nodeMap = &object->nodeMap;
    SZrHashKeyValuePair *pair = ZrHashSetFind(state, nodeMap, key);
    if (pair == ZR_NULL) {
        return ZR_NULL;
    }
    return &pair->value;
}
