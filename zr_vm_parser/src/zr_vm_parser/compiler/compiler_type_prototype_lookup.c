#include "compiler_type_prototype_lookup.h"

#include "compiler_type_value_alias.h"
#include "semantic/type_declaration_context.h"
#include "type_inference_generic_prototype.h"
#include "type_inference_internal.h"
#include "type_inference_nominal_prototype.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/semantic_type_use.h"

#include <stdio.h>
#include <string.h>

static SZrTypePrototypeInfo *find_compiler_type_prototype_with_initial_alias_policy(
        SZrCompilerState *cs, SZrString *typeName, TZrBool allowInitialRootAlias);

static SZrTypePrototypeInfo *find_registered_type_prototype_exact_only(SZrCompilerState *cs,
                                                                       SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL &&
            type_inference_prototype_is_root_visible(info) &&
            type_inference_generic_prototype_is_root_visible(cs, info) &&
            ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL &&
        type_inference_prototype_is_root_visible(cs->currentTypePrototypeInfo) &&
        type_inference_generic_prototype_is_root_visible(cs, cs->currentTypePrototypeInfo) &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

static TZrBool normalize_named_annotation_context(
        const SZrType *annotation, SZrSyntaxContext *outRawContext,
        SZrSyntaxContext *outItemContext, TZrSize depth) {
    const SZrIdentifier *identifier = ZR_NULL;
    SZrSyntaxContext rawContext = ZrParser_SyntaxContext_Root();
    SZrSyntaxContext itemContext = rawContext;

    if (annotation == ZR_NULL ||
        depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }
    if (annotation->name != ZR_NULL) {
        if (annotation->name->type == ZR_AST_IDENTIFIER_LITERAL) {
            identifier = &annotation->name->data.identifier;
        } else if (annotation->name->type == ZR_AST_GENERIC_TYPE) {
            identifier = annotation->name->data.genericType.name;
            if (identifier == ZR_NULL) {
                return ZR_FALSE;
            }
        }
    }
    if (identifier != ZR_NULL) {
        rawContext = identifier->syntaxContext;
        if (!type_declaration_normalize_context(rawContext, &itemContext)) {
            return ZR_FALSE;
        }
    }
    if (annotation->name != ZR_NULL && annotation->name->type == ZR_AST_GENERIC_TYPE) {
        const SZrAstNodeArray *arguments = annotation->name->data.genericType.params;

        for (TZrSize index = 0U; arguments != ZR_NULL && index < arguments->count; index++) {
            const SZrAstNode *argument = arguments->nodes[index];
            SZrSyntaxContext argumentRawContext;
            SZrSyntaxContext argumentItemContext;

            if (argument != ZR_NULL && argument->type == ZR_AST_TYPE &&
                !normalize_named_annotation_context(&argument->data.type,
                        &argumentRawContext, &argumentItemContext, depth + 1U)) {
                return ZR_FALSE;
            }
        }
    }
    if (annotation->subType != ZR_NULL) {
        SZrSyntaxContext subRawContext;
        SZrSyntaxContext subItemContext;

        if (!normalize_named_annotation_context(annotation->subType,
                    &subRawContext, &subItemContext, depth + 1U)) {
            return ZR_FALSE;
        }
    }
    *outRawContext = rawContext;
    *outItemContext = itemContext;
    return ZR_TRUE;
}

static TZrBool find_annotation_alias(SZrCompilerState *cs, SZrString *name,
        SZrSyntaxContext rawContext, const SZrTypeBinding **outBinding) {
    SZrSyntaxContext localContext =
            ZrParser_SyntaxContext_Normalize(rawContext, ZR_SYNTAX_NAMESPACE_LOCAL);

    *outBinding = ZR_NULL;
    if (!ZrParser_SyntaxContext_IsValid(localContext)) {
        return ZR_FALSE;
    }
    for (TZrSize index = cs->typeValueAliases.isValid ? cs->typeValueAliases.length : 0U;
         index > 0U; index--) {
        const SZrTypeBinding *binding = ZrCore_Array_Get(&cs->typeValueAliases, index - 1U);
        SZrSyntaxContext bindingContext;

        if (binding == ZR_NULL || binding->name == ZR_NULL ||
            !ZrCore_String_Equal(binding->name, name)) {
            continue;
        }
        if (!ZrParser_SyntaxContext_IsValid(binding->syntaxContext)) {
            return ZR_FALSE;
        }
        bindingContext = ZrParser_SyntaxContext_Normalize(
                binding->syntaxContext, ZR_SYNTAX_NAMESPACE_LOCAL);
        if (!ZrParser_SyntaxContext_IsValid(bindingContext)) {
            return ZR_FALSE;
        }
        if (ZrParser_SyntaxContext_Equal(localContext, bindingContext)) {
            *outBinding = binding;
            break;
        }
    }
    return ZR_TRUE;
}

static TZrBool root_generic_base_is_unpublished(SZrCompilerState *cs,
        SZrString *baseName, TZrBool *outUnpublished) {
    *outUnpublished = ZR_FALSE;
    for (TZrSize index = 0U; index <= cs->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype = index < cs->typePrototypes.length
                ? ZrCore_Array_Get(&cs->typePrototypes, index) : cs->currentTypePrototypeInfo;
        SZrSyntaxContext declarationContext;

        if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
            !ZrCore_String_Equal(prototype->name, baseName)) {
            continue;
        }
        if (prototype->declarationNode == ZR_NULL) {
            return ZR_TRUE;
        }
        if (!type_declaration_item_context(prototype->declarationNode, &declarationContext)) {
            return ZR_FALSE;
        }
        if (ZrParser_SyntaxContext_Equal(declarationContext, ZrParser_SyntaxContext_Root())) {
            return ZR_TRUE;
        }
    }
    *outUnpublished = ZR_TRUE;
    return ZR_TRUE;
}

static SZrTypePrototypeInfo *find_compiler_annotation_generic_prototype(
        SZrCompilerState *cs, const SZrInferredType *type) {
    SZrTypePrototypeInfo *prototype = type_inference_find_nominal_prototype(cs, type);
    TZrTypeId instanceTypeId;

    if (prototype == ZR_NULL || type->nominalDefinitionToken != 0U ||
        !type_inference_generic_prototype_is_root_visible(cs, prototype)) {
        return prototype;
    }
    instanceTypeId = prototype->genericInstanceTypeId;
    if (instanceTypeId == ZR_SEMANTIC_ID_INVALID) {
        return prototype;
    }
    for (TZrSize index = 0U; index <= cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *candidate = index < cs->typePrototypes.length
                ? ZrCore_Array_Get(&cs->typePrototypes, index) : cs->currentTypePrototypeInfo;

        if (candidate != ZR_NULL && candidate->genericInstanceTypeId == instanceTypeId &&
            type_inference_prototype_is_root_visible(candidate) &&
            type_inference_generic_prototype_is_root_visible(cs, candidate)) {
            return candidate;
        }
    }
    return prototype;
}

static TZrBool resolve_owned_annotation_type(SZrCompilerState *cs,
        const SZrInferredType *type, TZrBool allowRootDeferral,
        SZrTypePrototypeInfo **outPrototype) {
    if (type->typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    if (type->baseType == ZR_VALUE_TYPE_OBJECT && type->elementTypes.length > 0U) {
        if (!ensure_generic_instance_type_prototype_from_inferred(cs, type)) {
            return allowRootDeferral && type->nominalDefinitionToken == 0U && !cs->hasError;
        }
        *outPrototype = find_compiler_annotation_generic_prototype(cs, type);
        return *outPrototype != ZR_NULL && !cs->hasError;
    }
    if (type->nominalDefinitionToken != 0U) {
        *outPrototype = type_inference_find_nominal_prototype(cs, type);
        return *outPrototype != ZR_NULL && !cs->hasError;
    }
    *outPrototype = find_compiler_type_prototype(cs, type->typeName);
    return !cs->hasError;
}

static void compiler_publish_resolved_annotation_type(SZrCompilerState *cs,
        const SZrType *annotation, const SZrTypePrototypeInfo *prototype) {
    SZrSemanticContext *context = cs->semanticContext;
    TZrTypeId definitionTypeId = ZR_SEMANTIC_ID_INVALID;
    TZrSymbolId definitionSymbolId = ZR_SEMANTIC_ID_INVALID;
    TZrTypeId typeId;
    const SZrCanonicalTypeNode *canonical;
    const SZrSemanticReferenceFact *existing;

    if (context == ZR_NULL || prototype == ZR_NULL ||
        prototype->declarationNode == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol = ZrCore_Array_Get(&context->symbols, index);

        if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->astNode != prototype->declarationNode ||
            symbol->id == ZR_SEMANTIC_ID_INVALID ||
            symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        if (definitionSymbolId != ZR_SEMANTIC_ID_INVALID &&
            (definitionSymbolId != symbol->id || definitionTypeId != symbol->typeId)) {
            return;
        }
        definitionTypeId = symbol->typeId;
        definitionSymbolId = symbol->id;
    }
    typeId = prototype->genericInstanceTypeId != ZR_SEMANTIC_ID_INVALID
            ? prototype->genericInstanceTypeId : definitionTypeId;
    canonical = ZrParser_CanonicalType_Find(context, typeId);
    if (definitionTypeId == ZR_SEMANTIC_ID_INVALID || canonical == ZR_NULL ||
        (canonical->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE &&
         canonical->data.genericInstance.definitionTypeId != definitionTypeId)) {
        return;
    }
    existing = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE);
    /* The converter may already have recorded the raw generic key; never replace it. */
    if (existing != ZR_NULL &&
        (existing->typeId != typeId ||
         (existing->symbolId != ZR_SEMANTIC_ID_INVALID &&
          existing->symbolId != definitionSymbolId))) {
        return;
    }
    if (ZrParser_SemanticTypeUse_Publish(context, annotation, typeId, ZR_TRUE)) {
        return;
    }
    /* Unnamed sources still need compile identity, without navigation ranges. */
    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact = ZrCore_Array_Get(&context->referenceFacts, index);

        if (fact != ZR_NULL && fact->node == annotation->name &&
            fact->kind == ZR_SEMANTIC_REFERENCE_TYPE) {
            if (fact->typeId == typeId &&
                (fact->symbolId == ZR_SEMANTIC_ID_INVALID || fact->symbolId == definitionSymbolId)) {
                fact->symbolId = definitionSymbolId;
                fact->isResolved = ZR_TRUE;
            }
            return;
        }
    }
    {
        SZrSemanticReferenceFact fact = {0};
        const SZrIdentifier *identifier = annotation->name->type == ZR_AST_IDENTIFIER_LITERAL
                ? &annotation->name->data.identifier : annotation->name->data.genericType.name;

        fact.node = annotation->name;
        fact.kind = ZR_SEMANTIC_REFERENCE_TYPE;
        fact.typeId = typeId;
        fact.symbolId = definitionSymbolId;
        fact.name = identifier->name;
        fact.syntaxContext = identifier->syntaxContext;
        fact.isResolved = ZR_TRUE;
        (void)ZrParser_SemanticFacts_AppendReference(context, &fact);
    }
}

TZrBool compiler_resolve_type_annotation_prototype(
        SZrCompilerState *cs, SZrType *annotation, SZrString *legacyTypeName,
        SZrTypePrototypeInfo **outPrototype) {
    SZrSyntaxContext rawContext;
    SZrSyntaxContext itemContext;
    SZrInferredType type;
    const SZrTypeBinding *alias = ZR_NULL;
    TZrBool isGeneric;
    TZrBool resolved;

    if (outPrototype == ZR_NULL) {
        return ZR_FALSE;
    }
    *outPrototype = ZR_NULL;
    if (cs == ZR_NULL || cs->state == ZR_NULL || annotation == ZR_NULL ||
        annotation->name == ZR_NULL || legacyTypeName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }
    isGeneric = annotation->name->type == ZR_AST_GENERIC_TYPE;
    if (!isGeneric && annotation->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        *outPrototype = find_compiler_type_prototype(cs, legacyTypeName);
        return !cs->hasError;
    }
    if (!normalize_named_annotation_context(annotation, &rawContext, &itemContext, 0U)) {
        ZrParser_Compiler_Error(cs, "Cannot normalize class base type context", annotation->name->location);
        return ZR_FALSE;
    }
    if (!isGeneric) {
        if (ZrParser_SyntaxContext_Equal(itemContext, ZrParser_SyntaxContext_Root())) {
            *outPrototype = find_registered_type_prototype_exact_only(cs, legacyTypeName);
            if (*outPrototype != ZR_NULL) {
                compiler_publish_resolved_annotation_type(cs, annotation, *outPrototype);
                return ZR_TRUE;
            }
        }
        if (!find_annotation_alias(cs, legacyTypeName, rawContext, &alias)) {
            ZrParser_Compiler_Error(cs, "Cannot normalize class base type alias context", annotation->name->location);
            return ZR_FALSE;
        }
        if (alias == ZR_NULL &&
            ZrParser_SyntaxContext_Equal(itemContext, ZrParser_SyntaxContext_Root())) {
            *outPrototype = find_compiler_type_prototype_with_initial_alias_policy(
                    cs, legacyTypeName, ZR_FALSE);
            if (!cs->hasError) {
                compiler_publish_resolved_annotation_type(cs, annotation, *outPrototype);
            }
            return !cs->hasError;
        }
    }

    /* Alias storage and prototype tables may grow during conversion or materialization. */
    ZrParser_InferredType_Init(cs->state, &type, ZR_VALUE_TYPE_OBJECT);
    if (alias != ZR_NULL) {
        ZrParser_InferredType_Copy(cs->state, &type, &alias->type);
        resolved = resolve_owned_annotation_type(cs, &type, ZR_FALSE, outPrototype);
    } else {
        TZrBool allowRootDeferral = ZR_FALSE;

        resolved = ZrParser_AstTypeToInferredType_Convert(cs, annotation, &type);
        if (resolved && !isGeneric && type.nominalDefinitionToken == 0U) {
            resolved = ZR_FALSE;
        }
        if (resolved && isGeneric &&
            ZrParser_SyntaxContext_Equal(itemContext, ZrParser_SyntaxContext_Root())) {
            resolved = root_generic_base_is_unpublished(cs,
                    annotation->name->data.genericType.name->name, &allowRootDeferral);
        }
        if (resolved) {
            resolved = resolve_owned_annotation_type(cs, &type, allowRootDeferral, outPrototype);
        }
    }
    if (resolved) {
        compiler_publish_resolved_annotation_type(cs, annotation, *outPrototype);
    }
    ZrParser_InferredType_Free(cs->state, &type);
    if (!resolved) {
        *outPrototype = ZR_NULL;
        if (!cs->hasError) {
            ZrParser_Compiler_Error(cs, "Cannot resolve class base type identity", annotation->name->location);
        }
    }
    return resolved;
}

static const TZrChar *compile_expression_find_top_level_last_dot(const TZrChar *typeNameText) {
    const TZrChar *lastDot = ZR_NULL;
    TZrInt32 genericDepth = 0;

    if (typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    for (const TZrChar *cursor = typeNameText; *cursor != '\0'; cursor++) {
        if (*cursor == '<') {
            genericDepth++;
            continue;
        }
        if (*cursor == '>') {
            if (genericDepth > 0) {
                genericDepth--;
            }
            continue;
        }
        if (*cursor == '.' && genericDepth == 0) {
            lastDot = cursor;
        }
    }

    return lastDot;
}

static SZrString *compile_expression_create_trimmed_type_name_segment(SZrCompilerState *cs,
                                                                      const TZrChar *source,
                                                                      TZrSize start,
                                                                      TZrSize end) {
    if (cs == ZR_NULL || source == ZR_NULL || end <= start) {
        return ZR_NULL;
    }

    while (start < end && (source[start] == ' ' || source[start] == '\t' || source[start] == '\r' || source[start] == '\n')) {
        start++;
    }
    while (end > start &&
           (source[end - 1] == ' ' || source[end - 1] == '\t' || source[end - 1] == '\r' || source[end - 1] == '\n')) {
        end--;
    }

    if (end <= start) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, (TZrNativeString)(source + start), end - start);
}

static SZrString *compile_expression_extract_member_lookup_name_from_type_text(SZrCompilerState *cs,
                                                                               const TZrChar *memberTypeText) {
    TZrSize memberTypeLength;
    TZrSize rootEnd;

    if (cs == ZR_NULL || memberTypeText == ZR_NULL || memberTypeText[0] == '\0') {
        return ZR_NULL;
    }

    memberTypeLength = strlen(memberTypeText);
    rootEnd = memberTypeLength;
    for (TZrSize index = 0; index < memberTypeLength; index++) {
        if (memberTypeText[index] == '<') {
            rootEnd = index;
            break;
        }
    }

    return compile_expression_create_trimmed_type_name_segment(cs, memberTypeText, 0, rootEnd);
}

SZrString *compile_expression_build_canonical_module_member_type_name(SZrCompilerState *cs,
                                                                             SZrString *canonicalMemberTypeName,
                                                                             SZrString *requestedMemberTypeName) {
    const TZrChar *canonicalText;
    const TZrChar *requestedText;
    const TZrChar *requestedGenericStart;
    const TZrChar *canonicalGenericStart;
    TZrSize canonicalBaseLength;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || canonicalMemberTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    canonicalText = ZrCore_String_GetNativeString(canonicalMemberTypeName);
    if (canonicalText == ZR_NULL) {
        return ZR_NULL;
    }

    requestedText = requestedMemberTypeName != ZR_NULL ? ZrCore_String_GetNativeString(requestedMemberTypeName) : ZR_NULL;
    requestedGenericStart = requestedText != ZR_NULL ? strchr(requestedText, '<') : ZR_NULL;
    if (requestedGenericStart == ZR_NULL) {
        return canonicalMemberTypeName;
    }

    canonicalGenericStart = strchr(canonicalText, '<');
    canonicalBaseLength = canonicalGenericStart != ZR_NULL ? (TZrSize)(canonicalGenericStart - canonicalText)
                                                           : strlen(canonicalText);
    written = snprintf(buffer,
                       sizeof(buffer),
                       "%.*s%s",
                       (int)canonicalBaseLength,
                       canonicalText,
                       requestedGenericStart);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static SZrTypePrototypeInfo *find_compiler_type_prototype_with_initial_alias_policy(
        SZrCompilerState *cs, SZrString *typeName, TZrBool allowInitialRootAlias) {
    SZrTypePrototypeInfo *info;
    const TZrChar *typeNameText;
    const TZrChar *lastDot;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    info = find_registered_type_prototype_exact_only(cs, typeName);
    if (info != ZR_NULL) {
        return info;
    }

    if (allowInitialRootAlias) {
        SZrTypeBinding *binding = compiler_find_type_value_alias(
                cs, typeName, ZrParser_SyntaxContext_Root());
        if (binding != ZR_NULL && binding->type.typeName != ZR_NULL &&
            !ZrCore_String_Equal(binding->type.typeName, typeName)) {
            SZrInferredType aliasType;

            ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Copy(cs->state, &aliasType, &binding->type);
            if (aliasType.nominalDefinitionToken != 0U ||
                (aliasType.baseType == ZR_VALUE_TYPE_OBJECT &&
                 aliasType.elementTypes.length > 0U)) {
                ensure_generic_instance_type_prototype_from_inferred(cs, &aliasType);
                info = type_inference_find_nominal_prototype(cs, &aliasType);
            } else {
                info = find_compiler_type_prototype(cs, aliasType.typeName);
            }
            ZrParser_InferredType_Free(cs->state, &aliasType);
            return info;
        }
    }

    if (ensure_generic_instance_type_prototype(cs, typeName)) {
        info = find_registered_type_prototype_exact_only(cs, typeName);
        if (info != ZR_NULL) {
            return info;
        }
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    lastDot = compile_expression_find_top_level_last_dot(typeNameText);
    if (lastDot != ZR_NULL && lastDot > typeNameText && lastDot[1] != '\0') {
        SZrString *moduleName =
                ZrCore_String_Create(cs->state, (TZrNativeString)typeNameText, (TZrSize)(lastDot - typeNameText));
        SZrString *memberTypeName = ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)(lastDot + 1));
        SZrString *memberName =
                compile_expression_extract_member_lookup_name_from_type_text(cs, lastDot + 1);
        SZrTypePrototypeInfo *moduleInfo = ZR_NULL;

        if (moduleName != ZR_NULL && memberName != ZR_NULL && memberTypeName != ZR_NULL) {
            moduleInfo = find_registered_type_prototype_exact_only(cs, moduleName);
            if (moduleInfo == ZR_NULL && cs->typeEnv != ZR_NULL) {
                SZrInferredType aliasType;

                ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
                if (ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, moduleName, &aliasType) &&
                    aliasType.typeName != ZR_NULL &&
                    !ZrCore_String_Equal(aliasType.typeName, moduleName)) {
                    moduleInfo = find_compiler_type_prototype(cs, aliasType.typeName);
                    if (moduleInfo == ZR_NULL && ensure_import_module_compile_info(cs, aliasType.typeName)) {
                        moduleInfo = find_compiler_type_prototype(cs, aliasType.typeName);
                    }
                }
                ZrParser_InferredType_Free(cs->state, &aliasType);
            }
            if (moduleInfo == ZR_NULL && ensure_import_module_compile_info(cs, moduleName)) {
                moduleInfo = find_registered_type_prototype_exact_only(cs, moduleName);
            }
        }

        if (moduleInfo != ZR_NULL && moduleInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
            for (TZrSize memberIndex = 0; memberIndex < moduleInfo->members.length; memberIndex++) {
                SZrTypeMemberInfo *memberInfo =
                        (SZrTypeMemberInfo *)ZrCore_Array_Get(&moduleInfo->members, memberIndex);
                if (memberInfo != ZR_NULL && memberInfo->name != ZR_NULL &&
                    memberInfo->fieldTypeName != ZR_NULL &&
                    ZrCore_String_Equal(memberInfo->name, memberName)) {
                    SZrString *candidateTypeName =
                            compile_expression_build_canonical_module_member_type_name(cs,
                                                                                       memberInfo->fieldTypeName,
                                                                                       memberTypeName);
                    if (candidateTypeName != ZR_NULL && !ZrCore_String_Equal(candidateTypeName, typeName)) {
                        SZrTypePrototypeInfo *candidateExact =
                                find_registered_type_prototype_exact_only(cs, candidateTypeName);
                        if (candidateExact != ZR_NULL) {
                            return candidateExact;
                        }
                        if (ZrCore_String_GetNativeString(candidateTypeName) != ZR_NULL &&
                            strchr(ZrCore_String_GetNativeString(candidateTypeName), '<') != ZR_NULL) {
                            return find_compiler_type_prototype(cs, candidateTypeName);
                        }
                    }
                    if (!ZrCore_String_Equal(memberInfo->fieldTypeName, typeName)) {
                        SZrTypePrototypeInfo *fieldExact =
                                find_registered_type_prototype_exact_only(cs, memberInfo->fieldTypeName);
                        if (fieldExact != ZR_NULL) {
                            return fieldExact;
                        }
                    }
                    return ZR_NULL;
                }
            }
        }
    }

    return ZR_NULL;
}

SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName) {
    return find_compiler_type_prototype_with_initial_alias_policy(cs, typeName, ZR_TRUE);
}
