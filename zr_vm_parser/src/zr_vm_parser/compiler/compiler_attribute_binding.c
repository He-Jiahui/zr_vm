#include "compiler_attribute_binding.h"
#include "compiler_declaration_transform.h"

#include "compile_tool_binding.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/attribute_contract.h"

#include <stdio.h>
#include <string.h>

typedef struct SZrParsedMetadataAttribute {
    const SZrParserAttributeSchema *schema;
    const SZrCompilerAttributeSchemaBinding *boundSchema;
    SZrString *name;
    SZrFunctionCall *call;
} SZrParsedMetadataAttribute;

static TZrBool metadata_string_equals(const SZrString *value, const TZrChar *text) {
    TZrNativeString native;

    if (value == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    native = ZrCore_String_GetNativeStringShort((SZrString *)value);
    return native != ZR_NULL && strcmp(native, text) == 0 ? ZR_TRUE : ZR_FALSE;
}

static SZrString *metadata_member_name(SZrAstNode *member) {
    if (member == ZR_NULL || member->type != ZR_AST_MEMBER_EXPRESSION ||
        member->data.memberExpression.computed ||
        member->data.memberExpression.property == ZR_NULL ||
        member->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }
    return member->data.memberExpression.property->data.identifier.name;
}

static const SZrCompilerAttributeSchemaBinding *metadata_find_bound_schema(
        SZrCompilerState *cs,
        SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL || !cs->attributeSchemas.isValid) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < cs->attributeSchemas.length; index++) {
        const SZrCompilerAttributeSchemaBinding *schema =
                (const SZrCompilerAttributeSchemaBinding *)ZrCore_Array_Get(
                        &cs->attributeSchemas, index);
        if (schema != ZR_NULL && schema->name != ZR_NULL &&
            ZrCore_String_Equal(schema->name, name)) {
            return schema;
        }
    }
    return ZR_NULL;
}

static TZrBool metadata_parse_attribute(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        SZrParsedMetadataAttribute *parsed) {
    SZrAstNode *expression;
    SZrPrimaryExpression *primary;
    SZrString *rootName;
    SZrString *firstName = ZR_NULL;
    SZrString *secondName = ZR_NULL;
    TZrSize pathCount = 0;
    const SZrCompileToolBinding *compileToolBinding;

    if (parsed != ZR_NULL) {
        memset(parsed, 0, sizeof(*parsed));
    }
    if (cs == ZR_NULL || decoratorNode == ZR_NULL || parsed == ZR_NULL ||
        decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }
    expression = decoratorNode->data.decoratorExpression.expr;
    if (expression == ZR_NULL || expression->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    primary = &expression->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->property->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }
    rootName = primary->property->data.identifier.name;
    parsed->name = rootName;

    for (TZrSize index = 0;
         primary->members != ZR_NULL && index < primary->members->count;
         index++) {
        SZrAstNode *member = primary->members->nodes[index];
        SZrString *name;

        if (member != ZR_NULL && member->type == ZR_AST_FUNCTION_CALL) {
            if (index + 1U != primary->members->count || parsed->call != ZR_NULL) {
                return ZR_FALSE;
            }
            parsed->call = &member->data.functionCall;
            continue;
        }
        name = metadata_member_name(member);
        if (name == ZR_NULL || pathCount >= 2U) {
            return ZR_FALSE;
        }
        if (pathCount == 0U) {
            firstName = name;
        } else {
            secondName = name;
        }
        pathCount++;
    }

    if (metadata_string_equals(rootName, "zr") && pathCount == 2U) {
        if (metadata_string_equals(firstName, "reflection") &&
            metadata_string_equals(secondName, "attributeUsage")) {
            parsed->schema = ZrParser_AttributeContract_FindBuiltinByRole(
                    ZR_PARSER_ATTRIBUTE_ROLE_USAGE);
        } else if (metadata_string_equals(firstName, "compile") &&
                   metadata_string_equals(secondName, "conditional")) {
            parsed->schema = ZrParser_AttributeContract_FindBuiltinByRole(
                    ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL);
        } else if (metadata_string_equals(firstName, "compile") &&
                   metadata_string_equals(secondName, "declarationTransform")) {
            parsed->schema = ZrParser_AttributeContract_FindBuiltinByRole(
                    ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM);
        }
        return parsed->schema != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }

    if (pathCount == 0U) {
        parsed->boundSchema = metadata_find_bound_schema(cs, rootName);
        return parsed->boundSchema != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }

    compileToolBinding = ZrParser_CompileToolBinding_Resolve(cs, rootName);
    if (compileToolBinding == ZR_NULL ||
        compileToolBinding->kind != ZR_COMPILE_TOOL_BINDING_PROVIDER ||
        compileToolBinding->provider == ZR_NULL || pathCount != 1U) {
        return ZR_FALSE;
    }
    if (metadata_string_equals(firstName, "conditional")) {
        parsed->schema = ZrParser_CompileTool_FindMetadataRole(
                compileToolBinding->provider,
                ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL);
    } else if (metadata_string_equals(firstName, "declarationTransform")) {
        parsed->schema = ZrParser_AttributeContract_FindBuiltinByRole(
                ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM);
    }
    return parsed->schema != ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

static SZrString *metadata_path_leaf(SZrAstNode *node) {
    SZrPrimaryExpression *primary;

    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }
    if (node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_NULL;
    }
    primary = &node->data.primaryExpression;
    if (primary->members != ZR_NULL && primary->members->count > 0U) {
        return metadata_member_name(
                primary->members->nodes[primary->members->count - 1U]);
    }
    return primary->property != ZR_NULL &&
                   primary->property->type == ZR_AST_IDENTIFIER_LITERAL
           ? primary->property->data.identifier.name
           : ZR_NULL;
}

static SZrAstNode *metadata_named_argument(
        SZrFunctionCall *call,
        const TZrChar *name,
        TZrSize position) {
    if (call == ZR_NULL || call->args == ZR_NULL) {
        return ZR_NULL;
    }
    if (call->argNames != ZR_NULL) {
        for (TZrSize index = 0;
             index < call->args->count && index < call->argNames->length;
             index++) {
            SZrString **argName =
                    (SZrString **)ZrCore_Array_Get(call->argNames, index);
            if (argName != ZR_NULL && *argName != ZR_NULL &&
                metadata_string_equals(*argName, name)) {
                return call->args->nodes[index];
            }
        }
    }
    return position < call->args->count ? call->args->nodes[position] : ZR_NULL;
}

static TZrBool metadata_parse_target_flags(SZrAstNode *node, TZrUInt32 *targets) {
    SZrString *leaf;

    if (node == ZR_NULL || targets == ZR_NULL) {
        return ZR_FALSE;
    }
    if (node->type == ZR_AST_BINARY_EXPRESSION &&
        node->data.binaryExpression.op.op != ZR_NULL &&
        strcmp(node->data.binaryExpression.op.op, "|") == 0) {
        return metadata_parse_target_flags(node->data.binaryExpression.left, targets) &&
               metadata_parse_target_flags(node->data.binaryExpression.right, targets);
    }
    leaf = metadata_path_leaf(node);
    if (metadata_string_equals(leaf, "all")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_ALL;
    } else if (metadata_string_equals(leaf, "type") ||
               metadata_string_equals(leaf, "class") ||
               metadata_string_equals(leaf, "struct")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_TYPE;
    } else if (metadata_string_equals(leaf, "function")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION;
    } else if (metadata_string_equals(leaf, "field")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_FIELD;
    } else if (metadata_string_equals(leaf, "method")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_METHOD;
    } else if (metadata_string_equals(leaf, "property")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_PROPERTY;
    } else if (metadata_string_equals(leaf, "parameter")) {
        *targets |= ZR_PARSER_ATTRIBUTE_TARGET_PARAMETER;
    } else {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool metadata_parse_retention(
        SZrAstNode *node,
        EZrParserAttributeRetention *retention) {
    SZrString *leaf = metadata_path_leaf(node);

    if (retention == ZR_NULL) {
        return ZR_FALSE;
    }
    if (metadata_string_equals(leaf, "source")) {
        *retention = ZR_PARSER_ATTRIBUTE_RETENTION_SOURCE;
    } else if (metadata_string_equals(leaf, "artifact")) {
        *retention = ZR_PARSER_ATTRIBUTE_RETENTION_ARTIFACT;
    } else if (metadata_string_equals(leaf, "runtime")) {
        *retention = ZR_PARSER_ATTRIBUTE_RETENTION_RUNTIME;
    } else {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool metadata_parse_bool(SZrAstNode *node, TZrBool *value) {
    if (node == ZR_NULL || value == ZR_NULL ||
        node->type != ZR_AST_BOOLEAN_LITERAL) {
        return ZR_FALSE;
    }
    *value = node->data.booleanLiteral.value;
    return ZR_TRUE;
}

static EZrParserAttributeValueKind metadata_field_value_kind(SZrType *type) {
    SZrString *name;

    if (type == ZR_NULL || type->name == ZR_NULL || type->subType != ZR_NULL ||
        type->dimensions != 0 || type->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        type->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        type->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_PARSER_ATTRIBUTE_VALUE_INVALID;
    }
    name = type->name->data.identifier.name;
    if (metadata_string_equals(name, "bool")) {
        return ZR_PARSER_ATTRIBUTE_VALUE_BOOL;
    }
    if (metadata_string_equals(name, "string")) {
        return ZR_PARSER_ATTRIBUTE_VALUE_STRING;
    }
    if (metadata_string_equals(name, "float") ||
        metadata_string_equals(name, "double") ||
        metadata_string_equals(name, "f32") || metadata_string_equals(name, "f64")) {
        return ZR_PARSER_ATTRIBUTE_VALUE_FLOAT;
    }
    if (metadata_string_equals(name, "uint") || metadata_string_equals(name, "u8") ||
        metadata_string_equals(name, "u16") || metadata_string_equals(name, "u32") ||
        metadata_string_equals(name, "u64")) {
        return ZR_PARSER_ATTRIBUTE_VALUE_UINT;
    }
    if (metadata_string_equals(name, "int") || metadata_string_equals(name, "i8") ||
        metadata_string_equals(name, "i16") || metadata_string_equals(name, "i32") ||
        metadata_string_equals(name, "i64")) {
        return ZR_PARSER_ATTRIBUTE_VALUE_INT;
    }
    if (metadata_string_equals(name, "TypeId")) {
        return ZR_PARSER_ATTRIBUTE_VALUE_TYPE_ID;
    }
    return ZR_PARSER_ATTRIBUTE_VALUE_INVALID;
}

static TZrBool metadata_parse_usage(
        SZrCompilerState *cs,
        const SZrParsedMetadataAttribute *attribute,
        SZrParserAttributeUsage *usage,
        SZrFileRange location) {
    SZrAstNode *targets;
    SZrAstNode *retention;
    SZrAstNode *repeatable;
    SZrAstNode *inherited;

    if (attribute == ZR_NULL || attribute->call == ZR_NULL || usage == ZR_NULL ||
        attribute->call->args == ZR_NULL || attribute->call->args->count != 4U) {
        ZrParser_Compiler_Error(
                cs, "attributeUsage.arguments: expected targets, retention, repeatable, inherited", location);
        return ZR_FALSE;
    }
    targets = metadata_named_argument(attribute->call, "targets", 0U);
    retention = metadata_named_argument(attribute->call, "retention", 1U);
    repeatable = metadata_named_argument(attribute->call, "repeatable", 2U);
    inherited = metadata_named_argument(attribute->call, "inherited", 3U);
    memset(usage, 0, sizeof(*usage));
    if (!metadata_parse_target_flags(targets, &usage->targets) || usage->targets == 0U ||
        !metadata_parse_retention(retention, &usage->retention) ||
        !metadata_parse_bool(repeatable, &usage->repeatable) ||
        !metadata_parse_bool(inherited, &usage->inherited)) {
        ZrParser_Compiler_Error(
                cs, "attributeUsage.arguments: values must be canonical compile-time constants", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool metadata_function_returns_explicit_void(
        const SZrFunctionDeclaration *declaration) {
    SZrType *returnType;

    if (declaration == ZR_NULL || declaration->returnType == ZR_NULL) {
        return ZR_FALSE;
    }
    returnType = declaration->returnType;
    return returnType->subType == ZR_NULL && returnType->dimensions == 0 &&
                   returnType->referenceAccess == ZR_REFERENCE_ACCESS_NONE &&
                   returnType->name != ZR_NULL &&
                   returnType->name->type == ZR_AST_IDENTIFIER_LITERAL &&
                   metadata_string_equals(returnType->name->data.identifier.name, "void")
           ? ZR_TRUE
           : ZR_FALSE;
}

static TZrBool metadata_conditional_signature_is_valid(
        const SZrFunctionDeclaration *declaration) {
    if (declaration == ZR_NULL || declaration->isAsync || declaration->args != ZR_NULL ||
        declaration->body == ZR_NULL || !metadata_function_returns_explicit_void(declaration) ||
        (declaration->generic != ZR_NULL && declaration->generic->params != ZR_NULL &&
         declaration->generic->params->count > 0U)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0;
         declaration->params != ZR_NULL && index < declaration->params->count;
         index++) {
        SZrAstNode *parameter = declaration->params->nodes[index];
        if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
            parameter->data.parameter.passingMode != ZR_PARAMETER_PASSING_MODE_VALUE) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool metadata_conditional_feature(
        SZrCompilerState *cs,
        const SZrParsedMetadataAttribute *attribute,
        SZrString **featureName) {
    SZrAstNode *argument;
    SZrParserAttributeConstant value;

    if (featureName != ZR_NULL) {
        *featureName = ZR_NULL;
    }
    if (cs == ZR_NULL || attribute == ZR_NULL || attribute->schema == ZR_NULL ||
        attribute->schema->role != ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL ||
        attribute->call == ZR_NULL || attribute->call->args == ZR_NULL ||
        attribute->call->args->count != 1U) {
        return ZR_FALSE;
    }
    argument = attribute->call->args->nodes[0];
    if (argument == ZR_NULL || argument->type != ZR_AST_STRING_LITERAL ||
        argument->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }
    value.kind = ZR_PARSER_ATTRIBUTE_VALUE_STRING;
    value.value.stringValue = ZrCore_String_GetNativeStringShort(
            argument->data.stringLiteral.value);
    if (ZrParser_AttributeContract_ValidateApplication(
                attribute->schema,
                ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION,
                0,
                &value,
                1) != ZR_PARSER_ATTRIBUTE_VALID) {
        return ZR_FALSE;
    }
    if (featureName != ZR_NULL) {
        *featureName = argument->data.stringLiteral.value;
    }
    return ZR_TRUE;
}

static TZrBool metadata_project_feature_enabled(
        SZrCompilerState *cs,
        SZrString *featureName,
        SZrFileRange location,
        TZrBool *enabled) {
    const SZrLibrary_Project *project;

    if (enabled != ZR_NULL) {
        *enabled = ZR_FALSE;
    }
    if (cs == ZR_NULL || featureName == ZR_NULL || enabled == ZR_NULL) {
        return ZR_FALSE;
    }
    project = ZrLibrary_Project_GetFromGlobal(cs->state->global);
    for (TZrSize index = 0;
         project != ZR_NULL && index < project->featureSwitchCount;
         index++) {
        const SZrLibrary_ProjectFeatureSwitch *feature =
                &project->featureSwitches[index];
        if (feature->name != ZR_NULL && ZrCore_String_Equal(feature->name, featureName)) {
            *enabled = feature->value;
            return ZR_TRUE;
        }
    }
    ZrParser_Compiler_Error(
            cs, "conditional.unknown_feature: project feature is not declared", location);
    return ZR_FALSE;
}

static TZrBool metadata_find_conditional_attribute(
        SZrCompilerState *cs,
        SZrFunctionDeclaration *declaration,
        SZrFileRange location,
        SZrString **featureName) {
    TZrSize conditionalCount = 0;

    if (featureName != ZR_NULL) {
        *featureName = ZR_NULL;
    }
    for (TZrSize index = 0;
         declaration != ZR_NULL && declaration->decorators != ZR_NULL &&
         index < declaration->decorators->count;
         index++) {
        SZrParsedMetadataAttribute parsed;
        if (!metadata_parse_attribute(cs, declaration->decorators->nodes[index], &parsed) ||
            parsed.schema->role != ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL) {
            continue;
        }
        conditionalCount++;
        if (conditionalCount > 1U) {
            ZrParser_Compiler_Error(
                    cs, "conditional.repeatability: conditional metadata is not repeatable", location);
            return ZR_FALSE;
        }
        if (!metadata_conditional_feature(cs, &parsed, featureName)) {
            ZrParser_Compiler_Error(
                    cs, "conditional.arguments: expected one non-empty string feature", location);
            return ZR_FALSE;
        }
    }
    return conditionalCount > 0U ? ZR_TRUE : ZR_FALSE;
}

TZrBool ZrParser_Metadata_RegisterAttributeSchema(
        SZrCompilerState *cs,
        SZrAstNode *typeNode) {
    SZrStructDeclaration *declaration;
    SZrParsedMetadataAttribute usageAttribute;
    TZrSize usageCount = 0U;
    SZrCompilerAttributeSchemaBinding binding;
    SZrParserAttributeFieldSchema *validationFields = ZR_NULL;
    TZrBool *fieldIsPublicLet = ZR_NULL;
    TZrSize fieldCount = 0U;
    EZrParserAttributeValidationError validation;

    if (cs == ZR_NULL || typeNode == ZR_NULL ||
        typeNode->type != ZR_AST_STRUCT_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &typeNode->data.structDeclaration;
    memset(&usageAttribute, 0, sizeof(usageAttribute));
    for (TZrSize index = 0U;
         declaration->decorators != ZR_NULL && index < declaration->decorators->count;
         index++) {
        SZrParsedMetadataAttribute parsed;
        if (metadata_parse_attribute(cs, declaration->decorators->nodes[index], &parsed) &&
            parsed.schema != ZR_NULL &&
            parsed.schema->role == ZR_PARSER_ATTRIBUTE_ROLE_USAGE) {
            usageAttribute = parsed;
            usageCount++;
        }
    }
    if (usageCount == 0U) {
        return ZR_TRUE;
    }
    if (usageCount != 1U || declaration->name == ZR_NULL ||
        declaration->name->name == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "attributeUsage.repeatability: exactly one usage declaration is allowed", typeNode->location);
        return ZR_FALSE;
    }
    if (metadata_find_bound_schema(cs, declaration->name->name) != ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "attribute.schema_duplicate: attribute schema name is already registered", typeNode->location);
        return ZR_FALSE;
    }

    memset(&binding, 0, sizeof(binding));
    binding.name = declaration->name->name;
    binding.attributeId = ZrParser_AttributeContract_ComputeId(
            ZrCore_String_GetNativeStringShort(binding.name));
    binding.typeId = cs->semanticContext != ZR_NULL
                             ? ZrParser_CanonicalType_FromName(
                                       cs->semanticContext, binding.name)
                             : ZR_SEMANTIC_ID_INVALID;
    if (binding.typeId == ZR_SEMANTIC_ID_INVALID) {
        binding.typeId = (TZrTypeId)binding.attributeId;
    }
    binding.sourceRange = typeNode->location;
    if (!metadata_parse_usage(cs, &usageAttribute, &binding.usage, typeNode->location)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0U;
         declaration->members != ZR_NULL && index < declaration->members->count;
         index++) {
        if (declaration->members->nodes[index] != ZR_NULL &&
            declaration->members->nodes[index]->type == ZR_AST_STRUCT_FIELD) {
            fieldCount++;
        }
    }
    ZrCore_Array_Init(
            cs->state, &binding.fields, sizeof(SZrCompilerAttributeFieldBinding),
            fieldCount > 0U ? fieldCount : 1U);
    if (fieldCount > 0U) {
        validationFields = (SZrParserAttributeFieldSchema *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrParserAttributeFieldSchema) * fieldCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        fieldIsPublicLet = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(TZrBool) * fieldCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (validationFields == ZR_NULL || fieldIsPublicLet == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs, "attribute.schema_allocation: failed to allocate schema fields", typeNode->location);
            goto failure;
        }
    }
    for (TZrSize index = 0U, fieldIndex = 0U;
         declaration->members != ZR_NULL && index < declaration->members->count;
         index++) {
        SZrAstNode *member = declaration->members->nodes[index];
        SZrStructField *field;
        SZrCompilerAttributeFieldBinding fieldBinding;

        if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD) {
            continue;
        }
        field = &member->data.structField;
        memset(&fieldBinding, 0, sizeof(fieldBinding));
        fieldBinding.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        fieldBinding.valueKind = metadata_field_value_kind(field->typeInfo);
        fieldBinding.nullable = ZR_FALSE;
        validationFields[fieldIndex].name = fieldBinding.name != ZR_NULL
                                                    ? ZrCore_String_GetNativeStringShort(fieldBinding.name)
                                                    : ZR_NULL;
        validationFields[fieldIndex].valueKind = fieldBinding.valueKind;
        validationFields[fieldIndex].nullable = fieldBinding.nullable;
        fieldIsPublicLet[fieldIndex] =
                field->access == ZR_ACCESS_PUBLIC && field->isConst && !field->isStatic;
        ZrCore_Array_Push(cs->state, &binding.fields, &fieldBinding);
        fieldIndex++;
    }
    validation = ZrParser_AttributeContract_ValidateSchema(
            declaration->isReadonly,
            validationFields,
            fieldIsPublicLet,
            fieldCount);
    if (validation != ZR_PARSER_ATTRIBUTE_VALID) {
        const TZrChar *message = validation == ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_NOT_READONLY
                                        ? "attribute.schema_readonly: schema must be a readonly struct"
                                : validation == ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_FIELD_NOT_PUBLIC_LET
                                        ? "attribute.schema_field: every schema field must be a non-static pub let"
                                        : "attribute.schema_type: field type is not compile-constant-safe";
        ZrParser_Compiler_Error(cs, message, typeNode->location);
        goto failure;
    }
    ZrCore_Array_Push(cs->state, &cs->attributeSchemas, &binding);
    if (validationFields != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global, validationFields,
                sizeof(SZrParserAttributeFieldSchema) * fieldCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (fieldIsPublicLet != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global, fieldIsPublicLet, sizeof(TZrBool) * fieldCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ZR_TRUE;

failure:
    if (validationFields != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global, validationFields,
                sizeof(SZrParserAttributeFieldSchema) * fieldCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (fieldIsPublicLet != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global, fieldIsPublicLet, sizeof(TZrBool) * fieldCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (binding.fields.isValid) {
        ZrCore_Array_Free(cs->state, &binding.fields);
    }
    return ZR_FALSE;
}

static TZrBool metadata_constant_from_ast(
        SZrCompilerState *cs,
        SZrAstNode *node,
        EZrParserAttributeValueKind expectedKind,
        SZrParserAttributeConstant *constant,
        SZrTypeValue *value) {
    if (cs == ZR_NULL || node == ZR_NULL || constant == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(constant, 0, sizeof(*constant));
    switch (expectedKind) {
        case ZR_PARSER_ATTRIBUTE_VALUE_BOOL:
            if (node->type != ZR_AST_BOOLEAN_LITERAL) return ZR_FALSE;
            constant->kind = expectedKind;
            constant->value.boolValue = node->data.booleanLiteral.value;
            ZrCore_Value_InitAsBool(cs->state, value, constant->value.boolValue);
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_INT:
        case ZR_PARSER_ATTRIBUTE_VALUE_ENUM:
            if (node->type != ZR_AST_INTEGER_LITERAL) return ZR_FALSE;
            constant->kind = expectedKind;
            constant->value.intValue = node->data.integerLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, value, constant->value.intValue);
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_UINT:
        case ZR_PARSER_ATTRIBUTE_VALUE_TYPE_ID:
            if (node->type != ZR_AST_INTEGER_LITERAL ||
                node->data.integerLiteral.value < 0) return ZR_FALSE;
            constant->kind = expectedKind;
            constant->value.uintValue = (TZrUInt64)node->data.integerLiteral.value;
            ZrCore_Value_InitAsUInt(cs->state, value, constant->value.uintValue);
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_FLOAT:
            if (node->type != ZR_AST_FLOAT_LITERAL) return ZR_FALSE;
            constant->kind = expectedKind;
            constant->value.floatValue = node->data.floatLiteral.value;
            ZrCore_Value_InitAsFloat(cs->state, value, constant->value.floatValue);
            return ZR_TRUE;
        case ZR_PARSER_ATTRIBUTE_VALUE_STRING:
            if (node->type != ZR_AST_STRING_LITERAL ||
                node->data.stringLiteral.value == ZR_NULL) return ZR_FALSE;
            constant->kind = expectedKind;
            constant->value.stringValue = ZrCore_String_GetNativeStringShort(
                    node->data.stringLiteral.value);
            ZrCore_Value_InitAsRawObject(
                    cs->state, value,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(node->data.stringLiteral.value));
            value->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static TZrBool metadata_set_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *fieldName,
        const SZrTypeValue *value) {
    SZrString *name;
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }
    name = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)fieldName);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    return ZR_TRUE;
}

static const SZrTypeValue *metadata_get_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *fieldName) {
    SZrString *name;
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    name = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)fieldName);
    if (name == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(cs->state, object, &key);
}

static TZrBool metadata_apply_bound_attributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        EZrParserAttributeTarget target,
        SZrTypeValue *metadataValue,
        TZrBool *hasMetadata,
        SZrArray *decoratorNames,
        SZrFileRange location) {
    SZrObject *metadataObject = ZR_NULL;

    if (cs == ZR_NULL || metadataValue == ZR_NULL || hasMetadata == ZR_NULL) {
        return ZR_FALSE;
    }
    if (*hasMetadata && metadataValue->type == ZR_VALUE_TYPE_OBJECT &&
        metadataValue->value.object != ZR_NULL) {
        metadataObject = ZR_CAST_OBJECT(cs->state, metadataValue->value.object);
    }
    for (TZrSize decoratorIndex = 0U;
         decorators != ZR_NULL && decoratorIndex < decorators->count;
         decoratorIndex++) {
        SZrParsedMetadataAttribute parsed;
        const SZrCompilerAttributeSchemaBinding *schema;
        SZrParserAttributeConstant *constants = ZR_NULL;
        SZrTypeValue *values = ZR_NULL;
        TZrBool *provided = ZR_NULL;
        SZrObject *entry;
        SZrTypeValue entryValue;
        SZrTypeValue scalar;
        TZrChar metadataKey[64];
        TZrSize existingCount = 0U;

        if (!metadata_parse_attribute(cs, decorators->nodes[decoratorIndex], &parsed) ||
            parsed.boundSchema == ZR_NULL) {
            continue;
        }
        schema = parsed.boundSchema;
        if ((schema->usage.targets & (TZrUInt32)target) == 0U) {
            ZrParser_Compiler_Error(
                    cs, "attribute.target: attribute is not valid for this declaration", location);
            return ZR_FALSE;
        }
        if (parsed.call == ZR_NULL && schema->fields.length > 0U) {
            ZrParser_Compiler_Error(
                    cs, "attribute.arguments: attribute fields require an argument list", location);
            return ZR_FALSE;
        }
        if (parsed.call != ZR_NULL &&
            (parsed.call->args == ZR_NULL || parsed.call->args->count != schema->fields.length)) {
            ZrParser_Compiler_Error(
                    cs, "attribute.arguments: argument count does not match schema fields", location);
            return ZR_FALSE;
        }
        if (metadataObject != ZR_NULL) {
            for (;;) {
                snprintf(metadataKey, sizeof(metadataKey), "attribute:%08x:%llu",
                         (unsigned int)schema->attributeId,
                         (unsigned long long)existingCount);
                if (metadata_get_object_field(cs, metadataObject, metadataKey) == ZR_NULL) {
                    break;
                }
                existingCount++;
            }
        }
        if (!schema->usage.repeatable && existingCount > 0U) {
            ZrParser_Compiler_Error(
                    cs, "attribute.repeatability: attribute is not repeatable", location);
            return ZR_FALSE;
        }
        if (schema->fields.length > 0U) {
            constants = (SZrParserAttributeConstant *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    sizeof(SZrParserAttributeConstant) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
            values = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    sizeof(SZrTypeValue) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
            provided = (TZrBool *)ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    sizeof(TZrBool) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
            if (constants == ZR_NULL || values == ZR_NULL || provided == ZR_NULL) {
                ZrParser_Compiler_Error(
                        cs, "attribute.application_allocation: failed to allocate values", location);
                goto application_failure;
            }
            ZrCore_Memory_RawSet(provided, 0, sizeof(TZrBool) * schema->fields.length);
            for (TZrSize argumentIndex = 0U;
                 parsed.call != ZR_NULL && argumentIndex < parsed.call->args->count;
                 argumentIndex++) {
                TZrSize fieldIndex = argumentIndex;
                SZrString *argumentName = ZR_NULL;
                if (parsed.call->argNames != ZR_NULL &&
                    argumentIndex < parsed.call->argNames->length) {
                    SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(
                            parsed.call->argNames, argumentIndex);
                    argumentName = argumentNamePtr != ZR_NULL ? *argumentNamePtr : ZR_NULL;
                }
                if (argumentName != ZR_NULL) {
                    fieldIndex = schema->fields.length;
                    for (TZrSize candidate = 0U; candidate < schema->fields.length; candidate++) {
                        const SZrCompilerAttributeFieldBinding *field =
                                (const SZrCompilerAttributeFieldBinding *)ZrCore_Array_Get(
                                        (SZrArray *)&schema->fields, candidate);
                        if (field != ZR_NULL && field->name != ZR_NULL &&
                            ZrCore_String_Equal(field->name, argumentName)) {
                            fieldIndex = candidate;
                            break;
                        }
                    }
                }
                if (fieldIndex >= schema->fields.length || provided[fieldIndex]) {
                    ZrParser_Compiler_Error(
                            cs, "attribute.arguments: unknown or duplicate named argument", location);
                    goto application_failure;
                }
                const SZrCompilerAttributeFieldBinding *field =
                        (const SZrCompilerAttributeFieldBinding *)ZrCore_Array_Get(
                                (SZrArray *)&schema->fields, fieldIndex);
                if (field == ZR_NULL ||
                    !metadata_constant_from_ast(
                            cs, parsed.call->args->nodes[argumentIndex], field->valueKind,
                            &constants[fieldIndex], &values[fieldIndex])) {
                    ZrParser_Compiler_Error(
                            cs, "attribute.argument_type: argument must match the schema constant type", location);
                    goto application_failure;
                }
                provided[fieldIndex] = ZR_TRUE;
            }
            for (TZrSize fieldIndex = 0U; fieldIndex < schema->fields.length; fieldIndex++) {
                if (!provided[fieldIndex]) {
                    ZrParser_Compiler_Error(
                            cs, "attribute.arguments: every schema field requires a value", location);
                    goto application_failure;
                }
            }
        }

        if (metadataObject == ZR_NULL) {
            metadataObject = ZrCore_Object_New(cs->state, ZR_NULL);
            if (metadataObject == ZR_NULL) {
                goto application_failure;
            }
            ZrCore_Object_Init(cs->state, metadataObject);
        }
        entry = ZrCore_Object_New(cs->state, ZR_NULL);
        if (entry == ZR_NULL) {
            goto application_failure;
        }
        ZrCore_Object_Init(cs->state, entry);
        ZrCore_Value_InitAsUInt(cs->state, &scalar, schema->attributeId);
        if (!metadata_set_object_field(cs, entry, "attributeId", &scalar)) goto application_failure;
        ZrCore_Value_InitAsUInt(cs->state, &scalar, schema->typeId);
        if (!metadata_set_object_field(cs, entry, "typeId", &scalar)) goto application_failure;
        ZrCore_Value_InitAsInt(cs->state, &scalar, (TZrInt64)schema->usage.retention);
        if (!metadata_set_object_field(cs, entry, "retention", &scalar)) goto application_failure;
        ZrCore_Value_InitAsInt(cs->state, &scalar, location.start.line);
        if (!metadata_set_object_field(cs, entry, "sourceLineStart", &scalar)) goto application_failure;
        ZrCore_Value_InitAsInt(cs->state, &scalar, location.end.line);
        if (!metadata_set_object_field(cs, entry, "sourceLineEnd", &scalar)) goto application_failure;
        for (TZrSize fieldIndex = 0U; fieldIndex < schema->fields.length; fieldIndex++) {
            const SZrCompilerAttributeFieldBinding *field =
                    (const SZrCompilerAttributeFieldBinding *)ZrCore_Array_Get(
                            (SZrArray *)&schema->fields, fieldIndex);
            if (field == ZR_NULL || field->name == ZR_NULL ||
                !metadata_set_object_field(
                        cs, entry, ZrCore_String_GetNativeStringShort(field->name),
                        &values[fieldIndex])) {
                goto application_failure;
            }
        }
        snprintf(metadataKey, sizeof(metadataKey), "attribute:%08x:%llu",
                 (unsigned int)schema->attributeId,
                 (unsigned long long)existingCount);
        ZrCore_Value_InitAsRawObject(
                cs->state, &entryValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entry));
        entryValue.type = ZR_VALUE_TYPE_OBJECT;
        if (!metadata_set_object_field(cs, metadataObject, metadataKey, &entryValue)) {
            goto application_failure;
        }
        if (decoratorNames != ZR_NULL) {
            SZrTypeDecoratorInfo decoratorInfo = {schema->name};
            if (!decoratorNames->isValid || decoratorNames->head == ZR_NULL) {
                ZrCore_Array_Init(
                        cs->state,
                        decoratorNames,
                        sizeof(SZrTypeDecoratorInfo),
                        ZR_PARSER_INITIAL_CAPACITY_TINY);
            }
            ZrCore_Array_Push(cs->state, decoratorNames, &decoratorInfo);
        }
        if (constants != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global, constants,
                    sizeof(SZrParserAttributeConstant) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (values != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global, values,
                    sizeof(SZrTypeValue) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (provided != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global, provided,
                    sizeof(TZrBool) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        continue;

application_failure:
        if (constants != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global, constants,
                    sizeof(SZrParserAttributeConstant) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (values != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global, values,
                    sizeof(SZrTypeValue) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (provided != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global, provided,
                    sizeof(TZrBool) * schema->fields.length,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        if (!cs->hasError) {
            ZrParser_Compiler_Error(
                    cs, "attribute.application: failed to retain typed metadata", location);
        }
        return ZR_FALSE;
    }
    if (metadataObject != ZR_NULL) {
        ZrCore_Value_InitAsRawObject(
                cs->state, metadataValue, ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
        metadataValue->type = ZR_VALUE_TYPE_OBJECT;
        *hasMetadata = ZR_TRUE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Metadata_ValidateFunctionAttributes(
        SZrCompilerState *cs,
        SZrAstNode *functionNode) {
    SZrFunctionDeclaration *declaration;
    SZrString *featureName = ZR_NULL;
    TZrBool hasTransform = ZR_FALSE;

    if (cs == ZR_NULL || functionNode == ZR_NULL ||
        functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &functionNode->data.functionDeclaration;
    if (!ZrParser_Metadata_FunctionHasRole(
                cs,
                functionNode,
                ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM,
                &hasTransform)) {
        return ZR_FALSE;
    }
    if (hasTransform &&
        !ZrParser_DeclarationTransform_ValidateSignature(cs, functionNode)) {
        return ZR_FALSE;
    }
    if (!metadata_find_conditional_attribute(
                cs, declaration, functionNode->location, &featureName)) {
        return !cs->hasError;
    }
    ZR_UNUSED_PARAMETER(featureName);
    if (!metadata_conditional_signature_is_valid(declaration)) {
        ZrParser_Compiler_Error(
                cs,
                "conditional.signature: target must be a direct non-generic fn returning explicit void with value parameters",
                functionNode->location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_Metadata_TryElideConditionalCall(
        SZrCompilerState *cs,
        SZrAstNode *expression,
        TZrBool *handled) {
    SZrPrimaryExpression *primary;
    SZrAstNode *callNode;
    SZrAstNode *functionNode;
    SZrString *functionName;
    SZrString *featureName = ZR_NULL;
    TZrBool enabled = ZR_FALSE;
    SZrInferredType inferredType;

    if (handled != ZR_NULL) {
        *handled = ZR_FALSE;
    }
    if (cs == ZR_NULL || expression == ZR_NULL || handled == ZR_NULL ||
        expression->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_TRUE;
    }
    primary = &expression->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->property->data.identifier.name == ZR_NULL ||
        primary->members == ZR_NULL || primary->members->count != 1U) {
        return ZR_TRUE;
    }
    callNode = primary->members->nodes[0];
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_TRUE;
    }
    functionName = primary->property->data.identifier.name;
    functionNode = find_function_declaration(cs, functionName);
    if (functionNode == ZR_NULL || functionNode->type != ZR_AST_FUNCTION_DECLARATION ||
        !metadata_find_conditional_attribute(
                cs,
                &functionNode->data.functionDeclaration,
                functionNode->location,
                &featureName)) {
        return !cs->hasError;
    }
    if (!metadata_conditional_signature_is_valid(&functionNode->data.functionDeclaration)) {
        ZrParser_Compiler_Error(
                cs,
                "conditional.signature: target must be a direct non-generic fn returning explicit void with value parameters",
                functionNode->location);
        return ZR_FALSE;
    }
    if (!metadata_project_feature_enabled(cs, featureName, expression->location, &enabled)) {
        return ZR_FALSE;
    }
    if (enabled) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, expression, &inferredType) || cs->hasError) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_FALSE;
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
    *handled = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_Metadata_IsRegisteredAttribute(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode) {
    SZrParsedMetadataAttribute parsed;
    return metadata_parse_attribute(cs, decoratorNode, &parsed);
}

TZrBool ZrParser_Metadata_FunctionHasRole(
        SZrCompilerState *cs,
        SZrAstNode *functionNode,
        EZrParserAttributeRole role,
        TZrBool *hasRole) {
    SZrFunctionDeclaration *declaration;
    TZrSize count = 0U;

    if (hasRole != ZR_NULL) {
        *hasRole = ZR_FALSE;
    }
    if (cs == ZR_NULL || functionNode == ZR_NULL || hasRole == ZR_NULL ||
        functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &functionNode->data.functionDeclaration;
    for (TZrSize index = 0U;
         declaration->decorators != ZR_NULL &&
         index < declaration->decorators->count;
         index++) {
        SZrParsedMetadataAttribute parsed;
        if (!metadata_parse_attribute(
                    cs, declaration->decorators->nodes[index], &parsed) ||
            parsed.schema == ZR_NULL || parsed.schema->role != role) {
            continue;
        }
        count++;
        if (parsed.call != ZR_NULL && parsed.call->args != ZR_NULL &&
            parsed.call->args->count > 0U) {
            ZrParser_Compiler_Error(
                    cs,
                    "metadata.role_arguments: this compiler metadata role does not accept arguments",
                    declaration->decorators->nodes[index]->location);
            return ZR_FALSE;
        }
    }
    if (count > 1U) {
        ZrParser_Compiler_Error(
                cs,
                "metadata.role_repeatability: compiler metadata role is not repeatable",
                functionNode->location);
        return ZR_FALSE;
    }
    *hasRole = count == 1U ? ZR_TRUE : ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrParser_Metadata_ApplyTypeAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        SZrTypePrototypeInfo *typeInfo,
        SZrFileRange location) {
    if (typeInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    return metadata_apply_bound_attributes(
            cs, decorators, ZR_PARSER_ATTRIBUTE_TARGET_TYPE,
            &typeInfo->decoratorMetadataValue, &typeInfo->hasDecoratorMetadata,
            &typeInfo->decorators, location);
}

TZrBool ZrParser_Metadata_ApplyMemberAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        EZrParserAttributeTarget target,
        SZrTypeMemberInfo *memberInfo,
        SZrFileRange location) {
    if (memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }
    return metadata_apply_bound_attributes(
            cs, decorators, target,
            &memberInfo->decoratorMetadataValue, &memberInfo->hasDecoratorMetadata,
            &memberInfo->decorators, location);
}

static TZrBool metadata_append_function_names(
        SZrCompilerState *cs,
        SZrArray *names,
        SZrString ***destination,
        TZrUInt32 *destinationCount) {
    SZrString **combined;
    TZrUInt32 oldCount;
    TZrUInt32 addedCount;

    if (cs == ZR_NULL || names == ZR_NULL || destination == ZR_NULL ||
        destinationCount == ZR_NULL || names->length == 0U) {
        return ZR_TRUE;
    }
    oldCount = *destinationCount;
    addedCount = (TZrUInt32)names->length;
    combined = (SZrString **)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrString *) * (oldCount + addedCount),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (combined == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0U; index < oldCount; index++) {
        combined[index] = (*destination)[index];
    }
    for (TZrUInt32 index = 0U; index < addedCount; index++) {
        const SZrTypeDecoratorInfo *decorator =
                (const SZrTypeDecoratorInfo *)ZrCore_Array_Get(names, index);
        combined[oldCount + index] = decorator != ZR_NULL ? decorator->name : ZR_NULL;
    }
    if (*destination != ZR_NULL && oldCount > 0U) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global, *destination,
                sizeof(SZrString *) * oldCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    *destination = combined;
    *destinationCount = oldCount + addedCount;
    return ZR_TRUE;
}

TZrBool ZrParser_Metadata_ApplyFunctionAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        SZrFunction *function,
        SZrFileRange location) {
    SZrArray names;
    TZrBool success;

    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(
            cs->state, &names, sizeof(SZrTypeDecoratorInfo),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
    success = metadata_apply_bound_attributes(
            cs, decorators, ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION,
            &function->decoratorMetadataValue, &function->hasDecoratorMetadata,
            &names, location);
    if (success) {
        success = metadata_append_function_names(
                cs, &names, &function->decoratorNames, &function->decoratorCount);
    }
    ZrCore_Array_Free(cs->state, &names);
    return success;
}

TZrBool ZrParser_Metadata_ApplyParameterAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        SZrFunctionMetadataParameter *parameter,
        SZrFileRange location) {
    SZrArray names;
    TZrBool success;

    if (cs == ZR_NULL || parameter == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(
            cs->state, &names, sizeof(SZrTypeDecoratorInfo),
            ZR_PARSER_INITIAL_CAPACITY_TINY);
    success = metadata_apply_bound_attributes(
            cs, decorators, ZR_PARSER_ATTRIBUTE_TARGET_PARAMETER,
            &parameter->decoratorMetadataValue, &parameter->hasDecoratorMetadata,
            &names, location);
    if (success) {
        success = metadata_append_function_names(
                cs, &names, &parameter->decoratorNames, &parameter->decoratorCount);
    }
    ZrCore_Array_Free(cs->state, &names);
    return success;
}
