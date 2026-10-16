#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

/**
 * @brief 按语义类型 ID 查询 canonical type node。
 * @note ID 由 interner 单调分配并按节点追加顺序排列，因此可二分查询；返回的是 context 所有的借用指针。
 * @return 找到时返回节点地址；后续 interning 可能扩容数组并使地址失效，Reset/Free 后必然失效。
 */
const SZrCanonicalTypeNode *ZrParser_CanonicalType_Find(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    TZrSize low;
    TZrSize high;

    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    low = 0;
    high = context->canonicalTypes.length;
    while (low < high) {
        TZrSize middle = low + (high - low) / 2U;
        const SZrCanonicalTypeNode *node = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                (SZrArray *)&context->canonicalTypes,
                middle);

        if (node->id < typeId) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }

    if (low < context->canonicalTypes.length) {
        const SZrCanonicalTypeNode *node = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                (SZrArray *)&context->canonicalTypes,
                low);
        if (node->id == typeId) {
            return node;
        }
    }
    return ZR_NULL;
}
