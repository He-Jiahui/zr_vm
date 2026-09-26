#include "type_inference_semantic_facts.h"

#include <string.h>

static void type_inference_record_identifier_fact(SZrCompilerState *cs,
                                                  SZrAstNode *node,
                                                  const SZrTypeBinding *binding,
                                                  EZrSemanticReferenceKind kind) {
    SZrSemanticReferenceFact fact;

    if (cs == ZR_NULL ||
        cs->semanticContext == ZR_NULL ||
        node == ZR_NULL ||
        node->type != ZR_AST_IDENTIFIER_LITERAL ||
        binding == ZR_NULL ||
        binding->name == ZR_NULL) {
        return;
    }

    /* Inference-only temporaries have no source declaration. Canonical injected
     * bindings can belong to another context, so retain their validated opaque
     * identity without requiring or inventing a local declaration record. */
    if (binding->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION) {
        if (!binding->hasDeclarationRange || binding->symbolId == ZR_SEMANTIC_ID_INVALID) {
            return;
        }
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = node;
    fact.range = node->location;
    if (binding->hasDeclarationRange) {
        fact.declarationRange = binding->declarationRange;
    }
    fact.kind = kind;
    fact.symbolId = binding->symbolId;
    fact.typeId = binding->typeId;
    fact.placeId = binding->placeId;
    fact.originKind = binding->originKind;
    fact.runtimeRootKind = binding->runtimeRootKind;
    fact.originToken = binding->originToken;
    fact.originIndex = binding->originIndex;
    fact.ownershipQualifier = binding->type.ownershipQualifier;
    fact.name = binding->name;
    fact.isResolved = ZR_TRUE;
    if (kind == ZR_SEMANTIC_REFERENCE_WRITE) {
        fact.definitionRange = node->location;
        fact.hasDefinitionRange = ZR_TRUE;
        fact.definiteAssignmentState = ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT;
        fact.hasDefiniteAssignmentState = ZR_TRUE;
    }
    ZrParser_SemanticFacts_AppendReference(cs->semanticContext, &fact);
}

void type_inference_record_identifier_reference_fact(SZrCompilerState *cs,
                                                      SZrAstNode *node,
                                                      const SZrTypeBinding *binding) {
    type_inference_record_identifier_fact(cs, node, binding, ZR_SEMANTIC_REFERENCE_READ);
}

void type_inference_record_identifier_write_reference_fact(SZrCompilerState *cs,
                                                           SZrAstNode *node,
                                                           const SZrTypeBinding *binding) {
    type_inference_record_identifier_fact(cs, node, binding, ZR_SEMANTIC_REFERENCE_WRITE);
}
