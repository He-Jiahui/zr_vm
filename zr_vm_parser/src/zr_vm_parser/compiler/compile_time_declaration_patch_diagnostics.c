#include "compile_time_declaration_patch_diagnostics.h"

#include "compile_time_executor_internal.h"
#include "comptime_runtime_contract.h"
#include "zr_vm_parser/declaration_transform_contract.h"

/**
 * @brief 按字段名读取运行时诊断对象中的一个属性值。
 *
 * 仅供严格解码器查询已知的四个 schema 字段；返回值仍由原诊断对象持有。
 */
static const SZrTypeValue *patch_diagnostic_get_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name) {
    SZrString *keyString;
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    keyString = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(cs->state, object, &key);
}

/**
 * @brief 把非负整数值规范化为无符号宽整数，供角色号和符号 ID 检查复用。
 */
static TZrBool patch_diagnostic_read_uint64(
        const SZrTypeValue *value,
        TZrUInt64 *result) {
    if (value == ZR_NULL || result == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(value->type) ||
        (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type) &&
         value->value.nativeObject.nativeInt64 < 0)) {
        return ZR_FALSE;
    }
    *result = ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)
                      ? value->value.nativeObject.nativeUInt64
                      : (TZrUInt64)value->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

/**
 * @brief 将诊断 target 限定为可表示的 32 位语义符号 ID。
 */
static TZrBool patch_diagnostic_read_symbol_id(
        const SZrTypeValue *value,
        TZrSymbolId *symbolId) {
    TZrUInt64 rawValue;

    if (symbolId == ZR_NULL ||
        !patch_diagnostic_read_uint64(value, &rawValue)) {
        return ZR_FALSE;
    }
    if (rawValue > (TZrUInt64)UINT32_MAX) {
        return ZR_FALSE;
    }
    *symbolId = (TZrSymbolId)rawValue;
    return ZR_TRUE;
}

/**
 * @brief 拒绝 typed CompileDiagnostic 中 schema 未声明的额外键。
 *
 * 该检查与后续字段类型验证共同保证运行时对象只能携带受支持的诊断数据。
 */
static TZrBool patch_diagnostic_field_is_allowed(
        SZrState *state,
        const SZrTypeValue *key) {
    SZrString *name;

    if (state == ZR_NULL || key == ZR_NULL ||
        key->type != ZR_VALUE_TYPE_STRING || key->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    name = ZR_CAST_STRING(state, key->value.object);
    return (TZrBool)(ct_string_equals(name, "isError") ||
                     ct_string_equals(name, "message") ||
                     ct_string_equals(name, "target") ||
                     ct_string_equals(name, "__zrCompileToolTypeRole"));
}

/**
 * @brief 将 role-marked 的运行时对象解码为供 declaration patch contract 验证的 C 记录。
 *
 * 本函数不发布错误/警告；所有字段和键先整体验证，调用方再统一验证 target 与消息。
 */
static TZrBool patch_diagnostic_decode(
        SZrCompilerState *cs,
        const SZrTypeValue *value,
        SZrParserCompileDiagnostic *diagnostic) {
    SZrObject *object;
    const SZrTypeValue *roleValue;
    const SZrTypeValue *isErrorValue;
    const SZrTypeValue *messageValue;
    const SZrTypeValue *targetValue;
    TZrSymbolId targetSymbolId;
    TZrUInt64 role;

    if (cs == ZR_NULL || value == ZR_NULL || diagnostic == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    object = ZR_CAST_OBJECT(cs->state, value->value.object);
    /* 固定字段数并检查每个键，阻止额外属性绕过 typed diagnostic schema。 */
    if (object == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.elementCount != 4U) {
        return ZR_FALSE;
    }
    for (TZrSize bucketIndex = 0; bucketIndex < object->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (!patch_diagnostic_field_is_allowed(cs->state, &pair->key)) {
                return ZR_FALSE;
            }
            pair = pair->next;
        }
    }

    /* role 是内部类型身份标记；其余字段仍需逐项校验运行时值类型与范围。 */
    roleValue = patch_diagnostic_get_field(cs, object, "__zrCompileToolTypeRole");
    isErrorValue = patch_diagnostic_get_field(cs, object, "isError");
    messageValue = patch_diagnostic_get_field(cs, object, "message");
    targetValue = patch_diagnostic_get_field(cs, object, "target");
    if (!patch_diagnostic_read_uint64(roleValue, &role) ||
        role != ZR_PARSER_COMPILE_TOOL_TYPE_DIAGNOSTIC ||
        isErrorValue == ZR_NULL || isErrorValue->type != ZR_VALUE_TYPE_BOOL ||
        messageValue == ZR_NULL || messageValue->type != ZR_VALUE_TYPE_STRING ||
        messageValue->value.object == ZR_NULL ||
        !patch_diagnostic_read_symbol_id(targetValue, &targetSymbolId)) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->isError = isErrorValue->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    diagnostic->message = ZrCore_String_GetNativeString(
            ZR_CAST_STRING(cs->state, messageValue->value.object));
    diagnostic->targetSymbolId = targetSymbolId;
    return ZR_TRUE;
}

/**
 * @brief 以脚本数组索引读取一项诊断，维持 VM 数组取值的通用对象路径。
 */
static const SZrTypeValue *patch_diagnostic_array_at(
        SZrCompilerState *cs,
        const SZrTypeValue *arrayValue,
        TZrSize index) {
    SZrObject *array;
    SZrTypeValue key;

    if (cs == ZR_NULL || arrayValue == ZR_NULL ||
        arrayValue->type != ZR_VALUE_TYPE_ARRAY ||
        arrayValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZR_CAST_OBJECT(cs->state, arrayValue->value.object);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(cs->state, array, &key);
}

/**
 * @brief 验证并发布声明变换 patch 携带的编译期诊断。
 *
 * 唯一生产调用者在原子 patch commit 前运行本阶段：warning 保留并允许提交，
 * error 同样发布给用户但通过 hasErrorDiagnostic 阻止调用者提交声明改动。
 * 所有条目先完成解码和 patch contract 验证，再发出任一诊断，避免坏条目留下部分输出。
 * @pre cs 已初始化，patchTargetSymbolId 是目标声明的有效 ID，输出标记可写。
 * @return 输入全为有效 typed diagnostics 时返回真；hasErrorDiagnostic 独立表示是否有
 *         error 严重级别，故返回真不代表调用者可以提交 patch。
 * @note TODO: 数组物化或记录缓冲区分配失败会直接返回 false；需确认上层必定给出用户可见诊断。
 */
TZrBool ZrParser_CompileTime_ProcessPatchDiagnostics(
        SZrCompilerState *cs,
        const SZrTypeValue *diagnosticsValue,
        TZrSymbolId patchTargetSymbolId,
        SZrFileRange location,
        TZrBool *hasErrorDiagnostic) {
    SZrObject *array;
    SZrParserCompileDiagnostic *diagnostics = ZR_NULL;
    SZrParserDeclarationView view;
    SZrParserDeclarationPatch patch;
    TZrSize diagnosticCount;
    TZrBool result = ZR_FALSE;

    /* 不接受非数组、无效 target 或不可写输出；参数错误发生在输出标志初始化之前。 */
    if (cs == ZR_NULL || diagnosticsValue == ZR_NULL ||
        diagnosticsValue->type != ZR_VALUE_TYPE_ARRAY ||
        diagnosticsValue->value.object == ZR_NULL ||
        patchTargetSymbolId == ZR_SEMANTIC_ID_INVALID ||
        hasErrorDiagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    *hasErrorDiagnostic = ZR_FALSE;
    array = ZR_CAST_OBJECT(cs->state, diagnosticsValue->value.object);
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(cs->state, array)) {
        return ZR_FALSE;
    }
    /* 先具象化通用数组视图；空诊断列表是合法的无副作用结果。 */
    diagnosticCount = ZrCore_Object_SuperArrayLength(array);
    if (diagnosticCount == 0U) {
        return ZR_TRUE;
    }
    /* 在乘法分配前检查尺寸，并在解码前消费有界诊断预算。 */
    if (diagnosticCount > (TZrSize)(SIZE_MAX / sizeof(*diagnostics))) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.diagnostic: diagnostic array size overflow",
                location);
        return ZR_FALSE;
    }
    if (!ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_DIAGNOSTIC_COUNT,
                diagnosticCount,
                location)) {
        return ZR_FALSE;
    }

    /* 先暂存并验证整批记录；只有完整合法后才允许向编译器输出消息。 */
    diagnostics = (SZrParserCompileDiagnostic *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            diagnosticCount * sizeof(*diagnostics),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (diagnostics == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(diagnostics, 0, diagnosticCount * sizeof(*diagnostics));
    /* 把诊断 target 绑定到同一个 patch target，并复用统一 patch contract 校验消息。 */
    for (TZrSize index = 0; index < diagnosticCount; index++) {
        if (!patch_diagnostic_decode(
                    cs,
                    patch_diagnostic_array_at(cs, diagnosticsValue, index),
                    &diagnostics[index])) {
            ZrParser_CompileTime_Error(
                    cs,
                    ZR_COMPILE_TIME_ERROR_ERROR,
                    "declaration_transform.diagnostic: invalid typed CompileDiagnostic",
                    location);
            goto cleanup;
        }
    }
    ZrCore_Memory_RawSet(&view, 0, sizeof(view));
    ZrCore_Memory_RawSet(&patch, 0, sizeof(patch));
    view.symbolId = patchTargetSymbolId;
    patch.targetSymbolId = patchTargetSymbolId;
    patch.diagnostics = diagnostics;
    patch.diagnosticCount = diagnosticCount;
    if (ZrParser_DeclarationPatch_Validate(&view, &patch) !=
        ZR_PARSER_DECLARATION_PATCH_VALID) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.diagnostic: invalid typed CompileDiagnostic",
                location);
        goto cleanup;
    }
    /* 验证完成后才按输入顺序报告；Error 仍全部报告，但由上游据标志放弃事务提交。 */
    for (TZrSize index = 0; index < diagnosticCount; index++) {
        ZrParser_CompileTime_Error(
                cs,
                diagnostics[index].isError
                        ? ZR_COMPILE_TIME_ERROR_ERROR
                        : ZR_COMPILE_TIME_ERROR_WARNING,
                diagnostics[index].message,
                location);
        if (diagnostics[index].isError) {
            *hasErrorDiagnostic = ZR_TRUE;
        }
    }
    result = ZR_TRUE;

cleanup:
    /* 本地数组只持有解码后的借用 message 指针，释放的是暂存记录而非脚本文本。 */
    ZrCore_Memory_RawFreeWithType(
            cs->state->global,
            diagnostics,
            diagnosticCount * sizeof(*diagnostics),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return result;
}
