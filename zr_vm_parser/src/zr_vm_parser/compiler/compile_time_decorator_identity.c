#include "compile_time_decorator_identity.h"

/**
 * @brief 从编译期 declaration/patch 对象读取内部字段，供身份关联及形状验证使用。
 * @return 返回对象持有的借用值；对象或字段查找失败时返回 null。
 */
static const SZrTypeValue *decorator_identity_get_field(
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

/** @brief 把 semantic symbol id 写入 decorator snapshot，供 typed Patch 回指原声明。 */
static TZrBool decorator_identity_set_uint_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name,
        TZrUInt64 value) {
    SZrString *keyString;
    SZrTypeValue key;
    SZrTypeValue fieldValue;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsUInt(cs->state, &fieldValue, value);
    // BUG: Object_SetValue 返回 void；对象存储条目分配失败只记 core error，本函数仍会把未写入报告为成功。
    ZrCore_Object_SetValue(cs->state, object, &key, &fieldValue);
    return ZR_TRUE;
}

/** @brief 读取非负整数身份并拒绝负值，避免其转换成看似有效的大 unsigned id。 */
static TZrBool decorator_identity_read_uint(
        const SZrTypeValue *value,
        TZrUInt64 *outValue) {
    if (value == ZR_NULL || outValue == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }
    *outValue = (TZrUInt64)value->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

/** @brief 确认 leaf Patch 的成员/参数扩展集合确实为空，避免跨越此变换边界。 */
static TZrBool decorator_identity_array_is_empty(
        SZrCompilerState *cs,
        SZrObject *patch,
        const TZrChar *fieldName) {
    const SZrTypeValue *value = decorator_identity_get_field(cs, patch, fieldName);
    SZrObject *array;

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_ARRAY ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    array = ZR_CAST_OBJECT(cs->state, value->value.object);
    return array != ZR_NULL &&
           ZrCore_Object_SuperArrayLength(array) == 0U;
}

/**
 * @brief 为 function/member/parameter snapshot 复用或登记对应 AST 的 semantic symbol id。
 * @note 按 AST 节点与 symbol kind 复用，确保 Patch.target 指向本轮声明身份而非同名声明。
 * @return symbol 注册和 setter 调用完成时 true；无 semantic context 或注册失败时 false。
 * BUG: setter 依赖无返回值的 Object_SetValue；对象存储条目分配失败时可能没有写入 symbolId 却仍返回 true。
 */
TZrBool ZrParser_CompileTime_EnsureDecoratorSnapshotSymbol(
        SZrCompilerState *cs,
        SZrObject *snapshot,
        SZrAstNode *declarationNode,
        SZrString *name,
        EZrSemanticSymbolKind kind,
        TZrTypeId typeId,
        TZrSymbolId *outSymbolId) {
    TZrSymbolId symbolId = ZR_SEMANTIC_ID_INVALID;

    if (outSymbolId != ZR_NULL) {
        *outSymbolId = ZR_SEMANTIC_ID_INVALID;
    }
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || snapshot == ZR_NULL ||
        declarationNode == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0U; index < cs->semanticContext->symbols.length; index++) {
        const SZrSemanticSymbolRecord *record =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        &cs->semanticContext->symbols, index);
        if (record != ZR_NULL && record->astNode == declarationNode &&
            record->kind == kind) {
            symbolId = record->id;
            break;
        }
    }
    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
        symbolId = ZrParser_Semantic_RegisterSymbol(
                cs->semanticContext,
                name,
                kind,
                typeId,
                ZR_SEMANTIC_ID_INVALID,
                declarationNode,
                declarationNode->location);
    }
    if (symbolId == ZR_SEMANTIC_ID_INVALID ||
        !decorator_identity_set_uint_field(
                cs, snapshot, "symbolId", symbolId)) {
        return ZR_FALSE;
    }
    if (outSymbolId != ZR_NULL) {
        *outSymbolId = symbolId;
    }
    return ZR_TRUE;
}

/**
 * @brief 区分 typed declaration.Patch 与普通对象，并验证目标 id 及 leaf 变换允许的空扩展集合。
 * @note 普通对象作为未类型化结果返回 true 且 outIsTypedPatch=false；调用层负责按变换种类拒绝它。
 * @return malformed typed Patch 会记录 compile-time diagnostic 并返回 false。
 */
TZrBool ZrParser_CompileTime_ValidateLeafDeclarationPatch(
        SZrCompilerState *cs,
        const SZrTypeValue *targetSnapshot,
        const SZrTypeValue *patchValue,
        SZrFileRange location,
        TZrBool *outIsTypedPatch) {
    /* leaf transform 在此阶段不得添加成员、接口、属性或诊断项。 */
    static const TZrChar *const arrayFields[] = {
            "additions", "interfaceAdds", "attributeAdds", "diagnostics"};
    SZrObject *targetObject;
    SZrObject *patchObject;
    const SZrTypeValue *roleValue;
    TZrUInt64 role;
    TZrUInt64 targetSymbolId;
    TZrUInt64 patchTargetSymbolId;

    if (outIsTypedPatch != ZR_NULL) {
        *outIsTypedPatch = ZR_FALSE;
    }
    if (cs == ZR_NULL || targetSnapshot == ZR_NULL || patchValue == ZR_NULL ||
        targetSnapshot->type != ZR_VALUE_TYPE_OBJECT ||
        targetSnapshot->value.object == ZR_NULL ||
        patchValue->type != ZR_VALUE_TYPE_OBJECT ||
        patchValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    targetObject = ZR_CAST_OBJECT(cs->state, targetSnapshot->value.object);
    patchObject = ZR_CAST_OBJECT(cs->state, patchValue->value.object);
    roleValue = decorator_identity_get_field(
            cs, patchObject, "__zrCompileToolTypeRole");
    if (roleValue == ZR_NULL) {
        return ZR_TRUE;
    }
    if (outIsTypedPatch != ZR_NULL) {
        *outIsTypedPatch = ZR_TRUE;
    }
    if (!decorator_identity_read_uint(roleValue, &role) ||
        role != ZR_PARSER_COMPILE_TOOL_TYPE_PATCH ||
        patchObject == ZR_NULL || !patchObject->nodeMap.isValid ||
        patchObject->nodeMap.elementCount != 6U ||
        !decorator_identity_read_uint(
                decorator_identity_get_field(cs, targetObject, "symbolId"),
                &targetSymbolId) ||
        !decorator_identity_read_uint(
                decorator_identity_get_field(cs, patchObject, "target"),
                &patchTargetSymbolId) ||
        targetSymbolId == ZR_SEMANTIC_ID_INVALID ||
        patchTargetSymbolId != targetSymbolId) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.leaf_patch: typed Patch target must match the declaration view",
                location);
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < ZR_ARRAY_COUNT(arrayFields); index++) {
        if (!decorator_identity_array_is_empty(
                    cs, patchObject, arrayFields[index])) {
            ZrParser_CompileTime_Error(
                    cs,
                    ZR_COMPILE_TIME_ERROR_ERROR,
                    "declaration_transform.leaf_patch: member and parameter Patch collections must be empty",
                    location);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
