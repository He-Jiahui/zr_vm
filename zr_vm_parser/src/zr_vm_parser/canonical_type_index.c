#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include <stdint.h>

#define ZR_CANONICAL_TYPE_INDEX_INITIAL_BUCKETS ((TZrSize)64U)
#define ZR_CANONICAL_TYPE_INDEX_NONE ((TZrUInt32)UINT32_MAX)

static TZrSize canonical_type_index_bucket(TZrUInt64 structuralHash, TZrSize bucketCount) {
    return (TZrSize)(structuralHash & (TZrUInt64)(bucketCount - 1U));
}

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

void ZrParser_CanonicalTypeIndex_Free(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(context->state, &context->canonicalTypeHashBuckets);
    ZrCore_Array_Free(context->state, &context->canonicalTypeHashNext);
}

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
