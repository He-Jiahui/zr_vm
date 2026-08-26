#include "compiler_internal.h"

#include "zr_vm_parser/semantic_relations.h"

TZrBool compiler_publish_type_hierarchy_relation(
        SZrCompilerState *cs,
        const SZrAstNode *sourceDeclaration,
        const SZrTypePrototypeInfo *targetPrototype,
        TZrBool isImplementation) {
    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL ||
        sourceDeclaration == ZR_NULL || targetPrototype == ZR_NULL ||
        targetPrototype->declarationNode == ZR_NULL) {
        return ZR_TRUE;
    }
    return ZrParser_SemanticRelations_PublishTypeDeclarationRelation(
            cs->semanticContext,
            isImplementation
                    ? ZR_SEMANTIC_RELATION_IMPLEMENTATION
                    : ZR_SEMANTIC_RELATION_BASE_TYPE,
            sourceDeclaration,
            targetPrototype->declarationNode);
}
