#include "semantic/semantic_analyzer_union_patterns.h"

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
TZrBool try_resolve_union_variant_pattern_expression(SZrCompilerState *cs,
                                                     SZrAstNode *node,
                                                     SZrString **outVariantName,
                                                     SZrAstNodeArray **outBindings,
                                                     SZrAstNode **outVariant);
TZrBool try_resolve_union_variant_pattern_for_type(SZrCompilerState *cs,
                                                   SZrAstNode *node,
                                                   SZrString *typeName,
                                                   SZrString **outVariantName,
                                                   SZrAstNodeArray **outBindings,
                                                   SZrAstNode **outVariant);
TZrBool try_resolve_union_variant_pattern_with_type_annotation(SZrCompilerState *cs,
                                                               SZrAstNode *pattern,
                                                               SZrType *variantTypeInfo,
                                                               SZrString *resourceTypeName,
                                                               SZrString **outVariantName,
                                                               SZrAstNodeArray **outBindings,
                                                               SZrAstNode **outVariant);

static TZrBool union_lsp_switch_case_can_use_subject_union_type(SZrAstNode *caseValue) {
    return caseValue != ZR_NULL &&
           (caseValue->type == ZR_AST_IDENTIFIER_LITERAL ||
            caseValue->type == ZR_AST_PRIMARY_EXPRESSION);
}

static SZrParameter *union_lsp_variant_field_parameter(SZrAstNode *variant, TZrSize payloadIndex) {
    SZrAstNodeArray *fields;
    SZrAstNode *fieldNode;

    if (variant == ZR_NULL ||
        variant->type != ZR_AST_UNION_VARIANT ||
        variant->data.unionVariant.fields == ZR_NULL ||
        payloadIndex >= variant->data.unionVariant.fields->count) {
        return ZR_NULL;
    }

    fields = variant->data.unionVariant.fields;
    if (fields->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNode = fields->nodes[payloadIndex];
    return fieldNode != ZR_NULL && fieldNode->type == ZR_AST_PARAMETER
        ? &fieldNode->data.parameter
        : ZR_NULL;
}

static SZrString *union_lsp_binding_name(SZrAstNode *bindingNode) {
    return bindingNode != ZR_NULL &&
           bindingNode->type == ZR_AST_IDENTIFIER_LITERAL
        ? bindingNode->data.identifier.name
        : ZR_NULL;
}

static TZrBool union_lsp_text_equals_span(const TZrChar *text,
                                          const TZrChar *span,
                                          TZrSize spanLength) {
    return text != ZR_NULL &&
           span != ZR_NULL &&
           strlen(text) == spanLength &&
           strncmp(text, span, spanLength) == 0;
}

static void union_lsp_trim_span(const TZrChar **span, TZrSize *length) {
    const TZrChar *start;
    TZrSize valueLength;

    if (span == ZR_NULL || length == ZR_NULL || *span == ZR_NULL) {
        return;
    }

    start = *span;
    valueLength = *length;
    while (valueLength > 0 &&
           (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')) {
        start++;
        valueLength--;
    }
    while (valueLength > 0) {
        TZrChar last = start[valueLength - 1];
        if (last != ' ' && last != '\t' && last != '\r' && last != '\n') {
            break;
        }
        valueLength--;
    }

    *span = start;
    *length = valueLength;
}

static TZrBool union_lsp_primitive_type_from_span(const TZrChar *span,
                                                  TZrSize length,
                                                  EZrValueType *outType) {
    if (span == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    if (union_lsp_text_equals_span("bool", span, length)) {
        *outType = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (union_lsp_text_equals_span("int", span, length) ||
        union_lsp_text_equals_span("int64", span, length)) {
        *outType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (union_lsp_text_equals_span("int32", span, length)) {
        *outType = ZR_VALUE_TYPE_INT32;
        return ZR_TRUE;
    }
    if (union_lsp_text_equals_span("float", span, length) ||
        union_lsp_text_equals_span("double", span, length)) {
        *outType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }
    if (union_lsp_text_equals_span("string", span, length)) {
        *outType = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static SZrString *union_lsp_create_base_type_name(SZrState *state, const TZrChar *resourceText) {
    const TZrChar *genericStart;

    if (state == ZR_NULL || resourceText == ZR_NULL) {
        return ZR_NULL;
    }

    genericStart = strchr(resourceText, '<');
    if (genericStart == ZR_NULL || genericStart == resourceText) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state,
                                (TZrNativeString)resourceText,
                                (TZrSize)(genericStart - resourceText));
}

static TZrBool union_lsp_generic_argument_span_at(const TZrChar *resourceText,
                                                  TZrSize wantedIndex,
                                                  const TZrChar **outSpan,
                                                  TZrSize *outLength) {
    const TZrChar *genericStart;
    const TZrChar *cursor;
    const TZrChar *argStart;
    TZrSize index = 0;
    TZrInt32 depth = 0;

    if (resourceText == ZR_NULL || outSpan == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    genericStart = strchr(resourceText, '<');
    if (genericStart == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = genericStart + 1;
    argStart = cursor;
    for (; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            depth++;
            continue;
        }
        if (*cursor == '>') {
            if (depth == 0) {
                if (index == wantedIndex) {
                    *outSpan = argStart;
                    *outLength = (TZrSize)(cursor - argStart);
                    union_lsp_trim_span(outSpan, outLength);
                    return *outLength > 0;
                }
                return ZR_FALSE;
            }
            depth--;
            continue;
        }
        if (*cursor == ',' && depth == 0) {
            if (index == wantedIndex) {
                *outSpan = argStart;
                *outLength = (TZrSize)(cursor - argStart);
                union_lsp_trim_span(outSpan, outLength);
                return *outLength > 0;
            }
            index++;
            argStart = cursor + 1;
        }
    }

    return ZR_FALSE;
}

static void union_lsp_apply_generic_payload_substitution(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         const SZrInferredType *resourceType,
                                                         SZrInferredType *payloadType) {
    const TZrChar *resourceText;
    const TZrChar *genericNameText;
    SZrString *baseName;
    SZrTypePrototypeInfo *prototype;
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        resourceType == ZR_NULL || resourceType->typeName == ZR_NULL ||
        payloadType == ZR_NULL || payloadType->typeName == ZR_NULL) {
        return;
    }

    resourceText = semantic_string_native(resourceType->typeName);
    genericNameText = semantic_string_native(payloadType->typeName);
    if (resourceText == ZR_NULL || genericNameText == ZR_NULL || strchr(resourceText, '<') == ZR_NULL) {
        return;
    }

    baseName = union_lsp_create_base_type_name(state, resourceText);
    prototype = baseName != ZR_NULL
        ? find_compiler_type_prototype_inference(analyzer->compilerState, baseName)
        : ZR_NULL;
    if (prototype == ZR_NULL ||
        prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION ||
        !prototype->genericParameters.isValid) {
        return;
    }

    for (index = 0; index < prototype->genericParameters.length; index++) {
        SZrTypeGenericParameterInfo *genericInfo =
            (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&prototype->genericParameters, index);
        const TZrChar *argSpan = ZR_NULL;
        TZrSize argLength = 0;
        EZrValueType primitiveType;
        EZrOwnershipQualifier ownershipQualifier;
        TZrBool isNullable;

        if (genericInfo == ZR_NULL ||
            genericInfo->name == ZR_NULL ||
            !union_lsp_text_equals_span(semantic_string_native(genericInfo->name),
                                        genericNameText,
                                        strlen(genericNameText)) ||
            !union_lsp_generic_argument_span_at(resourceText, index, &argSpan, &argLength)) {
            continue;
        }

        ownershipQualifier = payloadType->ownershipQualifier;
        isNullable = payloadType->isNullable;
        if (union_lsp_primitive_type_from_span(argSpan, argLength, &primitiveType)) {
            ZrParser_InferredType_Free(state, payloadType);
            ZrParser_InferredType_Init(state, payloadType, primitiveType);
        } else {
            SZrString *argName = ZrCore_String_Create(state, (TZrNativeString)argSpan, argLength);
            if (argName == ZR_NULL) {
                return;
            }
            ZrParser_InferredType_Free(state, payloadType);
            ZrParser_InferredType_InitFull(state, payloadType, ZR_VALUE_TYPE_OBJECT, isNullable, argName);
        }
        payloadType->ownershipQualifier = ownershipQualifier;
        payloadType->isNullable = isNullable;
        return;
    }
}

static SZrInferredType *union_lsp_create_payload_type(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      const SZrSemanticUnionPatternResolution *resolution,
                                                      SZrParameter *field,
                                                      SZrAstNode *bindingNode) {
    SZrInferredType *typeInfo;
    TZrBool moveBinding;

    if (state == ZR_NULL || analyzer == ZR_NULL || field == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    if (field->typeInfo != ZR_NULL) {
        (void)ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
                analyzer,
                ZR_NULL,
                analyzer->compilerState != ZR_NULL ? analyzer->compilerState->currentFunctionNode : ZR_NULL,
                field->typeInfo,
                typeInfo);
    }

    if (resolution != ZR_NULL && resolution->hasResourceType) {
        union_lsp_apply_generic_payload_substitution(state,
                                                     analyzer,
                                                     &resolution->resourceType,
                                                     typeInfo);
    }

    moveBinding = bindingNode != ZR_NULL &&
                  bindingNode->type == ZR_AST_IDENTIFIER_LITERAL &&
                  bindingNode->data.identifier.isMoveBinding;
    if (!moveBinding &&
        (typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
         typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
         typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_LOANED ||
         typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED)) {
        typeInfo->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
    }

    return typeInfo;
}

static void union_lsp_register_payload_binding(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *bindingNode,
                                               SZrInferredType *typeInfo,
                                               TZrBool registerSymbols,
                                               TZrBool registerTypeEnv) {
    SZrString *bindingName = union_lsp_binding_name(bindingNode);
    SZrSymbol *symbol = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || bindingName == ZR_NULL || typeInfo == ZR_NULL) {
        return;
    }

    if (registerSymbols && analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                 analyzer->symbolTable,
                                                 ZR_SYMBOL_VARIABLE,
                                                 bindingName,
                                                 bindingNode->location,
                                                 typeInfo,
                                                 ZR_ACCESS_PRIVATE,
                                                 bindingNode,
                                                 &symbol);
        if (symbol != ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                                      symbol,
                                                                      ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                                                      typeInfo,
                                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
        }
    }

    if (registerTypeEnv &&
        analyzer->compilerState != ZR_NULL &&
        analyzer->compilerState->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterVariableEx(state,
                                                    analyzer->compilerState->typeEnv,
                                                    bindingName,
                                                    typeInfo,
                                                    bindingNode,
                                                    bindingNode->location);
    }
}

void ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(
        SZrState *state,
        SZrSemanticUnionPatternResolution *resolution) {
    if (resolution == ZR_NULL) {
        return;
    }

    ZR_UNUSED_PARAMETER(state);
    memset(resolution, 0, sizeof(*resolution));
}

void ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(
        SZrState *state,
        SZrSemanticUnionPatternResolution *resolution) {
    if (state == ZR_NULL || resolution == ZR_NULL) {
        return;
    }

    if (resolution->hasResourceType) {
        ZrParser_InferredType_Free(state, &resolution->resourceType);
    }
    memset(resolution, 0, sizeof(*resolution));
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveUsingUnionPattern(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrUsingStatement *usingStmt,
        SZrSemanticUnionPatternResolution *resolution) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        usingStmt == ZR_NULL || usingStmt->guardKind != ZR_USING_GUARD_PATTERN ||
        usingStmt->pattern == ZR_NULL || usingStmt->resource == ZR_NULL || resolution == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(state, resolution);
    ZrParser_InferredType_Init(state, &resolution->resourceType, ZR_VALUE_TYPE_OBJECT);
    resolution->hasResourceType = ZR_TRUE;
    if (!ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state,
                                                                    analyzer,
                                                                    usingStmt->resource,
                                                                    &resolution->resourceType)) {
        if (analyzer->compilerState->hasError) {
            ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state,
                                                                             analyzer,
                                                                             usingStmt->resource->location);
        }
        return ZR_FALSE;
    }

    if (resolution->resourceType.typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (usingStmt->guardTypeInfo != ZR_NULL) {
        (void)try_resolve_union_variant_pattern_with_type_annotation(analyzer->compilerState,
                                                                     usingStmt->pattern,
                                                                     usingStmt->guardTypeInfo,
                                                                     resolution->resourceType.typeName,
                                                                     &resolution->variantName,
                                                                     &resolution->bindings,
                                                                     &resolution->variant);
    } else {
        (void)try_resolve_union_variant_pattern_for_type(analyzer->compilerState,
                                                         usingStmt->pattern,
                                                         resolution->resourceType.typeName,
                                                         &resolution->variantName,
                                                         &resolution->bindings,
                                                         &resolution->variant);
    }

    if (analyzer->compilerState->hasError) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(
                state,
                analyzer,
                usingStmt->pattern != ZR_NULL ? usingStmt->pattern->location : usingStmt->resource->location);
    }

    return resolution->variant != ZR_NULL;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveSwitchUnionPattern(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *caseValue,
        const SZrInferredType *subjectType,
        SZrSemanticUnionPatternResolution *resolution) {
    SZrTypePrototypeInfo *prototype;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        caseValue == ZR_NULL || resolution == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(state, resolution);
    if (subjectType != ZR_NULL) {
        ZrParser_InferredType_Init(state, &resolution->resourceType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(state, &resolution->resourceType, subjectType);
        resolution->hasResourceType = ZR_TRUE;
    }

    prototype = subjectType != ZR_NULL && subjectType->typeName != ZR_NULL
        ? find_compiler_type_prototype_inference(analyzer->compilerState, subjectType->typeName)
        : ZR_NULL;
    if (prototype == ZR_NULL &&
        subjectType != ZR_NULL &&
        subjectType->typeName != ZR_NULL) {
        const TZrChar *subjectTypeText = semantic_string_native(subjectType->typeName);
        SZrString *baseName = subjectTypeText != ZR_NULL && strchr(subjectTypeText, '<') != ZR_NULL
            ? union_lsp_create_base_type_name(state, subjectTypeText)
            : ZR_NULL;
        prototype = baseName != ZR_NULL
            ? find_compiler_type_prototype_inference(analyzer->compilerState, baseName)
            : ZR_NULL;
    }

    if (prototype == ZR_NULL || prototype->type != ZR_OBJECT_PROTOTYPE_TYPE_UNION) {
        return ZR_FALSE;
    }

    if (subjectType != ZR_NULL &&
        subjectType->typeName != ZR_NULL &&
        union_lsp_switch_case_can_use_subject_union_type(caseValue)) {
        (void)try_resolve_union_variant_pattern_for_type(analyzer->compilerState,
                                                         caseValue,
                                                         subjectType->typeName,
                                                         &resolution->variantName,
                                                         &resolution->bindings,
                                                         &resolution->variant);
    } else {
        (void)try_resolve_union_variant_pattern_expression(analyzer->compilerState,
                                                           caseValue,
                                                           &resolution->variantName,
                                                           &resolution->bindings,
                                                           &resolution->variant);
    }

    if (analyzer->compilerState->hasError) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state,
                                                                         analyzer,
                                                                         caseValue->location);
    }

    return resolution->variant != ZR_NULL;
}

void ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticUnionPatternResolution *resolution,
        TZrBool registerSymbols,
        TZrBool registerTypeEnv) {
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL || resolution == ZR_NULL ||
        resolution->bindings == ZR_NULL || resolution->bindings->nodes == ZR_NULL ||
        resolution->variant == ZR_NULL) {
        return;
    }

    for (index = 0; index < resolution->bindings->count; index++) {
        SZrAstNode *bindingNode = resolution->bindings->nodes[index];
        SZrParameter *field = union_lsp_variant_field_parameter(resolution->variant, index);
        SZrInferredType *typeInfo;

        if (bindingNode == ZR_NULL || union_lsp_binding_name(bindingNode) == ZR_NULL) {
            continue;
        }

        typeInfo = union_lsp_create_payload_type(state, analyzer, resolution, field, bindingNode);
        if (typeInfo == ZR_NULL) {
            continue;
        }

        union_lsp_register_payload_binding(state,
                                           analyzer,
                                           bindingNode,
                                           typeInfo,
                                           registerSymbols,
                                           registerTypeEnv);
        if (!registerSymbols) {
            ZrParser_InferredType_Free(state, typeInfo);
            ZrCore_Memory_RawFree(state->global, typeInfo, sizeof(SZrInferredType));
        }
    }
}
