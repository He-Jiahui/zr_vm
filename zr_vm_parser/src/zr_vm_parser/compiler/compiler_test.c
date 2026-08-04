#include "compiler_internal.h"
#include "compiler_attribute_binding.h"

#include <string.h>

static TZrBool compiler_test_string_equals(
        const SZrString *value,
        const TZrChar *literal) {
    TZrNativeString native;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }
    native = ZrCore_String_GetNativeStringShort((SZrString *)value);
    return native != ZR_NULL && strcmp(native, literal) == 0
           ? ZR_TRUE
           : ZR_FALSE;
}

static TZrChar *compiler_test_duplicate_text(
        SZrCompilerState *cs,
        const TZrChar *text) {
    TZrSize length;
    TZrChar *copy;

    if (cs == ZR_NULL || cs->state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    length = strlen(text);
    copy = (TZrChar *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            length + 1U,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (copy != ZR_NULL) {
        memcpy(copy, text, length + 1U);
    }
    return copy;
}

static TZrChar *compiler_test_build_qualified_name(
        SZrCompilerState *cs,
        const TZrChar *moduleId,
        const TZrChar *functionName) {
    TZrSize moduleLength;
    TZrSize functionLength;
    TZrChar *qualifiedName;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        moduleId == ZR_NULL || functionName == ZR_NULL) {
        return ZR_NULL;
    }
    moduleLength = strlen(moduleId);
    functionLength = strlen(functionName);
    qualifiedName = (TZrChar *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            moduleLength + 2U + functionLength + 1U,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (qualifiedName == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(qualifiedName, moduleId, moduleLength);
    qualifiedName[moduleLength] = ':';
    qualifiedName[moduleLength + 1U] = ':';
    memcpy(qualifiedName + moduleLength + 2U, functionName, functionLength + 1U);
    return qualifiedName;
}

static const SZrFunctionTypeInfo *compiler_test_find_function_identity(
        const SZrCompilerState *cs,
        const SZrAstNode *functionNode) {
    const SZrTypeEnvironment *environment;

    if (cs == ZR_NULL || functionNode == ZR_NULL) {
        return ZR_NULL;
    }
    for (environment = cs->typeEnv;
         environment != ZR_NULL;
         environment = environment->parent) {
        for (TZrSize index = 0U;
             index < environment->functionReturnTypes.length;
             index++) {
            SZrFunctionTypeInfo *const *candidate =
                    (SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                            (SZrArray *)&environment->functionReturnTypes,
                            index);

            if (candidate != ZR_NULL && *candidate != ZR_NULL &&
                (*candidate)->declarationNode == functionNode) {
                return *candidate;
            }
        }
    }
    return ZR_NULL;
}

static TZrBool compiler_test_type_is_plain_void(const SZrType *type) {
    return type != ZR_NULL && type->subType == ZR_NULL &&
                   type->dimensions == 0 &&
                   type->referenceAccess == ZR_REFERENCE_ACCESS_NONE &&
                   type->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
                   type->name != ZR_NULL &&
                   type->name->type == ZR_AST_IDENTIFIER_LITERAL &&
                   compiler_test_string_equals(
                           type->name->data.identifier.name, "void")
           ? ZR_TRUE
           : ZR_FALSE;
}

static TZrBool compiler_test_type_is_task_void(const SZrType *type) {
    const SZrType *leaf = type;
    const SZrGenericType *generic;
    const SZrAstNode *argument;

    while (leaf != ZR_NULL && leaf->subType != ZR_NULL) {
        leaf = leaf->subType;
    }
    if (leaf == ZR_NULL || leaf->dimensions != 0 ||
        leaf->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        leaf->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        leaf->name == ZR_NULL || leaf->name->type != ZR_AST_GENERIC_TYPE) {
        return ZR_FALSE;
    }
    generic = &leaf->name->data.genericType;
    if (generic->name == ZR_NULL ||
        !compiler_test_string_equals(generic->name->name, "Task") ||
        generic->params == ZR_NULL || generic->params->count != 1U) {
        return ZR_FALSE;
    }
    argument = generic->params->nodes[0];
    return argument != ZR_NULL && argument->type == ZR_AST_TYPE &&
           compiler_test_type_is_plain_void(&argument->data.type);
}

static void compiler_test_free_entry(
        SZrCompilerState *cs,
        SZrParserTestEntry *entry) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || entry == ZR_NULL) {
        return;
    }
    for (TZrUInt32 caseIndex = 0U; caseIndex < entry->caseCount; caseIndex++) {
        SZrParserTestCaseDescriptor *testCase = &entry->cases[caseIndex];
        for (TZrUInt32 argumentIndex = 0U;
             testCase->arguments != ZR_NULL &&
             argumentIndex < testCase->argumentCount;
             argumentIndex++) {
            SZrParserTestConstant *argument = &testCase->arguments[argumentIndex];
            if (argument->kind == ZR_PARSER_TEST_CONSTANT_STRING &&
                argument->value.stringValue != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        cs->state->global,
                        argument->value.stringValue,
                        strlen(argument->value.stringValue) + 1U,
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        if (testCase->arguments != ZR_NULL && testCase->argumentCount > 0U) {
            ZrCore_Memory_RawFreeWithType(
                    cs->state->global,
                    testCase->arguments,
                    sizeof(SZrParserTestConstant) * testCase->argumentCount,
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
    }
    if (entry->cases != ZR_NULL && entry->caseCount > 0U) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                entry->cases,
                sizeof(SZrParserTestCaseDescriptor) * entry->caseCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (entry->moduleId != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                entry->moduleId,
                strlen(entry->moduleId) + 1U,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (entry->qualifiedName != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                entry->qualifiedName,
                strlen(entry->qualifiedName) + 1U,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (entry->skipReason != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                entry->skipReason,
                strlen(entry->skipReason) + 1U,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Memory_RawSet(entry, 0, sizeof(*entry));
}

void compiler_test_free_entries(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !cs->testManifestEntries.isValid) {
        return;
    }
    for (TZrSize index = 0U; index < cs->testManifestEntries.length; index++) {
        compiler_test_free_entry(
                cs,
                (SZrParserTestEntry *)ZrCore_Array_Get(
                        &cs->testManifestEntries, index));
    }
    ZrCore_Array_Free(cs->state, &cs->testManifestEntries);
}

static TZrBool compiler_test_value_to_constant(
        SZrCompilerState *cs,
        const SZrTypeValue *value,
        SZrParserTestConstant *constant,
        SZrFileRange location) {
    if (cs == ZR_NULL || value == ZR_NULL || constant == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(constant, 0, sizeof(*constant));
    if (ZR_VALUE_IS_TYPE_NULL(value->type)) {
        constant->kind = ZR_PARSER_TEST_CONSTANT_NULL;
    } else if (ZR_VALUE_IS_TYPE_BOOL(value->type)) {
        constant->kind = ZR_PARSER_TEST_CONSTANT_BOOL;
        constant->value.boolValue = value->value.nativeObject.nativeBool
                                           ? ZR_TRUE
                                           : ZR_FALSE;
    } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        constant->kind = ZR_PARSER_TEST_CONSTANT_INT;
        constant->value.intValue = value->value.nativeObject.nativeInt64;
    } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        constant->kind = ZR_PARSER_TEST_CONSTANT_UINT;
        constant->value.uintValue = value->value.nativeObject.nativeUInt64;
    } else if (ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
        constant->kind = ZR_PARSER_TEST_CONSTANT_FLOAT;
        constant->value.floatValue = value->value.nativeObject.nativeDouble;
    } else if (ZR_VALUE_IS_TYPE_STRING(value->type) &&
               value->value.object != ZR_NULL) {
        constant->kind = ZR_PARSER_TEST_CONSTANT_STRING;
        constant->value.stringValue = compiler_test_duplicate_text(
                cs,
                ZrCore_String_GetNativeStringShort(
                        ZR_CAST_STRING(cs->state, value->value.object)));
        if (constant->value.stringValue == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs, "test.case_allocation: failed to retain string constant", location);
            return ZR_FALSE;
        }
    } else {
        ZrParser_Compiler_Error(
                cs,
                "test.case_constant: case arguments must be null, bool, numeric, or string compile-time constants",
                location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool compiler_test_bind_case(
        SZrCompilerState *cs,
        const SZrFunctionDeclaration *declaration,
        SZrFunctionCall *call,
        TZrUInt32 ordinal,
        SZrParserTestCaseDescriptor *testCase,
        SZrFileRange location) {
    SZrAstNodeArray *arguments;
    TZrSize parameterCount = declaration->params != ZR_NULL
                             ? declaration->params->count
                             : 0U;

    if (call == ZR_NULL || call->args == ZR_NULL ||
        call->args->count != parameterCount || parameterCount == 0U) {
        ZrParser_Compiler_Error(
                cs,
                "test.case_arguments: each case must bind exactly one constant per test parameter",
                location);
        return ZR_FALSE;
    }
    arguments = ZrParser_Compiler_MatchNamedArguments(
            cs, call, declaration->params);
    if (cs->hasError || arguments == ZR_NULL ||
        arguments->count != parameterCount) {
        if (arguments != ZR_NULL && arguments != call->args) {
            ZrParser_AstNodeArray_Free(cs->state, arguments);
        }
        return ZR_FALSE;
    }

    testCase->ordinal = ordinal;
    testCase->argumentCount = (TZrUInt32)parameterCount;
    testCase->arguments = (SZrParserTestConstant *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrParserTestConstant) * parameterCount,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (testCase->arguments == ZR_NULL) {
        if (arguments != call->args) {
            ZrParser_AstNodeArray_Free(cs->state, arguments);
        }
        ZrParser_Compiler_Error(
                cs, "test.case_allocation: failed to allocate bound constants", location);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(
            testCase->arguments,
            0,
            sizeof(SZrParserTestConstant) * parameterCount);

    for (TZrSize index = 0U; index < parameterCount; index++) {
        SZrAstNode *parameterNode = declaration->params->nodes[index];
        SZrAstNode *argumentNode = arguments->nodes[index];
        SZrInferredType expectedType;
        SZrInferredType actualType;
        SZrTypeValue value;
        TZrBool compatible;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER ||
            parameterNode->data.parameter.typeInfo == ZR_NULL ||
            argumentNode == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "test.case_signature: test parameters require explicit value TypeRefs",
                    location);
            goto failure;
        }
        ZrParser_InferredType_Init(
                cs->state, &expectedType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Init(
                cs->state, &actualType, ZR_VALUE_TYPE_OBJECT);
        compatible = ZrParser_AstTypeToInferredType_Convert(
                             cs,
                             parameterNode->data.parameter.typeInfo,
                             &expectedType) &&
                     ZrParser_ExpressionType_Infer(
                             cs, argumentNode, &actualType) &&
                     ZrParser_TypeCompatibility_Check(
                             cs,
                             &actualType,
                             &expectedType,
                             argumentNode->location);
        ZrParser_InferredType_Free(cs->state, &actualType);
        ZrParser_InferredType_Free(cs->state, &expectedType);
        if (!compatible || cs->hasError ||
            !ZrParser_Compiler_EvaluateCompileTimeExpression(
                    cs, argumentNode, &value) ||
            !compiler_test_value_to_constant(
                    cs,
                    &value,
                    &testCase->arguments[index],
                    argumentNode->location)) {
            goto failure;
        }
    }
    if (arguments != call->args) {
        ZrParser_AstNodeArray_Free(cs->state, arguments);
    }
    return ZR_TRUE;

failure:
    if (arguments != call->args) {
        ZrParser_AstNodeArray_Free(cs->state, arguments);
    }
    return ZR_FALSE;
}

static TZrBool compiler_test_bind_skip_reason(
        SZrCompilerState *cs,
        SZrFunctionCall *call,
        TZrChar **reason,
        SZrFileRange location) {
    SZrAstNode *argument;
    SZrTypeValue value;
    SZrString *stringValue;
    TZrNativeString native;

    if (call == ZR_NULL || call->args == ZR_NULL ||
        call->args->count != 1U) {
        ZrParser_Compiler_Error(
                cs,
                "test.skip_reason: skip requires exactly one compile-time string reason",
                location);
        return ZR_FALSE;
    }
    argument = call->args->nodes[0];
    if (call->argNames != ZR_NULL && call->argNames->length > 0U) {
        SZrString **argumentName = (SZrString **)ZrCore_Array_Get(
                call->argNames, 0U);
        if (argumentName != ZR_NULL && *argumentName != ZR_NULL &&
            !compiler_test_string_equals(*argumentName, "reason")) {
            ZrParser_Compiler_Error(
                    cs,
                    "test.skip_reason: the named skip argument must be reason",
                    location);
            return ZR_FALSE;
        }
    }
    if (argument == ZR_NULL ||
        !ZrParser_Compiler_EvaluateCompileTimeExpression(cs, argument, &value) ||
        !ZR_VALUE_IS_TYPE_STRING(value.type) || value.value.object == ZR_NULL) {
        if (!cs->hasError) {
            ZrParser_Compiler_Error(
                    cs,
                    "test.skip_reason: skip reason must be a compile-time string",
                    location);
        }
        return ZR_FALSE;
    }
    stringValue = ZR_CAST_STRING(cs->state, value.value.object);
    native = ZrCore_String_GetNativeStringShort(stringValue);
    if (native == ZR_NULL || native[0] == '\0') {
        ZrParser_Compiler_Error(
                cs,
                "test.skip_reason: skip reason must not be empty",
                location);
        return ZR_FALSE;
    }
    *reason = compiler_test_duplicate_text(cs, native);
    if (*reason == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "test.manifest_allocation: failed to retain skip reason", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool compiler_test_signature_is_valid(
        SZrCompilerState *cs,
        const SZrFunctionDeclaration *declaration,
        SZrFileRange location) {
    TZrSize parameterCount;

    if (declaration == ZR_NULL || declaration->name == ZR_NULL ||
        declaration->name->name == ZR_NULL || declaration->body == ZR_NULL ||
        declaration->args != ZR_NULL ||
        (declaration->generic != ZR_NULL &&
         declaration->generic->params != ZR_NULL &&
         declaration->generic->params->count > 0U) ||
        (!declaration->isAsync &&
         !compiler_test_type_is_plain_void(declaration->returnType)) ||
        (declaration->isAsync &&
         !compiler_test_type_is_task_void(declaration->returnType))) {
        ZrParser_Compiler_Error(
                cs,
                "test.signature: expected a non-generic ordinary fn returning explicit void or async fn returning Task<void>",
                location);
        return ZR_FALSE;
    }
    parameterCount = declaration->params != ZR_NULL
                     ? declaration->params->count
                     : 0U;
    for (TZrSize index = 0U; index < parameterCount; index++) {
        SZrAstNode *parameterNode = declaration->params->nodes[index];
        SZrParameter *parameter;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            ZrParser_Compiler_Error(
                    cs, "test.signature: invalid test parameter", location);
            return ZR_FALSE;
        }
        parameter = &parameterNode->data.parameter;
        if (parameter->typeInfo == ZR_NULL ||
            parameter->passingMode != ZR_PARAMETER_PASSING_MODE_VALUE ||
            parameter->defaultValue != ZR_NULL ||
            parameter->typeInfo->referenceAccess != ZR_REFERENCE_ACCESS_NONE) {
            ZrParser_Compiler_Error(
                    cs,
                    "test.signature: test parameters require explicit non-reference value TypeRefs without defaults",
                    parameterNode->location);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static SZrAstNodeArray *compiler_test_decorators_for_node(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return node->data.functionDeclaration.decorators;
        case ZR_AST_CLASS_FIELD:
            return node->data.classField.decorators;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.decorators;
        case ZR_AST_CLASS_PROPERTY:
            return node->data.classProperty.decorators;
        case ZR_AST_STRUCT_FIELD:
            return node->data.structField.decorators;
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.decorators;
        case ZR_AST_PROPERTY_DECLARATION:
            return node->data.propertyDeclaration.decorators;
        default:
            return ZR_NULL;
    }
}

static TZrBool compiler_test_node_has_test_role(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNodeArray *decorators = compiler_test_decorators_for_node(node);

    for (TZrSize index = 0U;
         decorators != ZR_NULL && index < decorators->count;
         index++) {
        EZrParserAttributeRole role;
        SZrFunctionCall *call;

        if (ZrParser_Metadata_ParseAttributeRole(
                    cs, decorators->nodes[index], &role, &call) &&
            role == ZR_PARSER_ATTRIBUTE_ROLE_TEST) {
            ZR_UNUSED_PARAMETER(call);
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool compiler_test_validate_non_module_roles(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    SZrAstNodeArray *decorators;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }
    decorators = compiler_test_decorators_for_node(node);
    for (TZrSize index = 0U;
         decorators != ZR_NULL && index < decorators->count;
         index++) {
        EZrParserAttributeRole role;
        SZrFunctionCall *call;

        if (!ZrParser_Metadata_ParseAttributeRole(
                    cs, decorators->nodes[index], &role, &call)) {
            continue;
        }
        ZR_UNUSED_PARAMETER(call);
        if (role == ZR_PARSER_ATTRIBUTE_ROLE_TEST ||
            role == ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE ||
            role == ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP) {
            ZrParser_Compiler_Error(
                    cs,
                    "test.target: zr.testing test roles are valid only on module-scope ordinary functions",
                    decorators->nodes[index]->location);
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool compiler_test_validate_production_reference(
        SZrCompilerState *cs,
        const SZrAstNode *declarationNode,
        SZrFileRange location) {
    if (cs == ZR_NULL || declarationNode == ZR_NULL ||
        declarationNode->type != ZR_AST_FUNCTION_DECLARATION ||
        cs->emitTestManifest ||
        !compiler_test_node_has_test_role(cs, (SZrAstNode *)declarationNode)) {
        return ZR_TRUE;
    }
    if (cs->currentFunctionNode != ZR_NULL &&
        compiler_test_node_has_test_role(cs, cs->currentFunctionNode)) {
        return ZR_TRUE;
    }
    ZrParser_Compiler_Error(
            cs,
            "test.production_reference: production code cannot reference a test-only function",
            location);
    return ZR_FALSE;
}

TZrBool compiler_test_bind_function(
        SZrCompilerState *cs,
        SZrAstNode *functionNode,
        SZrFunction *function,
        TZrUInt32 callableChildIndex,
        TZrBool *isTest) {
    SZrFunctionDeclaration *declaration;
    TZrSize testCount = 0U;
    TZrSize caseCount = 0U;
    TZrSize skipCount = 0U;
    SZrParserTestEntry entry;
    const SZrFunctionTypeInfo *functionIdentity;
    const TZrChar *functionName;

    if (isTest != ZR_NULL) {
        *isTest = ZR_FALSE;
    }
    if (cs == ZR_NULL || functionNode == ZR_NULL || function == ZR_NULL ||
        isTest == ZR_NULL ||
        functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &functionNode->data.functionDeclaration;
    for (TZrSize index = 0U;
         declaration->decorators != ZR_NULL &&
         index < declaration->decorators->count;
         index++) {
        EZrParserAttributeRole role;
        SZrFunctionCall *call;

        if (!ZrParser_Metadata_ParseAttributeRole(
                    cs, declaration->decorators->nodes[index], &role, &call)) {
            continue;
        }
        if (role == ZR_PARSER_ATTRIBUTE_ROLE_TEST) {
            testCount++;
            if (call != ZR_NULL && call->args != ZR_NULL &&
                call->args->count > 0U) {
                ZrParser_Compiler_Error(
                        cs,
                        "test.arguments: zr.testing.test does not accept arguments",
                        declaration->decorators->nodes[index]->location);
                return ZR_FALSE;
            }
        } else if (role == ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE) {
            caseCount++;
        } else if (role == ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP) {
            skipCount++;
        }
    }
    if (testCount == 0U && caseCount == 0U && skipCount == 0U) {
        return ZR_TRUE;
    }
    if (testCount != 1U || skipCount > 1U) {
        ZrParser_Compiler_Error(
                cs,
                testCount == 0U
                        ? "test.role: case and skip require exactly one zr.testing.test role"
                        : "test.role: test and skip roles are not repeatable",
                functionNode->location);
        return ZR_FALSE;
    }
    *isTest = ZR_TRUE;
    if (!cs->isScriptLevel || cs->currentFunctionNode != ZR_NULL) {
        ZrParser_Compiler_Error(
                cs,
                "test.target: zr.testing.test is valid only on module-scope ordinary functions",
                functionNode->location);
        return ZR_FALSE;
    }
    if (!compiler_test_signature_is_valid(cs, declaration, functionNode->location)) {
        return ZR_FALSE;
    }
    if (declaration->params != ZR_NULL && declaration->params->count > 0U &&
        caseCount == 0U) {
        ZrParser_Compiler_Error(
                cs,
                "test.case_required: parameterized tests require at least one zr.testing.case",
                functionNode->location);
        return ZR_FALSE;
    }
    if ((declaration->params == ZR_NULL || declaration->params->count == 0U) &&
        caseCount > 0U) {
        ZrParser_Compiler_Error(
                cs,
                "test.case_arguments: parameterless tests cannot declare cases",
                functionNode->location);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(&entry, 0, sizeof(entry));
    entry.callableChildIndex = callableChildIndex;
    entry.moduleId = compiler_test_duplicate_text(
            cs,
            cs->currentModuleKey != ZR_NULL
                    ? ZrCore_String_GetNativeStringShort(cs->currentModuleKey)
                    : "main");
    functionName = ZrCore_String_GetNativeStringShort(declaration->name->name);
    entry.qualifiedName = compiler_test_build_qualified_name(
            cs, entry.moduleId, functionName);
    if (entry.moduleId == ZR_NULL || entry.qualifiedName == ZR_NULL) {
        compiler_test_free_entry(cs, &entry);
        ZrParser_Compiler_Error(
                cs, "test.manifest_allocation: failed to retain test identity", functionNode->location);
        return ZR_FALSE;
    }
    functionIdentity = compiler_test_find_function_identity(cs, functionNode);
    if (functionIdentity == ZR_NULL ||
        functionIdentity->symbolId == ZR_SEMANTIC_ID_INVALID ||
        functionIdentity->typeId == ZR_SEMANTIC_ID_INVALID) {
        compiler_test_free_entry(cs, &entry);
        ZrParser_Compiler_Error(
                cs,
                "test.semantic_identity: test function has no canonical symbol/type identity",
                functionNode->location);
        return ZR_FALSE;
    }
    entry.functionSymbolId = functionIdentity->symbolId;
    entry.functionTypeId = functionIdentity->typeId;
    entry.sourceRange = functionNode->location;
    entry.isAsync = declaration->isAsync;
    entry.caseCount = (TZrUInt32)caseCount;
    if (caseCount > 0U) {
        entry.cases = (SZrParserTestCaseDescriptor *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrParserTestCaseDescriptor) * caseCount,
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (entry.cases == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs, "test.manifest_allocation: failed to allocate cases", functionNode->location);
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(
                entry.cases,
                0,
                sizeof(SZrParserTestCaseDescriptor) * caseCount);
    }

    for (TZrSize index = 0U, caseIndex = 0U;
         declaration->decorators != ZR_NULL &&
         index < declaration->decorators->count;
         index++) {
        EZrParserAttributeRole role;
        SZrFunctionCall *call;
        SZrAstNode *decorator = declaration->decorators->nodes[index];

        if (!ZrParser_Metadata_ParseAttributeRole(cs, decorator, &role, &call)) {
            continue;
        }
        if (role == ZR_PARSER_ATTRIBUTE_ROLE_TEST_CASE) {
            if (!compiler_test_bind_case(
                        cs,
                        declaration,
                        call,
                        (TZrUInt32)caseIndex,
                        &entry.cases[caseIndex],
                        decorator->location)) {
                compiler_test_free_entry(cs, &entry);
                return ZR_FALSE;
            }
            caseIndex++;
        } else if (role == ZR_PARSER_ATTRIBUTE_ROLE_TEST_SKIP &&
                   !compiler_test_bind_skip_reason(
                           cs, call, &entry.skipReason, decorator->location)) {
            compiler_test_free_entry(cs, &entry);
            return ZR_FALSE;
        }
    }

    if (cs->emitTestManifest) {
        TZrSize oldLength = cs->testManifestEntries.length;
        ZrCore_Array_Push(cs->state, &cs->testManifestEntries, &entry);
        if (cs->testManifestEntries.length != oldLength + 1U) {
            compiler_test_free_entry(cs, &entry);
            ZrParser_Compiler_Error(
                    cs, "test.manifest_allocation: failed to retain test entry", functionNode->location);
            return ZR_FALSE;
        }
    } else {
        compiler_test_free_entry(cs, &entry);
    }
    return ZR_TRUE;
}

TZrBool compiler_test_finalize_manifest(SZrCompilerState *cs) {
    SZrParserTestManifest manifest;
    TZrByte *encodedData = ZR_NULL;
    TZrUInt32 encodedLength = 0U;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->currentFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!cs->emitTestManifest || cs->testManifestEntries.length == 0U) {
        return ZR_TRUE;
    }
    if (cs->testManifestEntries.length > UINT32_MAX) {
        ZrParser_Compiler_Error(
                cs, "test.manifest_limit: too many test entries", cs->currentAst->location);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(&manifest, 0, sizeof(manifest));
    manifest.schemaVersion = ZR_PARSER_TEST_MANIFEST_SCHEMA_VERSION;
#if defined(_WIN32)
    manifest.targetTriple = "windows-native";
#else
    manifest.targetTriple = "unix-native";
#endif
    manifest.moduleGraphHash = cs->currentFunction->moduleSignatureHash;
    if (manifest.moduleGraphHash == 0U) {
        ZrParser_Compiler_Error(
                cs,
                "test.module_graph_identity: typed module metadata has no canonical signature hash",
                cs->currentAst->location);
        return ZR_FALSE;
    }
    manifest.entries = (SZrParserTestEntry *)cs->testManifestEntries.head;
    manifest.entryCount = (TZrUInt32)cs->testManifestEntries.length;
    if (!ZrParser_TestManifest_Encode(
                cs->state,
                &manifest,
                &encodedData,
                &encodedLength)) {
        ZrParser_Compiler_Error(
                cs, "test.manifest_encode: failed to encode TestManifest", cs->currentAst->location);
        return ZR_FALSE;
    }
    cs->currentFunction->testManifestData = encodedData;
    cs->currentFunction->testManifestDataLength = (TZrSize)encodedLength;
    return ZR_TRUE;
}
