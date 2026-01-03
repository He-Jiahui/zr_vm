//
// Created by HeJiahui on 2025/6/20.
//
#include <stdarg.h>
#include <stdio.h>

#include "zr_vm_core/value.h"

#include "zr_vm_core/call_info.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

void ZrValueBarrier(struct SZrState *state, SZrRawObject *object, SZrTypeValue *value) {
    if (!value->isGarbageCollectable) {
        return;
    }
    ZrRawObjectBarrier(state, object, value->value.object);
}

void ZrValueResetAsNull(SZrTypeValue *value) {
    value->type = ZR_VALUE_TYPE_NULL;
    value->isGarbageCollectable = ZR_FALSE;
}

void ZrValueInitAsRawObject(SZrState *state, SZrTypeValue *value, SZrRawObject *object) {
    EZrValueType type = object->type;
    value->type = type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = object->isNative;
    // check liveness
    ZrGcValueStaticAssertIsAlive(state, value);
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
            SZrString *str1 = ZR_CAST_STRING(state, value1->value.object);
            SZrString *str2 = ZR_CAST_STRING(state, value2->value.object);
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


void ZrValueCopy(struct SZrState *state, SZrTypeValue *destination, const SZrTypeValue *source) {
    destination->value = source->value;
    destination->type = source->type;
    destination->isGarbageCollectable = ZrValueIsGarbageCollectable(source);
    destination->isNative = ZrValueIsNative(source);
    ZrGcValueStaticAssertIsAlive(state, destination);
}

TUInt64 ZrValueGetHash(struct SZrState *state, const SZrTypeValue *value) {
    EZrValueType type = value->type;
    TUInt64 hash = 0;
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            hash = 0;
        } break;
        case ZR_VALUE_TYPE_BOOL: {
            hash = value->value.nativeObject.nativeBool ? 1 : 0;
        } break;
            ZR_VALUE_CASES_INT { hash = value->value.nativeObject.nativeUInt64; }
            break;
            ZR_VALUE_CASES_FLOAT { hash = value->value.nativeObject.nativeUInt64; }
            break;
        case ZR_VALUE_TYPE_STRING: {
            SZrString *string = ZR_CAST_STRING(state, value->value.object);
            hash = string->super.hash;
        } break;
        case ZR_VALUE_TYPE_NATIVE_DATA: {
            hash = (TUInt64) value->value.nativeObject.nativePointer;
        } break;
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *object = ZR_CAST_OBJECT(state, value->value.object);
            hash = object->super.hash;
        } break;
            // todo: support more types
        default: {
            hash = value->value.nativeObject.nativeUInt64;
        } break;
    }
    return hash;
}

TBool ZrValueCompareDirectly(struct SZrState *state, const SZrTypeValue *value1, const SZrTypeValue *value2) {
    EZrValueType type1 = value1->type;
    EZrValueType type2 = value2->type;
    TBool typeEqual = type1 == type2;
    TBool result = ZR_FALSE;
    if (typeEqual) {
        switch (type1) {
            case ZR_VALUE_TYPE_NULL: {
                result = ZR_TRUE;
            } break;
            case ZR_VALUE_TYPE_BOOL: {
                result = (!value1->value.nativeObject.nativeBool) == (!value2->value.nativeObject.nativeBool);
            } break;
                ZR_VALUE_CASES_INT {
                    result = value1->value.nativeObject.nativeInt64 == value2->value.nativeObject.nativeInt64;
                }
                break;
                ZR_VALUE_CASES_FLOAT {
                    result = value1->value.nativeObject.nativeDouble == value2->value.nativeObject.nativeDouble;
                }
                break;
            case ZR_VALUE_TYPE_STRING: {
                SZrString *str1 = ZR_CAST_STRING(state, value1->value.object);
                SZrString *str2 = ZR_CAST_STRING(state, value2->value.object);
                result = ZrStringEqual(str1, str2);
            } break;
            case ZR_VALUE_TYPE_NATIVE_DATA: {
                result = value1->value.nativeObject.nativePointer == value2->value.nativeObject.nativePointer;
            } break;
            case ZR_VALUE_TYPE_OBJECT: {
                SZrObject *object1 = ZR_CAST_OBJECT(state, value1->value.object);
                SZrObject *object2 = ZR_CAST_OBJECT(state, value2->value.object);
                result = ZrObjectCompareWithAddress(state, object1, object2);
            } break;
                // todo: compare more types
            default: {
                result = value1->value.nativeObject.nativeUInt64 == value2->value.nativeObject.nativeUInt64;
            } break;
        }
    } else {
        result = ZR_FALSE;
    }
    return result;
}

SZrString *ZrValueConvertToString(struct SZrState *state, SZrTypeValue *value) {
    if (!ZrValueCanValueToString(state, value)) {
        return ZR_NULL;
    }
    
    // 优先查找并调用 TO_STRING 元方法
    SZrMeta *meta = ZrValueGetMeta(state, value, ZR_META_TO_STRING);
    if (meta != ZR_NULL && meta->function != ZR_NULL) {
        // 保存当前栈状态
        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
        SZrCallInfo *savedCallInfo = state->callInfoList;
        
        // 准备调用元方法：将 meta->function 和 self 放到栈上
        ZrFunctionCheckStackAndGc(state, 2, savedStackTop);
        TZrStackValuePointer base = savedStackTop;
        ZrStackSetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
        ZrStackCopyValue(state, base + 1, value);
        state->stackTop.valuePointer = base + 2;
        
        // 调用元方法
        ZrFunctionCallWithoutYield(state, base, 1);
        
        // 检查执行状态
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            // 获取返回值
            SZrTypeValue *returnValue = ZrStackGetValue(base);
            if (returnValue->type == ZR_VALUE_TYPE_STRING) {
                SZrString *result = ZR_CAST_STRING(state, returnValue->value.object);
                // 恢复栈状态
                state->stackTop.valuePointer = savedStackTop;
                state->callInfoList = savedCallInfo;
                return result;
            }
        }
        
        // 恢复栈状态
        state->stackTop.valuePointer = savedStackTop;
        state->callInfoList = savedCallInfo;
    }
    
    // 如果元方法不存在或调用失败，使用现有的直接转换逻辑作为后备
    EZrValueType type = value->type;
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
            // 如果元方法调用失败，返回默认字符串
            SZrObject *object = ZR_CAST_OBJECT(state, value->value.object);
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "[object type=%d]", (int)object->internalType);
            return ZrStringCreateFromNative(state, buffer);
        } break;

        default: {
            // LOG ERROR
            ZR_ASSERT(ZR_FALSE);
        } break;
    }
    return ZR_NULL;
}


struct SZrMeta *ZrValueGetMeta(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType) {
    EZrValueType type = value->type;
    switch (type) {
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *object = ZR_CAST_OBJECT(state, value->value.object);
            SZrMeta *meta = ZrObjectGetMetaRecursively(state->global, object, metaType);
            // 如果对象没有自己的元方法，回退到基本类型的元方法
            if (meta == ZR_NULL && state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_OBJECT] != ZR_NULL) {
                meta = state->global->basicTypeObjectPrototype[ZR_VALUE_TYPE_OBJECT]->metaTable.metas[metaType];
            }
            return meta;
        } break;
        case ZR_VALUE_TYPE_NATIVE_DATA: {
            // todo:
            return ZR_NULL;
        } break;
        default: {
            if (state->global->basicTypeObjectPrototype[type] == ZR_NULL) {
                return ZR_NULL;
            }
            return state->global->basicTypeObjectPrototype[type]->metaTable.metas[metaType];
        } break;
    }
}

// 调用指定值的元方法并返回结果
TBool ZrValueCallMetaMethod(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType,
                             SZrTypeValue *result, TZrSize argumentCount, ...) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrMeta *meta = ZrValueGetMeta(state, value, metaType);
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 保存当前栈状态
    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
    SZrCallInfo *savedCallInfo = state->callInfoList;
    
    // 准备调用元方法
    TZrStackValuePointer base = savedStackTop;
    TZrSize totalArgs = 1 + argumentCount; // self + 其他参数
    ZrFunctionCheckStackAndGc(state, totalArgs, base);
    
    // 将 meta->function 放到栈上
    ZrStackSetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    
    // 将 self 放到栈上
    ZrStackCopyValue(state, base + 1, value);
    
    // 处理可变参数
    if (argumentCount > 0) {
        va_list args;
        va_start(args, argumentCount);
        for (TZrSize i = 0; i < argumentCount; i++) {
            SZrTypeValue *arg = va_arg(args, SZrTypeValue *);
            ZrStackCopyValue(state, base + 2 + i, arg);
        }
        va_end(args);
    }
    
    state->stackTop.valuePointer = base + 1 + totalArgs;
    
    // 调用元方法
    ZrFunctionCallWithoutYield(state, base, 1);
    
    // 检查执行状态
    TBool success = ZR_FALSE;
    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        // 获取返回值
        SZrTypeValue *returnValue = ZrStackGetValue(base);
        if (result != ZR_NULL) {
            ZrValueCopy(state, result, returnValue);
        }
        success = ZR_TRUE;
    }
    
    // 恢复栈状态
    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;
    
    return success;
}

// 专门用于调用 TO_STRING 元方法的便捷函数
SZrString *ZrValueCallMetaToString(struct SZrState *state, SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrTypeValue result;
    TBool success = ZrValueCallMetaMethod(state, value, ZR_META_TO_STRING, &result, 0);
    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        return ZR_CAST_STRING(state, result.value.object);
    }
    
    return ZR_NULL;
}