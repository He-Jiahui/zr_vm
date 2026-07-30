#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compile_tool.h"

#include <string.h>

#define ZR_ATTRIBUTE_HASH_OFFSET ((TZrUInt32)2166136261U)
#define ZR_ATTRIBUTE_HASH_PRIME ((TZrUInt32)16777619U)

static const SZrParserAttributeFieldSchema g_usage_fields[] = {
        {"targets", ZR_PARSER_ATTRIBUTE_VALUE_UINT, ZR_FALSE},
        {"retention", ZR_PARSER_ATTRIBUTE_VALUE_ENUM, ZR_FALSE},
        {"repeatable", ZR_PARSER_ATTRIBUTE_VALUE_BOOL, ZR_FALSE},
        {"inherited", ZR_PARSER_ATTRIBUTE_VALUE_BOOL, ZR_FALSE},
};

static const SZrParserAttributeFieldSchema g_conditional_fields[] = {
        {"feature", ZR_PARSER_ATTRIBUTE_VALUE_STRING, ZR_FALSE},
};

static const SZrParserAttributeSchema g_builtin_schemas[] = {
        {
                .attributeId = (TZrUInt32)0xf5e3e9f1U,
                .typeId = (TZrTypeId)0xf5e3e9f1U,
                .qualifiedName = ZR_PARSER_ATTRIBUTE_USAGE_QUALIFIED_NAME,
                .ownerModule = ZR_PARSER_ATTRIBUTE_MODULE_REFLECTION,
                .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_RUNTIME,
                .role = ZR_PARSER_ATTRIBUTE_ROLE_USAGE,
                .usage = {ZR_PARSER_ATTRIBUTE_TARGET_TYPE,
                          ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
                          ZR_FALSE,
                          ZR_FALSE},
                .fields = g_usage_fields,
                .fieldCount = ZR_ARRAY_COUNT(g_usage_fields),
        },
        {
                .attributeId = (TZrUInt32)0x5471ac5cU,
                .typeId = (TZrTypeId)0x5471ac5cU,
                .qualifiedName = ZR_PARSER_ATTRIBUTE_CONDITIONAL_QUALIFIED_NAME,
                .ownerModule = ZR_PARSER_COMPILE_TOOL_MODULE_BUILD,
                .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
                .role = ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL,
                .usage = {ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION,
                          ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
                          ZR_FALSE,
                          ZR_FALSE},
                .fields = g_conditional_fields,
                .fieldCount = ZR_ARRAY_COUNT(g_conditional_fields),
        },
        {
                .attributeId = (TZrUInt32)0xf55d8f04U,
                .typeId = (TZrTypeId)0xf55d8f04U,
                .qualifiedName = ZR_PARSER_ATTRIBUTE_DECLARATION_TRANSFORM_QUALIFIED_NAME,
                .ownerModule = ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION,
                .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
                .role = ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM,
                .usage = {ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION,
                          ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT,
                          ZR_FALSE,
                          ZR_FALSE},
                .fields = ZR_NULL,
                .fieldCount = 0,
        },
};

TZrUInt32 ZrParser_AttributeContract_ComputeId(const TZrChar *qualifiedName) {
    TZrUInt32 hash = ZR_ATTRIBUTE_HASH_OFFSET;
    const TZrByte *cursor = (const TZrByte *)qualifiedName;

    if (qualifiedName == ZR_NULL || qualifiedName[0] == '\0') {
        return 0U;
    }
    while (*cursor != 0U) {
        hash ^= (TZrUInt32)*cursor++;
        hash *= ZR_ATTRIBUTE_HASH_PRIME;
    }
    return hash == 0U ? 1U : hash;
}

const SZrParserAttributeSchema *ZrParser_AttributeContract_FindBuiltin(
        const TZrChar *qualifiedName) {
    if (qualifiedName == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(g_builtin_schemas); index++) {
        if (strcmp(qualifiedName, g_builtin_schemas[index].qualifiedName) == 0) {
            return &g_builtin_schemas[index];
        }
    }
    return ZR_NULL;
}

const SZrParserAttributeSchema *ZrParser_AttributeContract_FindBuiltinByRole(
        EZrParserAttributeRole role) {
    if (role == ZR_PARSER_ATTRIBUTE_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < ZR_ARRAY_COUNT(g_builtin_schemas); index++) {
        if (g_builtin_schemas[index].role == role) {
            return &g_builtin_schemas[index];
        }
    }
    return ZR_NULL;
}

EZrParserAttributeValidationError ZrParser_AttributeContract_ValidateSchema(
        TZrBool isReadonly,
        const SZrParserAttributeFieldSchema *fields,
        const TZrBool *fieldIsPublicLet,
        TZrSize fieldCount) {
    if (!isReadonly) {
        return ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_NOT_READONLY;
    }
    for (TZrSize index = 0; index < fieldCount; index++) {
        if (fields == ZR_NULL || fieldIsPublicLet == ZR_NULL || !fieldIsPublicLet[index]) {
            return ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_FIELD_NOT_PUBLIC_LET;
        }
        if (fields[index].name == ZR_NULL || fields[index].name[0] == '\0' ||
            fields[index].valueKind < ZR_PARSER_ATTRIBUTE_VALUE_BOOL ||
            fields[index].valueKind > ZR_PARSER_ATTRIBUTE_VALUE_TYPE_ID) {
            return ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_FIELD_TYPE;
        }
    }
    return ZR_PARSER_ATTRIBUTE_VALID;
}

EZrParserAttributeValidationError ZrParser_AttributeContract_ValidateApplication(
        const SZrParserAttributeSchema *schema,
        EZrParserAttributeTarget target,
        TZrSize existingApplicationCount,
        const SZrParserAttributeConstant *values,
        TZrSize valueCount) {
    if (schema == ZR_NULL || (schema->usage.targets & (TZrUInt32)target) == 0U) {
        return ZR_PARSER_ATTRIBUTE_ERROR_TARGET;
    }
    if (!schema->usage.repeatable && existingApplicationCount > 0U) {
        return ZR_PARSER_ATTRIBUTE_ERROR_REPEATABILITY;
    }
    if (valueCount != schema->fieldCount) {
        return ZR_PARSER_ATTRIBUTE_ERROR_ARGUMENT_COUNT;
    }
    for (TZrSize index = 0; index < valueCount; index++) {
        if (values == ZR_NULL || schema->fields == ZR_NULL) {
            return ZR_PARSER_ATTRIBUTE_ERROR_ARGUMENT_COUNT;
        }
        if (values[index].kind == ZR_PARSER_ATTRIBUTE_VALUE_NULL) {
            if (!schema->fields[index].nullable) {
                return ZR_PARSER_ATTRIBUTE_ERROR_ARGUMENT_TYPE;
            }
        } else if (values[index].kind != schema->fields[index].valueKind) {
            return ZR_PARSER_ATTRIBUTE_ERROR_ARGUMENT_TYPE;
        }
    }
    if (schema->role == ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL &&
        (values == ZR_NULL || values[0].value.stringValue == ZR_NULL ||
         values[0].value.stringValue[0] == '\0')) {
        return ZR_PARSER_ATTRIBUTE_ERROR_EMPTY_CONDITIONAL_FEATURE;
    }
    return ZR_PARSER_ATTRIBUTE_VALID;
}
