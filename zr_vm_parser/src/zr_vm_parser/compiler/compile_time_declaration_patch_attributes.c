#include "compile_time_declaration_patch_attributes.h"

#include "zr_vm_core/reflection.h"
#include "zr_vm_parser/declaration_transform_contract.h"

#include <stdio.h>

static TZrBool patch_attribute_error(
        SZrCompilerState *cs,
        const TZrChar *message,
        SZrFileRange location) {
    ZrParser_CompileTime_Error(
            cs, ZR_COMPILE_TIME_ERROR_ERROR, message, location);
    return ZR_FALSE;
}

static const SZrTypeValue *patch_attribute_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name) {
    SZrString *keyString;
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    keyString = ZrCore_String_CreateFromNative(
            cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(cs->state, object, &key);
}

static TZrBool patch_attribute_set_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    ZrExternCompilerTempRoot keyRoot = {0};
    SZrString *keyString = ZR_NULL;
    SZrTypeValue key;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        value == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &keyRoot)) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(
            cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    if (!extern_compiler_temp_root_set_value(&keyRoot, &key)) {
        goto cleanup;
    }
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    result = ZrCore_Object_GetValue(cs->state, object, &key) != ZR_NULL;

cleanup:
    extern_compiler_temp_root_end(&keyRoot);
    return result;
}

static const SZrTypeValue *patch_attribute_array_at(
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

static TZrBool patch_attribute_read_nonnegative_integer(
        const SZrTypeValue *value,
        TZrUInt64 *result) {
    if (value == ZR_NULL || result == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        *result = value->value.nativeObject.nativeUInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeInt64 < 0) {
        return ZR_FALSE;
    }
    *result = (TZrUInt64)value->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static const SZrCompilerAttributeSchemaBinding *patch_attribute_find_schema(
        SZrCompilerState *cs,
        TZrTypeId typeId) {
    if (cs == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        !cs->attributeSchemas.isValid) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < cs->attributeSchemas.length; index++) {
        const SZrCompilerAttributeSchemaBinding *schema =
                (const SZrCompilerAttributeSchemaBinding *)ZrCore_Array_Get(
                        &cs->attributeSchemas, index);
        if (schema != ZR_NULL && schema->typeId == typeId) {
            return schema;
        }
    }
    return ZR_NULL;
}

static TZrSize patch_attribute_metadata_count(
        SZrCompilerState *cs,
        SZrObject *metadataObject,
        TZrUInt32 attributeId) {
    TZrSize count = 0U;
    TZrChar key[64];

    if (cs == ZR_NULL || metadataObject == ZR_NULL) {
        return 0U;
    }
    for (;;) {
        snprintf(key, sizeof(key), "attribute:%08x:%llu",
                 (unsigned int)attributeId,
                 (unsigned long long)count);
        if (patch_attribute_object_field(cs, metadataObject, key) == ZR_NULL) {
            return count;
        }
        count++;
    }
}

static TZrSize patch_attribute_existing_count(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        TZrUInt32 attributeId) {
    SZrObject *metadataObject;

    if (cs == ZR_NULL || targetInfo == ZR_NULL ||
        !targetInfo->hasDecoratorMetadata ||
        targetInfo->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
        targetInfo->decoratorMetadataValue.value.object == ZR_NULL) {
        return 0U;
    }
    metadataObject = ZR_CAST_OBJECT(
            cs->state, targetInfo->decoratorMetadataValue.value.object);
    return patch_attribute_metadata_count(cs, metadataObject, attributeId);
}

static TZrBool patch_attribute_normalize_constant(
        SZrCompilerState *cs,
        const SZrTypeValue *value,
        EZrParserAttributeValueKind expectedKind,
        SZrParserAttributeConstant *constant,
        SZrTypeValue *normalized) {
    SZrReflectionTypeIdentity identity;

    if (cs == ZR_NULL || value == ZR_NULL || constant == ZR_NULL ||
        normalized == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(constant, 0, sizeof(*constant));
    *normalized = *value;
    constant->kind = expectedKind;
    switch (expectedKind) {
        case ZR_PARSER_ATTRIBUTE_VALUE_BOOL:
            if (value->type != ZR_VALUE_TYPE_BOOL) return ZR_FALSE;
            constant->value.boolValue =
                    value->value.nativeObject.nativeBool != 0U
                            ? ZR_TRUE
                            : ZR_FALSE;
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_INT:
        case ZR_PARSER_ATTRIBUTE_VALUE_ENUM:
            if (!ZR_VALUE_IS_TYPE_INT(value->type) ||
                ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) return ZR_FALSE;
            constant->value.intValue = value->value.nativeObject.nativeInt64;
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_UINT:
            if (!ZR_VALUE_IS_TYPE_INT(value->type) ||
                (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type) &&
                 value->value.nativeObject.nativeInt64 < 0)) return ZR_FALSE;
            constant->value.uintValue = ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)
                                                  ? value->value.nativeObject.nativeUInt64
                                                  : (TZrUInt64)value->value.nativeObject.nativeInt64;
            ZrCore_Value_InitAsUInt(
                    cs->state, normalized, constant->value.uintValue);
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_FLOAT:
            if (value->type == ZR_VALUE_TYPE_FLOAT) {
                constant->value.floatValue = value->value.nativeObject.nativeDouble;
            } else if (value->type == ZR_VALUE_TYPE_DOUBLE) {
                constant->value.floatValue = value->value.nativeObject.nativeDouble;
            } else {
                return ZR_FALSE;
            }
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_STRING:
            if (value->type != ZR_VALUE_TYPE_STRING ||
                value->value.object == ZR_NULL) return ZR_FALSE;
            constant->value.stringValue = ZrCore_String_GetNativeStringShort(
                    ZR_CAST_STRING(cs->state, value->value.object));
            return constant->value.stringValue != ZR_NULL;
        case ZR_PARSER_ATTRIBUTE_VALUE_TYPE_ID:
            if (value->type != ZR_VALUE_TYPE_OBJECT ||
                value->value.object == ZR_NULL) return ZR_FALSE;
            ZrCore_Memory_RawSet(&identity, 0, sizeof(identity));
            if (!ZrCore_Reflection_ReadTypeIdObject(
                        cs->state,
                        ZR_CAST_OBJECT(cs->state, value->value.object),
                        &identity,
                        ZR_NULL) ||
                identity.canonicalTypeId == ZR_SEMANTIC_ID_INVALID) {
                return ZR_FALSE;
            }
            constant->value.typeId = identity.canonicalTypeId;
            ZrCore_Value_InitAsUInt(
                    cs->state, normalized, identity.canonicalTypeId);
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool patch_attribute_prepare_entry(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *value,
        const SZrParserCompileTimePatchAttributeAdds *prepared,
        TZrSize preparedCount,
        SZrFileRange location,
        SZrParserCompileTimePatchAttributeAdd *entry) {
    SZrObject *object;
    const SZrTypeValue *roleValue;
    const SZrTypeValue *typeIdValue;
    const SZrTypeValue *fieldValuesValue;
    const SZrTypeValue *sourceLineStartValue;
    const SZrTypeValue *sourceLineEndValue;
    SZrReflectionTypeIdentity identity;
    SZrString *canonicalName = ZR_NULL;
    TZrUInt64 role;
    TZrUInt64 sourceLineStart;
    TZrUInt64 sourceLineEnd;
    TZrSize pendingSameSchema = 0U;
    SZrObject *fieldValuesObject;

    if (cs == ZR_NULL || targetInfo == ZR_NULL || value == ZR_NULL ||
        entry == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: expected typed AttributeData",
                location);
    }
    object = ZR_CAST_OBJECT(cs->state, value->value.object);
    if (object == ZR_NULL || !object->nodeMap.isValid ||
        object->nodeMap.elementCount != 5U) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: invalid typed AttributeData",
                location);
    }
    roleValue = patch_attribute_object_field(
            cs, object, "__zrCompileToolTypeRole");
    typeIdValue = patch_attribute_object_field(cs, object, "typeId");
    fieldValuesValue = patch_attribute_object_field(cs, object, "fieldValues");
    sourceLineStartValue = patch_attribute_object_field(
            cs, object, "__zrSourceLineStart");
    sourceLineEndValue = patch_attribute_object_field(
            cs, object, "__zrSourceLineEnd");
    ZrCore_Memory_RawSet(&identity, 0, sizeof(identity));
    if (!patch_attribute_read_nonnegative_integer(roleValue, &role) ||
        role != ZR_PARSER_COMPILE_TOOL_TYPE_ATTRIBUTE_DATA ||
        typeIdValue == ZR_NULL || typeIdValue->type != ZR_VALUE_TYPE_OBJECT ||
        typeIdValue->value.object == ZR_NULL ||
        !ZrCore_Reflection_ReadTypeIdObject(
                cs->state,
                ZR_CAST_OBJECT(cs->state, typeIdValue->value.object),
                &identity,
                &canonicalName) ||
        identity.canonicalTypeId == ZR_SEMANTIC_ID_INVALID ||
        canonicalName == ZR_NULL ||
        fieldValuesValue == ZR_NULL ||
        fieldValuesValue->type != ZR_VALUE_TYPE_ARRAY ||
        fieldValuesValue->value.object == ZR_NULL ||
        !patch_attribute_read_nonnegative_integer(
                sourceLineStartValue, &sourceLineStart) ||
        !patch_attribute_read_nonnegative_integer(
                sourceLineEndValue, &sourceLineEnd) ||
        sourceLineStart > sourceLineEnd || sourceLineEnd > (TZrUInt64)UINT32_MAX) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: invalid typed AttributeData",
                location);
    }
    entry->schema = patch_attribute_find_schema(cs, identity.canonicalTypeId);
    if (entry->schema == ZR_NULL) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: TypeId must name a registered attribute schema",
                location);
    }
    if ((entry->schema->usage.targets & ZR_PARSER_ATTRIBUTE_TARGET_TYPE) == 0U) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: attribute schema does not target types",
                location);
    }
    fieldValuesObject = ZR_CAST_OBJECT(cs->state, fieldValuesValue->value.object);
    if (fieldValuesObject == ZR_NULL ||
        fieldValuesObject->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(cs->state, fieldValuesObject) ||
        ZrCore_Object_SuperArrayLength(fieldValuesObject) != entry->schema->fields.length) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: fieldValues must match the attribute schema",
                location);
    }
    for (TZrSize index = 0; index < preparedCount; index++) {
        if (prepared->entries[index].schema == entry->schema) {
            pendingSameSchema++;
        }
    }
    if (!entry->schema->usage.repeatable &&
        patch_attribute_existing_count(
                cs, targetInfo, entry->schema->attributeId) + pendingSameSchema > 0U) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: attribute is not repeatable",
                location);
    }

    entry->data.attributeId = entry->schema->attributeId;
    entry->data.typeId = entry->schema->typeId;
    entry->data.role = ZR_PARSER_ATTRIBUTE_ROLE_NONE;
    entry->data.retention = entry->schema->usage.retention;
    entry->data.sourceRange = location;
    entry->data.sourceRange.start.line = (TZrUInt32)sourceLineStart;
    entry->data.sourceRange.end.line = (TZrUInt32)sourceLineEnd;
    entry->data.fieldValueCount = entry->schema->fields.length;
    if (entry->data.fieldValueCount == 0U) {
        return ZR_TRUE;
    }
    entry->data.fieldValues =
            (SZrParserAttributeConstant *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    entry->data.fieldValueCount * sizeof(*entry->data.fieldValues),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    entry->values = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            entry->data.fieldValueCount * sizeof(*entry->values),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (entry->data.fieldValues == ZR_NULL || entry->values == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < entry->data.fieldValueCount; index++) {
        const SZrCompilerAttributeFieldBinding *field =
                (const SZrCompilerAttributeFieldBinding *)ZrCore_Array_Get(
                        (SZrArray *)&entry->schema->fields, index);
        const SZrTypeValue *fieldValue =
                patch_attribute_array_at(cs, fieldValuesValue, index);
        if (field == ZR_NULL || fieldValue == ZR_NULL ||
            !patch_attribute_normalize_constant(
                    cs,
                    fieldValue,
                    field->valueKind,
                    (SZrParserAttributeConstant *)&entry->data.fieldValues[index],
                    &entry->values[index])) {
            return patch_attribute_error(
                    cs,
                    "declaration_transform.attribute_add: field value does not match the attribute schema",
                    location);
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_CompileTime_PreparePatchAttributeAdds(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrTypeValue *attributeAddsValue,
        SZrFileRange location,
        SZrParserCompileTimePatchAttributeAdds *result) {
    SZrObject *array;

    if (cs == ZR_NULL || targetInfo == ZR_NULL ||
        attributeAddsValue == ZR_NULL || result == ZR_NULL ||
        attributeAddsValue->type != ZR_VALUE_TYPE_ARRAY ||
        attributeAddsValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(result, 0, sizeof(*result));
    array = ZR_CAST_OBJECT(cs->state, attributeAddsValue->value.object);
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(cs->state, array)) {
        return ZR_FALSE;
    }
    result->count = ZrCore_Object_SuperArrayLength(array);
    if (result->count == 0U) {
        return ZR_TRUE;
    }
    if (result->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        result->count > (TZrSize)(SIZE_MAX / sizeof(*result->entries))) {
        return patch_attribute_error(
                cs,
                "declaration_transform.attribute_add: attribute addition budget exceeded",
                location);
    }
    result->entries =
            (SZrParserCompileTimePatchAttributeAdd *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    result->count * sizeof(*result->entries),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (result->entries == ZR_NULL) {
        ZrParser_CompileTime_FreePatchAttributeAdds(cs, result);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(
            result->entries, 0, result->count * sizeof(*result->entries));
    result->contractData =
            (SZrParserAttributeData *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    result->count * sizeof(*result->contractData),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (result->contractData == ZR_NULL) {
        ZrParser_CompileTime_FreePatchAttributeAdds(cs, result);
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < result->count; index++) {
        if (!patch_attribute_prepare_entry(
                    cs,
                    targetInfo,
                    patch_attribute_array_at(cs, attributeAddsValue, index),
                    result,
                    index,
                    location,
                    &result->entries[index])) {
            ZrParser_CompileTime_FreePatchAttributeAdds(cs, result);
            return ZR_FALSE;
        }
        result->contractData[index] = result->entries[index].data;
    }
    return ZR_TRUE;
}

static TZrBool patch_attribute_copy_metadata_fields(
        SZrCompilerState *cs,
        SZrObject *target,
        SZrObject *source) {
    if (cs == ZR_NULL || target == ZR_NULL || source == ZR_NULL ||
        !source->nodeMap.isValid || source->nodeMap.buckets == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize bucketIndex = 0;
         bucketIndex < source->nodeMap.capacity;
         bucketIndex++) {
        SZrHashKeyValuePair *pair = source->nodeMap.buckets[bucketIndex];

        while (pair != ZR_NULL) {
            ZrCore_Object_SetValue(
                    cs->state, target, &pair->key, &pair->value);
            if (ZrCore_Object_GetValue(
                        cs->state, target, &pair->key) == ZR_NULL) {
                return ZR_FALSE;
            }
            pair = pair->next;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_CompileTime_BuildPatchAttributeMetadata(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds,
        SZrTypeValue *metadataValue) {
    SZrObject *metadataObject = ZR_NULL;
    SZrObject *existingMetadataObject = ZR_NULL;
    ZrExternCompilerTempRoot existingMetadataRoot = {0};
    ZrExternCompilerTempRoot metadataRoot = {0};
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || targetInfo == ZR_NULL || attributeAdds == ZR_NULL ||
        metadataValue == ZR_NULL ||
        (attributeAdds->count > 0U && attributeAdds->entries == ZR_NULL)) {
        return ZR_FALSE;
    }
    ZrCore_Value_ResetAsNull(metadataValue);
    if (attributeAdds->count == 0U) {
        return ZR_TRUE;
    }
    if (!extern_compiler_temp_root_begin(cs, &existingMetadataRoot) ||
        !extern_compiler_temp_root_begin(cs, &metadataRoot)) {
        goto cleanup;
    }
    if (targetInfo->hasDecoratorMetadata) {
        if (targetInfo->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
            targetInfo->decoratorMetadataValue.value.object == ZR_NULL) {
            goto cleanup;
        }
        existingMetadataObject = ZR_CAST_OBJECT(
                cs->state, targetInfo->decoratorMetadataValue.value.object);
        if (existingMetadataObject == ZR_NULL ||
            !extern_compiler_temp_root_set_value(
                    &existingMetadataRoot,
                    &targetInfo->decoratorMetadataValue)) {
            goto cleanup;
        }
    }
    metadataObject = extern_compiler_new_object_constant(cs);
    if (metadataObject == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &metadataRoot, metadataObject, ZR_VALUE_TYPE_OBJECT)) {
        goto cleanup;
    }
    if (!metadataObject->nodeMap.isValid ||
        metadataObject->nodeMap.buckets == ZR_NULL ||
        (existingMetadataObject != ZR_NULL &&
         !patch_attribute_copy_metadata_fields(
                 cs, metadataObject, existingMetadataObject))) {
        goto cleanup;
    }

    for (TZrSize index = 0; index < attributeAdds->count; index++) {
        const SZrParserCompileTimePatchAttributeAdd *addition =
                &attributeAdds->entries[index];
        SZrObject *entry;
        SZrTypeValue scalar;
        SZrTypeValue entryValue;
        TZrChar key[64];
        TZrSize existingCount;
        ZrExternCompilerTempRoot entryRoot = {0};
        TZrBool entryResult = ZR_FALSE;

        if (addition->schema == ZR_NULL ||
            !extern_compiler_temp_root_begin(cs, &entryRoot)) {
            goto cleanup;
        }
        entry = extern_compiler_new_object_constant(cs);
        if (entry == ZR_NULL ||
            !extern_compiler_temp_root_set_object(
                    &entryRoot, entry, ZR_VALUE_TYPE_OBJECT)) {
            goto entry_cleanup;
        }
        ZrCore_Value_InitAsUInt(cs->state, &scalar, addition->data.attributeId);
        if (!patch_attribute_set_object_field(
                    cs, entry, "attributeId", &scalar)) goto entry_cleanup;
        ZrCore_Value_InitAsUInt(cs->state, &scalar, addition->data.typeId);
        if (!patch_attribute_set_object_field(
                    cs, entry, "typeId", &scalar)) goto entry_cleanup;
        ZrCore_Value_InitAsInt(cs->state, &scalar, addition->data.retention);
        if (!patch_attribute_set_object_field(
                    cs, entry, "retention", &scalar)) goto entry_cleanup;
        ZrCore_Value_InitAsInt(
                cs->state, &scalar, addition->data.sourceRange.start.line);
        if (!patch_attribute_set_object_field(
                    cs, entry, "sourceLineStart", &scalar)) goto entry_cleanup;
        ZrCore_Value_InitAsInt(
                cs->state, &scalar, addition->data.sourceRange.end.line);
        if (!patch_attribute_set_object_field(
                    cs, entry, "sourceLineEnd", &scalar)) goto entry_cleanup;
        for (TZrSize fieldIndex = 0;
             fieldIndex < addition->data.fieldValueCount;
             fieldIndex++) {
            const SZrCompilerAttributeFieldBinding *field =
                    (const SZrCompilerAttributeFieldBinding *)ZrCore_Array_Get(
                            (SZrArray *)&addition->schema->fields, fieldIndex);
            if (field == ZR_NULL || field->name == ZR_NULL ||
                addition->values == ZR_NULL ||
                !patch_attribute_set_object_field(
                        cs,
                        entry,
                        ZrCore_String_GetNativeStringShort(field->name),
                        &addition->values[fieldIndex])) {
                goto entry_cleanup;
            }
        }
        existingCount = patch_attribute_metadata_count(
                cs, metadataObject, addition->data.attributeId);
        snprintf(key, sizeof(key), "attribute:%08x:%llu",
                 (unsigned int)addition->data.attributeId,
                 (unsigned long long)existingCount);
        ZrCore_Value_InitAsRawObject(
                cs->state, &entryValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entry));
        entryValue.type = ZR_VALUE_TYPE_OBJECT;
        if (!patch_attribute_set_object_field(
                    cs, metadataObject, key, &entryValue)) {
            goto entry_cleanup;
        }
        entryResult = ZR_TRUE;

entry_cleanup:
        extern_compiler_temp_root_end(&entryRoot);
        if (!entryResult) {
            goto cleanup;
        }
    }
    ZrCore_Value_InitAsRawObject(
            cs->state,
            metadataValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    metadataValue->type = ZR_VALUE_TYPE_OBJECT;
    result = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&metadataRoot);
    extern_compiler_temp_root_end(&existingMetadataRoot);
    return result;
}

void ZrParser_CompileTime_FreePatchAttributeAdds(
        SZrCompilerState *cs,
        SZrParserCompileTimePatchAttributeAdds *attributeAdds) {
    if (cs == ZR_NULL || attributeAdds == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0;
         attributeAdds->entries != ZR_NULL && index < attributeAdds->count;
         index++) {
        SZrParserCompileTimePatchAttributeAdd *entry =
                &attributeAdds->entries[index];
        if (entry->data.fieldValues != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global,
                    (TZrPtr)entry->data.fieldValues,
                    entry->data.fieldValueCount * sizeof(*entry->data.fieldValues),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (entry->values != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global,
                    entry->values,
                    entry->data.fieldValueCount * sizeof(*entry->values),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
    }
    if (attributeAdds->entries != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                attributeAdds->entries,
                attributeAdds->count * sizeof(*attributeAdds->entries),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (attributeAdds->contractData != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                attributeAdds->contractData,
                attributeAdds->count * sizeof(*attributeAdds->contractData),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    ZrCore_Memory_RawSet(attributeAdds, 0, sizeof(*attributeAdds));
}
