#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include "zr_vm_core/array.h"

void ZrParser_CanonicalType_Reset(SZrSemanticContext *context) {
    TZrSize index;

    if (context == ZR_NULL) {
        return;
    }

    for (index = 0; index < context->canonicalTypes.length; index++) {
        SZrCanonicalTypeNode *node = (SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                index);
        if (node == ZR_NULL) {
            continue;
        }
        if (node->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
            ZrCore_Array_Free(context->state, &node->data.genericInstance.arguments);
        } else if (node->kind == ZR_CANONICAL_TYPE_TUPLE) {
            ZrCore_Array_Free(context->state, &node->data.typeList.elementTypeIds);
        } else if (node->kind == ZR_CANONICAL_TYPE_UNION) {
            ZrCore_Array_Free(context->state, &node->data.unionType.variantTypeIds);
        } else if (node->kind == ZR_CANONICAL_TYPE_FUNCTION) {
            ZrCore_Array_Free(context->state, &node->data.function.parameterContracts);
        }
    }
    context->canonicalTypes.length = 0;
    ZrParser_CanonicalTypeIndex_Reset(context);
}

void ZrParser_CanonicalType_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    ZrParser_CanonicalType_Reset(context);
    ZrCore_Array_Free(context->state, &context->canonicalTypes);
    ZrParser_CanonicalTypeIndex_Free(context);
}
