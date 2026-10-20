#include "type_inference_generic_prototype.h"

#include "compiler_internal.h"
#include "type_inference_generic_substitution.h"
#include "type_inference_internal.h"
#include "type_inference_nominal_prototype.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TZrBool type_inference_copy_generic_parameter_info_array(SZrState *state,
                                                                SZrArray *dest,
                                                                const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(dest);
    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(SZrTypeGenericParameterInfo), src->length);
    for (TZrSize index = 0; index < src->length; index++) {
        SZrTypeGenericParameterInfo *srcInfo = (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)src, index);
        SZrTypeGenericParameterInfo destInfo;

        if (srcInfo == ZR_NULL) {
            continue;
        }

        memset(&destInfo, 0, sizeof(destInfo));
        destInfo = *srcInfo;
        ZrCore_Array_Construct(&destInfo.constraintTypeNames);
        if (srcInfo->constraintTypeNames.length > 0) {
            ZrCore_Array_Init(state,
                              &destInfo.constraintTypeNames,
                              sizeof(SZrString *),
                              srcInfo->constraintTypeNames.length);
            for (TZrSize constraintIndex = 0; constraintIndex < srcInfo->constraintTypeNames.length; constraintIndex++) {
                SZrString **constraintNamePtr =
                        (SZrString **)ZrCore_Array_Get(&srcInfo->constraintTypeNames, constraintIndex);
                if (constraintNamePtr != ZR_NULL && *constraintNamePtr != ZR_NULL) {
                    ZrCore_Array_Push(state, &destInfo.constraintTypeNames, constraintNamePtr);
                }
            }
        }

        ZrCore_Array_Push(state, dest, &destInfo);
    }

    return ZR_TRUE;
}

static TZrBool type_inference_parse_const_int_generic_argument_name(
        SZrString *argumentName,
        TZrInt64 *outValue) {
    const TZrChar *text;
    TZrChar *end = ZR_NULL;
    long long parsedValue;

    if (argumentName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(argumentName);
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    parsedValue = strtoll(text, &end, 10);
    if (end == text || end == ZR_NULL || *end != '\0') {
        return ZR_FALSE;
    }

    *outValue = (TZrInt64)parsedValue;
    return ZR_TRUE;
}

static TZrBool type_inference_copy_parameter_passing_mode_array(SZrState *state,
                                                                SZrArray *dest,
                                                                const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(dest);
    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(EZrParameterPassingMode), src->length);
    for (TZrSize index = 0; index < src->length; index++) {
        EZrParameterPassingMode *mode = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)src, index);
        if (mode != ZR_NULL) {
            ZrCore_Array_Push(state, dest, mode);
        }
    }

    return ZR_TRUE;
}


static SZrSemanticContext *generic_semantic_context(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return ZR_NULL;
    }
    return cs->typeEnv != ZR_NULL && cs->typeEnv->semanticContext != ZR_NULL
            ? cs->typeEnv->semanticContext : cs->semanticContext;
}

static SZrTypePrototypeInfo *find_generic_open_prototype(
        SZrCompilerState *cs, SZrString *baseName, TZrUInt32 definitionToken) {
    SZrInferredType definition;
    SZrTypePrototypeInfo *prototype;

    ZrParser_InferredType_InitFull(cs->state, &definition,
            ZR_VALUE_TYPE_OBJECT, ZR_FALSE, baseName);
    definition.nominalDefinitionToken = definitionToken;
    prototype = type_inference_find_nominal_prototype(cs, &definition);
    ZrParser_InferredType_Free(cs->state, &definition);
    return prototype;
}

static SZrGenericDeclaration *generic_declaration(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION: return node->data.functionDeclaration.generic;
        case ZR_AST_CLASS_METHOD: return node->data.classMethod.generic;
        case ZR_AST_STRUCT_METHOD: return node->data.structMethod.generic;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE: return node->data.interfaceMethodSignature.generic;
        case ZR_AST_CLASS_DECLARATION: return node->data.classDeclaration.generic;
        case ZR_AST_STRUCT_DECLARATION: return node->data.structDeclaration.generic;
        case ZR_AST_INTERFACE_DECLARATION: return node->data.interfaceDeclaration.generic;
        case ZR_AST_UNION_DECLARATION: return node->data.unionDeclaration.generic;
        default: return ZR_NULL;
    }
}

static TZrBool const_parameter_identity(SZrCompilerState *cs,
        SZrString *name, SZrCanonicalGenericArgument *argument) {
    SZrSemanticContext *context = generic_semantic_context(cs);
    SZrAstNode *owners[2] = {cs->currentFunctionNode, cs->currentTypeNode};

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize ownerIndex = 0U; ownerIndex < 2U; ownerIndex++) {
        SZrGenericDeclaration *generic = generic_declaration(owners[ownerIndex]);
        if (generic == ZR_NULL || generic->params == ZR_NULL) {
            continue;
        }
        for (TZrSize ordinal = 0U; ordinal < generic->params->count; ordinal++) {
            SZrAstNode *parameter = generic->params->nodes[ordinal];
            if (parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
                parameter->data.parameter.genericKind != ZR_GENERIC_PARAMETER_CONST_INT ||
                parameter->data.parameter.name == ZR_NULL ||
                !ZrCore_String_Equal(parameter->data.parameter.name->name, name)) {
                continue;
            }
            for (TZrSize symbolIndex = 0U; symbolIndex < context->symbols.length; symbolIndex++) {
                const SZrSemanticSymbolRecord *symbol = ZrCore_Array_Get(&context->symbols, symbolIndex);
                if (symbol != ZR_NULL && symbol->astNode == owners[ownerIndex] &&
                    (symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
                     symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION)) {
                    argument->kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
                    argument->data.constParameter.ownerSymbolId = symbol->id;
                    argument->data.constParameter.ordinal = (TZrUInt32)ordinal;
                    argument->data.constParameter.displayName = name;
                    return ZR_TRUE;
                }
            }
            return ZR_FALSE;
        }
    }
    return ZR_FALSE;
}

static TZrBool collect_const_argument_bindings(SZrCompilerState *cs,
        const SZrInferredType *type, SZrArray *bindings, TZrSize depth) {
    if (type == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }
    if (type->genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_CONST_PARAMETER) {
        SZrCanonicalGenericArgument argument;
        SZrCanonicalGenericBinding binding = {0};

        if (!const_parameter_identity(cs, type->typeName, &argument)) {
            return ZR_FALSE;
        }
        binding.name = argument.data.constParameter.displayName;
        binding.kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER;
        binding.ownerSymbolId = argument.data.constParameter.ownerSymbolId;
        binding.ordinal = argument.data.constParameter.ordinal;
        ZrCore_Array_Push(cs->state, bindings, &binding);
    }
    for (TZrSize index = 0U; index < type->elementTypes.length; index++) {
        const SZrInferredType *element =
                ZrCore_Array_Get((SZrArray *)&type->elementTypes, index);

        if (!collect_const_argument_bindings(cs, element, bindings, depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrTypeId generic_argument_identity(
        SZrSemanticContext *context, TZrTypeId typeId, TZrSize depth) {
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(context, typeId);
    SZrCanonicalTypeNode snapshot;
    TZrTypeId targetTypeId;

    if (node == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    snapshot = *node;
    switch (snapshot.kind) {
        case ZR_CANONICAL_TYPE_UNION:
            node = ZrParser_CanonicalType_Find(context, snapshot.data.unionType.definitionTypeId);
            return node != ZR_NULL && node->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE
                    ? generic_argument_identity(context, node->id, depth + 1U) : typeId;
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE: {
            SZrArray arguments;
            TZrTypeId result;

            ZrCore_Array_Init(context->state, &arguments,
                    sizeof(SZrCanonicalGenericArgument), snapshot.data.genericInstance.arguments.length);
            for (TZrSize index = 0U; index < snapshot.data.genericInstance.arguments.length; index++) {
                const SZrCanonicalGenericArgument *source = ZrCore_Array_Get(
                        &snapshot.data.genericInstance.arguments, index);
                SZrCanonicalGenericArgument argument;

                if (source == ZR_NULL) {
                    ZrCore_Array_Free(context->state, &arguments);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                argument = *source;
                if (argument.kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
                    argument.data.typeId = generic_argument_identity(
                            context, argument.data.typeId, depth + 1U);
                    if (argument.data.typeId == ZR_SEMANTIC_ID_INVALID) {
                        ZrCore_Array_Free(context->state, &arguments);
                        return ZR_SEMANTIC_ID_INVALID;
                    }
                }
                ZrCore_Array_Push(context->state, &arguments, &argument);
            }
            result = ZrParser_CanonicalType_InternGenericInstanceEx(context,
                    snapshot.data.genericInstance.definitionTypeId,
                    (const SZrCanonicalGenericArgument *)arguments.head, arguments.length);
            ZrCore_Array_Free(context->state, &arguments);
            return result;
        }
        case ZR_CANONICAL_TYPE_ARRAY:
            targetTypeId = generic_argument_identity(
                    context, snapshot.data.array.elementTypeId, depth + 1U);
            return ZrParser_CanonicalType_InternArray(context, targetTypeId,
                    snapshot.data.array.rank, snapshot.data.array.storageKind);
        case ZR_CANONICAL_TYPE_TUPLE: {
            SZrArray elements;
            TZrTypeId result;

            ZrCore_Array_Init(context->state, &elements, sizeof(TZrTypeId),
                    snapshot.data.typeList.elementTypeIds.length);
            for (TZrSize index = 0U; index < snapshot.data.typeList.elementTypeIds.length; index++) {
                const TZrTypeId *source = ZrCore_Array_Get(&snapshot.data.typeList.elementTypeIds, index);
                TZrTypeId element = source != ZR_NULL
                        ? generic_argument_identity(context, *source, depth + 1U)
                        : ZR_SEMANTIC_ID_INVALID;

                if (element == ZR_SEMANTIC_ID_INVALID) {
                    ZrCore_Array_Free(context->state, &elements);
                    return ZR_SEMANTIC_ID_INVALID;
                }
                ZrCore_Array_Push(context->state, &elements, &element);
            }
            result = ZrParser_CanonicalType_InternTuple(
                    context, (const TZrTypeId *)elements.head, elements.length);
            ZrCore_Array_Free(context->state, &elements);
            return result;
        }
        case ZR_CANONICAL_TYPE_REF:
            targetTypeId = generic_argument_identity(
                    context, snapshot.data.refType.pointeeTypeId, depth + 1U);
            return ZrParser_CanonicalType_InternRef(context, targetTypeId, snapshot.data.refType.access);
        case ZR_CANONICAL_TYPE_OWNER:
            targetTypeId = generic_argument_identity(
                    context, snapshot.data.owner.targetTypeId, depth + 1U);
            return ZrParser_CanonicalType_InternOwner(context, targetTypeId, snapshot.data.owner.ownerKind);
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
            targetTypeId = generic_argument_identity(
                    context, snapshot.data.target.targetTypeId, depth + 1U);
            return ZrParser_CanonicalType_InternReadonlyView(context, targetTypeId);
        case ZR_CANONICAL_TYPE_NULLABLE:
            targetTypeId = generic_argument_identity(
                    context, snapshot.data.target.targetTypeId, depth + 1U);
            return ZrParser_CanonicalType_InternNullable(context, targetTypeId);
        default:
            return typeId;
    }
}

static TZrTypeId generic_instance_key(SZrCompilerState *cs,
        SZrString *definitionName, TZrUInt32 definitionToken,
        const SZrArray *argumentTypes) {
    SZrSemanticContext *context = generic_semantic_context(cs);
    SZrInferredType definition;
    const SZrCanonicalTypeNode *definitionNode;
    SZrArray arguments;
    SZrArray genericBindings;
    TZrTypeId definitionTypeId;
    TZrTypeId result = ZR_SEMANTIC_ID_INVALID;

    if (context == ZR_NULL || argumentTypes == ZR_NULL || argumentTypes->length == 0U) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    ZrParser_InferredType_InitFull(cs->state, &definition,
            ZR_VALUE_TYPE_OBJECT, ZR_FALSE, definitionName);
    definition.nominalDefinitionToken = definitionToken;
    definitionTypeId = ZrParser_CanonicalType_FromInferred(context, &definition);
    ZrParser_InferredType_Free(cs->state, &definition);
    definitionNode = ZrParser_CanonicalType_Find(context, definitionTypeId);
    if (definitionNode != ZR_NULL && definitionNode->kind == ZR_CANONICAL_TYPE_UNION) {
        definitionTypeId = definitionNode->data.unionType.definitionTypeId;
        definitionNode = ZrParser_CanonicalType_Find(context, definitionTypeId);
    }
    if (definitionNode == ZR_NULL || definitionNode->kind != ZR_CANONICAL_TYPE_NOMINAL) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    ZrCore_Array_Init(cs->state, &arguments,
            sizeof(SZrCanonicalGenericArgument), argumentTypes->length);
    ZrCore_Array_Init(cs->state, &genericBindings,
            sizeof(SZrCanonicalGenericBinding), argumentTypes->length);
    for (TZrSize index = 0U; index < argumentTypes->length; index++) {
        const SZrInferredType *argument = ZrCore_Array_Get((SZrArray *)argumentTypes, index);
        SZrCanonicalGenericArgument canonicalArgument;

        memset(&canonicalArgument, 0, sizeof(canonicalArgument));
        if (argument == ZR_NULL) {
            goto cleanup;
        }
        if (argument->genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_CONST_INT) {
            canonicalArgument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;
            canonicalArgument.data.constIntValue = argument->genericConstIntValue;
        } else if (argument->genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_CONST_PARAMETER) {
            if (!const_parameter_identity(cs, argument->typeName, &canonicalArgument)) {
                goto cleanup;
            }
        } else if (argument->genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_TYPE) {
            if (!collect_const_argument_bindings(cs, argument, &genericBindings, 0U)) {
                goto cleanup;
            }
            canonicalArgument.kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
            canonicalArgument.data.typeId = generic_argument_identity(context,
                    ZrParser_CanonicalType_FromInferredWithGenericBindings(context, argument,
                            (const SZrCanonicalGenericBinding *)genericBindings.head,
                            genericBindings.length), 0U);
            if (canonicalArgument.data.typeId == ZR_SEMANTIC_ID_INVALID) {
                goto cleanup;
            }
        } else {
            goto cleanup;
        }
        ZrCore_Array_Push(cs->state, &arguments, &canonicalArgument);
    }
    result = ZrParser_CanonicalType_InternGenericInstanceEx(context, definitionTypeId,
            (const SZrCanonicalGenericArgument *)arguments.head, arguments.length);

cleanup:
    ZrCore_Array_Free(cs->state, &genericBindings);
    ZrCore_Array_Free(cs->state, &arguments);
    return result;
}

static SZrTypePrototypeInfo *find_generic_prototype_by_key(
        SZrCompilerState *cs, TZrTypeId instanceTypeId) {
    SZrTypePrototypeInfo *bestMatch = ZR_NULL;

    if (instanceTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype = ZrCore_Array_Get(&cs->typePrototypes, index);
        if (prototype != ZR_NULL && prototype->genericInstanceTypeId == instanceTypeId) {
            if (!prototype->isImportedNative) {
                return prototype;
            }
            if (bestMatch == ZR_NULL) {
                bestMatch = prototype;
            }
        }
    }
    return bestMatch;
}

SZrTypePrototypeInfo *type_inference_find_generic_prototype(
        SZrCompilerState *cs, const SZrInferredType *type) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentNames;
    SZrTypePrototypeInfo *openPrototype;
    SZrString *definitionName;
    TZrTypeId instanceTypeId;

    if (cs == ZR_NULL || type == ZR_NULL || type->baseType != ZR_VALUE_TYPE_OBJECT ||
        type->typeName == ZR_NULL || type->elementTypes.length == 0U) {
        return ZR_NULL;
    }
    if (!try_parse_generic_instance_type_name(
                cs->state, type->typeName, &baseName, &argumentNames)) {
        return ZR_NULL;
    }
    if (argumentNames.length != type->elementTypes.length) {
        ZrCore_Array_Free(cs->state, &argumentNames);
        return ZR_NULL;
    }
    ZrCore_Array_Free(cs->state, &argumentNames);
    openPrototype = find_generic_open_prototype(cs, baseName, type->nominalDefinitionToken);
    if (openPrototype == ZR_NULL ||
        openPrototype->genericParameters.length != type->elementTypes.length) {
        return ZR_NULL;
    }
    definitionName = openPrototype->name;
    instanceTypeId = generic_instance_key(cs, definitionName,
            type->nominalDefinitionToken, &type->elementTypes);
    return find_generic_prototype_by_key(cs, instanceTypeId);
}

static TZrBool const_parameter_is_current(SZrCompilerState *cs, TZrSymbolId ownerSymbolId) {
    const SZrSemanticSymbolRecord *owner =
            ZrParser_Semantic_FindSymbolById(generic_semantic_context(cs), ownerSymbolId);

    return owner != ZR_NULL && owner->astNode != ZR_NULL &&
           (owner->astNode == cs->currentFunctionNode || owner->astNode == cs->currentTypeNode);
}

static TZrBool canonical_type_is_root_visible(
        SZrCompilerState *cs, TZrTypeId typeId, TZrSize depth) {
    SZrSemanticContext *context = generic_semantic_context(cs);
    const SZrCanonicalTypeNode *node = ZrParser_CanonicalType_Find(context, typeId);

    if (node == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_FALSE;
    }
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
        case ZR_CANONICAL_TYPE_NEVER:
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_NOMINAL:
            return node->data.nominal.definitionToken == 0U;
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            if (!canonical_type_is_root_visible(
                        cs, node->data.genericInstance.definitionTypeId, depth + 1U)) {
                return ZR_FALSE;
            }
            for (TZrSize index = 0U; index < node->data.genericInstance.arguments.length; index++) {
                const SZrCanonicalGenericArgument *argument =
                        ZrCore_Array_Get((SZrArray *)&node->data.genericInstance.arguments, index);
                if (argument == ZR_NULL ||
                    (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE &&
                     !canonical_type_is_root_visible(cs, argument->data.typeId, depth + 1U)) ||
                    (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER &&
                     !const_parameter_is_current(cs, argument->data.constParameter.ownerSymbolId))) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_UNION:
            return canonical_type_is_root_visible(
                    cs, node->data.unionType.definitionTypeId, depth + 1U);
        case ZR_CANONICAL_TYPE_ARRAY:
            return canonical_type_is_root_visible(
                    cs, node->data.array.elementTypeId, depth + 1U);
        case ZR_CANONICAL_TYPE_TUPLE:
            for (TZrSize index = 0U; index < node->data.typeList.elementTypeIds.length; index++) {
                const TZrTypeId *element = ZrCore_Array_Get(
                        (SZrArray *)&node->data.typeList.elementTypeIds, index);
                if (element == ZR_NULL || !canonical_type_is_root_visible(cs, *element, depth + 1U)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_REF:
            return canonical_type_is_root_visible(cs, node->data.refType.pointeeTypeId, depth + 1U);
        case ZR_CANONICAL_TYPE_OWNER:
            return canonical_type_is_root_visible(cs, node->data.owner.targetTypeId, depth + 1U);
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
        case ZR_CANONICAL_TYPE_NULLABLE:
            return canonical_type_is_root_visible(cs, node->data.target.targetTypeId, depth + 1U);
        case ZR_CANONICAL_TYPE_FUNCTION:
            if (!canonical_type_is_root_visible(cs, node->data.function.returnTypeId, depth + 1U)) {
                return ZR_FALSE;
            }
            for (TZrSize index = 0U; index < node->data.function.parameterContracts.length; index++) {
                const SZrCanonicalParameterContract *parameter =
                        ZrCore_Array_Get((SZrArray *)&node->data.function.parameterContracts, index);
                if (parameter == ZR_NULL ||
                    !canonical_type_is_root_visible(cs, parameter->typeId, depth + 1U)) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

TZrBool type_inference_generic_prototype_is_root_visible(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype) {
    return cs != ZR_NULL && prototype != ZR_NULL &&
           (prototype->genericInstanceTypeId == ZR_SEMANTIC_ID_INVALID ||
            canonical_type_is_root_visible(
                    cs, prototype->genericInstanceTypeId, 0U));
}

static TZrBool validate_generic_prototype_arguments(SZrCompilerState *cs,
        const SZrArray *genericParameters, const SZrArray *argumentTypes,
        const SZrArray *argumentNames) {
    SZrArray bindings;
    SZrArray parameters;
    TZrChar diagnostic[ZR_PARSER_ERROR_BUFFER_LENGTH];
    EZrGenericCallResolveStatus status;

    if (!type_inference_copy_generic_parameter_info_array(cs->state, &parameters, genericParameters)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(cs->state, &bindings, sizeof(SZrGenericCallBinding), argumentTypes->length);
    for (TZrSize index = 0U; index < argumentTypes->length; index++) {
        const SZrTypeGenericParameterInfo *parameter =
                ZrCore_Array_Get(&parameters, index);
        const SZrInferredType *argument = ZrCore_Array_Get((SZrArray *)argumentTypes, index);
        SZrGenericCallBinding binding;

        if (parameter == ZR_NULL || argument == ZR_NULL) {
            goto invalid;
        }
        if ((parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE &&
             argument->genericArgumentKind != ZR_INFERRED_GENERIC_ARGUMENT_TYPE) ||
            (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT &&
             argument->genericArgumentKind != ZR_INFERRED_GENERIC_ARGUMENT_CONST_INT &&
             argument->genericArgumentKind != ZR_INFERRED_GENERIC_ARGUMENT_CONST_PARAMETER)) {
            SZrString **argumentName = ZrCore_Array_Get((SZrArray *)argumentNames, index);
            SZrString *displayName = argument->typeName != ZR_NULL
                    ? argument->typeName : argumentName != ZR_NULL ? *argumentName : ZR_NULL;
            snprintf(diagnostic, sizeof(diagnostic),
                    "Generic argument '%s' has the wrong kind for parameter '%s'",
                    displayName != ZR_NULL ? ZrCore_String_GetNativeString(displayName) : "",
                    parameter->name != ZR_NULL ? ZrCore_String_GetNativeString(parameter->name) : "");
            ZrParser_Compiler_Error(cs, diagnostic, (SZrFileRange){0});
            goto invalid;
        }
        binding.parameterInfo = parameter;
        binding.isBound = ZR_TRUE;
        binding.inferredType = *argument;
        ZrCore_Array_Push(cs->state, &bindings, &binding);
    }
    status = validate_generic_call_bindings_constraints(
            cs, &bindings, diagnostic, sizeof(diagnostic));
    for (TZrSize index = 0U; index < parameters.length; index++) {
        SZrTypeGenericParameterInfo *parameter = ZrCore_Array_Get(&parameters, index);
        if (parameter != ZR_NULL) {
            ZrCore_Array_Free(cs->state, &parameter->constraintTypeNames);
        }
    }
    ZrCore_Array_Free(cs->state, &parameters);
    ZrCore_Array_Free(cs->state, &bindings);
    if (status != ZR_GENERIC_CALL_RESOLVE_OK) {
        if (diagnostic[0] == 'g') {
            diagnostic[0] = 'G';
        }
        ZrParser_Compiler_Error(cs, diagnostic, (SZrFileRange){0});
        return ZR_FALSE;
    }
    return ZR_TRUE;

invalid:
    for (TZrSize index = 0U; index < parameters.length; index++) {
        SZrTypeGenericParameterInfo *parameter = ZrCore_Array_Get(&parameters, index);
        if (parameter != ZR_NULL) {
            ZrCore_Array_Free(cs->state, &parameter->constraintTypeNames);
        }
    }
    ZrCore_Array_Free(cs->state, &parameters);
    ZrCore_Array_Free(cs->state, &bindings);
    return ZR_FALSE;
}

static TZrBool ensure_generic_instance_type_prototype_internal(
        SZrCompilerState *cs,
        SZrString *typeName,
        const SZrInferredType *structuredType) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrArray argumentTypes;
    SZrTypePrototypeInfo *openPrototype;
    SZrString *materializedTypeName;
    TZrUInt32 definitionToken = structuredType != ZR_NULL ? structuredType->nominalDefinitionToken : 0U;
    TZrTypeId instanceTypeId;

    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    if (!try_parse_generic_instance_type_name(cs->state, typeName, &baseName, &argumentTypeNames)) {
        return ZR_FALSE;
    }

    openPrototype = find_generic_open_prototype(cs, baseName, definitionToken);
    if (openPrototype == ZR_NULL || openPrototype->genericParameters.length == 0 ||
        openPrototype->genericParameters.length != argumentTypeNames.length ||
        (structuredType != ZR_NULL && structuredType->elementTypes.length != argumentTypeNames.length)) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }

    ZrCore_Array_Init(cs->state, &argumentTypes, sizeof(SZrInferredType), argumentTypeNames.length);
    for (TZrSize index = 0; index < argumentTypeNames.length; index++) {
        SZrString **argumentNamePtr = (SZrString **)ZrCore_Array_Get(&argumentTypeNames, index);
        TZrInt64 constIntValue;
        const SZrInferredType *structuredArgument =
                structuredType != ZR_NULL &&
                        structuredType->elementTypes.length == argumentTypeNames.length
                        ? (const SZrInferredType *)ZrCore_Array_Get(
                                  (SZrArray *)&structuredType->elementTypes,
                                  index)
                        : ZR_NULL;
        SZrInferredType argumentType;
        if (argumentNamePtr == ZR_NULL || *argumentNamePtr == ZR_NULL) {
            continue;
        }
        if (structuredArgument != ZR_NULL) {
            ZrParser_InferredType_Copy(cs->state, &argumentType, structuredArgument);
        } else if (type_inference_is_const_generic_parameter_reference(cs, *argumentNamePtr)) {
            ZrParser_InferredType_InitConstParameterGenericArgument(
                    cs->state,
                    &argumentType,
                    *argumentNamePtr);
        } else if (type_inference_parse_const_int_generic_argument_name(
                           *argumentNamePtr,
                           &constIntValue)) {
            ZrParser_InferredType_InitConstIntGenericArgument(
                    cs->state,
                    &argumentType,
                    constIntValue,
                    *argumentNamePtr);
        } else if (!inferred_type_from_type_name(cs, *argumentNamePtr, &argumentType)) {
            ZrParser_InferredType_Free(cs->state, &argumentType);
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        if (argumentType.genericArgumentKind == ZR_INFERRED_GENERIC_ARGUMENT_TYPE &&
            argumentType.typeName == ZR_NULL) {
            argumentType.typeName = *argumentNamePtr;
        }
        ZrCore_Array_Push(cs->state, &argumentTypes, &argumentType);
    }

    openPrototype = find_generic_open_prototype(cs, baseName, definitionToken);
    if (openPrototype == ZR_NULL ||
        !validate_generic_prototype_arguments(cs, &openPrototype->genericParameters,
                &argumentTypes, &argumentTypeNames)) {
        free_inferred_type_array(cs->state, &argumentTypes);
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }
    openPrototype = find_generic_open_prototype(cs, baseName, definitionToken);
    if (openPrototype == ZR_NULL) {
        free_inferred_type_array(cs->state, &argumentTypes);
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }
    instanceTypeId = generic_instance_key(cs, openPrototype->name, definitionToken, &argumentTypes);
    if (instanceTypeId == ZR_SEMANTIC_ID_INVALID) {
        free_inferred_type_array(cs->state, &argumentTypes);
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }
    if (find_generic_prototype_by_key(cs, instanceTypeId) != ZR_NULL) {
        free_inferred_type_array(cs->state, &argumentTypes);
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_TRUE;
    }

    {
        SZrTypePrototypeInfo closedPrototype;
        SZrTypePrototypeInfo openPrototypeSnapshot;
        SZrTypePrototypeInfo *registeredPrototype;
        openPrototype = find_generic_open_prototype(cs, baseName, definitionToken);
        if (openPrototype == ZR_NULL) {
            free_inferred_type_array(cs->state, &argumentTypes);
            ZrCore_Array_Free(cs->state, &argumentTypeNames);
            return ZR_FALSE;
        }
        memset(&closedPrototype, 0, sizeof(closedPrototype));
        openPrototypeSnapshot = *openPrototype;
        materializedTypeName = typeName;
        if (openPrototype->name != ZR_NULL && !ZrCore_String_Equal(openPrototype->name, baseName)) {
            SZrString *canonicalTypeName = build_generic_instance_name(cs->state, openPrototype->name, &argumentTypes);
            if (canonicalTypeName != ZR_NULL) {
                materializedTypeName = canonicalTypeName;
            }
        }
        closedPrototype.name = materializedTypeName;
        closedPrototype.declarationNode = openPrototypeSnapshot.declarationNode;
        closedPrototype.isDeclarationShell = openPrototypeSnapshot.isDeclarationShell;
        closedPrototype.genericInstanceTypeId = instanceTypeId;
        closedPrototype.type = openPrototypeSnapshot.type;
        closedPrototype.accessModifier = openPrototypeSnapshot.accessModifier;
        closedPrototype.modifierFlags = openPrototypeSnapshot.modifierFlags & ~ZR_TYPE_MODIFIER_FLAG_OPEN_GENERIC;
        closedPrototype.isImportedNative = openPrototypeSnapshot.isImportedNative;
        closedPrototype.importModuleName = openPrototypeSnapshot.importModuleName;
        closedPrototype.isNativeRuntime = openPrototypeSnapshot.isNativeRuntime;
        closedPrototype.protocolMask = openPrototypeSnapshot.protocolMask;
        closedPrototype.extendsTypeName = substitute_generic_type_name(cs->state,
                                                                       openPrototypeSnapshot.extendsTypeName,
                                                                       &openPrototypeSnapshot.genericParameters,
                                                                       &argumentTypeNames);
        closedPrototype.enumValueTypeName = substitute_generic_type_name(cs->state,
                                                                         openPrototypeSnapshot.enumValueTypeName,
                                                                         &openPrototypeSnapshot.genericParameters,
                                                                         &argumentTypeNames);
        closedPrototype.allowValueConstruction = openPrototypeSnapshot.allowValueConstruction;
        closedPrototype.allowBoxedConstruction = openPrototypeSnapshot.allowBoxedConstruction;
        closedPrototype.constructorSignature = openPrototypeSnapshot.constructorSignature;
        closedPrototype.nextVirtualSlotIndex = openPrototypeSnapshot.nextVirtualSlotIndex;
        closedPrototype.nextPropertyIdentity = openPrototypeSnapshot.nextPropertyIdentity;
        closedPrototype.layoutByteSize = openPrototypeSnapshot.layoutByteSize;
        closedPrototype.layoutByteAlign = openPrototypeSnapshot.layoutByteAlign;
        ZrCore_Array_Init(cs->state, &closedPrototype.inherits, sizeof(SZrString *), openPrototypeSnapshot.inherits.length);
        ZrCore_Array_Init(cs->state, &closedPrototype.implements, sizeof(SZrString *), openPrototypeSnapshot.implements.length);
        ZrCore_Array_Init(cs->state, &closedPrototype.genericParameters, sizeof(SZrTypeGenericParameterInfo), 1);
        ZrCore_Array_Init(cs->state, &closedPrototype.decorators, sizeof(SZrTypeDecoratorInfo), openPrototypeSnapshot.decorators.length);
        ZrCore_Array_Init(cs->state, &closedPrototype.members, sizeof(SZrTypeMemberInfo), openPrototypeSnapshot.members.length);

        registeredPrototype = &closedPrototype;

        for (TZrSize index = 0; index < openPrototypeSnapshot.inherits.length; index++) {
            SZrString **inheritNamePtr = (SZrString **)ZrCore_Array_Get(&openPrototypeSnapshot.inherits, index);
            SZrString *inheritName;
            if (inheritNamePtr == ZR_NULL || *inheritNamePtr == ZR_NULL) {
                continue;
            }
            inheritName = substitute_generic_type_name(cs->state,
                                                       *inheritNamePtr,
                                                       &openPrototypeSnapshot.genericParameters,
                                                       &argumentTypeNames);
            ZrCore_Array_Push(cs->state, &registeredPrototype->inherits, &inheritName);
        }

        for (TZrSize index = 0; index < openPrototypeSnapshot.implements.length; index++) {
            SZrString **implementNamePtr = (SZrString **)ZrCore_Array_Get(&openPrototypeSnapshot.implements, index);
            SZrString *implementName;
            if (implementNamePtr == ZR_NULL || *implementNamePtr == ZR_NULL) {
                continue;
            }
            implementName = substitute_generic_type_name(cs->state,
                                                         *implementNamePtr,
                                                         &openPrototypeSnapshot.genericParameters,
                                                         &argumentTypeNames);
            ZrCore_Array_Push(cs->state, &registeredPrototype->implements, &implementName);
        }

        for (TZrSize index = 0; index < openPrototypeSnapshot.decorators.length; index++) {
            SZrTypeDecoratorInfo *sourceDecorator =
                    (SZrTypeDecoratorInfo *)ZrCore_Array_Get(&openPrototypeSnapshot.decorators, index);
            if (sourceDecorator == ZR_NULL) {
                continue;
            }
            ZrCore_Array_Push(cs->state, &registeredPrototype->decorators, sourceDecorator);
        }

        for (TZrSize index = 0; index < openPrototypeSnapshot.members.length; index++) {
            SZrTypeMemberInfo *sourceMember = (SZrTypeMemberInfo *)ZrCore_Array_Get(&openPrototypeSnapshot.members, index);
            SZrTypeMemberInfo copiedMember;
            memset(&copiedMember, 0, sizeof(copiedMember));
            if (sourceMember == ZR_NULL) {
                continue;
            }

            copiedMember = *sourceMember;
            copiedMember.ownerTypeName = registeredPrototype->name;
            copiedMember.fieldTypeName = substitute_generic_type_name(cs->state,
                                                                      sourceMember->fieldTypeName,
                                                                      &openPrototypeSnapshot.genericParameters,
                                                                      &argumentTypeNames);
            copiedMember.returnTypeName = substitute_generic_type_name(cs->state,
                                                                        sourceMember->returnTypeName,
                                                                        &openPrototypeSnapshot.genericParameters,
                                                                        &argumentTypeNames);
            copiedMember.hasStructuredReturnType = ZR_FALSE;
            memset(&copiedMember.structuredReturnType, 0, sizeof(copiedMember.structuredReturnType));
            if (sourceMember->hasStructuredReturnType &&
                substitute_parameter_type(
                        cs,
                        &sourceMember->structuredReturnType,
                        &openPrototypeSnapshot.genericParameters,
                        &argumentTypeNames,
                        &argumentTypes,
                        &copiedMember.structuredReturnType)) {
                copiedMember.hasStructuredReturnType = ZR_TRUE;
                if (copiedMember.structuredReturnType.typeName != ZR_NULL) {
                    if (copiedMember.memberType == ZR_AST_STRUCT_FIELD ||
                        copiedMember.memberType == ZR_AST_CLASS_FIELD) {
                        copiedMember.fieldTypeName = copiedMember.structuredReturnType.typeName;
                    } else {
                        copiedMember.returnTypeName = copiedMember.structuredReturnType.typeName;
                    }
                }
            }
            ZrCore_Array_Construct(&copiedMember.parameterTypes);
            ZrCore_Array_Construct(&copiedMember.parameterNames);
            ZrCore_Array_Construct(&copiedMember.parameterHasDefaultValues);
            ZrCore_Array_Construct(&copiedMember.parameterDefaultValues);
            ZrCore_Array_Construct(&copiedMember.genericParameters);
            ZrCore_Array_Construct(&copiedMember.parameterPassingModes);
            ZrCore_Array_Construct(&copiedMember.decorators);
            copiedMember.declarationNode = sourceMember->declarationNode;
            if (sourceMember->parameterTypes.length > 0) {
                ZrCore_Array_Init(cs->state,
                                  &copiedMember.parameterTypes,
                                  sizeof(SZrInferredType),
                                  sourceMember->parameterTypes.length);
                for (TZrSize paramIndex = 0; paramIndex < sourceMember->parameterTypes.length; paramIndex++) {
                    SZrInferredType *sourceParameter =
                            (SZrInferredType *)ZrCore_Array_Get(&sourceMember->parameterTypes, paramIndex);
                    SZrInferredType copiedParameter;
                    if (sourceParameter == ZR_NULL) {
                        continue;
                    }
                    if (!substitute_parameter_type(cs,
                                                   sourceParameter,
                                                   &openPrototypeSnapshot.genericParameters,
                                                   &argumentTypeNames,
                                                   &argumentTypes,
                                                   &copiedParameter)) {
                        continue;
                    }
                    ZrCore_Array_Push(cs->state, &copiedMember.parameterTypes, &copiedParameter);
                }
            }
            if (sourceMember->parameterNames.length > 0) {
                ZrCore_Array_Init(cs->state,
                                  &copiedMember.parameterNames,
                                  sizeof(SZrString *),
                                  sourceMember->parameterNames.length);
                for (TZrSize paramIndex = 0; paramIndex < sourceMember->parameterNames.length; paramIndex++) {
                    SZrString **sourceName =
                            (SZrString **)ZrCore_Array_Get(&sourceMember->parameterNames, paramIndex);
                    SZrString *copiedName = sourceName != ZR_NULL ? *sourceName : ZR_NULL;
                    ZrCore_Array_Push(cs->state, &copiedMember.parameterNames, &copiedName);
                }
            }
            if (sourceMember->parameterHasDefaultValues.length > 0) {
                ZrCore_Array_Init(cs->state,
                                  &copiedMember.parameterHasDefaultValues,
                                  sizeof(TZrBool),
                                  sourceMember->parameterHasDefaultValues.length);
                for (TZrSize paramIndex = 0; paramIndex < sourceMember->parameterHasDefaultValues.length; paramIndex++) {
                    TZrBool *sourceHasDefault =
                            (TZrBool *)ZrCore_Array_Get(&sourceMember->parameterHasDefaultValues, paramIndex);
                    TZrBool copiedHasDefault = sourceHasDefault != ZR_NULL ? *sourceHasDefault : ZR_FALSE;
                    ZrCore_Array_Push(cs->state, &copiedMember.parameterHasDefaultValues, &copiedHasDefault);
                }
            }
            if (sourceMember->parameterDefaultValues.length > 0) {
                ZrCore_Array_Init(cs->state,
                                  &copiedMember.parameterDefaultValues,
                                  sizeof(SZrTypeValue),
                                  sourceMember->parameterDefaultValues.length);
                for (TZrSize paramIndex = 0; paramIndex < sourceMember->parameterDefaultValues.length; paramIndex++) {
                    SZrTypeValue *sourceDefault =
                            (SZrTypeValue *)ZrCore_Array_Get(&sourceMember->parameterDefaultValues, paramIndex);
                    SZrTypeValue copiedDefault;
                    ZrCore_Value_ResetAsNull(&copiedDefault);
                    if (sourceDefault != ZR_NULL) {
                        copiedDefault = *sourceDefault;
                    }
                    ZrCore_Array_Push(cs->state, &copiedMember.parameterDefaultValues, &copiedDefault);
                }
            }
            type_inference_copy_generic_parameter_info_array(cs->state,
                                                             &copiedMember.genericParameters,
                                                             &sourceMember->genericParameters);
            type_inference_copy_parameter_passing_mode_array(cs->state,
                                                             &copiedMember.parameterPassingModes,
                                                             &sourceMember->parameterPassingModes);
            if (sourceMember->decorators.length > 0) {
                ZrCore_Array_Init(cs->state,
                                  &copiedMember.decorators,
                                  sizeof(SZrTypeDecoratorInfo),
                                  sourceMember->decorators.length);
                for (TZrSize decoratorIndex = 0; decoratorIndex < sourceMember->decorators.length; decoratorIndex++) {
                    SZrTypeDecoratorInfo *sourceDecorator =
                            (SZrTypeDecoratorInfo *)ZrCore_Array_Get(&sourceMember->decorators, decoratorIndex);
                    if (sourceDecorator == ZR_NULL) {
                        continue;
                    }
                    ZrCore_Array_Push(cs->state, &copiedMember.decorators, sourceDecorator);
                }
            }
            ZrCore_Array_Push(cs->state, &registeredPrototype->members, &copiedMember);
        }
        type_inference_publish_property_contracts(cs, registeredPrototype);
        finalize_generic_instance_inline_layout(cs, registeredPrototype);
        if (cs->typeEnv != ZR_NULL &&
            type_inference_prototype_is_root_visible(&closedPrototype) &&
            type_inference_generic_prototype_is_root_visible(cs, &closedPrototype)) {
            ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, materializedTypeName);
        }
        ZrCore_Array_Push(cs->state, &cs->typePrototypes, &closedPrototype);
    }

    free_inferred_type_array(cs->state, &argumentTypes);
    ZrCore_Array_Free(cs->state, &argumentTypeNames);
    return ZR_TRUE;
}

TZrBool ensure_generic_instance_type_prototype(SZrCompilerState *cs, SZrString *typeName) {
    if (find_compiler_type_prototype_inference_exact(cs, typeName) != ZR_NULL) {
        return ZR_TRUE;
    }
    return ensure_generic_instance_type_prototype_internal(cs, typeName, ZR_NULL);
}

TZrBool ensure_generic_instance_type_prototype_from_inferred(
        SZrCompilerState *cs,
        const SZrInferredType *type) {
    if (type == ZR_NULL || type->typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    return ensure_generic_instance_type_prototype_internal(cs, type->typeName, type);
}
