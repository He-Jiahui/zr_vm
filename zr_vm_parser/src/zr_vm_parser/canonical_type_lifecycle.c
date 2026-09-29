#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include "zr_vm_core/array.h"

/* SemanticContext_Reset 先清空类型定义，再在此释放节点自有容器；外层池仍可供下一轮分析复用。 */
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
    /* 旧索引不能跨分析轮次保留，否则下一轮驻留可能命中已释放的节点内容。 */
    context->canonicalTypes.length = 0;
    ZrParser_CanonicalTypeIndex_Reset(context);
}

/* SemanticContext_Free 在清理类型定义后调用；复用清理完成才释放池和索引的存储。 */
void ZrParser_CanonicalType_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }
    ZrParser_CanonicalType_Reset(context);
    ZrCore_Array_Free(context->state, &context->canonicalTypes);
    ZrParser_CanonicalTypeIndex_Free(context);
}
