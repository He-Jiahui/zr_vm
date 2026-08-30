#include "type_inference_type_display_alias.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_display.h"

void type_inference_publish_explicit_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        SZrString *alias,
        const SZrAstNode *typeUseNode) {
    TZrTypeId typeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || type == ZR_NULL ||
        alias == ZR_NULL || typeUseNode == ZR_NULL) {
        return;
    }
    typeId = ZrParser_CanonicalType_FromInferred(cs->semanticContext, type);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    (void)ZrParser_SemanticTypeDisplayAlias_Publish(
            cs->semanticContext, typeId, &typeUseNode->location, alias);
}
