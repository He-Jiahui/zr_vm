//
// Created by HeJiahui on 2025/6/20.
//
#include "zr_vm_core/value.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/convertion.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"

void ZrValueResetAsNull(SZrTypeValue *value) {
    value->type = ZR_VALUE_TYPE_NULL;
    value->isGarbageCollectable = ZR_FALSE;
}

void ZrValueInitAsRawObject(SZrState *state, SZrTypeValue *value, SZrRawObject *object) {
    EZrValueType type = object->type;
    value->type = type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = ZR_FALSE;
    // check liveness
    ZrGlobalValueStaticAssertIsAlive(state, value);
}


void ZrValueInitAsUInt(struct SZrState *state, SZrTypeValue *value, TUInt64 intValue) {
    value->type = ZR_VALUE_TYPE_UINT64;
    value->value.nativeObject.nativeUInt64 = intValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
}

void ZrValueInitAsInt(struct SZrState *state, SZrTypeValue *value, TInt64 intValue) {
    value->type = ZR_VALUE_TYPE_INT64;
    value->value.nativeObject.nativeInt64 = intValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
}

void ZrValueInitAsFloat(struct SZrState *state, SZrTypeValue *value, TFloat64 floatValue) {
    value->type = ZR_VALUE_TYPE_DOUBLE;
    value->value.nativeObject.nativeDouble = floatValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
}

void ZrValueInitAsNativePointer(struct SZrState *state, SZrTypeValue *value, TZrPtr pointerValue) {
    value->type = ZR_VALUE_TYPE_NATIVE_POINTER;
    value->value.nativeObject.nativePointer = pointerValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
}

TBool ZrValueEqual(struct SZrState *state, SZrTypeValue *value1, SZrTypeValue *value2) {
    EZrValueType type1 = value1->type;
    EZrValueType type2 = value2->type;
    TBool typeEqual = type1 == type2;
    TBool result = ZR_FALSE;
    if (typeEqual) {
        if (ZR_VALUE_IS_TYPE_NULL(type1)) {
            // type null
            result = ZR_TRUE;
        } else if (ZR_VALUE_IS_TYPE_STRING(type1)) {
            TZrString *str1 = ZR_CAST_STRING(state, value1->value.object);
            TZrString *str2 = ZR_CAST_STRING(state, value2->value.object);
            result = ZrStringEqual(str1, str2);
        } else if (ZR_VALUE_IS_TYPE_BASIC(type1)) {
            TBool valueEqual = value1->value.nativeObject.nativePointer == value2->value.nativeObject.nativePointer;
            result = valueEqual;
        } else {
            // todo: obj equal & struct equal
            result = ZR_FALSE;
        }
    } else {
        result = ZR_FALSE;
    }
    return result;
}

SZrTypeValue *ZrValueGetStackOffsetValue(SZrState *state, TZrMemoryOffset offset) {
    // ==max STACK_MAX
    // ==ft function top
    // ==t stack current top()
    // == $$ offset(<0) + stack_top
    // == $$ offset(>0) + function_base
    // ==f function base
    // ==0 stack base
    // == protection area
    // == invalid space (used as things as follows)

    // == global registry
    // == closure values


    SZrGlobalState *global = state->global;
    SZrCallInfo *callInfoTop = state->callInfoList;
    if (offset > 0) {
        TZrStackValuePointer valuePointer = callInfoTop->functionBase.valuePointer + offset;
        ZR_CHECK(state, offset <= callInfoTop->functionTop.valuePointer - (callInfoTop->functionBase.valuePointer + 1),
                 "call info offset is out of range from function base to stack top");
        if (valuePointer >= state->stackTop.valuePointer) {
            return &global->nullValue;
        }
        return ZrStackGetValue(valuePointer);
    }
    // access invalid stack space
    if (offset > ZR_VM_STACK_GLOBAL_MODULE_REGISTRY) {
        ZR_CHECK(state,
                 offset != 0 && -offset <= state->stackTop.valuePointer - (callInfoTop->functionBase.valuePointer + 1),
                 "call info offset is out of range from stack top to function base");
        return ZrStackGetValue(state->stackTop.valuePointer + offset);
    }
    // access global module registry
    if (offset == ZR_VM_STACK_GLOBAL_MODULE_REGISTRY) {
        return &global->loadedModulesRegistry;
    }
    // access closure values
    // convert to closure offset
    TZrMemoryOffset closureIndex = ZR_VM_STACK_GLOBAL_MODULE_REGISTRY - offset;
    ZR_CHECK(state, offset <= ZR_VM_STACK_CLOSURE_MAX, "closure offset is out of range");
    SZrTypeValue *functionBaseValue = ZrStackGetValue(callInfoTop->functionBase.valuePointer);
    if (ZrValueIsNative(functionBaseValue) && ZrValueGetType(functionBaseValue) == ZR_VALUE_TYPE_FUNCTION) {
        // is native function closure
        SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, functionBaseValue);
        return (closureIndex <= (TZrMemoryOffset) closure->closureValueCount)
                       ? &closure->closureValuesExtend[closureIndex - 1]
                       : &global->nullValue;
    }
    // no such closure or closure is lightweight function
    ZR_CHECK(state,
             ZrValueIsNative(functionBaseValue) && ZrValueGetType(functionBaseValue) == ZR_VALUE_TYPE_NATIVE_POINTER,
             "function base is not a native function");
    // is zr closure
    return &global->nullValue;
}


TZrString *ZrValueConvertToString(struct SZrState *state, SZrTypeValue *value) {
    if (!ZrValueCanValueToString(state, value)) {
        return ZR_NULL;
    }
    EZrValueType type = value->type;
    // todo: basic type to string
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            return ZrStringCreateFromNative(state, ZR_STRING_NULL_STRING);
        } break;
        case ZR_VALUE_TYPE_STRING: {
            return ZR_CAST_STRING(state, value->value.object);
        } break;
            ZR_VALUE_CASES_NUMBER
            ZR_VALUE_CASES_NATIVE { return ZrStringFromNumber(state, value); }
            break;
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *object = ZR_CAST_OBJECT(state, value->value.object);
            SZrMeta *meta = ZrObjectGetMetaRecursively(state, object, ZR_META_TO_STRING);
            // todo: call meta function
            // make it as closure
            // call meta function
        } break;

        default: {
            // LOG ERROR
            ZR_ASSERT(ZR_FALSE);
        } break;
    }
    return ZR_NULL;
}
