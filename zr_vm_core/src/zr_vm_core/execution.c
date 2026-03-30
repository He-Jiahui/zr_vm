//
// Created by HeJiahui on 2025/6/15.
//

#include "zr_vm_core/execution.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_string_conf.h"

static const TZrChar *kNativeEnumValueFieldName = "__zr_enumValue";
static const TZrChar *kNativeEnumNameFieldName = "__zr_enumName";
static const TZrChar *kNativeEnumValueTypeFieldName = "__zr_enumValueTypeName";

typedef enum EZrExecutionNumericFallbackOp {
    ZR_EXEC_NUMERIC_FALLBACK_ADD = 0,
    ZR_EXEC_NUMERIC_FALLBACK_SUB,
    ZR_EXEC_NUMERIC_FALLBACK_MUL,
    ZR_EXEC_NUMERIC_FALLBACK_DIV,
    ZR_EXEC_NUMERIC_FALLBACK_MOD,
    ZR_EXEC_NUMERIC_FALLBACK_POW
} EZrExecutionNumericFallbackOp;

typedef enum EZrExecutionNumericCompareOp {
    ZR_EXEC_NUMERIC_COMPARE_GREATER = 0,
    ZR_EXEC_NUMERIC_COMPARE_LESS,
    ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
    ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL
} EZrExecutionNumericCompareOp;

static TZrBool prototype_type_matches(EZrObjectPrototypeType expectedType, EZrObjectPrototypeType actualType) {
    return expectedType == ZR_OBJECT_PROTOTYPE_TYPE_INVALID || expectedType == actualType;
}

static TZrBool execution_extract_numeric_double(const SZrTypeValue *value, TZrFloat64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = (TZrFloat64) value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = (TZrFloat64) value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        *outValue = value->value.nativeObject.nativeDouble;
        return ZR_TRUE;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        *outValue = value->value.nativeObject.nativeBool ? 1.0 : 0.0;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool execution_eval_binary_numeric_float(EZrExecutionNumericFallbackOp operation,
                                                   TZrFloat64 leftValue,
                                                   TZrFloat64 rightValue,
                                                   TZrFloat64 *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_EXEC_NUMERIC_FALLBACK_ADD:
            *outResult = leftValue + rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_SUB:
            *outResult = leftValue - rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_MUL:
            *outResult = leftValue * rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_DIV:
            *outResult = leftValue / rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_MOD:
            *outResult = fmod(leftValue, rightValue);
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_FALLBACK_POW:
            *outResult = pow(leftValue, rightValue);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_try_binary_numeric_float_fallback(EZrExecutionNumericFallbackOp operation,
                                                           SZrTypeValue *destination,
                                                           const SZrTypeValue *opA,
                                                           const SZrTypeValue *opB);

static TZrBool execution_apply_binary_numeric_float(EZrExecutionNumericFallbackOp operation,
                                                    SZrTypeValue *destination,
                                                    const SZrTypeValue *opA,
                                                    const SZrTypeValue *opB) {
    TZrFloat64 resultValue;

    if (destination == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type) && ZR_VALUE_IS_TYPE_FLOAT(opB->type)) {
        if (!execution_eval_binary_numeric_float(operation,
                                                 opA->value.nativeObject.nativeDouble,
                                                 opB->value.nativeObject.nativeDouble,
                                                 &resultValue)) {
            return ZR_FALSE;
        }

        ZR_VALUE_FAST_SET(destination, nativeDouble, resultValue, ZR_VALUE_TYPE_DOUBLE);
        return ZR_TRUE;
    }

    return execution_try_binary_numeric_float_fallback(operation, destination, opA, opB);
}

static void execution_apply_binary_numeric_float_or_raise(SZrState *state,
                                                          EZrExecutionNumericFallbackOp operation,
                                                          SZrTypeValue *destination,
                                                          const SZrTypeValue *opA,
                                                          const SZrTypeValue *opB,
                                                          const TZrChar *instructionName) {
    if (!execution_apply_binary_numeric_float(operation, destination, opA, opB)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
    }
}

static TZrBool execution_eval_binary_numeric_compare(EZrExecutionNumericCompareOp operation,
                                                     TZrFloat64 leftValue,
                                                     TZrFloat64 rightValue,
                                                     TZrBool *outResult) {
    if (outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (operation) {
        case ZR_EXEC_NUMERIC_COMPARE_GREATER:
            *outResult = leftValue > rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_COMPARE_LESS:
            *outResult = leftValue < rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL:
            *outResult = leftValue >= rightValue;
            return ZR_TRUE;
        case ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL:
            *outResult = leftValue <= rightValue;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool execution_apply_binary_numeric_compare(EZrExecutionNumericCompareOp operation,
                                                      SZrTypeValue *destination,
                                                      const SZrTypeValue *opA,
                                                      const SZrTypeValue *opB) {
    TZrFloat64 leftValue;
    TZrFloat64 rightValue;
    TZrBool resultValue;

    if (destination == ZR_NULL ||
        !execution_extract_numeric_double(opA, &leftValue) ||
        !execution_extract_numeric_double(opB, &rightValue) ||
        !execution_eval_binary_numeric_compare(operation, leftValue, rightValue, &resultValue)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destination, nativeBool, resultValue, ZR_VALUE_TYPE_BOOL);
    return ZR_TRUE;
}

static void execution_apply_binary_numeric_compare_or_raise(SZrState *state,
                                                            EZrExecutionNumericCompareOp operation,
                                                            SZrTypeValue *destination,
                                                            const SZrTypeValue *opA,
                                                            const SZrTypeValue *opB,
                                                            const TZrChar *instructionName) {
    if (!execution_apply_binary_numeric_compare(operation, destination, opA, opB)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
    }
}

static TZrBool execution_try_binary_numeric_float_fallback(EZrExecutionNumericFallbackOp operation,
                                                           SZrTypeValue *destination,
                                                           const SZrTypeValue *opA,
                                                           const SZrTypeValue *opB) {
    TZrFloat64 leftValue;
    TZrFloat64 rightValue;
    TZrFloat64 resultValue;

    if (destination == ZR_NULL ||
        !execution_extract_numeric_double(opA, &leftValue) ||
        !execution_extract_numeric_double(opB, &rightValue)) {
        return ZR_FALSE;
    }

    if (!execution_eval_binary_numeric_float(operation, leftValue, rightValue, &resultValue)) {
        return ZR_FALSE;
    }

    ZR_VALUE_FAST_SET(destination, nativeDouble, resultValue, ZR_VALUE_TYPE_DOUBLE);
    return ZR_TRUE;
}

static void execution_try_binary_numeric_float_fallback_or_raise(SZrState *state,
                                                                 EZrExecutionNumericFallbackOp operation,
                                                                 SZrTypeValue *destination,
                                                                 const SZrTypeValue *opA,
                                                                 const SZrTypeValue *opB,
                                                                 const TZrChar *instructionName) {
    if (!execution_try_binary_numeric_float_fallback(operation, destination, opA, opB)) {
        ZrCore_Debug_RunError(state, "%s requires numeric operands", instructionName);
    }
}

// 辅助函数：从模块中查找类型原型
// 返回找到的原型对象，如果未找到返回 ZR_NULL
static SZrObjectPrototype *find_prototype_in_module(SZrState *state, struct SZrObjectModule *module, 
                                                     SZrString *typeName, EZrObjectPrototypeType expectedType) {
    if (state == ZR_NULL || module == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 从模块的 pub 导出中查找类型
    const SZrTypeValue *typeValue = ZrCore_Module_GetPubExport(state, module, typeName);
    if (typeValue == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 检查值类型是否为对象
    if (typeValue->type != ZR_VALUE_TYPE_OBJECT) {
        return ZR_NULL;
    }
    
    SZrObject *typeObject = ZR_CAST_OBJECT(state, typeValue->value.object);
    if (typeObject == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 检查对象是否为原型对象
    if (typeObject->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }
    
    SZrObjectPrototype *prototype = (SZrObjectPrototype *)typeObject;
    
    // 检查原型类型是否匹配
    if (prototype_type_matches(expectedType, prototype->type)) {
        return prototype;
    }
    
    return ZR_NULL;
}

// 辅助函数：解析类型名称，支持 "module.TypeName" 格式
// 返回解析后的模块名和类型名，如果类型名不包含模块路径，moduleName 返回 ZR_NULL
static void parse_type_name(SZrState *state, SZrString *fullTypeName, SZrString **moduleName, SZrString **typeName) {
    if (state == ZR_NULL || fullTypeName == ZR_NULL) {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = ZR_NULL;
        return;
    }
    
    // 获取类型名称字符串
    TZrNativeString typeNameStr;
    TZrSize nameLen;
    if (fullTypeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        typeNameStr = (TZrNativeString)ZrCore_String_GetNativeStringShort(fullTypeName);
        nameLen = fullTypeName->shortStringLength;
    } else {
        typeNameStr = (TZrNativeString)ZrCore_String_GetNativeString(fullTypeName);
        nameLen = fullTypeName->longStringLength;
    }
    
    if (typeNameStr == ZR_NULL || nameLen == 0) {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
        return;
    }
    
    // 查找 '.' 分隔符
    const TZrChar *dotPos = (const TZrChar *)memchr(typeNameStr, '.', nameLen);
    if (dotPos == ZR_NULL) {
        // 没有模块路径，类型名就是完整名称
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
        return;
    }
    
    // 解析模块名和类型名
    TZrSize moduleNameLen = (TZrSize)(dotPos - typeNameStr);
    TZrSize typeNameLen = nameLen - moduleNameLen - 1;
    const TZrChar *typeNameStart = dotPos + 1;
    
    if (moduleNameLen > 0 && typeNameLen > 0) {
        if (moduleName != ZR_NULL) {
            *moduleName = ZrCore_String_Create(state, typeNameStr, moduleNameLen);
        }
        if (typeName != ZR_NULL) {
            *typeName = ZrCore_String_Create(state, typeNameStart, typeNameLen);
        }
    } else {
        if (moduleName != ZR_NULL) *moduleName = ZR_NULL;
        if (typeName != ZR_NULL) *typeName = fullTypeName;
    }
}

static TZrInt64 value_to_int64(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return 0;
    }

    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? 1 : 0;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeDouble;
    }

    return 0;
}

static TZrUInt64 value_to_uint64(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return 0;
    }

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return (TZrUInt64)value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? 1u : 0u;
    }
    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return (TZrUInt64)value->value.nativeObject.nativeDouble;
    }

    return 0;
}

static TZrDouble value_to_double(const SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return 0.0;
    }

    if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        return value->value.nativeObject.nativeDouble;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return (TZrDouble)value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrDouble)value->value.nativeObject.nativeUInt64;
    }
    if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        return value->value.nativeObject.nativeBool ? 1.0 : 0.0;
    }

    return 0.0;
}

static TZrBool concat_values_to_destination(SZrState *state,
                                          SZrTypeValue *destination,
                                          const SZrTypeValue *opA,
                                          const SZrTypeValue *opB,
                                          TZrBool safeMode) {
    TZrMemoryOffset savedStackTopOffset;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer tempBase;
    SZrCallInfo *currentCallInfo;
    SZrTypeValue *resultValue;

    if (state == ZR_NULL || destination == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    savedStackTop = state->stackTop.valuePointer;
    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, savedStackTop);
    currentCallInfo = state->callInfoList;
    tempBase = currentCallInfo != ZR_NULL ? currentCallInfo->functionTop.valuePointer : savedStackTop;
    tempBase = ZrCore_Function_CheckStackAndGc(state, 2, tempBase);
    if (currentCallInfo != ZR_NULL) {
        tempBase = currentCallInfo->functionTop.valuePointer;
    }

    ZrCore_Stack_CopyValue(state, tempBase, (SZrTypeValue *)opA);
    ZrCore_Stack_CopyValue(state, tempBase + 1, (SZrTypeValue *)opB);
    state->stackTop.valuePointer = tempBase + 2;
    if (currentCallInfo != ZR_NULL && currentCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        currentCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    if (safeMode) {
        ZrCore_String_ConcatSafe(state, 2);
    } else {
        ZrCore_String_Concat(state, 2);
    }

    resultValue = ZrCore_Stack_GetValue(tempBase);
    if (resultValue != ZR_NULL) {
        ZrCore_Value_Copy(state, destination, resultValue);
    } else {
        ZrCore_Value_ResetAsNull(destination);
    }
    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    return ZR_TRUE;
}

static TZrSize close_scope_cleanup_registrations(SZrState *state, TZrSize cleanupCount) {
    TZrSize closedCount = 0;
    TZrMemoryOffset savedStackTopOffset;
    SZrCallInfo *currentCallInfo;

    if (state == ZR_NULL || cleanupCount == 0) {
        return 0;
    }

    savedStackTopOffset = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    currentCallInfo = state->callInfoList;
    if (currentCallInfo != ZR_NULL &&
        state->stackTop.valuePointer < currentCallInfo->functionTop.valuePointer) {
        state->stackTop.valuePointer = currentCallInfo->functionTop.valuePointer;
    }

    while (closedCount < cleanupCount &&
           state->toBeClosedValueList.valuePointer > state->stackBase.valuePointer) {
        TZrStackPointer toBeClosed = state->toBeClosedValueList;
        ZrCore_Closure_CloseStackValue(state, toBeClosed.valuePointer);
        ZrCore_Closure_CloseRegisteredValues(state, 1, ZR_THREAD_STATUS_INVALID, ZR_FALSE);
        closedCount++;
    }

    state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedStackTopOffset);
    return closedCount;
}

static TZrBool try_builtin_add(SZrState *state, SZrTypeValue *destination, const SZrTypeValue *opA, const SZrTypeValue *opB) {
    if (state == ZR_NULL || destination == ZR_NULL || opA == ZR_NULL || opB == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZR_VALUE_IS_TYPE_STRING(opA->type) || ZR_VALUE_IS_TYPE_STRING(opB->type)) {
        return concat_values_to_destination(state, destination, opA, opB, ZR_TRUE);
    }

    if ((ZR_VALUE_IS_TYPE_NUMBER(opA->type) || ZR_VALUE_IS_TYPE_BOOL(opA->type)) &&
        (ZR_VALUE_IS_TYPE_NUMBER(opB->type) || ZR_VALUE_IS_TYPE_BOOL(opB->type))) {
        if (ZR_VALUE_IS_TYPE_FLOAT(opA->type) || ZR_VALUE_IS_TYPE_FLOAT(opB->type)) {
            ZrCore_Value_InitAsFloat(state, destination, value_to_double(opA) + value_to_double(opB));
        } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type) || ZR_VALUE_IS_TYPE_SIGNED_INT(opB->type) ||
                   ZR_VALUE_IS_TYPE_BOOL(opA->type) || ZR_VALUE_IS_TYPE_BOOL(opB->type)) {
            ZrCore_Value_InitAsInt(state, destination, value_to_int64(opA) + value_to_int64(opB));
        } else {
            ZrCore_Value_InitAsUInt(state, destination, value_to_uint64(opA) + value_to_uint64(opB));
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

// 辅助函数：查找类型原型（从当前模块或全局模块注册表）
// 返回找到的原型对象，如果未找到返回 ZR_NULL
static SZrObjectPrototype *find_type_prototype(SZrState *state, SZrString *typeName, EZrObjectPrototypeType expectedType) {
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrGlobalState *global = state->global;
    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 解析类型名称，支持 "module.TypeName" 格式
    SZrString *moduleName = ZR_NULL;
    SZrString *actualTypeName = ZR_NULL;
    parse_type_name(state, typeName, &moduleName, &actualTypeName);
    
    if (actualTypeName == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果指定了模块名，从该模块中查找
    if (moduleName != ZR_NULL) {
        // 从模块注册表中获取模块
        struct SZrObjectModule *module = ZrCore_Module_GetFromCache(state, moduleName);
        if (module != ZR_NULL) {
            SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
            return prototype;
        }
    } else {
        // 没有指定模块名，尝试从当前调用栈的闭包中查找模块
        // 如果函数有模块信息，从模块中查找类型
        // 通过查找调用栈中的entry function，然后查找对应的模块
        if (state->callInfoList != ZR_NULL) {
            // 查找当前调用栈中的entry function（包含prototypeData的函数）
            SZrCallInfo *callInfo = state->callInfoList;
            while (callInfo != ZR_NULL) {
                if (callInfo->functionBase.valuePointer >= state->stackBase.valuePointer &&
                    callInfo->functionBase.valuePointer < state->stackTop.valuePointer) {
                    SZrTypeValue *closureValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
                    if (closureValue != ZR_NULL && closureValue->type == ZR_VALUE_TYPE_CLOSURE) {
                        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, closureValue->value.object);
                        if (closure != ZR_NULL && closure->function != ZR_NULL) {
                            struct SZrFunction *func = closure->function;
                            // 检查是否是entry function（有prototypeData）
                            if (func->prototypeData != ZR_NULL && func->prototypeCount > 0) {
                                // 查找对应的模块
                                // TODO: 注意：这里需要遍历模块注册表查找，简化实现：遍历所有模块
                                if (state->global != ZR_NULL) {
                                    SZrGlobalState *global = state->global;
                                    if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) &&
                                        global->loadedModulesRegistry.type == ZR_VALUE_TYPE_OBJECT) {
                                        SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
                                        if (registry != ZR_NULL && registry->nodeMap.isValid && 
                                            registry->nodeMap.buckets != ZR_NULL) {
                                            // 遍历模块注册表，查找包含该entry function的模块
                                            for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
                                                SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
                                                while (pair != ZR_NULL) {
                                                    if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                                                        SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                                                        if (cachedObject != ZR_NULL && 
                                                            cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                                                            struct SZrObjectModule *module = (struct SZrObjectModule *)cachedObject;
                                                            // 检查模块的导出中是否有该类型
                                                            SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
                                                            if (prototype != ZR_NULL) {
                                                                return prototype;
                                                            }
                                                        }
                                                    }
                                                    pair = pair->next;
                                                }
                                            }
                                        }
                                    }
                                }
                                break;  // 找到entry function后，不再继续查找
                            }
                        }
                    }
                }
                callInfo = callInfo->previous;
            }
        }
        
        // 从全局模块注册表中查找（遍历所有已加载的模块）
        // 如果上面的查找失败，遍历所有模块查找类型
        if (state->global != ZR_NULL) {
            SZrGlobalState *global = state->global;
            if (ZrCore_Value_IsGarbageCollectable(&global->loadedModulesRegistry) &&
                global->loadedModulesRegistry.type == ZR_VALUE_TYPE_OBJECT) {
                SZrObject *registry = ZR_CAST_OBJECT(state, global->loadedModulesRegistry.value.object);
                if (registry != ZR_NULL && registry->nodeMap.isValid && 
                    registry->nodeMap.buckets != ZR_NULL) {
                    for (TZrSize i = 0; i < registry->nodeMap.capacity; i++) {
                        SZrHashKeyValuePair *pair = registry->nodeMap.buckets[i];
                        while (pair != ZR_NULL) {
                            if (pair->value.type == ZR_VALUE_TYPE_OBJECT) {
                                SZrObject *cachedObject = ZR_CAST_OBJECT(state, pair->value.value.object);
                                if (cachedObject != ZR_NULL && 
                                    cachedObject->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE) {
                                    struct SZrObjectModule *module = (struct SZrObjectModule *)cachedObject;
                                    SZrObjectPrototype *prototype = find_prototype_in_module(state, module, actualTypeName, expectedType);
                                    if (prototype != ZR_NULL) {
                                        return prototype;
                                    }
                                }
                            }
                            pair = pair->next;
                        }
                    }
                }
            }
        }
    }
    
    if (state->global != ZR_NULL && state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
        SZrObject *zrObject = ZR_CAST_OBJECT(state, state->global->zrObject.value.object);
        if (zrObject != ZR_NULL) {
            SZrTypeValue key;
            ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(actualTypeName));
            key.type = ZR_VALUE_TYPE_STRING;
            const SZrTypeValue *prototypeValue = ZrCore_Object_GetValue(state, zrObject, &key);
            if (prototypeValue != ZR_NULL && prototypeValue->type == ZR_VALUE_TYPE_OBJECT) {
                SZrObject *candidate = ZR_CAST_OBJECT(state, prototypeValue->value.object);
                    if (candidate != ZR_NULL &&
                    candidate->internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
                    SZrObjectPrototype *prototype = (SZrObjectPrototype *)candidate;
                    if (prototype_type_matches(expectedType, prototype->type)) {
                        return prototype;
                    }
                }
            }
        }
    }

    // 如果找不到，返回 ZR_NULL（后续可以通过元方法或创建新原型）
    // 注意：完整的实现需要：
    // - 模块加载时将类型原型注册到全局类型表
    // - 或者通过类型名称的模块路径（如 "module.TypeName"）来查找
    return ZR_NULL;
}

// 辅助函数：执行 struct 类型转换
// 将源对象转换为目标 struct 类型
static TZrBool convert_to_struct(SZrState *state, SZrTypeValue *source, SZrObjectPrototype *targetPrototype, 
                                SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查目标原型类型
    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        return ZR_FALSE;
    }
    
    // 如果源值是对象，尝试转换
    if (ZR_VALUE_IS_TYPE_OBJECT(source->type)) {
        SZrObject *sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 创建新的 struct 对象（值类型）
        SZrObject *structObject = ZrCore_Object_New(state, targetPrototype);
        if (structObject == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Object_Init(state, structObject);
        
        // 设置内部类型为 STRUCT
        structObject->internalType = ZR_OBJECT_INTERNAL_TYPE_STRUCT;
        
        // 复制源对象的字段到新对象
        // 对于 struct，字段存储在 nodeMap 中（与普通对象相同）
        // 遍历源对象的 nodeMap，复制匹配的字段到新对象
        if (sourceObject->nodeMap.isValid && sourceObject->nodeMap.buckets != ZR_NULL && sourceObject->nodeMap.elementCount > 0) {
            // 注意：ZrHashSet 没有迭代接口，我们需要通过其他方式复制字段
            // 一个方案是：通过元方法 @to_struct 来处理字段复制
            // 或者：如果源对象已经是 struct 类型，直接复制其 nodeMap
            
            // TODO: 暂时先复制所有字段（后续需要根据 struct 定义进行字段验证和类型转换）
            // 由于无法直接迭代 nodeMap，我们依赖元方法或构造函数来处理字段复制
            // 如果源对象有 @to_struct 元方法，应该已经在上层调用了
            // 这里只是创建了新的 struct 对象，字段复制由元方法或构造函数完成
        }
        
        ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(structObject));
        destination->type = ZR_VALUE_TYPE_OBJECT;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

// 辅助函数：执行 class 类型转换
// 将源对象转换为目标 class 类型
static TZrBool convert_to_class(SZrState *state, SZrTypeValue *source, SZrObjectPrototype *targetPrototype, 
                                SZrTypeValue *destination) {
    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查目标原型类型
    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        return ZR_FALSE;
    }
    
    // 如果源值是对象，尝试转换
    if (ZR_VALUE_IS_TYPE_OBJECT(source->type)) {
        SZrObject *sourceObject = ZR_CAST_OBJECT(state, source->value.object);
        if (sourceObject == ZR_NULL) {
            return ZR_FALSE;
        }
        
        // 创建新的 class 对象（引用类型）
        SZrObject *classObject = ZrCore_Object_New(state, targetPrototype);
        if (classObject == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Object_Init(state, classObject);
        
        // 设置内部类型为 OBJECT（class 是引用类型）
        classObject->internalType = ZR_OBJECT_INTERNAL_TYPE_OBJECT;
        
        // 复制源对象的字段到新对象
        // 对于 class，字段存储在 nodeMap 中
        // 遍历源对象的 nodeMap，复制匹配的字段到新对象
        if (sourceObject->nodeMap.isValid && sourceObject->nodeMap.buckets != ZR_NULL && sourceObject->nodeMap.elementCount > 0) {
            // 注意：ZrHashSet 没有迭代接口，我们需要通过其他方式复制字段
            // 一个方案是：通过元方法 @to_object 来处理字段复制
            // 或者：如果源对象已经是 class 类型，直接复制其 nodeMap
            
            // TODO: 暂时先复制所有字段（后续需要根据 class 定义进行字段验证和类型转换）
            // 由于无法直接迭代 nodeMap，我们依赖元方法或构造函数来处理字段复制
            // 如果源对象有 @to_object 元方法，应该已经在上层调用了
            // 这里只是创建了新的 class 对象，字段复制由元方法或构造函数完成
        }
        
        ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(classObject));
        destination->type = ZR_VALUE_TYPE_OBJECT;
        return ZR_TRUE;
    }
    
    return ZR_FALSE;
}

static const SZrTypeValue *execution_get_object_field_cstring(SZrState *state,
                                                              SZrObject *object,
                                                              const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_Create(state, fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static void execution_set_object_field_cstring(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               const SZrTypeValue *value) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    fieldNameString = ZrCore_String_Create(state, fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(state, object, &key, value);
}

static const TZrChar *execution_get_enum_value_type_name(SZrState *state, SZrObjectPrototype *prototype) {
    const SZrTypeValue *typeValue;

    if (state == ZR_NULL || prototype == ZR_NULL) {
        return ZR_NULL;
    }

    typeValue = execution_get_object_field_cstring(state, &prototype->super, kNativeEnumValueTypeFieldName);
    if (typeValue == ZR_NULL || typeValue->type != ZR_VALUE_TYPE_STRING || typeValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, typeValue->value.object));
}

static TZrBool execution_extract_enum_underlying_value(SZrState *state,
                                                       const SZrTypeValue *source,
                                                       SZrObjectPrototype *targetPrototype,
                                                       SZrTypeValue *underlyingValue) {
    const SZrTypeValue *fieldValue;
    SZrObject *object;

    if (state == ZR_NULL || source == ZR_NULL || underlyingValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (source->type != ZR_VALUE_TYPE_OBJECT || source->value.object == ZR_NULL) {
        ZrCore_Value_Copy(state, underlyingValue, (SZrTypeValue *)source);
        return ZR_TRUE;
    }

    object = ZR_CAST_OBJECT(state, source->value.object);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetPrototype != ZR_NULL && object->prototype == targetPrototype) {
        ZrCore_Value_Copy(state, underlyingValue, (SZrTypeValue *)source);
        return ZR_TRUE;
    }

    fieldValue = execution_get_object_field_cstring(state, object, kNativeEnumValueFieldName);
    if (fieldValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, underlyingValue, (SZrTypeValue *)fieldValue);
    return ZR_TRUE;
}

static TZrBool execution_normalize_enum_underlying_value(SZrState *state,
                                                         const SZrTypeValue *source,
                                                         const TZrChar *expectedTypeName,
                                                         SZrTypeValue *normalizedValue) {
    if (state == ZR_NULL || source == ZR_NULL || normalizedValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (expectedTypeName == ZR_NULL || strcmp(expectedTypeName, "int") == 0) {
        if (ZR_VALUE_IS_TYPE_SIGNED_INT(source->type)) {
            ZrCore_Value_InitAsInt(state, normalizedValue, source->value.nativeObject.nativeInt64);
            return ZR_TRUE;
        }
        if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(source->type)) {
            ZrCore_Value_InitAsInt(state, normalizedValue, (TZrInt64)source->value.nativeObject.nativeUInt64);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "float") == 0) {
        if (ZR_VALUE_IS_TYPE_NUMBER(source->type) || ZR_VALUE_IS_TYPE_BOOL(source->type)) {
            ZrCore_Value_InitAsFloat(state, normalizedValue, value_to_double(source));
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "bool") == 0) {
        if (ZR_VALUE_IS_TYPE_BOOL(source->type)) {
            normalizedValue->type = ZR_VALUE_TYPE_BOOL;
            normalizedValue->value.nativeObject.nativeBool = source->value.nativeObject.nativeBool;
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "string") == 0) {
        if (ZR_VALUE_IS_TYPE_STRING(source->type)) {
            ZrCore_Value_Copy(state, normalizedValue, (SZrTypeValue *)source);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    if (strcmp(expectedTypeName, "null") == 0) {
        if (source->type == ZR_VALUE_TYPE_NULL) {
            ZrCore_Value_ResetAsNull(normalizedValue);
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    ZrCore_Value_Copy(state, normalizedValue, (SZrTypeValue *)source);
    return ZR_TRUE;
}

static TZrBool execution_enum_values_equal(SZrState *state,
                                           const SZrTypeValue *left,
                                           const SZrTypeValue *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    if (left->type == ZR_VALUE_TYPE_NULL || right->type == ZR_VALUE_TYPE_NULL) {
        return left->type == right->type;
    }

    if (ZR_VALUE_IS_TYPE_STRING(left->type) && ZR_VALUE_IS_TYPE_STRING(right->type)) {
        return ZrCore_String_Equal(ZR_CAST_STRING(state, left->value.object), ZR_CAST_STRING(state, right->value.object));
    }

    if (ZR_VALUE_IS_TYPE_BOOL(left->type) && ZR_VALUE_IS_TYPE_BOOL(right->type)) {
        return left->value.nativeObject.nativeBool == right->value.nativeObject.nativeBool;
    }

    if ((ZR_VALUE_IS_TYPE_NUMBER(left->type) || ZR_VALUE_IS_TYPE_BOOL(left->type)) &&
        (ZR_VALUE_IS_TYPE_NUMBER(right->type) || ZR_VALUE_IS_TYPE_BOOL(right->type))) {
        if (ZR_VALUE_IS_TYPE_FLOAT(left->type) || ZR_VALUE_IS_TYPE_FLOAT(right->type)) {
            return value_to_double(left) == value_to_double(right);
        }
        return value_to_int64(left) == value_to_int64(right);
    }

    return ZR_FALSE;
}

static SZrString *execution_find_enum_member_name(SZrState *state,
                                                  SZrObjectPrototype *prototype,
                                                  const SZrTypeValue *underlyingValue) {
    TZrSize bucketIndex;

    if (state == ZR_NULL || prototype == ZR_NULL || underlyingValue == ZR_NULL ||
        !prototype->super.nodeMap.isValid || prototype->super.nodeMap.buckets == ZR_NULL) {
        return ZR_NULL;
    }

    for (bucketIndex = 0; bucketIndex < prototype->super.nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = prototype->super.nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pair->key.type == ZR_VALUE_TYPE_STRING &&
                pair->value.type == ZR_VALUE_TYPE_OBJECT &&
                pair->value.value.object != ZR_NULL) {
                SZrObject *candidate = ZR_CAST_OBJECT(state, pair->value.value.object);
                const SZrTypeValue *candidateValue;

                if (candidate != ZR_NULL && candidate->prototype == prototype) {
                    candidateValue = execution_get_object_field_cstring(state, candidate, kNativeEnumValueFieldName);
                    if (candidateValue != ZR_NULL && execution_enum_values_equal(state, candidateValue, underlyingValue)) {
                        return ZR_CAST_STRING(state, pair->key.value.object);
                    }
                }
            }

            pair = pair->next;
        }
    }

    return ZR_NULL;
}

static TZrBool convert_to_enum(SZrState *state,
                               SZrTypeValue *source,
                               SZrObjectPrototype *targetPrototype,
                               SZrTypeValue *destination) {
    SZrObject *enumObject;
    SZrString *memberName;
    SZrTypeValue extractedValue;
    SZrTypeValue normalizedValue;
    const TZrChar *expectedTypeName;

    if (state == ZR_NULL || source == ZR_NULL || targetPrototype == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    if (targetPrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
        return ZR_FALSE;
    }

    if (source->type == ZR_VALUE_TYPE_OBJECT && source->value.object != ZR_NULL) {
        SZrObject *object = ZR_CAST_OBJECT(state, source->value.object);
        if (object != ZR_NULL && object->prototype == targetPrototype) {
            ZrCore_Value_Copy(state, destination, source);
            return ZR_TRUE;
        }
    }

    if (!execution_extract_enum_underlying_value(state, source, targetPrototype, &extractedValue)) {
        return ZR_FALSE;
    }

    expectedTypeName = execution_get_enum_value_type_name(state, targetPrototype);
    if (!execution_normalize_enum_underlying_value(state, &extractedValue, expectedTypeName, &normalizedValue)) {
        return ZR_FALSE;
    }

    enumObject = ZrCore_Object_New(state, targetPrototype);
    if (enumObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Object_Init(state, enumObject);
    execution_set_object_field_cstring(state, enumObject, kNativeEnumValueFieldName, &normalizedValue);
    memberName = execution_find_enum_member_name(state, targetPrototype, &normalizedValue);
    if (memberName != ZR_NULL) {
        SZrTypeValue nameValue;
        ZrCore_Value_InitAsRawObject(state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        nameValue.type = ZR_VALUE_TYPE_STRING;
        execution_set_object_field_cstring(state, enumObject, kNativeEnumNameFieldName, &nameValue);
    }

    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(enumObject));
    destination->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool execution_invoke_meta_call(SZrState *state,
                                          SZrCallInfo *savedCallInfo,
                                          TZrStackValuePointer savedStackTop,
                                          TZrStackValuePointer scratchBase,
                                          TZrBool reloadScratchFromFunctionTop,
                                          SZrMeta *meta,
                                          const SZrTypeValue *arg0,
                                          const SZrTypeValue *arg1,
                                          TZrSize argumentCount,
                                          TZrStackValuePointer *outMetaBase,
                                          TZrStackValuePointer *outSavedStackTop) {
    SZrTypeValue stableArguments[2];
    SZrFunctionStackAnchor savedStackTopAnchor;
    TZrStackValuePointer metaBase;

    if (outMetaBase != ZR_NULL) {
        *outMetaBase = scratchBase;
    }
    if (outSavedStackTop != ZR_NULL) {
        *outSavedStackTop = savedStackTop;
    }

    if (state == ZR_NULL || meta == ZR_NULL || meta->function == ZR_NULL || arg0 == ZR_NULL || argumentCount == 0 ||
        argumentCount > 2) {
        return ZR_FALSE;
    }

    stableArguments[0] = *arg0;
    if (argumentCount > 1) {
        if (arg1 == ZR_NULL) {
            return ZR_FALSE;
        }
        stableArguments[1] = *arg1;
    }

    ZrCore_Function_StackAnchorInit(state, savedStackTop, &savedStackTopAnchor);
    metaBase = ZrCore_Function_CheckStackAndGc(state, 1 + argumentCount, scratchBase);
    if (reloadScratchFromFunctionTop && savedCallInfo != ZR_NULL) {
        metaBase = savedCallInfo->functionTop.valuePointer;
    }

    ZrCore_Stack_SetRawObjectValue(state, metaBase, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
    ZrCore_Stack_CopyValue(state, metaBase + 1, &stableArguments[0]);
    if (argumentCount > 1) {
        ZrCore_Stack_CopyValue(state, metaBase + 2, &stableArguments[1]);
    }

    state->stackTop.valuePointer = metaBase + 1 + argumentCount;
    if (savedCallInfo != ZR_NULL && savedCallInfo->functionTop.valuePointer < state->stackTop.valuePointer) {
        savedCallInfo->functionTop.valuePointer = state->stackTop.valuePointer;
    }

    metaBase = ZrCore_Function_CallWithoutYieldAndRestore(state, metaBase, 1);
    savedStackTop = ZrCore_Function_StackAnchorRestore(state, &savedStackTopAnchor);
    if (outMetaBase != ZR_NULL) {
        *outMetaBase = metaBase;
    }
    if (outSavedStackTop != ZR_NULL) {
        *outSavedStackTop = savedStackTop;
    }

    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

static SZrFunction *execution_call_info_function(SZrState *state, SZrCallInfo *callInfo) {
    SZrTypeValue *baseValue;

    if (state == ZR_NULL || callInfo == ZR_NULL || callInfo->functionBase.valuePointer == ZR_NULL) {
        return ZR_NULL;
    }

    baseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    if (baseValue == ZR_NULL || baseValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    if (baseValue->type == ZR_VALUE_TYPE_CLOSURE) {
        SZrClosure *frameClosure = ZR_CAST_VM_CLOSURE(state, baseValue->value.object);
        return frameClosure != ZR_NULL ? frameClosure->function : ZR_NULL;
    }
    if (baseValue->type == ZR_VALUE_TYPE_FUNCTION) {
        return ZR_CAST_FUNCTION(state, baseValue->value.object);
    }

    return ZR_NULL;
}

static TZrBool execution_exception_handler_stack_ensure_capacity(SZrState *state, TZrUInt32 minCapacity) {
    SZrVmExceptionHandlerState *newHandlers;
    TZrUInt32 newCapacity;
    TZrSize bytes;

    if (state == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (state->exceptionHandlerStackCapacity >= minCapacity) {
        return ZR_TRUE;
    }

    newCapacity = state->exceptionHandlerStackCapacity > 0 ? state->exceptionHandlerStackCapacity : 8;
    while (newCapacity < minCapacity) {
        newCapacity *= 2;
    }

    bytes = newCapacity * sizeof(SZrVmExceptionHandlerState);
    newHandlers = (SZrVmExceptionHandlerState *)ZrCore_Memory_RawMallocWithType(state->global,
                                                                                bytes,
                                                                                ZR_MEMORY_NATIVE_TYPE_STATE);
    if (newHandlers == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(newHandlers, 0, bytes);
    if (state->exceptionHandlerStack != ZR_NULL && state->exceptionHandlerStackLength > 0) {
        memcpy(newHandlers,
               state->exceptionHandlerStack,
               state->exceptionHandlerStackLength * sizeof(SZrVmExceptionHandlerState));
        ZrCore_Memory_RawFreeWithType(state->global,
                                      state->exceptionHandlerStack,
                                      state->exceptionHandlerStackCapacity * sizeof(SZrVmExceptionHandlerState),
                                      ZR_MEMORY_NATIVE_TYPE_STATE);
    }

    state->exceptionHandlerStack = newHandlers;
    state->exceptionHandlerStackCapacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool execution_push_exception_handler(SZrState *state, SZrCallInfo *callInfo, TZrUInt32 handlerIndex) {
    SZrVmExceptionHandlerState *handlerState;

    if (state == ZR_NULL || callInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!execution_exception_handler_stack_ensure_capacity(state, state->exceptionHandlerStackLength + 1)) {
        return ZR_FALSE;
    }

    handlerState = &state->exceptionHandlerStack[state->exceptionHandlerStackLength++];
    handlerState->callInfo = callInfo;
    handlerState->handlerIndex = handlerIndex;
    handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_TRY;
    return ZR_TRUE;
}

static SZrVmExceptionHandlerState *execution_find_handler_state(SZrState *state,
                                                                SZrCallInfo *callInfo,
                                                                TZrUInt32 handlerIndex) {
    if (state == ZR_NULL || callInfo == ZR_NULL || state->exceptionHandlerStackLength == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = state->exceptionHandlerStackLength; index > 0; index--) {
        SZrVmExceptionHandlerState *handlerState = &state->exceptionHandlerStack[index - 1];
        if (handlerState->callInfo == callInfo && handlerState->handlerIndex == handlerIndex) {
            return handlerState;
        }
    }

    return ZR_NULL;
}

static SZrVmExceptionHandlerState *execution_find_top_handler_for_callinfo(SZrState *state, SZrCallInfo *callInfo) {
    if (state == ZR_NULL || callInfo == ZR_NULL || state->exceptionHandlerStackLength == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = state->exceptionHandlerStackLength; index > 0; index--) {
        SZrVmExceptionHandlerState *handlerState = &state->exceptionHandlerStack[index - 1];
        if (handlerState->callInfo == callInfo) {
            return handlerState;
        }
    }

    return ZR_NULL;
}

static void execution_pop_exception_handler(SZrState *state, SZrVmExceptionHandlerState *handlerState) {
    TZrUInt32 index;

    if (state == ZR_NULL || handlerState == ZR_NULL || state->exceptionHandlerStackLength == 0) {
        return;
    }

    index = (TZrUInt32)(handlerState - state->exceptionHandlerStack);
    if (index >= state->exceptionHandlerStackLength) {
        return;
    }

    memmove(&state->exceptionHandlerStack[index],
            &state->exceptionHandlerStack[index + 1],
            (state->exceptionHandlerStackLength - index - 1) * sizeof(SZrVmExceptionHandlerState));
    state->exceptionHandlerStackLength--;
}

static void execution_pop_handlers_for_callinfo(SZrState *state, SZrCallInfo *callInfo) {
    while (state != ZR_NULL && state->exceptionHandlerStackLength > 0 &&
           state->exceptionHandlerStack[state->exceptionHandlerStackLength - 1].callInfo == callInfo) {
        state->exceptionHandlerStackLength--;
    }
}

static const SZrFunctionExceptionHandlerInfo *execution_lookup_exception_handler_info(SZrState *state,
                                                                                      const SZrVmExceptionHandlerState *handlerState,
                                                                                      SZrFunction **outFunction) {
    SZrFunction *function;

    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }

    if (state == ZR_NULL || handlerState == ZR_NULL) {
        return ZR_NULL;
    }

    function = execution_call_info_function(state, handlerState->callInfo);
    if (outFunction != ZR_NULL) {
        *outFunction = function;
    }
    if (function == ZR_NULL || function->exceptionHandlerList == ZR_NULL ||
        handlerState->handlerIndex >= function->exceptionHandlerCount) {
        return ZR_NULL;
    }

    return &function->exceptionHandlerList[handlerState->handlerIndex];
}

static TZrBool execution_jump_to_instruction_offset(SZrState *state,
                                                    SZrCallInfo **ioCallInfo,
                                                    SZrCallInfo *targetCallInfo,
                                                    TZrMemoryOffset instructionOffset) {
    SZrFunction *function;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || targetCallInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    function = execution_call_info_function(state, targetCallInfo);
    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionOffset > function->instructionsLength) {
        return ZR_FALSE;
    }

    targetCallInfo->context.context.programCounter = function->instructionsList + instructionOffset;
    state->callInfoList = targetCallInfo;
    state->stackTop.valuePointer = targetCallInfo->functionTop.valuePointer;
    *ioCallInfo = targetCallInfo;
    return ZR_TRUE;
}

static void execution_set_pending_exception(SZrState *state, SZrCallInfo *callInfo) {
    if (state == ZR_NULL) {
        return;
    }

    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_EXCEPTION;
    state->pendingControl.callInfo = callInfo;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

static void execution_clear_pending_control(SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    state->pendingControl.kind = ZR_VM_PENDING_CONTROL_NONE;
    state->pendingControl.callInfo = ZR_NULL;
    state->pendingControl.targetInstructionOffset = 0;
    state->pendingControl.valueSlot = 0;
    ZrCore_Value_ResetAsNull(&state->pendingControl.value);
    state->pendingControl.hasValue = ZR_FALSE;
}

static void execution_set_pending_control(SZrState *state,
                                          EZrVmPendingControlKind kind,
                                          SZrCallInfo *callInfo,
                                          TZrMemoryOffset targetInstructionOffset,
                                          TZrUInt32 valueSlot,
                                          const SZrTypeValue *value) {
    if (state == ZR_NULL) {
        return;
    }

    state->pendingControl.kind = kind;
    state->pendingControl.callInfo = callInfo;
    state->pendingControl.targetInstructionOffset = targetInstructionOffset;
    state->pendingControl.valueSlot = valueSlot;
    if (value != ZR_NULL) {
        ZrCore_Value_Copy(state, &state->pendingControl.value, (SZrTypeValue *)value);
        state->pendingControl.hasValue = ZR_TRUE;
    } else {
        ZrCore_Value_ResetAsNull(&state->pendingControl.value);
        state->pendingControl.hasValue = ZR_FALSE;
    }
}

static TZrBool execution_resume_pending_via_outer_finally(SZrState *state, SZrCallInfo **ioCallInfo) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || *ioCallInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    callInfo = *ioCallInfo;
    for (TZrUInt32 index = state->exceptionHandlerStackLength; index > 0; index--) {
        SZrVmExceptionHandlerState *handlerState = &state->exceptionHandlerStack[index - 1];
        SZrFunction *function = ZR_NULL;
        const SZrFunctionExceptionHandlerInfo *handlerInfo;

        if (handlerState->callInfo != callInfo) {
            break;
        }

        handlerInfo = execution_lookup_exception_handler_info(state, handlerState, &function);
        if (handlerInfo == ZR_NULL || !handlerInfo->hasFinally ||
            handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY) {
            continue;
        }

        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
        return execution_jump_to_instruction_offset(state,
                                                    ioCallInfo,
                                                    callInfo,
                                                    handlerInfo->finallyTargetInstructionOffset);
    }

    return ZR_FALSE;
}

static TZrBool execution_unwind_exception_to_handler(SZrState *state, SZrCallInfo **ioCallInfo) {
    SZrCallInfo *callInfo;

    if (state == ZR_NULL || ioCallInfo == ZR_NULL || *ioCallInfo == ZR_NULL || !state->hasCurrentException) {
        return ZR_FALSE;
    }

    callInfo = *ioCallInfo;
    while (callInfo != ZR_NULL) {
        if (!ZR_CALL_INFO_IS_VM(callInfo)) {
            state->callInfoList = callInfo;
            if (callInfo->functionTop.valuePointer != ZR_NULL) {
                state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
            }
            break;
        }

        for (;;) {
            SZrVmExceptionHandlerState *handlerState = execution_find_top_handler_for_callinfo(state, callInfo);
            SZrFunction *function = ZR_NULL;
            const SZrFunctionExceptionHandlerInfo *handlerInfo;

            if (handlerState == ZR_NULL) {
                break;
            }

            handlerInfo = execution_lookup_exception_handler_info(state, handlerState, &function);
            if (handlerInfo == ZR_NULL) {
                execution_pop_exception_handler(state, handlerState);
                continue;
            }

            if (handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY) {
                execution_pop_exception_handler(state, handlerState);
                continue;
            }

            if (handlerState->phase == ZR_VM_EXCEPTION_HANDLER_PHASE_TRY) {
                for (TZrUInt32 catchIndex = 0; catchIndex < handlerInfo->catchClauseCount; catchIndex++) {
                    SZrFunctionCatchClauseInfo *catchInfo =
                            &function->catchClauseList[handlerInfo->catchClauseStartIndex + catchIndex];
                    if (ZrCore_Exception_CatchMatchesTypeName(state, &state->currentException, catchInfo->typeName)) {
                        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_CATCH;
                        return execution_jump_to_instruction_offset(state,
                                                                    ioCallInfo,
                                                                    callInfo,
                                                                    catchInfo->targetInstructionOffset);
                    }
                }
            }

            if (handlerInfo->hasFinally) {
                handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
                execution_set_pending_exception(state, callInfo);
                return execution_jump_to_instruction_offset(state,
                                                            ioCallInfo,
                                                            callInfo,
                                                            handlerInfo->finallyTargetInstructionOffset);
            }

            execution_pop_exception_handler(state, handlerState);
        }

        execution_pop_handlers_for_callinfo(state, callInfo);
        state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        ZrCore_Closure_CloseClosure(state,
                                    callInfo->functionBase.valuePointer + 1,
                                    state->currentExceptionStatus,
                                    ZR_FALSE);
        state->callInfoList = callInfo->previous;
        callInfo = callInfo->previous;
        if (callInfo != ZR_NULL) {
            state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
        }
    }

    return ZR_FALSE;
}

void ZrCore_Execute(SZrState *state, SZrCallInfo *callInfo) {
    SZrClosure *closure;
    SZrTypeValue *constants;
    TZrStackValuePointer base;
    SZrTypeValue ret;
    ZrCore_Value_ResetAsNull(&ret);
    const TZrInstruction *programCounter;
    TZrDebugSignal trap;
    SZrTypeValue *opA;
    SZrTypeValue *opB;
    /*
     * registers macros
     */

    /*
     *
     */
    ZR_INSTRUCTION_DISPATCH_TABLE
#define DONE(N) ZR_INSTRUCTION_DONE(instruction, programCounter, N)
// extra operand
#define E(INSTRUCTION) INSTRUCTION.instruction.operandExtra
// 4 OPERANDS
#define A0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[0]
#define B0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[1]
#define C0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[2]
#define D0(INSTRUCTION) INSTRUCTION.instruction.operand.operand0[3]
// 2 OPERANDS
#define A1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[0]
#define B1(INSTRUCTION) INSTRUCTION.instruction.operand.operand1[1]
// 1 OPERAND
#define A2(INSTRUCTION) INSTRUCTION.instruction.operand.operand2[0]

#define BASE(OFFSET) (base + (OFFSET))
#define CONST(OFFSET) (constants + (OFFSET))
#define CLOSURE(OFFSET) (closure->closureValuesExtend[OFFSET])

#define ALGORITHM_1(REGION, OP, TYPE) ZR_VALUE_FAST_SET(destination, REGION, OP(opA->value.nativeObject.REGION), TYPE);
#define ALGORITHM_2(REGION, OP, TYPE)                                                                                  \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CVT_2(CVT, REGION, OP, TYPE)                                                                         \
    ZR_VALUE_FAST_SET(destination, CVT, (opA->value.nativeObject.REGION) OP(opB->value.nativeObject.REGION), TYPE);
#define ALGORITHM_CONST_2(REGION, OP, TYPE, RIGHT)                                                                     \
    ZR_VALUE_FAST_SET(destination, REGION, (opA->value.nativeObject.REGION) OP(RIGHT), TYPE);
#define ALGORITHM_FUNC_2(REGION, OP_FUNC, TYPE)                                                                        \
    ZR_VALUE_FAST_SET(destination, REGION, OP_FUNC(opA->value.nativeObject.REGION, opB->value.nativeObject.REGION),    \
                      TYPE);

#define UPDATE_TRAP(CALL_INFO) (trap = (CALL_INFO)->context.context.trap)
#define UPDATE_BASE(CALL_INFO) (base = (CALL_INFO)->functionBase.valuePointer + 1)
#define UPDATE_STACK(CALL_INFO)                                                                                        \
    {                                                                                                                  \
        if (ZR_UNLIKELY(trap)) {                                                                                       \
            UPDATE_BASE(CALL_INFO);                                                                                    \
        }                                                                                                              \
    }
#define SAVE_PC(STATE, CALL_INFO) ((CALL_INFO)->context.context.programCounter = programCounter)
#define SAVE_STATE(STATE, CALL_INFO)                                                                                   \
    (SAVE_PC(STATE, CALL_INFO), ((STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer))
    // MODIFIABLE: ERROR & STACK & HOOK
#if defined(_MSC_VER)
    // MSVC 不支持语句表达式，使用 do-while 循环
    #define PROTECT_ESH(STATE, CALL_INFO, EXP) \
        do { \
            SAVE_PC(STATE, CALL_INFO); \
            (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer; \
            EXP; \
            UPDATE_TRAP(CALL_INFO); \
        } while(0)
    #define PROTECT_EH(STATE, CALL_INFO, EXP) \
        do { \
            SAVE_PC(STATE, CALL_INFO); \
            EXP; \
            UPDATE_TRAP(CALL_INFO); \
        } while(0)
    #define PROTECT_E(STATE, CALL_INFO, EXP) \
        do { \
            SAVE_PC(STATE, CALL_INFO); \
            (STATE)->stackTop.valuePointer = (CALL_INFO)->functionTop.valuePointer; \
            EXP; \
        } while(0)
#else
    // GCC/Clang 支持语句表达式
    #define PROTECT_ESH(STATE, CALL_INFO, EXP) (SAVE_STATE(STATE, CALL_INFO), (EXP), UPDATE_TRAP(CALL_INFO))
    #define PROTECT_EH(STATE, CALL_INFO, EXP) (SAVE_PC(STATE, CALL_INFO), (EXP), UPDATE_TRAP(CALL_INFO))
    #define PROTECT_E(STATE, CALL_INFO, EXP) (SAVE_STATE(STATE, CALL_INFO), (EXP))
#endif

#define JUMP(CALL_INFO, INSTRUCTION, OFFSET)                                                                           \
    {                                                                                                                  \
        programCounter += A2(INSTRUCTION) + (OFFSET);                                                                  \
        UPDATE_TRAP(CALL_INFO);                                                                                        \
    }

LZrStart:
    trap = state->debugHookSignal;
LZrReturning: {
    SZrTypeValue *functionBaseValue = ZrCore_Stack_GetValue(callInfo->functionBase.valuePointer);
    closure = ZR_CAST_VM_CLOSURE(state, functionBaseValue->value.object);
    constants = closure->function->constantValueList;
    programCounter = callInfo->context.context.programCounter - 1;
    base = callInfo->functionBase.valuePointer + 1;
}
    if (ZR_UNLIKELY(trap != ZR_DEBUG_SIGNAL_NONE)) {
        // todo
    }
    for (;;) {

        TZrInstruction instruction;
        /*
         * fetch instruction
         */
        ZR_INSTRUCTION_FETCH(instruction, programCounter, trap = ZrCore_Debug_TraceExecution(state, programCounter);
                             UPDATE_STACK(callInfo), 1);
        // 检查 programCounter 是否超出指令范围
        const TZrInstruction *instructionsEnd =
                closure->function->instructionsList + closure->function->instructionsLength;
        if (ZR_UNLIKELY(programCounter >= instructionsEnd)) {
            // 超出指令范围，退出循环（相当于隐式返回）
            break;
        }
        // debug line
#if ZR_DEBUG
#endif
        ZR_ASSERT(base == callInfo->functionBase.valuePointer + 1);
        ZR_ASSERT(base <= state->stackTop.valuePointer &&
                  state->stackTop.valuePointer <= state->stackTail.valuePointer);

        TZrBool destinationIsRet = E(instruction) == ZR_INSTRUCTION_USE_RET_FLAG;
        SZrTypeValue *destination = destinationIsRet ? &ret : &BASE(E(instruction))->value;

        ZR_INSTRUCTION_DISPATCH(instruction) {
            ZR_INSTRUCTION_LABEL(GET_STACK) {
                SZrTypeValue *source = &BASE(A2(instruction))->value;
                if (destinationIsRet) {
                    *destination = *source;
                } else {
                    ZrCore_Value_Copy(state, destination, source);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_STACK) {
                // SET_STACK 指令格式：
                // operandExtra (E) = destSlot (目标栈槽)
                // operand2[0] (A2) = srcSlot (源栈槽)
                // 将 srcSlot 的值复制到 destSlot
                SZrTypeValue *srcValue = &BASE(A2(instruction))->value;
                ZrCore_Value_Copy(state, &BASE(E(instruction))->value, srcValue);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // BASE(B1(instruction))->value = *CONST(ret.value.nativeObject.nativeUInt64);
                if (destinationIsRet) {
                    *destination = *CONST(A2(instruction));
                } else {
                    ZrCore_Value_Copy(state, destination, CONST(A2(instruction)));
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CONSTANT) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                //*CONST(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                *CONST(A2(instruction)) = *destination;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                // closure function to access
                ZrCore_Value_Copy(state, destination, ZrCore_ClosureValue_GetValue(CLOSURE(A2(instruction))));
                // BASE(B1(instruction))->value = CLOSURE(ret.value.nativeObject.nativeUInt64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_CLOSURE) {
                // ZR_ASSERT(ZR_VALUE_IS_TYPE_UNSIGNED_INT(A2(instruction)));
                SZrClosureValue *closureValue = CLOSURE(A2(instruction));
                SZrTypeValue *value = ZrCore_ClosureValue_GetValue(closureValue);
                SZrTypeValue *newValue = destination;
                // closure function to access
                ZrCore_Value_Copy(state, value, newValue);
                // CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
                ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closureValue), newValue);
                // *CLOSURE(ret.value.nativeObject.nativeUInt64) = BASE(B1(instruction))->value;
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_BOOL) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_BOOL);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       callInfo->functionTop.valuePointer,
                                                       ZR_TRUE,
                                                       meta,
                                                       opA,
                                                       ZR_NULL,
                                                       1,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (returnValue->type == ZR_VALUE_TYPE_BOOL) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeInt64 != 0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeUInt64 != 0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZR_VALUE_FAST_SET(destination, nativeBool, opA->value.nativeObject.nativeDouble != 0.0,
                                          ZR_VALUE_TYPE_BOOL);
                    } else if (ZR_VALUE_IS_TYPE_STRING(opA->type)) {
                        SZrString *str = ZR_CAST_STRING(state, opA->value.object);
                        TZrSize len = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? str->shortStringLength : str->longStringLength;
                        ZR_VALUE_FAST_SET(destination, nativeBool, len > 0, ZR_VALUE_TYPE_BOOL);
                    } else {
                        // 对象类型，默认返回 true
                        ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_INT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_INT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       ZR_NULL,
                                                       1,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, (TZrInt64) opA->value.nativeObject.nativeUInt64);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, (TZrInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsInt(state, destination, opA->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrCore_Value_InitAsInt(state, destination, 0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_UINT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_UINT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       ZR_NULL,
                                                       1,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_INT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsUInt(state, destination, 0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsUInt(state, destination, 0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, (TZrUInt64) opA->value.nativeObject.nativeInt64);
                    } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, (TZrUInt64) opA->value.nativeObject.nativeDouble);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsUInt(state, destination, opA->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0
                        ZrCore_Value_InitAsUInt(state, destination, 0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_TO_FLOAT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       ZR_NULL,
                                                       1,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            if (ZR_VALUE_IS_TYPE_FLOAT(returnValue->type)) {
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                // 元方法返回类型错误，使用默认转换
                                ZrCore_Value_InitAsFloat(state, destination, 0.0);
                            }
                        } else {
                            // 调用失败，使用默认转换
                            ZrCore_Value_InitAsFloat(state, destination, 0.0);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，使用内置转换逻辑
                    if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                        *destination = *opA;
                    } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, (TZrFloat64) opA->value.nativeObject.nativeInt64);
                    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, (TZrFloat64) opA->value.nativeObject.nativeUInt64);
                    } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                        ZrCore_Value_InitAsFloat(state, destination, opA->value.nativeObject.nativeBool ? (TZrFloat64)ZR_TRUE : (TZrFloat64)ZR_FALSE);
                    } else {
                        // 其他类型无法转换，返回 0.0
                        ZrCore_Value_InitAsFloat(state, destination, 0.0);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRING) {
                opA = &BASE(A1(instruction))->value;
                SZrString *result = ZrCore_Value_ConvertToString(state, opA);
                if (result != ZR_NULL) {
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                } else {
                    // 转换失败，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_STRUCT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TZrUInt16 typeNameConstantIndex = B1(instruction);
                if (closure->function != ZR_NULL && typeNameConstantIndex < closure->function->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 如果定义了 ZR_META_TO_STRUCT，使用它；否则直接查找原型
                        // 注意：ZR_META_TO_STRUCT 可能不在标准元方法列表中，需要检查是否支持
                        // 如果opA是对象类型，尝试从对象的prototype中查找元方法
                        SZrMeta *meta = ZR_NULL;
                        if (opA->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *obj = ZR_CAST_OBJECT(state, opA->value.object);
                            if (obj != ZR_NULL && obj->prototype != ZR_NULL) {
                                // 尝试查找TO_STRUCT元方法（如果存在）
                                // 注意：ZR_META_TO_STRUCT可能不在标准枚举中，这里先查找原型链
                                // 如果未来添加了ZR_META_TO_STRUCT，可以在这里使用
                                // TODO: 目前暂时跳过元方法查找，直接查找原型
                            }
                        }
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                TZrStackValuePointer metaBase = ZR_NULL;
                                TZrStackValuePointer restoredStackTop = savedStackTop;
                                if (execution_invoke_meta_call(state,
                                                               savedCallInfo,
                                                               savedStackTop,
                                                               savedStackTop,
                                                               ZR_FALSE,
                                                               meta,
                                                               opA,
                                                               typeNameValue,
                                                               2,
                                                               &metaBase,
                                                               &restoredStackTop)) {
                                    SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = restoredStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                if (!convert_to_struct(state, opA, prototype, destination)) {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 struct（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // TODO: 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrCore_Value_Copy(state, destination, opA);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TO_OBJECT) {
                opA = &BASE(A1(instruction))->value;
                // operand1[1] 存储类型名称常量索引
                TZrUInt16 typeNameConstantIndex = B1(instruction);
                if (closure->function != ZR_NULL && typeNameConstantIndex < closure->function->constantValueLength) {
                    SZrTypeValue *typeNameValue = CONST(typeNameConstantIndex);
                    if (typeNameValue->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *typeName = ZR_CAST_STRING(state, typeNameValue->value.object);
                        
                        // 1. 优先尝试通过元方法进行转换
                        // 如果定义了 ZR_META_TO_OBJECT，使用它；否则直接查找原型
                        // 注意：ZR_META_TO_OBJECT 可能不在标准元方法列表中，需要检查是否支持
                        // 如果opA是对象类型，尝试从对象的prototype中查找元方法
                        SZrMeta *meta = ZR_NULL;
                        if (opA->type == ZR_VALUE_TYPE_OBJECT) {
                            SZrObject *obj = ZR_CAST_OBJECT(state, opA->value.object);
                            if (obj != ZR_NULL && obj->prototype != ZR_NULL) {
                                // 尝试查找TO_OBJECT元方法（如果存在）
                                // 注意：ZR_META_TO_OBJECT可能不在标准枚举中，这里先查找原型链
                                // 如果未来添加了ZR_META_TO_OBJECT，可以在这里使用
                                // TODO: 目前暂时跳过元方法查找，直接查找原型
                            }
                        }
                        if (meta != ZR_NULL && meta->function != ZR_NULL) {
                            // 调用元方法
                            TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                            SZrCallInfo *savedCallInfo = state->callInfoList;
                            PROTECT_E(state, callInfo, {
                                TZrStackValuePointer metaBase = ZR_NULL;
                                TZrStackValuePointer restoredStackTop = savedStackTop;
                                if (execution_invoke_meta_call(state,
                                                               savedCallInfo,
                                                               savedStackTop,
                                                               savedStackTop,
                                                               ZR_FALSE,
                                                               meta,
                                                               opA,
                                                               typeNameValue,
                                                               2,
                                                               &metaBase,
                                                               &restoredStackTop)) {
                                    SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                    ZrCore_Value_Copy(state, destination, returnValue);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                                state->stackTop.valuePointer = restoredStackTop;
                                state->callInfoList = savedCallInfo;
                            });
                        } else {
                            // 2. 无元方法，尝试查找类型原型并执行转换
                            SZrObjectPrototype *prototype = find_type_prototype(state, typeName, ZR_OBJECT_PROTOTYPE_TYPE_INVALID);
                            if (prototype != ZR_NULL) {
                                // 找到原型，执行转换
                                TZrBool converted = ZR_FALSE;

                                if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                                    converted = convert_to_class(state, opA, prototype, destination);
                                } else if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM) {
                                    converted = convert_to_enum(state, opA, prototype, destination);
                                }

                                if (!converted) {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            } else {
                                // 3. 找不到原型，如果源值是对象，尝试直接复制
                                // 这允许在运行时动态创建 class（如果类型系统支持）
                                if (ZR_VALUE_IS_TYPE_OBJECT(opA->type)) {
                                    // TODO: 暂时直接复制对象（后续需要设置正确的原型）
                                    ZrCore_Value_Copy(state, destination, opA);
                                } else {
                                    ZrCore_Value_ResetAsNull(destination);
                                }
                            }
                        }
                    } else {
                        // 类型名称常量类型错误
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 常量索引越界
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (try_builtin_add(state, destination, opA, opB)) {
                    // 基础数值和字符串拼接直接在运行时处理。
                } else {
                    SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_ADD);
                    if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                        TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                        SZrCallInfo *savedCallInfo = state->callInfoList;
                        PROTECT_E(state, callInfo, {
                            TZrStackValuePointer metaBase = ZR_NULL;
                            TZrStackValuePointer restoredStackTop = savedStackTop;
                            if (execution_invoke_meta_call(state,
                                                           savedCallInfo,
                                                           savedStackTop,
                                                           savedStackTop,
                                                           ZR_FALSE,
                                                           meta,
                                                           opA,
                                                           opB,
                                                           2,
                                                           &metaBase,
                                                           &restoredStackTop)) {
                                SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                                ZrCore_Value_Copy(state, destination, returnValue);
                            } else {
                                ZrCore_Value_ResetAsNull(destination);
                            }
                            state->stackTop.valuePointer = restoredStackTop;
                            state->callInfoList = savedCallInfo;
                        });
                    } else {
                        // 无元方法，返回 null
                        ZrCore_Value_ResetAsNull(destination);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    // 根据操作数类型选择使用有符号还是无符号整数
                    if (ZR_VALUE_IS_TYPE_SIGNED_INT(opA->type)) {
                        ALGORITHM_2(nativeInt64, +, opA->type);
                    } else {
                        ALGORITHM_2(nativeUInt64, +, opA->type);
                    }
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, opB, "ADD_INT");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_ADD, destination, opA, opB, "ADD_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(ADD_STRING) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_STRING(opA->type) && ZR_VALUE_IS_TYPE_STRING(opB->type));
                if (!concat_values_to_destination(state, destination, opA, opB, ZR_FALSE)) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SUB);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeInt64, -, opA->type);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, opB, "SUB_INT");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SUB_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_SUB, destination, opA, opB, "SUB_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_MUL);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeInt64, *, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    ALGORITHM_2(nativeUInt64, *, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MUL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_MUL, destination, opA, opB, "MUL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(NEG) {
                opA = &BASE(A1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_NEG);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       ZR_NULL,
                                                       1,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_DIV);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: divide by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                        // ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                        ZrCore_Debug_RunError(state, "divide by zero");
                    }
                    ALGORITHM_2(nativeInt64, /, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: divide by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeUInt64 == 0)) {
                        // ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
                        ZrCore_Debug_RunError(state, "divide by zero");
                    }
                    ALGORITHM_2(nativeUInt64, /, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(DIV_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_DIV, destination, opA, opB, "DIV_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_MOD);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: modulo by zero
                    if (ZR_UNLIKELY(opB->value.nativeObject.nativeInt64 == 0)) {
                        ZrCore_Debug_RunError(state, "modulo by zero");
                    }
                    TZrInt64 divisor = opB->value.nativeObject.nativeInt64;
                    if (ZR_UNLIKELY(divisor < 0)) {
                        divisor = -divisor;
                    }
                    ALGORITHM_CONST_2(nativeInt64, %, ZR_VALUE_TYPE_INT64, divisor);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: modulo by zero
                    if (opB->value.nativeObject.nativeUInt64 == 0) {
                        ZrCore_Debug_RunError(state, "modulo by zero");
                    }
                    ALGORITHM_2(nativeUInt64, %, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MOD_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_MOD, destination, opA, opB, "MOD_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_POW);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: power domain error
                    TZrInt64 valueA = opA->value.nativeObject.nativeInt64;
                    TZrInt64 valueB = opB->value.nativeObject.nativeInt64;
                    if (ZR_UNLIKELY(valueA == 0 && valueB <= 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    if (ZR_UNLIKELY(valueA < 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    ALGORITHM_FUNC_2(nativeInt64, ZrCore_Math_IntPower, ZR_VALUE_TYPE_INT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_SIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type)) {
                    SAVE_STATE(state, callInfo); // error: power domain error
                    TZrUInt64 valueA = opA->value.nativeObject.nativeUInt64;
                    TZrUInt64 valueB = opB->value.nativeObject.nativeUInt64;
                    if (ZR_UNLIKELY(valueA == 0 && valueB == 0)) {
                        ZrCore_Debug_RunError(state, "power domain error");
                    }
                    ALGORITHM_FUNC_2(nativeUInt64, ZrCore_Math_UIntPower, ZR_VALUE_TYPE_UINT64);
                } else {
                    execution_try_binary_numeric_float_fallback_or_raise(
                            state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_UNSIGNED");
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(POW_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_float_or_raise(
                        state, ZR_EXEC_NUMERIC_FALLBACK_POW, destination, opA, opB, "POW_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SHIFT_LEFT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_LEFT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                SZrMeta *meta = ZrCore_Value_GetMeta(state, opA, ZR_META_SHIFT_RIGHT);
                if (meta != ZR_NULL && meta->function != ZR_NULL) {
                    // 调用元方法
                    TZrStackValuePointer savedStackTop = state->stackTop.valuePointer;
                    SZrCallInfo *savedCallInfo = state->callInfoList;
                    PROTECT_E(state, callInfo, {
                        TZrStackValuePointer metaBase = ZR_NULL;
                        TZrStackValuePointer restoredStackTop = savedStackTop;
                        if (execution_invoke_meta_call(state,
                                                       savedCallInfo,
                                                       savedStackTop,
                                                       savedStackTop,
                                                       ZR_FALSE,
                                                       meta,
                                                       opA,
                                                       opB,
                                                       2,
                                                       &metaBase,
                                                       &restoredStackTop)) {
                            SZrTypeValue *returnValue = ZrCore_Stack_GetValue(metaBase);
                            ZrCore_Value_Copy(state, destination, returnValue);
                        } else {
                            ZrCore_Value_ResetAsNull(destination);
                        }
                        state->stackTop.valuePointer = restoredStackTop;
                        state->callInfoList = savedCallInfo;
                    });
                } else {
                    // 无元方法，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SHIFT_RIGHT_INT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT) {
                opA = &BASE(A1(instruction))->value;
                if (ZR_VALUE_IS_TYPE_NULL(opA->type)) {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_BOOL(opA->type)) {
                    ALGORITHM_1(nativeBool, !, ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_INT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeInt64 == 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else if (ZR_VALUE_IS_TYPE_FLOAT(opA->type)) {
                    ZR_VALUE_FAST_SET(destination,
                                      nativeBool,
                                      opA->value.nativeObject.nativeDouble == 0,
                                      ZR_VALUE_TYPE_BOOL);
                } else {
                    ZR_VALUE_FAST_SET(destination, nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, &&, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_BOOL(opA->type) && ZR_VALUE_IS_TYPE_BOOL(opB->type));
                ALGORITHM_2(nativeBool, ||, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state, ZR_EXEC_NUMERIC_COMPARE_GREATER, destination, opA, opB, "LOGICAL_GREATER_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state, ZR_EXEC_NUMERIC_COMPARE_LESS, destination, opA, opB, "LOGICAL_LESS_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TZrBool result = ZrCore_Value_Equal(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_NOT_EQUAL) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                TZrBool result = !ZrCore_Value_Equal(state, opA, opB);
                ZR_VALUE_FAST_SET(destination, nativeBool, result, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, >=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_GREATER_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state,
                        ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
                        destination,
                        opA,
                        opB,
                        "LOGICAL_GREATER_EQUAL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_SIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_UNSIGNED) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_CVT_2(nativeBool, nativeUInt64, <=, ZR_VALUE_TYPE_BOOL);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(LOGICAL_LESS_EQUAL_FLOAT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                execution_apply_binary_numeric_compare_or_raise(
                        state,
                        ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL,
                        destination,
                        opA,
                        opB,
                        "LOGICAL_LESS_EQUAL_FLOAT");
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_NOT) {
                opA = &BASE(A1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type));
                ALGORITHM_1(nativeInt64, ~, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_AND) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, &, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_OR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, |, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_XOR) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, ^, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_LEFT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeInt64, <<, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(BITWISE_SHIFT_RIGHT) {
                opA = &BASE(A1(instruction))->value;
                opB = &BASE(B1(instruction))->value;
                ZR_ASSERT(ZR_VALUE_IS_TYPE_INT(opA->type) && ZR_VALUE_IS_TYPE_INT(opB->type));
                ALGORITHM_2(nativeUInt64, >>, ZR_VALUE_TYPE_INT64);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_CALL) {
                // FUNCTION_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位，用于编译时；ZrCore_Function_PreCall 需要的是 expectedReturnCount)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize expectedReturnCount = 1;  // 期望 1 个返回值；ZrCore_Function_PreCall 的 resultCount 表示 expectedReturnCount；E(instruction)=resultSlot 仅编译时用
                
                opA = &BASE(functionSlot)->value;
                // 这里只保证“非空可调用目标”进入统一预调用分派。
                // ZrCore_Function_PreCall 会继续分流 function/closure/native pointer，
                // 并在其它值类型上解析 @call 元方法。
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                
                // 设置栈顶指针（函数在 functionSlot，参数在 functionSlot+1 到 functionSlot+parametersCount）
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // save 下一条指令的地址：fetch 使用 *(PC+=1)，当前 programCounter 指向本指令，故保存 programCounter+1
                callInfo->context.context.programCounter = programCounter + 1;
                SZrCallInfo *nextCallInfo = ZrCore_Function_PreCall(state, BASE(functionSlot), expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    // NULL means native call
                    UPDATE_BASE(callInfo);
                    trap = callInfo->context.context.trap;
                } else {
                    // a vm call
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_TAIL_CALL) {
                // FUNCTION_TAIL_CALL 指令格式：
                // operandExtra (E) = resultSlot (返回值槽位，编译时；ZrCore_Function_PreCall 需要的是 expectedReturnCount)
                // operand1[0] (A1) = functionSlot (函数在栈上的槽位)
                // operand1[1] (B1) = parametersCount (参数数量，直接使用，不从栈读取)
                TZrSize functionSlot = A1(instruction);
                TZrSize parametersCount = B1(instruction);  // 参数数量直接使用，不是栈槽位
                TZrSize expectedReturnCount = 1;  // 期望 1 个返回值；E(instruction)=resultSlot 仅编译时用
                
                opA = &BASE(functionSlot)->value;
                // 与普通调用保持一致，把实际可调用性判断交给统一预调用分派，
                // 以便对象值通过 @call 元方法进入调用链。
                ZR_ASSERT(!ZR_VALUE_IS_TYPE_NULL(opA->type) && "Function value is NULL in FUNCTION_CALL");
                
                // 设置栈顶指针
                if (parametersCount > 0) {
                    state->stackTop.valuePointer = BASE(functionSlot) + parametersCount + 1;
                } else {
                    state->stackTop.valuePointer = BASE(functionSlot) + 1;
                }
                
                // 尾调用：重用当前调用帧
                // 保存下一条指令的地址：fetch 使用 *(PC+=1)，故保存 programCounter+1
                callInfo->context.context.programCounter = programCounter + 1;
                // 设置尾调用标志
                callInfo->callStatus |= ZR_CALL_STATUS_TAIL_CALL;
                // 准备调用参数（函数在BASE(functionSlot)，参数在BASE(functionSlot+1)到BASE(functionSlot+parametersCount)）
                TZrStackValuePointer functionPointer = BASE(functionSlot);
                // 调用函数（expectedReturnCount=1，与 FUNCTION_CALL 一致）；返回值写入 BASE(E(instruction))
                SZrCallInfo *nextCallInfo = ZrCore_Function_PreCall(state, functionPointer, expectedReturnCount, BASE(E(instruction)));
                if (nextCallInfo == ZR_NULL) {
                    // Native调用，清除尾调用标志
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    UPDATE_BASE(callInfo);
                    trap = callInfo->context.context.trap;
                } else {
                    // VM调用：对于尾调用，重用当前callInfo而不是创建新的
                    // 但ZrFunctionPreCall总是创建新的callInfo，所以我们需要调整
                    // 实际上，对于真正的尾调用优化，我们需要手动设置callInfo的字段
                    // 这里先使用简单的实现：清除尾调用标志，使用普通调用
                    callInfo->callStatus &= ~ZR_CALL_STATUS_TAIL_CALL;
                    callInfo = nextCallInfo;
                    goto LZrStart;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(FUNCTION_RETURN) {
                // FUNCTION_RETURN 指令格式：
                // operandExtra (E) = 返回值数量 (returnCount)
                // operand1[0] (A1) = 返回值槽位 (resultSlot)
                // operand1[1] (B1) = 可变参数参数数量 (variableArguments, 0 表示非可变参数函数)
                TZrSize returnCount = E(instruction);
                TZrSize resultSlot = A1(instruction);
                TZrSize variableArguments = B1(instruction);

                // save its program counter
                callInfo->context.context.programCounter = programCounter;

                if (state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
                    state->stackTop.valuePointer = callInfo->functionTop.valuePointer;
                }
                // Always close open upvalues for the returning frame. The to-be-closed
                // list only tracks close metas, not ordinary captured locals.
                ZrCore_Closure_CloseClosure(state,
                                      callInfo->functionBase.valuePointer + 1,
                                      ZR_THREAD_STATUS_INVALID,
                                      ZR_FALSE);

                // 如果是可变参数函数，需要调整 functionBase 指针
                // 参考 Lua: if (nparams1) ci->func.p -= ci->u.l.nextraargs + nparams1;
                if (variableArguments > 0) {
                    callInfo->functionBase.valuePointer -=
                            callInfo->context.context.variableArgumentCount + variableArguments;
                }
                state->stackTop.valuePointer = BASE(resultSlot) + returnCount;
                ZrCore_Function_PostCall(state, callInfo, returnCount);
                trap = callInfo->context.context.trap;
                goto LZrReturn;
            }

        LZrReturn: {
            // return from vm
            if (callInfo->callStatus & ZR_CALL_STATUS_CREATE_FRAME) {
                return;
            } else {
                callInfo = callInfo->previous;
                goto LZrReturning;
            }
        }
            DONE(1);
            ZR_INSTRUCTION_LABEL(GETUPVAL) {
                // GETUPVAL 指令格式：
                // operandExtra (E) = destination slot
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(base - 1)->value.object);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrCore_Debug_RunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    // 如果闭包值为 NULL，尝试初始化（这可能是第一次访问）
                    // 注意：这不应该发生在正常执行中，但为了测试的兼容性，我们允许这种情况
                    ZrCore_Debug_RunError(state, "upvalue is null - closure values may not be initialized");
                }
                ZrCore_Value_Copy(state, destination, ZrCore_ClosureValue_GetValue(closureValue));
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SETUPVAL) {
                // SETUPVAL 指令格式：
                // operandExtra (E) = source slot (destination)
                // operand1[0] (A1) = upvalue index
                // operand1[1] (B1) = 未使用
                TZrSize upvalueIndex = A1(instruction);
                SZrClosure *currentClosure = ZR_CAST_VM_CLOSURE(state, ZrCore_Stack_GetValue(base - 1)->value.object);
                if (ZR_UNLIKELY(upvalueIndex >= currentClosure->closureValueCount)) {
                    ZrCore_Debug_RunError(state, "upvalue index out of range");
                }
                SZrClosureValue *closureValue = currentClosure->closureValuesExtend[upvalueIndex];
                if (ZR_UNLIKELY(closureValue == ZR_NULL)) {
                    ZrCore_Debug_RunError(state, "upvalue is null");
                }
                SZrTypeValue *target = ZrCore_ClosureValue_GetValue(closureValue);
                ZrCore_Value_Copy(state, target, destination);
                ZrCore_Value_Barrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(currentClosure->closureValuesExtend[upvalueIndex]),
                               destination);
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GET_SUB_FUNCTION) {
                // GET_SUB_FUNCTION 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = childFunctionIndex (子函数在 childFunctionList 中的索引)
                // operand1[1] (B1) = 0 (未使用)
                // GET_SUB_FUNCTION 用于从父函数的 childFunctionList 中通过索引获取子函数并压入栈
                // 这是编译时确定的静态索引，运行时直接通过索引访问，无需名称查找
                // 注意：GET_SUB_FUNCTION 只操作函数类型（ZR_VALUE_TYPE_FUNCTION 或 ZR_VALUE_TYPE_CLOSURE）
                TZrSize childFunctionIndex = A1(instruction);
                
                // 获取父函数的 callInfo
                SZrCallInfo *parentCallInfo = callInfo->previous;
                TZrBool found = ZR_FALSE;
                SZrFunction *parentFunction = ZR_NULL;
                
                if (parentCallInfo != ZR_NULL) {
                    TZrBool isVM = ZR_CALL_INFO_IS_VM(parentCallInfo);
                    if (isVM) {
                        // 获取父函数的闭包和函数
                        SZrTypeValue *parentFunctionBaseValue = ZrCore_Stack_GetValue(parentCallInfo->functionBase.valuePointer);
                        if (parentFunctionBaseValue != ZR_NULL) {
                            // 类型检查：确保父函数是函数类型或闭包类型
                            if (parentFunctionBaseValue->type == ZR_VALUE_TYPE_FUNCTION || 
                                parentFunctionBaseValue->type == ZR_VALUE_TYPE_CLOSURE) {
                                SZrClosure *parentClosure = ZR_CAST_VM_CLOSURE(state, parentFunctionBaseValue->value.object);
                                if (parentClosure != ZR_NULL && parentClosure->function != ZR_NULL) {
                                    parentFunction = parentClosure->function;
                                }
                            } else {
                                ZrCore_Debug_RunError(state, "GET_SUB_FUNCTION: parent must be a function or closure");
                            }
                        }
                    } else {
                        // 如果不是 VM 调用，尝试从当前函数的闭包获取子函数
                        if (closure != ZR_NULL && closure->function != ZR_NULL) {
                            parentFunction = closure->function;
                        }
                    }
                } else if (parentCallInfo == ZR_NULL) {
                    // 如果没有父函数的 callInfo，尝试从当前函数的闭包获取子函数
                    // 这适用于顶层函数或测试函数直接调用的情况
                    if (closure != ZR_NULL && closure->function != ZR_NULL) {
                        parentFunction = closure->function;
                    }
                }
                
                // 从父函数获取子函数
                if (parentFunction != ZR_NULL) {
                    // 通过索引直接访问 childFunctionList
                    if (childFunctionIndex < parentFunction->childFunctionLength) {
                        SZrFunction *childFunction = &parentFunction->childFunctionList[childFunctionIndex];
                        if (childFunction != ZR_NULL &&
                            childFunction->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                            SZrClosureValue **parentClosureValues =
                                    closure != ZR_NULL ? closure->closureValuesExtend : ZR_NULL;
                            ZrCore_Closure_PushToStack(state, childFunction, parentClosureValues, base, BASE(E(instruction)));
                            destination->type = ZR_VALUE_TYPE_CLOSURE;
                            destination->isGarbageCollectable = ZR_TRUE;
                            destination->isNative = ZR_FALSE;
                            found = ZR_TRUE;
                        }
                    }
                }
                
                // 如果没找到，返回 null
                if (!found) {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);


            ZR_INSTRUCTION_LABEL(GET_GLOBAL) {
                // GET_GLOBAL 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = 0 (未使用)
                // operand1[1] (B1) = 0 (未使用)
                // GET_GLOBAL 用于获取全局 zr 对象到堆栈
                SZrGlobalState *global = state->global;
                if (global != ZR_NULL && global->zrObject.type == ZR_VALUE_TYPE_OBJECT) {
                    ZrCore_Value_Copy(state, destination, &global->zrObject);
                } else {
                    // 如果 zr 对象未初始化，返回 null
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(GETTABLE) {
                // GETTABLE 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = tableSlot (对象在栈中的位置)
                // operand1[1] (B1) = keySlot (键在栈中的位置)
                // GETTABLE 用于从 object 的键值对（nodeMap）中获取值
                // 注意：GETTABLE 只操作对象类型（ZR_VALUE_TYPE_OBJECT 或 ZR_VALUE_TYPE_ARRAY）
                opA = &BASE(A1(instruction))->value; // table object
                opB = &BASE(B1(instruction))->value; // key
                
                // 类型检查：确保 table 是对象类型或数组类型
                if (opA->type == ZR_VALUE_TYPE_OBJECT || opA->type == ZR_VALUE_TYPE_ARRAY) {
                    const SZrTypeValue *result = ZrCore_Object_GetValue(state, ZR_CAST_OBJECT(state, opA->value.object), opB);
                    if (result != ZR_NULL) {
                        ZrCore_Value_Copy(state, destination, result);
                    } else {
                        ZrCore_Value_ResetAsNull(destination);
                    }
                } else {
                    // 类型错误：table 不是对象类型
                    ZrCore_Debug_RunError(state, "GETTABLE: table must be an object or array");
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);

            ZR_INSTRUCTION_LABEL(SETTABLE) {
                opA = &BASE(A1(instruction))->value; // table object
                opB = &BASE(B1(instruction))->value; // key
                ZrCore_Object_SetValue(state, ZR_CAST_OBJECT(state, opA->value.object), opB, destination);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP) { JUMP(callInfo, instruction, 0); }
            DONE(1);
            ZR_INSTRUCTION_LABEL(JUMP_IF) {
                // JUMP_IF 指令格式：
                // operandExtra (E) = condSlot (条件值的栈槽)
                // operand2[0] (A2) = offset (相对跳转偏移量)
                // 如果条件为假，跳转到 else 分支；如果条件为真，继续执行 then 分支
                SZrTypeValue *condValue = &BASE(E(instruction))->value;
                // 检查条件值是否为真（支持 bool、int、非零值等）
                TZrBool condition = ZR_FALSE;
                if (ZR_VALUE_IS_TYPE_BOOL(condValue->type)) {
                    condition = condValue->value.nativeObject.nativeBool;
                } else if (ZR_VALUE_IS_TYPE_INT(condValue->type)) {
                    condition = condValue->value.nativeObject.nativeInt64 != 0;
                } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(condValue->type)) {
                    condition = condValue->value.nativeObject.nativeUInt64 != 0;
                } else if (ZR_VALUE_IS_TYPE_FLOAT(condValue->type)) {
                    condition = condValue->value.nativeObject.nativeDouble != 0.0;
                } else if (ZR_VALUE_IS_TYPE_NULL(condValue->type)) {
                    condition = ZR_FALSE;
                } else if (ZR_VALUE_IS_TYPE_STRING(condValue->type)) {
                    SZrString *str = ZR_CAST_STRING(state, condValue->value.object);
                    TZrSize len = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? str->shortStringLength : str->longStringLength;
                    condition = len > 0;
                } else {
                    // 对象类型，默认为真
                    condition = ZR_TRUE;
                }
                
                // 如果条件为假，跳转到 else 分支
                if (!condition) {
                    JUMP(callInfo, instruction, 0);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_CLOSURE) {
                // CREATE_CLOSURE 指令格式：
                // operandExtra (E) = destSlot (destination已通过E(instruction)定义)
                // operand1[0] (A1) = functionConstantIndex
                // operand1[1] (B1) = closureVarCount
                TZrSize functionConstantIndex = A1(instruction);
                TZrSize closureVarCount = B1(instruction);
                SZrTypeValue *functionConstant = CONST(functionConstantIndex);
                // 从常量池获取函数对象
                // 注意：编译器将SZrFunction*存储为ZR_VALUE_TYPE_CLOSURE类型，但value.object实际指向SZrFunction*
                SZrFunction *function = ZR_NULL;
                if (functionConstant->type == ZR_VALUE_TYPE_CLOSURE ||
                    functionConstant->type == ZR_VALUE_TYPE_FUNCTION) {
                    // 从raw object获取实际的函数对象
                    SZrRawObject *rawObject = functionConstant->value.object;
                    if (rawObject != ZR_NULL && rawObject->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                        function = ZR_CAST(SZrFunction *, rawObject);
                    }
                }
                if (function != ZR_NULL) {
                    SZrClosureValue **parentClosureValues =
                            closure != ZR_NULL ? closure->closureValuesExtend : ZR_NULL;
                    ZrCore_Closure_PushToStack(state, function, parentClosureValues, base, BASE(E(instruction)));
                    destination->type = ZR_VALUE_TYPE_CLOSURE;
                    destination->isGarbageCollectable = ZR_TRUE;
                    destination->isNative = ZR_FALSE;
                } else {
                    // 类型错误或函数为NULL
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_OBJECT) {
                // 创建空对象
                SZrObject *object = ZrCore_Object_New(state, ZR_NULL);
                if (object != ZR_NULL) {
                    ZrCore_Object_Init(state, object);
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CREATE_ARRAY) {
                // 创建空数组对象
                SZrObject *array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
                if (array != ZR_NULL) {
                    ZrCore_Object_Init(state, array);
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(MARK_TO_BE_CLOSED) {
                TZrSize closeSlot = E(instruction);
                TZrStackValuePointer closePointer = BASE(closeSlot);
                ZrCore_Closure_ToBeClosedValueClosureNew(state, closePointer);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(CLOSE_SCOPE) {
                close_scope_cleanup_registrations(state, E(instruction));
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(TRY) {
                if (!execution_push_exception_handler(state, callInfo, E(instruction))) {
                    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_MEMORY_ERROR)) {
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
                    }
                    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(END_TRY) {
                SZrVmExceptionHandlerState *handlerState = execution_find_handler_state(state, callInfo, E(instruction));
                SZrFunction *handlerFunction = ZR_NULL;
                const SZrFunctionExceptionHandlerInfo *handlerInfo =
                        execution_lookup_exception_handler_info(state, handlerState, &handlerFunction);

                if (handlerState != ZR_NULL) {
                    if (handlerInfo != ZR_NULL && handlerInfo->hasFinally) {
                        handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY;
                    } else {
                        execution_pop_exception_handler(state, handlerState);
                    }
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(THROW) {
                SZrTypeValue payload;

                SAVE_PC(state, callInfo);
                execution_clear_pending_control(state);
                payload = BASE(E(instruction))->value;
                if (!ZrCore_Exception_NormalizeThrownValue(state,
                                                          &payload,
                                                          callInfo,
                                                          ZR_THREAD_STATUS_RUNTIME_ERROR)) {
                    if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                    }
                    ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                }

                if (execution_unwind_exception_to_handler(state, &callInfo)) {
                    goto LZrReturning;
                }

                ZrCore_Exception_Throw(state, state->currentExceptionStatus);
            }
            ZR_INSTRUCTION_LABEL(CATCH) {
                if (state->hasCurrentException) {
                    ZrCore_Value_Copy(state, destination, &state->currentException);
                    ZrCore_Exception_ClearCurrent(state);
                } else {
                    ZrCore_Value_ResetAsNull(destination);
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(END_FINALLY) {
                SZrVmExceptionHandlerState *handlerState = execution_find_handler_state(state, callInfo, E(instruction));
                SZrCallInfo *resumeCallInfo;
                TZrStackValuePointer targetSlot;

                if (handlerState != ZR_NULL) {
                    execution_pop_exception_handler(state, handlerState);
                }

                switch (state->pendingControl.kind) {
                    case ZR_VM_PENDING_CONTROL_NONE:
                        break;
                    case ZR_VM_PENDING_CONTROL_EXCEPTION:
                        resumeCallInfo = state->pendingControl.callInfo != ZR_NULL
                                                 ? state->pendingControl.callInfo
                                                 : callInfo;
                        callInfo = resumeCallInfo;
                        if (execution_unwind_exception_to_handler(state, &callInfo)) {
                            goto LZrReturning;
                        }
                        ZrCore_Exception_Throw(state, state->currentExceptionStatus);
                        break;
                    case ZR_VM_PENDING_CONTROL_RETURN:
                    case ZR_VM_PENDING_CONTROL_BREAK:
                    case ZR_VM_PENDING_CONTROL_CONTINUE:
                        resumeCallInfo = state->pendingControl.callInfo != ZR_NULL
                                                 ? state->pendingControl.callInfo
                                                 : callInfo;
                        callInfo = resumeCallInfo;
                        if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                            goto LZrReturning;
                        }

                        if (state->pendingControl.kind == ZR_VM_PENDING_CONTROL_RETURN &&
                            state->pendingControl.hasValue &&
                            resumeCallInfo != ZR_NULL &&
                            resumeCallInfo->functionBase.valuePointer != ZR_NULL) {
                            targetSlot = resumeCallInfo->functionBase.valuePointer + 1 + state->pendingControl.valueSlot;
                            ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value);
                        }

                        if (execution_jump_to_instruction_offset(state,
                                                                 &callInfo,
                                                                 resumeCallInfo,
                                                                 state->pendingControl.targetInstructionOffset)) {
                            execution_clear_pending_control(state);
                            goto LZrReturning;
                        }

                        execution_clear_pending_control(state);
                        if (!ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
                            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                        }
                        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_EXCEPTION_ERROR);
                        break;
                    default:
                        execution_clear_pending_control(state);
                        break;
                }
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_RETURN) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_RETURN,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              E(instruction),
                                              &BASE(E(instruction))->value);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_BREAK) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_BREAK,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              0,
                                              ZR_NULL);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_LABEL(SET_PENDING_CONTINUE) {
                execution_set_pending_control(state,
                                              ZR_VM_PENDING_CONTROL_CONTINUE,
                                              callInfo,
                                              (TZrMemoryOffset)A2(instruction),
                                              0,
                                              ZR_NULL);
                if (execution_resume_pending_via_outer_finally(state, &callInfo)) {
                    goto LZrReturning;
                }
                if (execution_jump_to_instruction_offset(state,
                                                         &callInfo,
                                                         callInfo,
                                                         state->pendingControl.targetInstructionOffset)) {
                    execution_clear_pending_control(state);
                    goto LZrReturning;
                }
                execution_clear_pending_control(state);
            }
            DONE(1);
            ZR_INSTRUCTION_DEFAULT() {
                // todo: error unreachable
                char message[256];
                sprintf(message, "Not implemented op code:%d at offset %d\n", instruction.instruction.operationCode,
                        (int) (instructionsEnd - programCounter));
                ZrCore_Debug_RunError(state, message);
                ZR_ABORT();
            }
            DONE(1);
        }
    }

#undef DONE
}
