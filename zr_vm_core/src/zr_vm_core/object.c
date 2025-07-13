//
// Created by HeJiahui on 2025/6/22.
//
#include "zr_vm_core/object.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/state.h"


SZrObject *ZrObjectNew(SZrState *state, SZrObjectPrototype *prototype) {
    SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_OBJECT, sizeof(SZrObject));
    SZrObject *object = ZR_CAST(SZrObject *, rawObject);
    object->prototype = prototype;
    object->gcList = ZR_NULL;
    object->nodeCapacity = 0;
    object->nodeMap = ZR_NULL;
    return object;
}

void ZrObjectInit(struct SZrState *state, SZrObject *object) {
    // todo:
}
