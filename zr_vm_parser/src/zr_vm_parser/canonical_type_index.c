#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include <stdint.h>

#define ZR_CANONICAL_TYPE_INDEX_INITIAL_BUCKETS ((TZrSize)64U)
#define ZR_CANONICAL_TYPE_INDEX_NONE ((TZrUInt32)UINT32_MAX)

/**
 * @brief 将结构 hash 映射到容量为二次幂的桶数组。
 * @pre bucketCount 非零且为二次幂；容量由 Index_Init/扩容路径提供。
 */
static TZrSize canonical_type_index_bucket(TZrUInt64 structuralHash, TZrSize bucketCount) {
    return (TZrSize)(structuralHash & (TZrUInt64)(bucketCount - 1U));
}

/**
 * @brief 创建空桶表并将每个 head 初始化为链尾哨兵。
 * @note Init 与扩容重建共用此步骤，桶表只存 node index，不持有节点对象。
 * BUG: Array_Init/Push 不暴露分配失败；失败时无法保证空桶表 backing storage 可写。
 */
static void canonical_type_index_init_buckets(
        SZrSemanticContext *context,
        SZrArray *buckets,
        TZrSize bucketCount) {
    TZrUInt32 empty = ZR_CANONICAL_TYPE_INDEX_NONE;
    TZrSize index;

    ZrCore_Array_Init(context->state, buckets, sizeof(TZrUInt32), bucketCount);
    for (index = 0; index < bucketCount; index++) {
        ZrCore_Array_Push(context->state, buckets, &empty);
    }
}

/**
 * @brief 根据 canonicalTypes 现有节点重建所有碰撞链，再替换旧桶数组。
 * @note 必须在节点数组与 canonicalTypeHashNext 同步增长后运行；重建按节点当前顺序头插。
 * BUG: 新桶分配与填充失败不能通过 Array_Init/Push 传播；OOM 时继续 Get/写桶会访问无效存储。
 */
static void canonical_type_index_rebuild(SZrSemanticContext *context, TZrSize bucketCount) {
    SZrArray buckets;
    TZrSize nodeIndex;

    canonical_type_index_init_buckets(context, &buckets, bucketCount);
    for (nodeIndex = 0; nodeIndex < context->canonicalTypes.length; nodeIndex++) {
        const SZrCanonicalTypeNode *node = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &context->canonicalTypes,
                nodeIndex);
        TZrSize bucketIndex = canonical_type_index_bucket(node->structuralHash, bucketCount);
        TZrUInt32 *bucketHead = (TZrUInt32 *)ZrCore_Array_Get(&buckets, bucketIndex);
        TZrUInt32 *next = (TZrUInt32 *)ZrCore_Array_Get(&context->canonicalTypeHashNext, nodeIndex);

        *next = *bucketHead;
        *bucketHead = (TZrUInt32)nodeIndex;
    }

    ZrCore_Array_Free(context->state, &context->canonicalTypeHashBuckets);
    context->canonicalTypeHashBuckets = buckets;
}

/**
 * @brief 为 canonical type interning 建立结构哈希桶和碰撞链数组。
 * @note semantic context 初始化时调用；Reset 保留桶容量，Free 才释放 backing storage。
 * BUG: 桶初始化后立即逐项 Push；Array_Init/Push 不报告分配失败，OOM 可使后续写入 NULL backing storage。
 */
void ZrParser_CanonicalTypeIndex_Init(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    canonical_type_index_init_buckets(
            context,
            &context->canonicalTypeHashBuckets,
            ZR_CANONICAL_TYPE_INDEX_INITIAL_BUCKETS);
    ZrCore_Array_Init(
            context->state,
            &context->canonicalTypeHashNext,
            sizeof(TZrUInt32),
            ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

/**
 * @brief 清空碰撞链索引，使当前 type 节点不再能通过结构哈希命中。
 * @note 由 canonical type reset 在释放节点子数组后调用；桶数组保留以供同一 context 重用。
 */
void ZrParser_CanonicalTypeIndex_Reset(SZrSemanticContext *context) {
    TZrUInt32 empty = ZR_CANONICAL_TYPE_INDEX_NONE;
    TZrSize index;

    if (context == ZR_NULL || !context->canonicalTypeHashBuckets.isValid) {
        return;
    }

    for (index = 0; index < context->canonicalTypeHashBuckets.length; index++) {
        ZrCore_Array_Set(&context->canonicalTypeHashBuckets, index, &empty);
    }
    ZrCore_Array_Empty(&context->canonicalTypeHashNext);
}

/**
 * @brief 释放哈希桶与节点碰撞链的 backing storage。
 * @note 生命周期由 canonical type free 管理；释放后须重新 Init 才能插入或查桶。
 */
void ZrParser_CanonicalTypeIndex_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(context->state, &context->canonicalTypeHashBuckets);
    ZrCore_Array_Free(context->state, &context->canonicalTypeHashNext);
}

/**
 * @brief 将新追加的 canonical type node 接入结构哈希的碰撞链。
 * @pre nodeIndex 指向刚进入 canonicalTypes 的节点；正常生产路径由 canonical_type_store 每节点调用一次。
 * @note 负载过高时重建并扩容全部链；First/Next 再沿链枚举同 hash 候选，结构相等性仍由 interner 判定。
 * TODO: 重复插入同一 nodeIndex 会令节点自链接并使遍历不终止；当前调用者只插入新节点，需明确是否强化为幂等或拒绝重复。
 * BUG: 扩展碰撞链和重建桶依赖无失败返回值的 Array_Push，内存分配失败不能安全传播到 interner。
 */
TZrBool ZrParser_CanonicalTypeIndex_Insert(SZrSemanticContext *context, TZrSize nodeIndex) {
    const SZrCanonicalTypeNode *node;
    TZrUInt32 empty = ZR_CANONICAL_TYPE_INDEX_NONE;
    TZrSize bucketIndex;
    TZrUInt32 *bucketHead;
    TZrUInt32 *next;

    if (context == ZR_NULL ||
        nodeIndex >= context->canonicalTypes.length ||
        nodeIndex >= (TZrSize)UINT32_MAX ||
        !context->canonicalTypeHashBuckets.isValid) {
        return ZR_FALSE;
    }

    while (context->canonicalTypeHashNext.length <= nodeIndex) {
        ZrCore_Array_Push(context->state, &context->canonicalTypeHashNext, &empty);
    }

    if (context->canonicalTypes.length >
        context->canonicalTypeHashBuckets.length - context->canonicalTypeHashBuckets.length / 4U) {
        canonical_type_index_rebuild(context, context->canonicalTypeHashBuckets.length * 2U);
        return ZR_TRUE;
    }

    node = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(&context->canonicalTypes, nodeIndex);
    bucketIndex = canonical_type_index_bucket(
            node->structuralHash,
            context->canonicalTypeHashBuckets.length);
    bucketHead = (TZrUInt32 *)ZrCore_Array_Get(&context->canonicalTypeHashBuckets, bucketIndex);
    next = (TZrUInt32 *)ZrCore_Array_Get(&context->canonicalTypeHashNext, nodeIndex);
    *next = *bucketHead;
    *bucketHead = (TZrUInt32)nodeIndex;
    return ZR_TRUE;
}

/**
 * @brief 返回给定 structural hash 对应碰撞链的首节点索引。
 * @pre 索引已初始化且未与 canonicalTypes 分离；返回 ZR_MAX_SIZE 表示该桶为空或不可用。
 * @note 此接口只定位候选；调用者必须再比较完整类型结构，不能把 hash 当作类型身份。
 */
TZrSize ZrParser_CanonicalTypeIndex_First(
        const SZrSemanticContext *context,
        TZrUInt64 structuralHash) {
    const TZrUInt32 *bucketHead;
    TZrSize bucketIndex;

    if (context == ZR_NULL ||
        !context->canonicalTypeHashBuckets.isValid ||
        context->canonicalTypeHashBuckets.length == 0) {
        return ZR_MAX_SIZE;
    }

    bucketIndex = canonical_type_index_bucket(
            structuralHash,
            context->canonicalTypeHashBuckets.length);
    bucketHead = (const TZrUInt32 *)ZrCore_Array_Get(
            (SZrArray *)&context->canonicalTypeHashBuckets,
            bucketIndex);
    return *bucketHead == ZR_CANONICAL_TYPE_INDEX_NONE ? ZR_MAX_SIZE : (TZrSize)*bucketHead;
}

/**
 * @brief 返回同一哈希碰撞链中指定节点之后的索引。
 * @pre nodeIndex 是 First/Next 返回的有效节点索引；ZR_MAX_SIZE 代表遍历结束。
 * @note 链索引绑定当前 canonicalTypes 布局，Reset/Free 后不得继续使用旧索引。
 */
TZrSize ZrParser_CanonicalTypeIndex_Next(
        const SZrSemanticContext *context,
        TZrSize nodeIndex) {
    const TZrUInt32 *next;

    if (context == ZR_NULL || nodeIndex >= context->canonicalTypeHashNext.length) {
        return ZR_MAX_SIZE;
    }

    next = (const TZrUInt32 *)ZrCore_Array_Get(
            (SZrArray *)&context->canonicalTypeHashNext,
            nodeIndex);
    return *next == ZR_CANONICAL_TYPE_INDEX_NONE ? ZR_MAX_SIZE : (TZrSize)*next;
}

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
