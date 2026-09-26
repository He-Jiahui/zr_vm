#include "zr_vm_parser/semantic_source_metadata.h"

#include <string.h>

#include "zr_vm_core/string.h"
#include "zr_vm_parser/type_inference.h"

TZrBool ZrParser_SemanticMetadata_InferUsingResourceType(
        SZrCompilerState *cs, SZrAstNode *resource, SZrInferredType *outType) {
    if (cs == ZR_NULL || resource == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }
    if (resource->type == ZR_AST_IDENTIFIER_LITERAL && cs->typeEnv != ZR_NULL &&
        resource->data.identifier.name != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupVariable(
                cs->state, cs->typeEnv, resource->data.identifier.name, outType)) {
        return ZR_TRUE;
    }
    return ZrParser_ExpressionType_Infer(cs, resource, outType);
}

EZrOwnershipBuiltinKind ZrParser_SemanticMetadata_UsingCleanupBuiltin(
        EZrOwnershipQualifier ownershipQualifier) {
    switch (ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return ZR_OWNERSHIP_BUILTIN_KIND_DROP;
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return ZR_OWNERSHIP_BUILTIN_KIND_RETURN_LOAN;
        default:
            return ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
}

TZrBool ZrParser_SemanticMetadata_RecordUsingCleanup(
        SZrCompilerState *cs, SZrAstNode *usingNode, const SZrInferredType *resourceType) {
    SZrDeterministicCleanupStep step;
    SZrAstNode *resource;
    const SZrTypeBinding *binding = ZR_NULL;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || usingNode == ZR_NULL ||
        usingNode->type != ZR_AST_USING_STATEMENT ||
        usingNode->data.usingStatement.guardKind != ZR_USING_GUARD_DROP) {
        return ZR_FALSE;
    }
    resource = usingNode->data.usingStatement.resource;
    if (resource == ZR_NULL) {
        return ZR_FALSE;
    }
    if (resource->type == ZR_AST_IDENTIFIER_LITERAL && cs->typeEnv != ZR_NULL) {
        binding = ZrParser_TypeEnvironment_FindVariableBinding(
                cs->typeEnv, resource->data.identifier.name);
    }
    memset(&step, 0, sizeof(step));
    step.kind = ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(cs->semanticContext);
    step.ownerRegionId = step.regionId;
    step.symbolId = binding != ZR_NULL ? binding->symbolId : ZR_SEMANTIC_ID_INVALID;
    step.declarationOrder = (TZrInt32)cs->semanticContext->cleanupPlan.length;
    step.ownershipQualifier = resourceType != ZR_NULL
                                      ? resourceType->ownershipQualifier
                                      : ZR_OWNERSHIP_QUALIFIER_NONE;
    step.ownershipBuiltinKind =
            ZrParser_SemanticMetadata_UsingCleanupBuiltin(step.ownershipQualifier);
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    return ZrParser_Semantic_AppendCleanupStep(cs->semanticContext, &step);
}

TZrBool ZrParser_SemanticMetadata_RecordTemplateSegments(
        SZrSemanticContext *context, SZrAstNode *templateNode) {
    SZrAstNodeArray *segments;

    if (context == ZR_NULL || templateNode == ZR_NULL ||
        templateNode->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        return ZR_FALSE;
    }
    segments = templateNode->data.templateStringLiteral.segments;
    if (segments == ZR_NULL || segments->nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0; index < segments->count; index++) {
        SZrAstNode *node = segments->nodes[index];
        SZrTemplateSegment segment;
        if (node == ZR_NULL) {
            continue;
        }
        memset(&segment, 0, sizeof(segment));
        if (node->type == ZR_AST_STRING_LITERAL) {
            segment.staticText = node->data.stringLiteral.value;
            if (segment.staticText == ZR_NULL) {
                segment.staticText = ZrCore_String_Create(context->state, "", 0);
            }
        } else if (node->type == ZR_AST_INTERPOLATED_SEGMENT) {
            segment.isInterpolation = ZR_TRUE;
            segment.expression = node->data.interpolatedSegment.expression;
        } else {
            continue;
        }
        if (!ZrParser_Semantic_AppendTemplateSegment(context, &segment)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
