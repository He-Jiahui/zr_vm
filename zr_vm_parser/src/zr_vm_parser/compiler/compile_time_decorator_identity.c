#include "compile_time_decorator_identity.h"

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
    ZrCore_Object_SetValue(cs->state, object, &key, &fieldValue);
    return ZR_TRUE;
}

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

TZrBool ZrParser_CompileTime_ValidateLeafDeclarationPatch(
        SZrCompilerState *cs,
        const SZrTypeValue *targetSnapshot,
        const SZrTypeValue *patchValue,
        SZrFileRange location,
        TZrBool *outIsTypedPatch) {
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
