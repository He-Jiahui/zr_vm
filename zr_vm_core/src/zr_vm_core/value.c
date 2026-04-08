//
// Created by HeJiahui on 2025/6/20.
//
#include <stdarg.h>
#include <stdio.h>

#include "zr_vm_core/value.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"

void ZrCore_Value_Barrier(struct SZrState *state, SZrRawObject *object, SZrTypeValue *value) {
    if (!value->isGarbageCollectable) {
        return;
    }
    ZrCore_RawObject_Barrier(state, object, value->value.object);
}

void ZrCore_Value_ResetAsNull(SZrTypeValue *value) {
    value->type = ZR_VALUE_TYPE_NULL;
    value->value.nativeObject.nativeUInt64 = 0;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

void ZrCore_Value_InitAsRawObject(SZrState *state, SZrTypeValue *value, SZrRawObject *object) {
    EZrValueType type = (EZrValueType)object->type;
    value->type = type;
    value->value.object = object;
    value->isGarbageCollectable = ZR_TRUE;
    value->isNative = object->isNative;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
    // check liveness
    ZrCore_Gc_ValueStaticAssertIsAlive(state, value);
}


void ZrCore_Value_InitAsUInt(struct SZrState *state, SZrTypeValue *value, TZrUInt64 intValue) {
    ZR_UNUSED_PARAMETER(state);
    value->type = ZR_VALUE_TYPE_UINT64;
    value->value.nativeObject.nativeUInt64 = intValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

void ZrCore_Value_InitAsInt(struct SZrState *state, SZrTypeValue *value, TZrInt64 intValue) {
    ZR_UNUSED_PARAMETER(state);
    value->type = ZR_VALUE_TYPE_INT64;
    value->value.nativeObject.nativeInt64 = intValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

void ZrCore_Value_InitAsBool(struct SZrState *state, SZrTypeValue *value, TZrBool boolValue) {
    ZR_UNUSED_PARAMETER(state);
    value->type = ZR_VALUE_TYPE_BOOL;
    value->value.nativeObject.nativeBool = boolValue ? ZR_TRUE : ZR_FALSE;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

void ZrCore_Value_InitAsFloat(struct SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue) {
    ZR_UNUSED_PARAMETER(state);
    value->type = ZR_VALUE_TYPE_DOUBLE;
    value->value.nativeObject.nativeDouble = floatValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

void ZrCore_Value_InitAsNativePointer(struct SZrState *state, SZrTypeValue *value, TZrPtr pointerValue) {
    ZR_UNUSED_PARAMETER(state);
    value->type = ZR_VALUE_TYPE_NATIVE_POINTER;
    value->value.nativeObject.nativePointer = pointerValue;
    value->isGarbageCollectable = ZR_FALSE;
    value->isNative = ZR_TRUE;
    value->ownershipKind = ZR_OWNERSHIP_VALUE_KIND_NONE;
    value->ownershipControl = ZR_NULL;
    value->ownershipWeakRef = ZR_NULL;
}

TZrBool ZrCore_Value_Equal(struct SZrState *state, SZrTypeValue *value1, SZrTypeValue *value2) {
    EZrValueType type1 = value1->type;
    EZrValueType type2 = value2->type;
    TZrBool typeEqual = type1 == type2;
    TZrBool result = ZR_FALSE;
    if (typeEqual) {
        if (ZR_VALUE_IS_TYPE_NULL(type1)) {
            // type null
            result = ZR_TRUE;
        } else if (ZR_VALUE_IS_TYPE_STRING(type1)) {
            SZrString *str1 = ZR_CAST_STRING(state, value1->value.object);
            SZrString *str2 = ZR_CAST_STRING(state, value2->value.object);
            result = ZrCore_String_Equal(str1, str2);
        } else if (ZR_VALUE_IS_TYPE_BOOL(type1)) {
            result = (value1->value.nativeObject.nativeBool != 0) == (value2->value.nativeObject.nativeBool != 0);
        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(type1)) {
            result = value1->value.nativeObject.nativeInt64 == value2->value.nativeObject.nativeInt64;
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(type1)) {
            result = value1->value.nativeObject.nativeUInt64 == value2->value.nativeObject.nativeUInt64;
        } else if (ZR_VALUE_IS_TYPE_FLOAT(type1)) {
            result = value1->value.nativeObject.nativeDouble == value2->value.nativeObject.nativeDouble;
        } else if (ZR_VALUE_IS_TYPE_NATIVE(type1)) {
            result = value1->value.nativeObject.nativePointer == value2->value.nativeObject.nativePointer;
        } else if (value1->isGarbageCollectable && value2->isGarbageCollectable) {
            result = value1->value.object == value2->value.object;
        } else {
            result = value1->value.nativeObject.nativeUInt64 == value2->value.nativeObject.nativeUInt64;
        }
    } else {
        result = ZR_FALSE;
    }
    return result;
}

SZrTypeValue *ZrCore_Value_GetStackOffsetValue(SZrState *state, TZrMemoryOffset offset) {
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
        return ZrCore_Stack_GetValue(valuePointer);
    }
    // access invalid stack space
    if (offset > ZR_VM_STACK_GLOBAL_MODULE_REGISTRY) {
        ZR_CHECK(state,
                 offset != 0 && -offset <= state->stackTop.valuePointer - (callInfoTop->functionBase.valuePointer + 1),
                 "call info offset is out of range from stack top to function base");
        return ZrCore_Stack_GetValue(state->stackTop.valuePointer + offset);
    }
    // access global module registry
    if (offset == ZR_VM_STACK_GLOBAL_MODULE_REGISTRY) {
        return &global->loadedModulesRegistry;
    }
    // access closure values
    // convert to closure offset
    TZrMemoryOffset closureIndex = ZR_VM_STACK_GLOBAL_MODULE_REGISTRY - offset;
    ZR_CHECK(state, offset <= ZR_VM_STACK_CLOSURE_MAX, "closure offset is out of range");
    SZrTypeValue *functionBaseValue = ZrCore_Stack_GetValue(callInfoTop->functionBase.valuePointer);
    if (ZrCore_Value_IsNative(functionBaseValue) &&
        (ZrCore_Value_GetType(functionBaseValue) == ZR_VALUE_TYPE_FUNCTION ||
         ZrCore_Value_GetType(functionBaseValue) == ZR_VALUE_TYPE_CLOSURE)) {
        // Native callables are backed by SZrClosureNative. Some producers tag them
        // as FUNCTION, while native bindings currently surface them as CLOSURE.
        SZrClosureNative *closure = ZR_CAST_NATIVE_CLOSURE(state, functionBaseValue->value.object);
        return (closureIndex <= (TZrMemoryOffset) closure->closureValueCount)
                       ? ZrCore_ClosureNative_GetCaptureValue(closure, closureIndex - 1)
                       : &global->nullValue;
    }
    // no such closure or closure is lightweight function
    ZR_CHECK(state,
             ZrCore_Value_IsNative(functionBaseValue) && ZrCore_Value_GetType(functionBaseValue) == ZR_VALUE_TYPE_NATIVE_POINTER,
             "function base is not a native function");
    // is zr closure
    return &global->nullValue;
}


void ZrCore_Value_CopySlow(struct SZrState *state, SZrTypeValue *destination, const SZrTypeValue *source) {
    ZrCore_Ownership_AssignValue(state, destination, source);
}

TZrUInt64 ZrCore_Value_GetHash(struct SZrState *state, const SZrTypeValue *value) {
    EZrValueType type = value->type;
    TZrUInt64 hash = 0;
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            hash = 0;
        } break;
        case ZR_VALUE_TYPE_BOOL: {
            hash = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
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
            hash = (TZrUInt64) value->value.nativeObject.nativePointer;
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

TZrBool ZrCore_Value_CompareDirectly(struct SZrState *state, const SZrTypeValue *value1, const SZrTypeValue *value2) {
    EZrValueType type1 = value1->type;
    EZrValueType type2 = value2->type;
    TZrBool typeEqual = type1 == type2;
    TZrBool result = ZR_FALSE;
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
                result = ZrCore_String_Equal(str1, str2);
            } break;
            case ZR_VALUE_TYPE_NATIVE_DATA: {
                result = value1->value.nativeObject.nativePointer == value2->value.nativeObject.nativePointer;
            } break;
            case ZR_VALUE_TYPE_OBJECT: {
                SZrObject *object1 = ZR_CAST_OBJECT(state, value1->value.object);
                SZrObject *object2 = ZR_CAST_OBJECT(state, value2->value.object);
                result = ZrCore_Object_CompareWithAddress(state, object1, object2);
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

SZrString *ZrCore_Value_ConvertToString(struct SZrState *state, SZrTypeValue *value) {
    SZrTypeValue stableValue;
    SZrFunctionStackAnchor savedStackTopAnchor;

    if (!ZrCore_Value_CanValueToString(state, value)) {
        return ZR_NULL;
    }
    stableValue = *value;

    // 优先查找并调用 TO_STRING 元方法
    SZrMeta *meta = ZrCore_Value_GetMeta(state, &stableValue, ZR_META_TO_STRING);
    if (meta != ZR_NULL && meta->function != ZR_NULL) {
        // 保存当前栈状态
        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
        SZrCallInfo *savedCallInfo = state->callInfoList;
        TZrStackValuePointer scratchBase =
                savedCallInfo != ZR_NULL ? savedCallInfo->functionTop.valuePointer : savedStackTop;
        SZrFunctionStackAnchor scratchBaseAnchor;
        SZrFunctionStackAnchor callInfoBaseAnchor;
        SZrFunctionStackAnchor callInfoTopAnchor;
        TZrBool hasCallInfoAnchors = ZR_FALSE;
        ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
        ZrCore_Function_StackAnchorInit(state, scratchBase, &scratchBaseAnchor);
        if (savedCallInfo != ZR_NULL) {
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
            hasCallInfoAnchors = ZR_TRUE;
        }

        // 准备调用元方法：将 meta->function 和 self 放到栈上
        scratchBase = ZrCore_Function_ReserveScratchSlots(state, 2, scratchBase);
        savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
        scratchBase = ZrCore_Function_StackAnchorRestore(state, &scratchBaseAnchor);
        if (savedCallInfo != ZR_NULL) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
            scratchBase = savedCallInfo->functionTop.valuePointer;
        }
        state->stackTop.valuePointer = scratchBase + 2;
        if (savedCallInfo != ZR_NULL &&
            savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
            savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
            ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        }
        ZrCore_Stack_CopyValue(state, scratchBase + 1, &stableValue);
        scratchBase = ZrCore_Function_StackAnchorRestore(state, &scratchBaseAnchor);
        if (hasCallInfoAnchors) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        }
        ZrCore_Stack_SetRawObjectValue(state, scratchBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
        // 调用元方法
        scratchBase = ZrCore_Function_CallWithoutYieldAndRestore(state, scratchBase, 1);
        savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
        if (hasCallInfoAnchors) {
            savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
            savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        }

        // 检查执行状态
        if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
            // 获取返回值
            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(scratchBase);
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
    EZrValueType type = stableValue.type;
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            return ZrCore_String_CreateFromNative(state, ZR_STRING_NULL_STRING);
        } break;
        case ZR_VALUE_TYPE_STRING: {
            return ZR_CAST_STRING(state, stableValue.value.object);
        } break;
            ZR_VALUE_CASES_NUMBER
            ZR_VALUE_CASES_NATIVE { return ZrCore_String_FromNumber(state, &stableValue); }
            break;
        case ZR_VALUE_TYPE_OBJECT: {
            // 如果元方法调用失败，返回默认字符串
            SZrObject *object = ZR_CAST_OBJECT(state, stableValue.value.object);
            char buffer[ZR_RUNTIME_SMALL_TEXT_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "[object type=%d]", (int) object->internalType);
            return ZrCore_String_CreateFromNative(state, buffer);
        } break;
        case ZR_VALUE_TYPE_FUNCTION: {
            return ZrCore_String_CreateFromNative(state, "[function]");
        } break;
        case ZR_VALUE_TYPE_CLOSURE: {
            return ZrCore_String_CreateFromNative(state, "[closure]");
        } break;

        default: {
            // LOG ERROR
            ZR_ASSERT(ZR_FALSE);
        } break;
    }
    return ZR_NULL;
}


struct SZrMeta *ZrCore_Value_GetMeta(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType) {
    EZrValueType type = value->type;
    switch (type) {
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *object = ZR_CAST_OBJECT(state, value->value.object);
            SZrMeta *meta = ZrCore_Object_GetMetaRecursively(state->global, object, metaType);
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
TZrBool ZrCore_Value_CallMetaMethod(struct SZrState *state, SZrTypeValue *value, EZrMetaType metaType, SZrTypeValue *result,
                            TZrSize argumentCount, ...) {
    SZrTypeValue stableValue;
    SZrTypeValue copiedResult;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor baseAnchor;
    SZrFunctionStackAnchor callInfoBaseAnchor;
    SZrFunctionStackAnchor callInfoTopAnchor;
    TZrBool hasCallInfoAnchors = ZR_FALSE;

    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    stableValue = *value;

    SZrMeta *meta = ZrCore_Value_GetMeta(state, &stableValue, metaType);
    if (meta == ZR_NULL || meta->function == ZR_NULL) {
        return ZR_FALSE;
    }

    // 保存当前栈状态
    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
    SZrCallInfo *savedCallInfo = state->callInfoList;

    // 准备调用元方法
    TZrSize totalArgs = 1 + argumentCount; // self + 其他参数
    TZrSize scratchSlots = 1 + totalArgs; // function + self + 其他参数
    TZrStackValuePointer base =
            savedCallInfo != ZR_NULL ? savedCallInfo->functionTop.valuePointer : savedStackTop;
    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    ZrCore_Function_StackAnchorInit(state, base, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
        hasCallInfoAnchors = ZR_TRUE;
    }
    base = ZrCore_Function_ReserveScratchSlots(state, scratchSlots, base);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (savedCallInfo != ZR_NULL) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
        base = savedCallInfo->functionTop.valuePointer;
    }

    state->stackTop.valuePointer = base + 1 + totalArgs;
    if (savedCallInfo != ZR_NULL &&
        savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionBase.valuePointer, &callInfoBaseAnchor);
        ZrCore_Function_StackAnchorInit(state, savedCallInfo->functionTop.valuePointer, &callInfoTopAnchor);
    }

    // 将 self 放到栈上
    ZrCore_Stack_CopyValue(state, base + 1, &stableValue);
    base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
    if (hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
    }

    // 处理可变参数
    if (argumentCount > 0) {
        va_list args;
        va_start(args, argumentCount);
        for (TZrSize i = 0; i < argumentCount; i++) {
            SZrTypeValue *arg = va_arg(args, SZrTypeValue *);
            ZrCore_Stack_CopyValue(state, base + 2 + i, arg);
            base = ZrCore_Function_StackAnchorRestore(state, &baseAnchor);
            if (hasCallInfoAnchors) {
                savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
                savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
            }
        }
        va_end(args);
    }
    
    // 将 meta->function 放到栈上
    ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    // 调用元方法
    base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (hasCallInfoAnchors) {
        savedCallInfo->functionBase.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoBaseAnchor);
        savedCallInfo->functionTop.valuePointer = ZrCore_Function_StackAnchorRestore(state, &callInfoTopAnchor);
    }

    // 检查执行状态
    TZrBool success = ZR_FALSE;
    if (state->threadStatus == ZR_THREAD_STATUS_FINE) {
        // 获取返回值
        SZrTypeValue *returnValue = ZrCore_Stack_GetValue(base);
        if (result != ZR_NULL) {
            ZrCore_Value_ResetAsNull(&copiedResult);
            ZrCore_Value_Copy(state, &copiedResult, returnValue);
            *result = copiedResult;
        }
        success = ZR_TRUE;
    }

    // 恢复栈状态
    state->stackTop.valuePointer = savedStackTop;
    state->callInfoList = savedCallInfo;

    return success;
}

// 专门用于调用 TO_STRING 元方法的便捷函数
SZrString *ZrCore_Value_CallMetaToString(struct SZrState *state, SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }

    SZrTypeValue result;
    TZrBool success = ZrCore_Value_CallMetaMethod(state, value, ZR_META_TO_STRING, &result, 0);
    if (success && result.type == ZR_VALUE_TYPE_STRING) {
        return ZR_CAST_STRING(state, result.value.object);
    }

    return ZR_NULL;
}
// 将值转换为调试字符串，包含详细信息（用于测试和调试）
SZrString *ZrCore_Value_ToDebugString(struct SZrState *state, SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL) {
        return ZrCore_String_CreateFromNative(state, "<null>");
    }


    const TZrSize maxElementsToShow = ZR_RUNTIME_DEBUG_COLLECTION_PREVIEW_MAX;
    char buffer[ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH];
    TZrSize offset = 0;

    switch (value->type) {
        case ZR_VALUE_TYPE_OBJECT: {
            SZrObject *obj = ZR_CAST_OBJECT(state, value->value.object);
            if (obj == ZR_NULL) {
                return ZrCore_String_CreateFromNative(state, "<null object>");
            }

            // 获取类型名称
            const char *typeName = "object";
            if (obj->prototype != ZR_NULL && obj->prototype->name != ZR_NULL) {
                TZrNativeString nameStr = ZrCore_String_GetNativeString(obj->prototype->name);
                if (nameStr != ZR_NULL) {
                    typeName = nameStr;
                }
            }

            offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "<object type=%s>{", typeName);

            // 遍历 nodeMap 获取字段（最多展示预览上限）
            TZrSize fieldCount = 0;
            TZrSize totalFieldCount = 0;
            if (obj->nodeMap.isValid && obj->nodeMap.buckets != ZR_NULL) {
                totalFieldCount = obj->nodeMap.elementCount;
                for (TZrSize i = 0; i < obj->nodeMap.capacity && fieldCount < maxElementsToShow; i++) {
                    SZrHashKeyValuePair *pair = obj->nodeMap.buckets[i];
                    while (pair != ZR_NULL && fieldCount < maxElementsToShow) {
                        // 获取键名（假设是字符串）
                        if (pair->key.type == ZR_VALUE_TYPE_STRING) {
                            SZrString *keyStr = ZR_CAST_STRING(state, pair->key.value.object);
                            TZrNativeString keyNative = ZrCore_String_GetNativeString(keyStr);
                            if (keyNative != ZR_NULL) {
                                if (fieldCount > 0) {
                                    offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, ", ");
                                }
                                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "%s : ", keyNative);

                                // 获取值的字符串表示
                                SZrString *valueStr = ZrCore_Value_ConvertToString(state, &pair->value);
                                if (valueStr != ZR_NULL) {
                                    TZrNativeString valueNative = ZrCore_String_GetNativeString(valueStr);
                                    if (valueNative != ZR_NULL) {
                                        offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "%s",
                                                           valueNative);
                                    }
                                } else {
                                    offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "<?>");
                                }
                                fieldCount++;
                            }
                        }
                        pair = pair->next;
                    }
                }
            }

            // 如果有更多字段，添加省略号
            if (totalFieldCount > maxElementsToShow) {
                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, ", ...");
            }

            // 只有当总数达到预览上限时才显示 count 信息
            if (totalFieldCount >= maxElementsToShow) {
                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, " | count = %lu}",
                                   (unsigned long) totalFieldCount);
            } else {
                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "}");
            }
            break;
        }
        case ZR_VALUE_TYPE_ARRAY: {
            SZrObject *obj = ZR_CAST_OBJECT(state, value->value.object);
            if (obj == ZR_NULL || obj->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
                return ZrCore_String_CreateFromNative(state, "<invalid array>");
            }

            // 尝试从 nodeMap 中获取数组元素（假设数组元素通过 nodeMap 存储）
            // 注意：这可能不是数组的实际存储方式，需要根据实际实现调整
            offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "[");

            TZrSize elementCount = 0;
            TZrSize totalElementCount = 0;
            if (obj->nodeMap.isValid && obj->nodeMap.buckets != ZR_NULL) {
                totalElementCount = obj->nodeMap.elementCount;
                for (TZrSize i = 0; i < obj->nodeMap.capacity && elementCount < maxElementsToShow; i++) {
                    SZrHashKeyValuePair *pair = obj->nodeMap.buckets[i];
                    while (pair != ZR_NULL && elementCount < maxElementsToShow) {
                        if (elementCount > 0) {
                            offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, ", ");
                        }
                        // 获取值的字符串表示
                        SZrString *valueStr = ZrCore_Value_ConvertToString(state, &pair->value);
                        if (valueStr != ZR_NULL) {
                            TZrNativeString valueNative = ZrCore_String_GetNativeString(valueStr);
                            if (valueNative != ZR_NULL) {
                                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "%s", valueNative);
                            }
                        } else {
                            offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "<?>");
                        }
                        elementCount++;
                        pair = pair->next;
                    }
                }
            }

            // 如果有更多元素，添加省略号
            if (totalElementCount > maxElementsToShow) {
                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, ", ...");
            }

            // 只有当总数达到预览上限时才显示 count 信息
            if (totalElementCount >= maxElementsToShow) {
                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, " | count = %lu]",
                                   (unsigned long) totalElementCount);
            } else {
                offset += snprintf(buffer + offset, ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH - offset, "]");
            }
            break;
        }
        default: {
            // 对于其他类型，使用普通的字符串转换
            SZrString *str = ZrCore_Value_ConvertToString(state, value);
            if (str != ZR_NULL) {
                return str;
            }
            return ZrCore_String_CreateFromNative(state, "<unknown>");
        }
    }

    buffer[offset] = '\0';
    return ZrCore_String_CreateFromNative(state, buffer);
}
