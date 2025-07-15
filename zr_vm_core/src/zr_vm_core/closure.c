//
// Created by HeJiahui on 2025/7/15.
//
#include "zr_vm_core/closure.h"

#include "zr_vm_core/convertion.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"

SZrClosureNative *ZrClosureNativeNew(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object = ZrRawObjectNew(state, ZR_VALUE_TYPE_FUNCTION, sizeof(SZrClosureNative), ZR_TRUE);
    SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    return closure;
}

SZrClosure *ZrClosureNew(struct SZrState *state, TZrSize closureValueCount) {
    SZrRawObject *object = ZrRawObjectNew(state, ZR_VALUE_TYPE_FUNCTION,
                                          sizeof(SZrClosure) + sizeof(SZrClosureValue) * closureValueCount, ZR_FALSE);
    SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, object);
    closure->closureValueCount = closureValueCount;
    closure->function = ZR_NULL;
    ZrMemoryRawSet(closure->closureValuesExtend, (TByte) ZR_NULL, sizeof(SZrClosureValue) * closureValueCount);
    return closure;
}

void ZrClosureInitValue(struct SZrState *state, SZrClosure *closure) {
    for (TZrSize i = 0; i < closure->closureValueCount; i++) {
        SZrRawObject *rawObject = ZrRawObjectNew(state, ZR_VALUE_TYPE_CLOSURE_VALUE, sizeof(SZrClosureValue), ZR_FALSE);
        SZrClosureValue *closureValue = ZR_CAST_VM_CLOSURE_VALUE(state, rawObject);
        // if value is on stack
        closureValue->value.valuePointer = ZR_CAST_STACK_OBJECT(&closureValue->link.independentValue);
        ZrValueResetAsNull(&closureValue->value.valuePointer->value);
        closure->closureValuesExtend[i] = *closureValue;
        ZrGarbageCollectorBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                                  ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue));
    }
}
