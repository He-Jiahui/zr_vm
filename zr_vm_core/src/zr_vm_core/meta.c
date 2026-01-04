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

void ZrMetaGlobalStaticsInit(SZrState *state) {
    SZrGlobalState *global = state->global;
    for (TEnum i = 0; i < ZR_META_ENUM_MAX; i++) {
        global->metaFunctionName[i] = ZrStringCreateFromNative(state, CZrMetaName[i]);
        ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->metaFunctionName[i]));
    }
}


void ZrMetaTableConstruct(SZrMetaTable *table) {
    for (TEnum i = 0; i < ZR_META_ENUM_MAX; i++) {
        table->metas[i] = ZR_NULL;
    }
}

// ==================== Native Meta Method Functions ====================

// TO_STRING 元方法实现

// NULL 类型转字符串
static TInt64 meta_to_string_null(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    // self 在 base + 1
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    // 返回值放在 base 位置
    SZrString *result = ZrStringCreateFromNative(state, ZR_STRING_NULL_STRING);
    ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// BOOL 类型转字符串
static TInt64 meta_to_string_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrString *result = ZR_NULL;
    if (self->value.nativeObject.nativeBool) {
        result = ZrStringCreateFromNative(state, ZR_STRING_TRUE_STRING);
    } else {
        result = ZrStringCreateFromNative(state, ZR_STRING_FALSE_STRING);
    }
    ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 数字类型转字符串
static TInt64 meta_to_string_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrString *result = ZrStringFromNumber(state, self);
    if (result == ZR_NULL) {
        result = ZrStringCreateFromNative(state, "");
    }
    ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// STRING 类型直接返回
static TInt64 meta_to_string_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    // 直接返回自身
    ZrValueCopy(state, ZrStackGetValue(base), self);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// OBJECT 类型转字符串
static TInt64 meta_to_string_object(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrObject *object = ZR_CAST_OBJECT(state, self->value.object);
    SZrString *result = ZR_NULL;

    // 尝试调用对象的 TO_STRING 元方法（递归查找）
    SZrMeta *meta = ZrObjectGetMetaRecursively(state->global, object, ZR_META_TO_STRING);
    if (meta != ZR_NULL && meta->function != ZR_NULL) {
        // 如果对象有自己的 TO_STRING 元方法，调用它
        // 将 meta->function 放到栈上，self 作为参数
        ZrStackSetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
        ZrStackCopyValue(state, base + 1, self);
        state->stackTop.valuePointer = base + 2;
        // 调用元方法
        ZrFunctionCallWithoutYield(state, base, 1);
        // 返回值在 base 位置
        SZrTypeValue *returnValue = ZrStackGetValue(base);
        if (returnValue->type == ZR_VALUE_TYPE_STRING) {
            ZrValueCopy(state, ZrStackGetValue(base), returnValue);
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    // 默认返回 [object type=X]
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "[object type=%d]", (int) object->internalType);
    result = ZrStringCreateFromNative(state, buffer);
    ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_BOOL 元方法实现

// NULL 转布尔值
static TInt64 meta_to_bool_null(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    // 返回值放在 base 位置
    ZrValueResetAsNull(ZrStackGetValue(base));
    ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, ZR_FALSE, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// BOOL 直接返回
static TInt64 meta_to_bool_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    // 直接返回自身
    ZrValueCopy(state, ZrStackGetValue(base), self);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 数字转布尔值（非零为 true）
static TInt64 meta_to_bool_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TBool result = ZR_FALSE;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = (self->value.nativeObject.nativeInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = (self->value.nativeObject.nativeUInt64 != 0);
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = (self->value.nativeObject.nativeDouble != 0.0);
    }

    ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
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
static TInt64 meta_to_bool_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrString *str = ZR_CAST_STRING(state, self->value.object);
    TZrSize length = get_string_length(str);
    TBool result = (length > 0);

    ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// OBJECT 转布尔值（默认返回 true）
static TInt64 meta_to_bool_object(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrObject *object = ZR_CAST_OBJECT(state, self->value.object);

    // 尝试调用对象的 TO_BOOL 元方法
    SZrMeta *meta = ZrObjectGetMetaRecursively(state->global, object, ZR_META_TO_BOOL);
    if (meta != ZR_NULL && meta->function != ZR_NULL) {
        ZrStackSetRawObjectValue(state, base, ZR_CAST_RAW_OBJECT_AS_SUPER(meta->function));
        ZrStackCopyValue(state, base + 1, self);
        state->stackTop.valuePointer = base + 2;
        ZrFunctionCallWithoutYield(state, base, 1);
        SZrTypeValue *returnValue = ZrStackGetValue(base);
        if (returnValue->type == ZR_VALUE_TYPE_BOOL) {
            ZrValueCopy(state, ZrStackGetValue(base), returnValue);
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    // 默认返回 true
    ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, ZR_TRUE, ZR_VALUE_TYPE_BOOL);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_INT 元方法实现

// 数字转整数（截断）
static TInt64 meta_to_int_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TInt64 result = 0;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = self->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = (TInt64) self->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = (TInt64) self->value.nativeObject.nativeDouble;
    }

    ZrValueInitAsInt(state, ZrStackGetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔转整数
static TInt64 meta_to_int_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TInt64 result = self->value.nativeObject.nativeBool ? 1 : 0;
    ZrValueInitAsInt(state, ZrStackGetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_UINT 元方法实现

// 数字转无符号整数
static TInt64 meta_to_uint_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TUInt64 result = 0;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = (TUInt64) self->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = self->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = (TUInt64) self->value.nativeObject.nativeDouble;
    }

    ZrValueInitAsUInt(state, ZrStackGetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔转无符号整数
static TInt64 meta_to_uint_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TUInt64 result = self->value.nativeObject.nativeBool ? 1 : 0;
    ZrValueInitAsUInt(state, ZrStackGetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// TO_FLOAT 元方法实现

// 数字转浮点数
static TInt64 meta_to_float_number(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TFloat64 result = 0.0;

    if (ZR_VALUE_IS_TYPE_INT(self->type)) {
        result = (TFloat64) self->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type)) {
        result = (TFloat64) self->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(self->type)) {
        result = self->value.nativeObject.nativeDouble;
    }

    ZrValueInitAsFloat(state, ZrStackGetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔转浮点数
static TInt64 meta_to_float_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    TFloat64 result = self->value.nativeObject.nativeBool ? 1.0 : 0.0;
    ZrValueInitAsFloat(state, ZrStackGetValue(base), result);
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 运算元方法实现

// ADD 元方法 - 整数加法
static TInt64 meta_add_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TInt64 result = self->value.nativeObject.nativeInt64 + other->value.nativeObject.nativeInt64;
        ZrValueInitAsInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    // 类型不匹配，返回 null
    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ADD 元方法 - 无符号整数加法
static TInt64 meta_add_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TUInt64 result = self->value.nativeObject.nativeUInt64 + other->value.nativeObject.nativeUInt64;
        ZrValueInitAsUInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ADD 元方法 - 浮点数加法
static TInt64 meta_add_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TFloat64 result = self->value.nativeObject.nativeDouble + other->value.nativeObject.nativeDouble;
        ZrValueInitAsFloat(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ADD 元方法 - 字符串连接
static TInt64 meta_add_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_STRING(other->type)) {
        SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
        SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
        TNativeString native1 = ZrStringGetNativeString(str1);
        TNativeString native2 = ZrStringGetNativeString(str2);

        TZrSize len1 = get_string_length(str1);
        TZrSize len2 = get_string_length(str2);
        TZrSize totalLen = len1 + len2;

        char *buffer = (char *) malloc(totalLen + 1);
        if (buffer != ZR_NULL) {
            memcpy(buffer, native1, len1);
            memcpy(buffer + len1, native2, len2);
            buffer[totalLen] = '\0';

            SZrString *result = ZrStringCreateFromNative(state, buffer);
            free(buffer);
            ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 整数减法
static TInt64 meta_sub_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TInt64 result = self->value.nativeObject.nativeInt64 - other->value.nativeObject.nativeInt64;
        ZrValueInitAsInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 无符号整数减法
static TInt64 meta_sub_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TUInt64 result = self->value.nativeObject.nativeUInt64 - other->value.nativeObject.nativeUInt64;
        ZrValueInitAsUInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 浮点数减法
static TInt64 meta_sub_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TFloat64 result = self->value.nativeObject.nativeDouble - other->value.nativeObject.nativeDouble;
        ZrValueInitAsFloat(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 整数乘法
static TInt64 meta_mul_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TInt64 result = self->value.nativeObject.nativeInt64 * other->value.nativeObject.nativeInt64;
        ZrValueInitAsInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 无符号整数乘法
static TInt64 meta_mul_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TUInt64 result = self->value.nativeObject.nativeUInt64 * other->value.nativeObject.nativeUInt64;
        ZrValueInitAsUInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 浮点数乘法
static TInt64 meta_mul_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TFloat64 result = self->value.nativeObject.nativeDouble * other->value.nativeObject.nativeDouble;
        ZrValueInitAsFloat(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// DIV 元方法 - 整数除法
static TInt64 meta_div_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TInt64 divisor = other->value.nativeObject.nativeInt64;
        if (divisor == 0) {
            // 除零错误，返回 null
            ZrValueResetAsNull(ZrStackGetValue(base));
        } else {
            TInt64 result = self->value.nativeObject.nativeInt64 / divisor;
            ZrValueInitAsInt(state, ZrStackGetValue(base), result);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// DIV 元方法 - 无符号整数除法
static TInt64 meta_div_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TUInt64 divisor = other->value.nativeObject.nativeUInt64;
        if (divisor == 0) {
            ZrValueResetAsNull(ZrStackGetValue(base));
        } else {
            TUInt64 result = self->value.nativeObject.nativeUInt64 / divisor;
            ZrValueInitAsUInt(state, ZrStackGetValue(base), result);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// DIV 元方法 - 浮点数除法
static TInt64 meta_div_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TFloat64 divisor = other->value.nativeObject.nativeDouble;
        if (divisor == 0.0) {
            ZrValueResetAsNull(ZrStackGetValue(base));
        } else {
            TFloat64 result = self->value.nativeObject.nativeDouble / divisor;
            ZrValueInitAsFloat(state, ZrStackGetValue(base), result);
        }
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 整数比较
static TInt64 meta_compare_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_INT(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        TInt64 diff = self->value.nativeObject.nativeInt64 - other->value.nativeObject.nativeInt64;
        ZrValueInitAsInt(state, ZrStackGetValue(base), diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 无符号整数比较
static TInt64 meta_compare_uint(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(self->type) && ZR_VALUE_IS_TYPE_UNSIGNED_INT(other->type)) {
        TInt64 diff = (TInt64) (self->value.nativeObject.nativeUInt64 - other->value.nativeObject.nativeUInt64);
        ZrValueInitAsInt(state, ZrStackGetValue(base), diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 浮点数比较
static TInt64 meta_compare_float(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_FLOAT(self->type) && ZR_VALUE_IS_TYPE_FLOAT(other->type)) {
        TFloat64 diff = self->value.nativeObject.nativeDouble - other->value.nativeObject.nativeDouble;
        TInt64 result = 0;
        if (diff > 0.0) {
            result = 1;
        } else if (diff < 0.0) {
            result = -1;
        }
        ZrValueInitAsInt(state, ZrStackGetValue(base), result);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 字符串比较（字典序）
static TInt64 meta_compare_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_STRING(other->type)) {
        SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
        SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
        TNativeString native1 = ZrStringGetNativeString(str1);
        TNativeString native2 = ZrStringGetNativeString(str2);

        TInt32 diff = ZrNativeStringCompare(native1, native2);
        ZrValueInitAsInt(state, ZrStackGetValue(base), (TInt64) diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// COMPARE 元方法 - 布尔比较（true 为 1，false 为 0）
static TInt64 meta_compare_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TInt64 val1 = self->value.nativeObject.nativeBool ? 1 : 0;
        TInt64 val2 = other->value.nativeObject.nativeBool ? 1 : 0;
        TInt64 diff = val1 - val2;
        ZrValueInitAsInt(state, ZrStackGetValue(base), diff);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 字符串减整数（删除后N个字符）
static TInt64 meta_sub_string_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type)) {
        if (ZR_VALUE_IS_TYPE_STRING(other->type)) {
            // 字符串减字符串（删除匹配字符串）
            SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
            SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
            TNativeString native1 = ZrStringGetNativeString(str1);
            TNativeString native2 = ZrStringGetNativeString(str2);

            // 简单的字符串替换：删除第一次出现的匹配
            TNativeString pos = strstr(native1, native2);
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
                    SZrString *result = ZrStringCreateFromNative(state, buffer);
                    free(buffer);
                    ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                    state->stackTop.valuePointer = base + 1;
                    return 1;
                }
            } else {
                // 没有匹配，返回原字符串
                ZrValueCopy(state, ZrStackGetValue(base), self);
                state->stackTop.valuePointer = base + 1;
                return 1;
            }
        } else if (ZR_VALUE_IS_TYPE_INT(other->type)) {
            // 字符串减整数（删除末尾字符）
            SZrString *str = ZR_CAST_STRING(state, self->value.object);
            TInt64 count = other->value.nativeObject.nativeInt64;
            TZrSize length = get_string_length(str);

            if (count < 0) {
                count = 0;
            }
            if ((TUInt64) count > length) {
                count = (TInt64) length;
            }

            TZrSize newLength = length - (TZrSize) count;
            TNativeString nativeStr = ZrStringGetNativeString(str);

            char *buffer = (char *) malloc(newLength + 1);
            if (buffer != ZR_NULL) {
                memcpy(buffer, nativeStr, newLength);
                buffer[newLength] = '\0';
                SZrString *result = ZrStringCreateFromNative(state, buffer);
                free(buffer);
                ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                state->stackTop.valuePointer = base + 1;
                return 1;
            }
        }
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 字符串减字符串（删除匹配字符串）
static TInt64 meta_sub_string_string(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_STRING(other->type)) {
        SZrString *str1 = ZR_CAST_STRING(state, self->value.object);
        SZrString *str2 = ZR_CAST_STRING(state, other->value.object);
        TNativeString native1 = ZrStringGetNativeString(str1);
        TNativeString native2 = ZrStringGetNativeString(str2);

        // 简单的字符串替换：删除第一次出现的匹配
        TNativeString pos = strstr(native1, native2);
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
                SZrString *result = ZrStringCreateFromNative(state, buffer);
                free(buffer);
                ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
                state->stackTop.valuePointer = base + 1;
                return 1;
            }
        } else {
            // 没有匹配，返回原字符串
            ZrValueCopy(state, ZrStackGetValue(base), self);
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 字符串乘整数（复制相同字符串）
static TInt64 meta_mul_string_int(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_STRING(self->type) && ZR_VALUE_IS_TYPE_INT(other->type)) {
        SZrString *str = ZR_CAST_STRING(state, self->value.object);
        TInt64 count = other->value.nativeObject.nativeInt64;
        TZrSize length = get_string_length(str);

        if (count <= 0) {
            // 返回空字符串
            SZrString *result = ZrStringCreateFromNative(state, "");
            ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
            state->stackTop.valuePointer = base + 1;
            return 1;
        }

        TZrSize totalLength = length * (TZrSize) count;
        TNativeString nativeStr = ZrStringGetNativeString(str);

        char *buffer = (char *) malloc(totalLength + 1);
        if (buffer != ZR_NULL) {
            for (TInt64 i = 0; i < count; i++) {
                memcpy(buffer + i * length, nativeStr, length);
            }
            buffer[totalLength] = '\0';
            SZrString *result = ZrStringCreateFromNative(state, buffer);
            free(buffer);
            ZrValueInitAsRawObject(state, ZrStackGetValue(base), ZR_CAST_RAW_OBJECT_AS_SUPER(result));
            state->stackTop.valuePointer = base + 1;
            return 1;
        }
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// 布尔运算元方法

// ADD 元方法 - 布尔按位或（逻辑或）
static TInt64 meta_add_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TBool result = self->value.nativeObject.nativeBool || other->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// SUB 元方法 - 布尔按位与（逻辑与）
static TInt64 meta_sub_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TBool result = self->value.nativeObject.nativeBool && other->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// MUL 元方法 - 布尔按位异或
static TInt64 meta_mul_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);
    SZrTypeValue *other = ZrStackGetValue(base + 2);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type) && ZR_VALUE_IS_TYPE_BOOL(other->type)) {
        TBool result = self->value.nativeObject.nativeBool != other->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// NEG 元方法 - 布尔非
static TInt64 meta_neg_bool(SZrState *state) {
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    SZrTypeValue *self = ZrStackGetValue(base + 1);

    if (ZR_VALUE_IS_TYPE_BOOL(self->type)) {
        TBool result = !self->value.nativeObject.nativeBool;
        ZR_VALUE_FAST_SET(ZrStackGetValue(base), nativeBool, result, ZR_VALUE_TYPE_BOOL);
        state->stackTop.valuePointer = base + 1;
        return 1;
    }

    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// OBJECT 未实现元方法的默认处理（抛出错误）
static TInt64 meta_object_not_implemented(SZrState *state) {
    // TODO: 抛出未实现元方法异常
    // 目前返回 null
    SZrCallInfo *callInfo = state->callInfoList;
    TZrStackValuePointer base = callInfo->functionBase.valuePointer;
    ZrValueResetAsNull(ZrStackGetValue(base));
    state->stackTop.valuePointer = base + 1;
    return 1;
}

// ==================== Meta Method Registration Functions ====================

// 注册单个元方法到指定类型的原型
static void ZrMetaRegisterMetaMethod(SZrState *state, EZrValueType valueType, EZrMetaType metaType,
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
    SZrClosureNative *closure = ZrClosureNativeNew(state, 0);
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
    ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
}

// 为指定类型初始化所有默认元方法
void ZrMetaInitBuiltinTypeMetaMethods(SZrState *state, EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_NULL: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_null);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_null);
        } break;

        case ZR_VALUE_TYPE_BOOL: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_INT, meta_to_int_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_UINT, meta_to_uint_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_FLOAT, meta_to_float_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_ADD, meta_add_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_SUB, meta_sub_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_MUL, meta_mul_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_NEG, meta_neg_bool);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_COMPARE, meta_compare_bool);
        } break;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_INT, meta_to_int_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_UINT, meta_to_uint_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_FLOAT, meta_to_float_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_ADD, meta_add_int);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_SUB, meta_sub_int);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_MUL, meta_mul_int);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_DIV, meta_div_int);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_COMPARE, meta_compare_int);
        } break;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_INT, meta_to_int_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_UINT, meta_to_uint_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_FLOAT, meta_to_float_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_ADD, meta_add_uint);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_SUB, meta_sub_uint);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_MUL, meta_mul_uint);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_DIV, meta_div_uint);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_COMPARE, meta_compare_uint);
        } break;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_INT, meta_to_int_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_UINT, meta_to_uint_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_FLOAT, meta_to_float_number);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_ADD, meta_add_float);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_SUB, meta_sub_float);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_MUL, meta_mul_float);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_DIV, meta_div_float);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_COMPARE, meta_compare_float);
        } break;

        case ZR_VALUE_TYPE_STRING: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_string);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_string);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_ADD, meta_add_string);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_SUB, meta_sub_string_int);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_MUL, meta_mul_string_int);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_COMPARE, meta_compare_string);
            // 字符串减字符串需要特殊处理，这里先注册减整数的版本
            // TODO: 需要根据参数类型动态选择
        } break;

        case ZR_VALUE_TYPE_OBJECT: {
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_STRING, meta_to_string_object);
            ZrMetaRegisterMetaMethod(state, valueType, ZR_META_TO_BOOL, meta_to_bool_object);
            // 其他元方法未实现时使用默认处理
            // TODO: 为每个未实现的元方法注册 meta_object_not_implemented
        } break;

        default:
            break;
    }
}
