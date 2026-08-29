#include "type_inference_internal.h"

#include <string.h>

#include "zr_vm_parser/place.h"

static const TZrChar *intrinsic_operand_error(
        EZrOwnershipIntrinsicOperation operation) {
    switch (operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
            return "share(owner) requires a Unique owner";
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            return "degrade(shared) requires a Shared owner";
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            return "wake(weak) requires a Weak owner";
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
            return "intoGc(owner) requires a Unique resource owner";
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            return "drop(owner) requires a Unique, Shared, or Weak owner";
        default:
            return "Invalid ownership intrinsic";
    }
}

static TZrBool intrinsic_operand_qualifier_matches(
        EZrOwnershipIntrinsicOperation operation,
        EZrOwnershipQualifier qualifier) {
    switch (operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE;
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED;
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            return qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
                   qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
                   qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK;
        default:
            return ZR_FALSE;
    }
}

static TZrBool intrinsic_is_consuming(
        EZrOwnershipIntrinsicOperation operation) {
    return operation == ZR_OWNERSHIP_INTRINSIC_SHARE ||
           operation == ZR_OWNERSHIP_INTRINSIC_INTO_GC ||
           operation == ZR_OWNERSHIP_INTRINSIC_DROP;
}

static TZrBool intrinsic_into_gc_has_resource_target(
        SZrCompilerState *cs,
        const SZrInferredType *inputType) {
    SZrTypePrototypeInfo *prototype;

    if (cs == ZR_NULL || inputType == ZR_NULL || inputType->typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    prototype = find_compiler_type_prototype_inference(cs, inputType->typeName);
    return prototype != ZR_NULL &&
           (prototype->modifierFlags & ZR_DECLARATION_MODIFIER_RESOURCE) != 0u;
}

static TZrUInt32 intrinsic_argument_place_id(
        SZrCompilerState *cs,
        SZrAstNode *argument) {
    const SZrTypeBinding *binding;
    const SZrSemanticReferenceFact *reference;

    if (cs == ZR_NULL || argument == ZR_NULL) {
        return 0u;
    }
    if (argument->type == ZR_AST_IDENTIFIER_LITERAL &&
        argument->data.identifier.name != ZR_NULL) {
        binding = ZrParser_TypeEnvironment_FindVariableBinding(
                cs->typeEnv, argument->data.identifier.name);
        if (binding != ZR_NULL) {
            return binding->placeId;
        }
    }
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext, argument, ZR_SEMANTIC_REFERENCE_READ);
    return reference != ZR_NULL ? reference->placeId : 0u;
}

static EZrSemanticOwnershipFactKind intrinsic_ownership_fact_kind(
        EZrOwnershipIntrinsicOperation operation) {
    switch (operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
            return ZR_SEMANTIC_OWNERSHIP_FACT_MOVE;
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            return ZR_SEMANTIC_OWNERSHIP_FACT_COPY;
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            return ZR_SEMANTIC_OWNERSHIP_FACT_BORROW;
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            return ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE;
        default:
            return ZR_SEMANTIC_OWNERSHIP_FACT_UNKNOWN;
    }
}

static void publish_intrinsic_facts(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const SZrInferredType *inputType,
        const SZrInferredType *resultType,
        TZrUInt32 placeId) {
    SZrOwnershipIntrinsicExpression *intrinsic =
            &node->data.ownershipIntrinsicExpression;
    SZrOwnershipIntrinsicFact intrinsicFact;
    SZrSemanticOwnershipFact ownershipFact;

    memset(&intrinsicFact, 0, sizeof(intrinsicFact));
    intrinsicFact.node = node;
    intrinsicFact.argument = intrinsic->argument;
    intrinsicFact.range = node->location;
    intrinsicFact.nameRange = intrinsic->nameRange;
    intrinsicFact.argumentRange = intrinsic->argument->location;
    intrinsicFact.operation = intrinsic->operation;
    intrinsicFact.inputType = *inputType;
    intrinsicFact.resultType = *resultType;
    intrinsicFact.placeId = placeId;
    intrinsicFact.loanId = 0u;
    intrinsicFact.consuming = intrinsic_is_consuming(intrinsic->operation);
    ZrParser_SemanticFacts_AppendOwnershipIntrinsic(
            cs->semanticContext, &intrinsicFact);

    memset(&ownershipFact, 0, sizeof(ownershipFact));
    ownershipFact.node = node;
    ownershipFact.range = node->location;
    ownershipFact.kind = intrinsic_ownership_fact_kind(intrinsic->operation);
    ownershipFact.qualifier = resultType->ownershipQualifier;
    ownershipFact.relatedNode = intrinsic->argument;
    ZrParser_SemanticFacts_AppendOwnership(cs->semanticContext, &ownershipFact);
}

TZrBool infer_ownership_intrinsic_expression_type(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrInferredType *result) {
    SZrOwnershipIntrinsicExpression *intrinsic;
    SZrInferredType inputType;
    EZrParserPlaceExpressionKind placeKind;
    TZrUInt32 placeId;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL ||
        node->type != ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION) {
        return ZR_FALSE;
    }
    intrinsic = &node->data.ownershipIntrinsicExpression;
    if (intrinsic->argument == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &inputType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(cs, intrinsic->argument, &inputType)) {
        ZrParser_InferredType_Free(cs->state, &inputType);
        return ZR_FALSE;
    }
    if (!intrinsic_operand_qualifier_matches(
                intrinsic->operation, inputType.ownershipQualifier)) {
        ZrParser_Compiler_Error(
                cs,
                intrinsic_operand_error(intrinsic->operation),
                intrinsic->argument->location);
        ZrParser_InferredType_Free(cs->state, &inputType);
        return ZR_FALSE;
    }
    /* A nullable owner cannot satisfy a transition that requires a live handle.
     * `drop` is deliberately exempt: releasing a nullable wake result is a
     * defined no-op and is part of the cleanup contract. */
    if (inputType.isNullable &&
        intrinsic->operation != ZR_OWNERSHIP_INTRINSIC_DROP) {
        SZrStructuredDiagnostic diagnostic;

        ZrParser_StructuredDiagnostic_Init(&diagnostic);
        if (ZrParser_DiagnosticBuilder_BuildNullableOwnershipIntrinsicOperand(
                    cs->state,
                    &diagnostic,
                    intrinsic->argument->location)) {
            ZrParser_Compiler_StructuredError(cs, &diagnostic);
        } else {
            ZrParser_Compiler_Error(
                    cs,
                    "Ownership transition requires a live owner",
                    intrinsic->argument->location);
        }
        ZrParser_InferredType_Free(cs->state, &inputType);
        return ZR_FALSE;
    }
    if (intrinsic->operation == ZR_OWNERSHIP_INTRINSIC_INTO_GC &&
        !intrinsic_into_gc_has_resource_target(cs, &inputType)) {
        ZrParser_Compiler_Error(
                cs,
                "intoGc(owner) requires a Unique<T> resource owner",
                intrinsic->argument->location);
        ZrParser_InferredType_Free(cs->state, &inputType);
        return ZR_FALSE;
    }
    if (intrinsic_is_consuming(intrinsic->operation)) {
        placeKind = ZrParser_PlaceExpression_Classify(intrinsic->argument);
        if (placeKind == ZR_PARSER_PLACE_EXPRESSION_INVALID) {
            ZrParser_Compiler_Error(
                    cs,
                    "Consuming ownership intrinsic requires a place expression",
                    intrinsic->argument->location);
            ZrParser_InferredType_Free(cs->state, &inputType);
            return ZR_FALSE;
        }
        if (intrinsic->argument->type != ZR_AST_IDENTIFIER_LITERAL) {
            ZrParser_Compiler_Error(
                    cs,
                    "Consuming ownership intrinsic currently requires a local owner binding",
                    intrinsic->argument->location);
            ZrParser_InferredType_Free(cs->state, &inputType);
            return ZR_FALSE;
        }
    }

    placeId = intrinsic_argument_place_id(cs, intrinsic->argument);
    ZrParser_InferredType_Copy(cs->state, result, &inputType);
    result->referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    switch (intrinsic->operation) {
        case ZR_OWNERSHIP_INTRINSIC_SHARE:
            result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
            break;
        case ZR_OWNERSHIP_INTRINSIC_DEGRADE:
            result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
            break;
        case ZR_OWNERSHIP_INTRINSIC_WAKE:
            result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_SHARED;
            result->isNullable = ZR_TRUE;
            break;
        case ZR_OWNERSHIP_INTRINSIC_INTO_GC:
            result->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            result->gcBridgeKind = ZR_GC_BRIDGE_BOX;
            break;
        case ZR_OWNERSHIP_INTRINSIC_DROP:
            ZrParser_InferredType_Free(cs->state, result);
            ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_NULL);
            break;
        default:
            ZrParser_InferredType_Free(cs->state, &inputType);
            return ZR_FALSE;
    }

    publish_intrinsic_facts(cs, node, &inputType, result, placeId);
    ZrParser_InferredType_Free(cs->state, &inputType);
    return ZR_TRUE;
}
