//
// Created by HeJiahui on 2025/6/25.
//

#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/meta.h"

#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/exception.h"

void ZrCore_Meta_GlobalStaticsInit(SZrState *state) {
    SZrGlobalState *global = state->global;
    for (TZrEnum i = 0; i < ZR_META_ENUM_MAX; i++) {
        global->metaFunctionName[i] = ZrCore_String_CreateFromNative(state, CZrMetaName[i]);
        ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->metaFunctionName[i]));
    }
}


void ZrCore_MetaTable_Construct(SZrMetaTable *table) {
    for (TZrEnum i = 0; i < ZR_META_ENUM_MAX; i++) {
        table->metas[i] = ZR_NULL;
    }
}

// ==================== Native Meta Method Functions ====================

// TO_STRING 元方法实现

// NULL 类型转字符串
static TZrInt64 meta_to_string_null(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    // self 在 base + 1
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    // 返回值放在 base 位置
    SZrString *result = ZrCore_String_CreateFromNative(state, ZR_STRING_NULL_STRING);
    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// BOOL 类型转字符串
static TZrInt64 meta_to_string_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrString *result = ZR_NULL;
    if (self->value.nativeObject.nativeBool) {
        result = ZrCore_String_CreateFromNative(state, ZR_STRING_TRUE_STRING);
    } else {
        result = ZrCore_String_CreateFromNative(state, ZR_STRING_FALSE_STRING);
    }
    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 数字类型转字符串
static TZrInt64 meta_to_string_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrString *result = ZrCore_String_FromNumber(state, self);
    if (result == ZR_NULL) {
        result = ZrCore_String_CreateFromNative(state, "");
    }
    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// STRING 类型直接返回
static TZrInt64 meta_to_string_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    // 直接返回自身
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), self);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// OBJECT 类型转字符串
static TZrInt64 meta_to_string_object(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue stableSelf = *self;
    SZrObject *object = ZR_CAST_OBJECT(state, stableSelf.value.object);
    SZrString *result = ZR_NULL;

    // 尝试调用对象的 TO_STRING 元方法（递归查找）
    SZrMeta *meta = ZrCore_Object_GetMetaRecursively(state->global, object, ZR_META_TO_STRING);
    if (meta != ZR_NULL && meta->function != ZR_NULL) {
        // 如果对象有自己的 TO_STRING 元方法，调用它
        // 将 meta->function 放到栈上，self 作为参数
        ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
        ZrCore_Stack_CopyValue(state, base + 1, &stableSelf);
        state->stackTop.valuePointer = base + 2;
        // 调用元方法
        base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
        // 返回值在 base 位置
        SZrTypeValue *returnValue = ZrCore_Stack_GetValue(base);
        if (returnValue->type == ZR_VALUE_TYPE_STRING) {
            ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), returnValue);
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    // 默认返回 [object type=X]
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "[object type=%d]", (int) object->internalType);
    result = ZrCore_String_CreateFromNative(state, buffer);
    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_BOOL 元方法实现

// NULL 转布尔值
static TZrInt64 meta_to_bool_null(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    // 返回值放在 base 位置
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// BOOL 直接返回
static TZrInt64 meta_to_bool_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    // 直接返回自身
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), self);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 数字转布尔值（非零为 true）
static TZrInt64 meta_to_bool_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrBool result = ZR_FALSE;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = (self->value.nativeObject.nativeInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = (self->value.nativeObject.nativeUInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = (self->value.nativeObject.nativeDouble != 0.0);
    }

    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 获取字符串长度的辅助函数
static TZrSize get_string_length(SZrString *str) {
    if (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return str->shortStringLength;
    } else {
        return str->longStringLength;
    }
}

// 字符串转布尔值（非空为 true）
static TZrInt64 meta_to_bool_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrString *str = ZR_CAST_STRING(state, self->value.object);
    TZrSize length = get_string_length(str);
    TZrBool result = (length > 0);

    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// OBJECT 转布尔值（默认返回 true）
static TZrInt64 meta_to_bool_object(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue stableSelf = *self;
    SZrObject *object = ZR_CAST_OBJECT(state, stableSelf.value.object);

    // 尝试调用对象的 TO_BOOL 元方法
    SZrMeta *meta = ZrCore_Object_GetMetaRecursively(state->global, object, ZR_META_TO_BOOL);
    if (meta != ZR_NULL && meta->function != ZR_NULL) {
        ZrCore_Stack_SetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
        ZrCore_Stack_CopyValue(state, base + 1, &stableSelf);
        state->stackTop.valuePointer = base + 2;
        base = ZrCore_Function_CallWithoutYieldAndRestore(state, base, 1);
        SZrTypeValue *returnValue = ZrCore_Stack_GetValue(base);
        if (returnValue->type == ZR_VALUE_TYPE_BOOL) {
            ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), returnValue);
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    // 默认返回 true
    ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_INT 元方法实现

// 数字转整数（截断）
static TZrInt64 meta_to_int_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrInt64 result = 0;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = self->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = (TZrInt64) self->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = (TZrInt64) self->value.nativeObject.nativeDouble;
    }

    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔转整数
static TZrInt64 meta_to_int_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrInt64 result = self->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_UINT 元方法实现

// 数字转无符号整数
static TZrInt64 meta_to_uint_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrUInt64 result = 0;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = (TZrUInt64) self->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = self->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = (TZrUInt64) self->value.nativeObject.nativeDouble;
    }

    ZrCore_Value_InitAsUInt(state, ZrCore_Stack_GetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔转无符号整数
static TZrInt64 meta_to_uint_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrUInt64 result = self->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    ZrCore_Value_InitAsUInt(state, ZrCore_Stack_GetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_FLOAT 元方法实现

// 数字转浮点数
static TZrInt64 meta_to_float_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrFloat64 result = 0.0;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = (TZrFloat64) self->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = (TZrFloat64) self->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = self->value.nativeObject.nativeDouble;
    }

    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔转浮点数
static TZrInt64 meta_to_float_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    TZrFloat64 result = self->value.nativeObject.nativeBool ? (TZrFloat64)ZR_TRUE : (TZrFloat64)ZR_FALSE;
    ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 运算元方法实现

// ADD 元方法 - 整数加法
static TZrInt64 meta_add_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TZrInt64 result = self->value.nativeObject.nativeInt64 + other->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    // 类型不匹配，返回 null
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ADD 元方法 - 无符号整数加法
static TZrInt64 meta_add_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TZrUInt64 result = self->value.nativeObject.nativeUInt64 + other->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ADD 元方法 - 浮点数加法
static TZrInt64 meta_add_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TZrFloat64 result = self->value.nativeObject.nativeDouble + other->value.nativeObject.nativeDouble;
        ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ADD 元方法 - 字符串连接
static TZrInt64 meta_add_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_STRING(other->type)) {
        SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
        SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
        TZrNativeString native1 = ZrCore_String_GetNativeString(str1);
        TZrNativeString native2 = ZrCore_String_GetNativeString(str2);

        TZrSize len1 = get_string_length(str1);
        TZrSize len2 = get_string_length(str2);
        TZrSize totalLen = len1 + len2;

        char *buffer = (char *) malloc(totalLen + 1);
        if (buffer != ZR_NULL) {
            memcpy(buffer, native1, len1);
            memcpy(buffer + len1, native2, len2);
            buffer[totalLen] = '\0';

            SZrString *result = ZrCore_String_CreateFromNative(state, buffer);
            free(buffer);
            ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 整数减法
static TZrInt64 meta_sub_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TZrInt64 result = self->value.nativeObject.nativeInt64 - other->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 无符号整数减法
static TZrInt64 meta_sub_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TZrUInt64 result = self->value.nativeObject.nativeUInt64 - other->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 浮点数减法
static TZrInt64 meta_sub_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TZrFloat64 result = self->value.nativeObject.nativeDouble - other->value.nativeObject.nativeDouble;
        ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 整数乘法
static TZrInt64 meta_mul_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TZrInt64 result = self->value.nativeObject.nativeInt64 * other->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 无符号整数乘法
static TZrInt64 meta_mul_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TZrUInt64 result = self->value.nativeObject.nativeUInt64 * other->value.nativeObject.nativeUInt64;
        ZrCore_Value_InitAsUInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 浮点数乘法
static TZrInt64 meta_mul_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TZrFloat64 result = self->value.nativeObject.nativeDouble * other->value.nativeObject.nativeDouble;
        ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// DIV 元方法 - 整数除法
static TZrInt64 meta_div_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TZrInt64 divisor = other->value.nativeObject.nativeInt64;
        if (divisor == 0) {
            // 除零错误，返回 null
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        } else {
            TZrInt64 result = self->value.nativeObject.nativeInt64 / divisor;
            ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// DIV 元方法 - 无符号整数除法
static TZrInt64 meta_div_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TZrUInt64 divisor = other->value.nativeObject.nativeUInt64;
        if (divisor == 0) {
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        } else {
            TZrUInt64 result = self->value.nativeObject.nativeUInt64 / divisor;
            ZrCore_Value_InitAsUInt(state, ZrCore_Stack_GetValue(base), result);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// DIV 元方法 - 浮点数除法
static TZrInt64 meta_div_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TZrFloat64 divisor = other->value.nativeObject.nativeDouble;
        if (divisor == 0.0) {
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
        } else {
            TZrFloat64 result = self->value.nativeObject.nativeDouble / divisor;
            ZrCore_Value_InitAsFloat(state, ZrCore_Stack_GetValue(base), result);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 整数比较
static TZrInt64 meta_compare_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TZrInt64 diff = self->value.nativeObject.nativeInt64 - other->value.nativeObject.nativeInt64;
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 无符号整数比较
static TZrInt64 meta_compare_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TZrInt64 diff = (TZrInt64) (self->value.nativeObject.nativeUInt64 - other->value.nativeObject.nativeUInt64);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 浮点数比较
static TZrInt64 meta_compare_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TZrFloat64 diff = self->value.nativeObject.nativeDouble - other->value.nativeObject.nativeDouble;
        TZrInt64 result = 0;
        if (diff > 0.0) {
            result = 1;
        } else if (diff < 0.0) {
            result = -1;
        }
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 字符串比较（字典序）
static TZrInt64 meta_compare_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_STRING(other->type)) {
        SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
        SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
        TZrNativeString native1 = ZrCore_String_GetNativeString(str1);
        TZrNativeString native2 = ZrCore_String_GetNativeString(str2);

        TZrInt32 diff = ZrCore_NativeString_Compare(native1, native2);
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), (TZrInt64) diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 布尔比较（true 为 1，false 为 0）
static TZrInt64 meta_compare_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TZrInt64 val1 = self->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        TZrInt64 val2 = other->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
        TZrInt64 diff = val1 - val2;
        ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(base), diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 字符串减整数或字符串（按参数类型删除末尾字符或删除匹配子串）
static TZrInt64 meta_sub_string_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type)) {
        if (ZR_VALUE_IS_TYPE_STRING(other->type)) {
            // 字符串减字符串（删除匹配字符串）
            SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
            SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
            TZrNativeString native1 = ZrCore_String_GetNativeString(str1);
            TZrNativeString native2 = ZrCore_String_GetNativeString(str2);

            // 简单的字符串替换：删除第一次出现的匹配
            TZrNativeString pos = strstr(native1, native2);
            if (pos != ZR_NULL) {
                TZrSize len1 = get_string_length(str1);
                TZrSize len2 = get_string_length(str2);
                TZrSize posOffset = pos - native1;
                TZrSize newLength = len1 - len2;

                char *buffer = (char *) malloc(newLength + 1);
                if (buffer != ZR_NULL) {
                    memcpy(buffer, native1, posOffset);
                    memcpy(buffer + posOffset, pos + len2, len1 - posOffset - len2);
                    buffer[newLength] = '\0';
                    SZrString *result = ZrCore_String_CreateFromNative(state, buffer);
                    free(buffer);
                    ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                    state->stackTop.valuePointer = base + 1;
                    return 1;
                }
            } else {
                // 没有匹配，返回原字符串
                ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), self);
                state->stackTop.valuePointer = base + 1;
                return 1;
            }
        } else if (ZR_VALUE_IS_TYPE_INT(other->type)) {
            // 字符串减整数（删除末尾字符）
            SZrString *str = ZR_CAST_STRING(state, self->value.object);
            TZrInt64 count = other->value.nativeObject.nativeInt64;
            TZrSize length = get_string_length(str);

            if (count < 0) {
                count = 0;
            }
            if ((TZrUInt64) count > length) {
                count = (TZrInt64) length;
            }

            TZrSize newLength = length - (TZrSize) count;
            TZrNativeString nativeStr = ZrCore_String_GetNativeString(str);

            char *buffer = (char *) malloc(newLength + 1);
            if (buffer != ZR_NULL) {
                memcpy(buffer, nativeStr, newLength);
                buffer[newLength] = '\0';
                SZrString *result = ZrCore_String_CreateFromNative(state, buffer);
                free(buffer);
                ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                state->stackTop.valuePointer = base + 1;
                return 1;
            }
        }
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 字符串乘整数（复制相同字符串）
static TZrInt64 meta_mul_string_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        SZrString *str = ZR_CAST_STRING(state, self->value.object);
        TZrInt64 count = other->value.nativeObject.nativeInt64;
        TZrSize length = get_string_length(str);

        if (count <= 0) {
            // 返回空字符串
            SZrString *result = ZrCore_String_CreateFromNative(state, "");
            ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
            state->stackTop.valuePointer = base + 1;
            return 1;
        }

        TZrSize totalLength = length * (TZrSize) count;
        TZrNativeString nativeStr = ZrCore_String_GetNativeString(str);

        char *buffer = (char *) malloc(totalLength + 1);
        if (buffer != ZR_NULL) {
            for (TZrInt64 i = 0; i < count; i++) {
                memcpy(buffer + i * length, nativeStr, length);
            }
            buffer[totalLength] = '\0';
            SZrString *result = ZrCore_String_CreateFromNative(state, buffer);
            free(buffer);
            ZrCore_Value_InitAsRawObject(state, ZrCore_Stack_GetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔运算元方法

// ADD 元方法 - 布尔按位或（逻辑或）
static TZrInt64 meta_add_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TZrBool result = self->value.nativeObject.nativeBool || other->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 布尔按位与（逻辑与）
static TZrInt64 meta_sub_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TZrBool result = self->value.nativeObject.nativeBool && other->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 布尔按位异或
static TZrInt64 meta_mul_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);
    SZrTypeValue *other = ZrCore_Stack_GetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TZrBool result = self->value.nativeObject.nativeBool != other->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// NEG 元方法 - 布尔非
static TZrInt64 meta_neg_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrCore_Stack_GetValue(base + 1);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type)) {
        TZrBool result = !self->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrCore_Stack_GetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// OBJECT 未实现元方法的默认处理（抛出错误）
static TZrInt64 meta_object_not_implemented(SZrState *state) {
    // 抛出未实现元方法异常
    // 获取元方法类型（从调用栈中获取）
    SZrCallInfo *callInfo = state->callInfoList;
    if (callInfo != ZR_NULL) {
        // 抛出运行时错误：未实现的元方法
        // 注意：这里使用运行时错误状态，表示元方法未实现
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    }
    // 如果无法抛出异常，返回null
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ==================== Meta Method Registration Functions ====================

// 注册单个元方法到指定类型的原型
static void meta_register_meta_method(SZrState *state, EZrValueType valueType, EZrMetaType metaType,
                                     FZrNativeFunction nativeFunction) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }

    SZrObjectPrototype *prototype = state->global->basicTypeObjectPrototype[valueType];
    if (prototype == ZR_NULL) {
        return;
    }

    // 如果已经存在元方法，跳过
    if (prototype->metaTable.metas[metaType] != ZR_NULL) {
        return;
    }

    // 创建 native 闭包
    SZrClosureNative *closure = ZrCore_ClosureNative_New(state, 0);
    if (closure == ZR_NULL) {
        return;
    }

    closure->nativeFunction = nativeFunction;

    // 创建 Meta 对象（使用 zr 的内存分配器）
    SZrGlobalState *global = state->global;
    SZrMeta *meta = (SZrMeta *) global->allocator(global->userAllocationArguments, ZR_NULL, 0, sizeof(SZrMeta),
                                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (meta == ZR_NULL) {
        return;
    }

    meta->metaType = metaType;
    // 注意：SZrMeta 的 function 字段是 SZrFunction*，但我们需要存储 native 闭包
    // SZrClosureNative 和 SZrFunction 都继承自 SZrRawObject，类型兼容
    // 在调用时会根据 isNative 标志进行正确的类型转换
    meta->function = ZR_CAST(SZrFunction *, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    // 注册到原型
    prototype->metaTable.metas[metaType] = meta;

    // 标记为永久对象（避免被 GC 回收）
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
}

// 为指定类型初始化所有默认元方法
void ZrCore_Meta_InitBuiltinTypeMetaMethods(SZrState *state, EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_NULL: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_null);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_null);
        } break;

        case ZR_VALUE_TYPE_BOOL: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_bool);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_bool);
            meta_register_meta_method(state, valueType, ZR_META_TO_INT, meta_to_int_bool);
            meta_register_meta_method(state, valueType, ZR_META_TO_UINT, meta_to_uint_bool);
            meta_register_meta_method(state, valueType, ZR_META_TO_FLOAT, meta_to_float_bool);
            meta_register_meta_method(state, valueType, ZR_META_ADD, meta_add_bool);
            meta_register_meta_method(state, valueType, ZR_META_SUB, meta_sub_bool);
            meta_register_meta_method(state, valueType, ZR_META_MUL, meta_mul_bool);
            meta_register_meta_method(state, valueType, ZR_META_NEG, meta_neg_bool);
            meta_register_meta_method(state, valueType, ZR_META_COMPARE, meta_compare_bool);
        } break;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_INT, meta_to_int_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_UINT, meta_to_uint_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_FLOAT, meta_to_float_number);
            meta_register_meta_method(state, valueType, ZR_META_ADD, meta_add_int);
            meta_register_meta_method(state, valueType, ZR_META_SUB, meta_sub_int);
            meta_register_meta_method(state, valueType, ZR_META_MUL, meta_mul_int);
            meta_register_meta_method(state, valueType, ZR_META_DIV, meta_div_int);
            meta_register_meta_method(state, valueType, ZR_META_COMPARE, meta_compare_int);
        } break;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_INT, meta_to_int_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_UINT, meta_to_uint_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_FLOAT, meta_to_float_number);
            meta_register_meta_method(state, valueType, ZR_META_ADD, meta_add_uint);
            meta_register_meta_method(state, valueType, ZR_META_SUB, meta_sub_uint);
            meta_register_meta_method(state, valueType, ZR_META_MUL, meta_mul_uint);
            meta_register_meta_method(state, valueType, ZR_META_DIV, meta_div_uint);
            meta_register_meta_method(state, valueType, ZR_META_COMPARE, meta_compare_uint);
        } break;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_INT, meta_to_int_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_UINT, meta_to_uint_number);
            meta_register_meta_method(state, valueType, ZR_META_TO_FLOAT, meta_to_float_number);
            meta_register_meta_method(state, valueType, ZR_META_ADD, meta_add_float);
            meta_register_meta_method(state, valueType, ZR_META_SUB, meta_sub_float);
            meta_register_meta_method(state, valueType, ZR_META_MUL, meta_mul_float);
            meta_register_meta_method(state, valueType, ZR_META_DIV, meta_div_float);
            meta_register_meta_method(state, valueType, ZR_META_COMPARE, meta_compare_float);
        } break;

        case ZR_VALUE_TYPE_STRING: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_string);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_string);
            meta_register_meta_method(state, valueType, ZR_META_ADD, meta_add_string);
            meta_register_meta_method(state, valueType, ZR_META_SUB, meta_sub_string_int);
            meta_register_meta_method(state, valueType, ZR_META_MUL, meta_mul_string_int);
            meta_register_meta_method(state, valueType, ZR_META_COMPARE, meta_compare_string);
            // 字符串 SUB 统一走 meta_sub_string_int，并在运行时根据参数类型处理 string/string 与 string/int。
        } break;

        case ZR_VALUE_TYPE_OBJECT: {
            meta_register_meta_method(state, valueType, ZR_META_TO_STRING, meta_to_string_object);
            meta_register_meta_method(state, valueType, ZR_META_TO_BOOL, meta_to_bool_object);
            // 其他元方法未实现时使用默认处理
            // 为每个未实现的元方法注册 meta_object_not_implemented
            // 遍历所有元方法类型，为未注册的元方法注册默认处理函数
            for (EZrMetaType metaType = 0; metaType < ZR_META_ENUM_MAX; metaType++) {
                // 跳过已注册的元方法
                if (metaType == ZR_META_TO_STRING || metaType == ZR_META_TO_BOOL) {
                    continue;
                }
                // 为未实现的元方法注册默认处理函数
                meta_register_meta_method(state, valueType, metaType, meta_object_not_implemented);
            }
        } break;

        default:
            break;
    }
}
