#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_FACT_CAPACITY_ORIGINAL) && defined(ZR_FACT_CAPACITY_UNCHECKED)
#error Select exactly one source epoch
#endif
#include "../../zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c"

typedef enum EFactKind { FACT_RANGE, FACT_SHAPE, FACT_NULL } EFactKind;
typedef union UFact {
    SZrExecIrRangeFact range;
    SZrExecIrShapeFact shape;
    SZrExecIrNullabilityFact nullability;
} UFact;

static const char *kind_name(EFactKind kind) {
    return kind == FACT_RANGE ? "range" : kind == FACT_SHAPE ? "shape" : "null";
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%u: %s\n", __FILE__, (unsigned int)__LINE__, #condition); \
        ok = ZR_FALSE; \
        goto cleanup; \
    } \
} while (0)

#define NUM_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%u: %s\n", __FILE__, (unsigned int)__LINE__, #condition); \
        ok = ZR_FALSE; \
    } \
} while (0)

#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
typedef struct SSnapshot {
    unsigned char header[sizeof(SZrExecIrAnalysisFacts)];
    SZrExecIrRangeFact ranges[17];
    SZrExecIrShapeFact shapes[17];
    SZrExecIrNullabilityFact nulls[17];
    TZrUInt32 rangeCount;
    TZrUInt32 shapeCount;
    TZrUInt32 nullCount;
} SSnapshot;

static UFact make_fact(EFactKind kind, TZrUInt32 valueId, TZrUInt32 serial,
                       TZrUInt64 generation) {
    UFact value;
    memset(&value, 0, sizeof(value));
    if (kind == FACT_RANGE) {
        value.range.valueId = valueId;
        value.range.lower = (TZrInt64)serial;
        value.range.upper = (TZrInt64)serial + 3;
        value.range.hasLower = value.range.hasUpper = ZR_TRUE;
        value.range.generation = generation;
    } else if (kind == FACT_SHAPE) {
        value.shape.valueId = valueId;
        value.shape.typeToken = 2u;
        value.shape.layoutId = serial + 10u;
        value.shape.shapeId = serial + 20u;
        value.shape.generation = generation;
    } else {
        value.nullability.valueId = valueId;
        value.nullability.state = (serial % 2u) ? ZR_EXEC_IR_NULL_FACT_NULL :
                                                ZR_EXEC_IR_NULL_FACT_NONNULL;
        value.nullability.generation = generation;
    }
    return value;
}

static TZrBool add_fact(SZrExecIrAnalysisFacts *facts, EFactKind kind,
                        const UFact *value) {
    if (kind == FACT_RANGE) {
        return ZrParser_ExecIr_AnalysisFacts_AddRange(facts,
                    value == ZR_NULL ? ZR_NULL : &value->range);
    }
    if (kind == FACT_SHAPE) {
        return ZrParser_ExecIr_AnalysisFacts_AddShape(facts,
                    value == ZR_NULL ? ZR_NULL : &value->shape);
    }
    return ZrParser_ExecIr_AnalysisFacts_AddNullability(facts,
                    value == ZR_NULL ? ZR_NULL : &value->nullability);
}

static TZrUInt32 fact_count(const SZrExecIrAnalysisFacts *facts, EFactKind kind) {
    return kind == FACT_RANGE ? facts->rangeCount : kind == FACT_SHAPE ?
                               facts->shapeCount : facts->nullabilityCount;
}

static TZrUInt32 fact_capacity(const SZrExecIrAnalysisFacts *facts, EFactKind kind) {
    return kind == FACT_RANGE ? facts->rangeCapacity : kind == FACT_SHAPE ?
                               facts->shapeCapacity : facts->nullabilityCapacity;
}

static TZrBool equal_fact(const SZrExecIrAnalysisFacts *facts, EFactKind kind,
                          TZrUInt32 index, const UFact *expected) {
    if (kind == FACT_RANGE) {
        const SZrExecIrRangeFact *a = &facts->ranges[index];
        const SZrExecIrRangeFact *b = &expected->range;
        return (TZrBool)(a->valueId == b->valueId && a->lower == b->lower &&
               a->upper == b->upper && a->generation == b->generation &&
               a->hasLower == b->hasLower && a->hasUpper == b->hasUpper &&
               a->overflowed == b->overflowed && a->lengthMutable == b->lengthMutable);
    }
    if (kind == FACT_SHAPE) {
        const SZrExecIrShapeFact *a = &facts->shapes[index];
        const SZrExecIrShapeFact *b = &expected->shape;
        return (TZrBool)(a->valueId == b->valueId && a->typeToken == b->typeToken &&
               a->layoutId == b->layoutId && a->shapeId == b->shapeId &&
               a->generation == b->generation);
    }
    return (TZrBool)(facts->nullability[index].valueId == expected->nullability.valueId &&
           facts->nullability[index].state == expected->nullability.state &&
           facts->nullability[index].generation == expected->nullability.generation);
}

static void capture(const SZrExecIrAnalysisFacts *facts, SSnapshot *out) {
    memcpy(out->header, facts, sizeof(*facts));
    out->rangeCount = facts->rangeCount;
    out->shapeCount = facts->shapeCount;
    out->nullCount = facts->nullabilityCount;
    if (out->rangeCount) memcpy(out->ranges, facts->ranges,
                               out->rangeCount * sizeof(out->ranges[0]));
    if (out->shapeCount) memcpy(out->shapes, facts->shapes,
                               out->shapeCount * sizeof(out->shapes[0]));
    if (out->nullCount) memcpy(out->nulls, facts->nullability,
                              out->nullCount * sizeof(out->nulls[0]));
}

static TZrBool unchanged(const SZrExecIrAnalysisFacts *facts, const SSnapshot *old) {
    if (memcmp(old->header, facts, sizeof(*facts)) != 0) return ZR_FALSE;
    if (old->rangeCount && memcmp(old->ranges, facts->ranges,
            old->rangeCount * sizeof(old->ranges[0])) != 0) return ZR_FALSE;
    if (old->shapeCount && memcmp(old->shapes, facts->shapes,
            old->shapeCount * sizeof(old->shapes[0])) != 0) return ZR_FALSE;
    if (old->nullCount && memcmp(old->nulls, facts->nullability,
            old->nullCount * sizeof(old->nulls[0])) != 0) return ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool test_growth(EFactKind kind) {
    SZrExecIrAnalysisFacts facts;
    UFact expected[17];
    UFact input;
    unsigned char inputBefore[sizeof(input)];
    TZrUInt32 i;
    TZrUInt32 j;
    TZrUInt32 previousCapacity = 0u;
    TZrBool ok = ZR_TRUE;
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    CHECK(facts.generation == 1u && fact_count(&facts, kind) == 0u &&
          fact_capacity(&facts, kind) == 0u);
    for (i = 0u; i < 17u; ++i) {
        input = make_fact(kind, i + 1u, i, 1u);
        memcpy(inputBefore, &input, sizeof(input));
        memcpy(&expected[i], &input, sizeof(input));
        CHECK(add_fact(&facts, kind, &input));
        CHECK(memcmp(inputBefore, &input, sizeof(input)) == 0);
        CHECK(fact_count(&facts, kind) == i + 1u);
        CHECK(fact_capacity(&facts, kind) >= i + 1u);
        if (i == 0u || i == 8u || i == 16u) {
            CHECK(fact_capacity(&facts, kind) > previousCapacity);
        } else {
            CHECK(fact_capacity(&facts, kind) == previousCapacity);
        }
        previousCapacity = fact_capacity(&facts, kind);
        CHECK(facts.generation == 1u);
        CHECK((kind == FACT_RANGE || (facts.rangeCount == 0u &&
              facts.rangeCapacity == 0u && facts.ranges == ZR_NULL)) &&
              (kind == FACT_SHAPE || (facts.shapeCount == 0u &&
              facts.shapeCapacity == 0u && facts.shapes == ZR_NULL)) &&
              (kind == FACT_NULL || (facts.nullabilityCount == 0u &&
              facts.nullabilityCapacity == 0u && facts.nullability == ZR_NULL)));
        for (j = 0u; j <= i; ++j) CHECK(equal_fact(&facts, kind, j, &expected[j]));
    }
cleanup:
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
    return ok;
}

static void invalidate_input(UFact *value, EFactKind kind, unsigned int mode) {
    if (kind == FACT_RANGE) {
        if (mode == 0u) value->range.valueId = 0u;
        if (mode == 1u) value->range.generation = 0u;
        if (mode == 2u) value->range.generation = 2u;
        if (mode == 3u) value->range.lower = value->range.upper + 1;
    } else if (kind == FACT_SHAPE) {
        if (mode == 0u) value->shape.valueId = 0u;
        if (mode == 1u) value->shape.generation = 0u;
        if (mode == 2u) value->shape.generation = 2u;
        if (mode == 3u) value->shape.typeToken = 0u;
        if (mode == 4u) value->shape.layoutId = 0u;
        if (mode == 5u) value->shape.shapeId = 0u;
    } else {
        if (mode == 0u) value->nullability.valueId = 0u;
        if (mode == 1u) value->nullability.generation = 0u;
        if (mode == 2u) value->nullability.generation = 2u;
        if (mode == 3u) value->nullability.state =
                (EZrExecIrNullFact)(ZR_EXEC_IR_NULL_FACT_NULL + 1);
    }
}

static TZrBool test_rejection_rollback(EFactKind kind) {
    SZrExecIrAnalysisFacts facts;
    SSnapshot before;
    UFact seed;
    UFact input;
    unsigned char inputBefore[sizeof(input)];
    unsigned int i;
    unsigned int limit = kind == FACT_SHAPE ? 6u : 4u;
    TZrBool ok = ZR_TRUE;
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    /* Keep real records in all three owned arrays while rejecting one kind. */
    for (i = 0u; i < 3u; ++i) {
        seed = make_fact((EFactKind)i, 1u, 0u, 1u);
        CHECK(add_fact(&facts, (EFactKind)i, &seed));
    }
    capture(&facts, &before);
    CHECK(!add_fact(&facts, kind, ZR_NULL));
    CHECK(unchanged(&facts, &before));
    input = make_fact(kind, 2u, 5u, 1u);
    memcpy(inputBefore, &input, sizeof(input));
    CHECK(!add_fact(ZR_NULL, kind, &input));
    CHECK(memcmp(inputBefore, &input, sizeof(input)) == 0);
    CHECK(unchanged(&facts, &before));
    for (i = 0u; i < limit; ++i) {
        input = make_fact(kind, 2u, 5u, 1u);
        invalidate_input(&input, kind, i);
        memcpy(inputBefore, &input, sizeof(input));
        CHECK(!add_fact(&facts, kind, &input));
        CHECK(unchanged(&facts, &before));
        CHECK(memcmp(inputBefore, &input, sizeof(input)) == 0);
    }
cleanup:
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
    return ok;
}

static TZrBool test_generation_latest(EFactKind kind) {
    SZrExecIrAnalysisFacts facts;
    SZrExecIrRangeFact *rangeStorage;
    SZrExecIrShapeFact *shapeStorage;
    SZrExecIrNullabilityFact *nullStorage;
    TZrUInt32 rangeCapacity;
    TZrUInt32 shapeCapacity;
    TZrUInt32 nullCapacity;
    SSnapshot before;
    UFact old = make_fact(kind, 1u, 0u, 1u);
    UFact latest = make_fact(kind, 1u, 1u, 1u);
    UFact fresh;
    unsigned char oldBefore[sizeof(old)];
    TZrBool ok = ZR_TRUE;
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    CHECK(add_fact(&facts, kind, &old));
    CHECK(add_fact(&facts, kind, &latest));
    CHECK(equal_fact(&facts, kind, 0u, &old) && equal_fact(&facts, kind, 1u, &latest));
    if (kind == FACT_SHAPE) {
        CHECK(ZrParser_ExecIr_AnalysisFacts_FindShape(&facts, 1u) == &facts.shapes[1]);
    } else if (kind == FACT_NULL) {
        CHECK(ZrParser_ExecIr_AnalysisFacts_FindNullability(&facts, 1u) ==
              &facts.nullability[1]);
    }
    capture(&facts, &before);
    rangeStorage = facts.ranges;
    shapeStorage = facts.shapes;
    nullStorage = facts.nullability;
    rangeCapacity = facts.rangeCapacity;
    shapeCapacity = facts.shapeCapacity;
    nullCapacity = facts.nullabilityCapacity;
    ZrParser_ExecIr_AnalysisFacts_InvalidateGeneration(&facts, 2u);
    CHECK(facts.generation == 2u && facts.rangeCount == 0u &&
          facts.shapeCount == 0u && facts.nullabilityCount == 0u);
    CHECK(facts.ranges == rangeStorage && facts.shapes == shapeStorage &&
          facts.nullability == nullStorage && facts.rangeCapacity == rangeCapacity &&
          facts.shapeCapacity == shapeCapacity && facts.nullabilityCapacity == nullCapacity);
    CHECK(ZrParser_ExecIr_AnalysisFacts_FindShape(&facts, 1u) == ZR_NULL &&
          ZrParser_ExecIr_AnalysisFacts_FindNullability(&facts, 1u) == ZR_NULL);
    /* Keep the two retained records in the snapshot after logical invalidation. */
    memcpy(before.header, &facts, sizeof(facts));
    memcpy(oldBefore, &old, sizeof(old));
    CHECK(!add_fact(&facts, kind, &old));
    CHECK(unchanged(&facts, &before));
    CHECK(memcmp(oldBefore, &old, sizeof(old)) == 0);
    fresh = make_fact(kind, 1u, 4u, 2u);
    CHECK(add_fact(&facts, kind, &fresh));
    CHECK(fact_count(&facts, kind) == 1u && equal_fact(&facts, kind, 0u, &fresh));
    if (kind == FACT_SHAPE) {
        CHECK(ZrParser_ExecIr_AnalysisFacts_FindShape(&facts, 1u) == &facts.shapes[0]);
    } else if (kind == FACT_NULL) {
        CHECK(ZrParser_ExecIr_AnalysisFacts_FindNullability(&facts, 1u) ==
              &facts.nullability[0]);
    } else {
        SZrExecIrRangeFact index;
        SZrExecIrRangeFact length;
        memset(&index, 0, sizeof(index));
        memset(&length, 0, sizeof(length));
        index.valueId = 1u;
        length.valueId = 2u;
        index.lower = 0;
        index.upper = 3;
        length.lower = length.upper = 4;
        index.hasLower = index.hasUpper = length.hasLower = length.hasUpper = ZR_TRUE;
        index.generation = length.generation = 2u;
        CHECK(ZrParser_ExecIr_RangeProvesBounds(&index, &length));
        length.generation = 1u;
        CHECK(!ZrParser_ExecIr_RangeProvesBounds(&index, &length));
    }
cleanup:
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
    return ok;
}

/* Pristine source RED has no owned allocation or backing access to clean up. */
static TZrBool test_reserve_storage(void) {
    void *items = ZR_NULL;
    TZrUInt32 capacity = 1u;
    TZrBool ok = ZR_TRUE;
    TZrBool result = reserve(&items, &capacity, 1u, sizeof(SZrExecIrRangeFact));
    NUM_CHECK(!result);
    NUM_CHECK(items == ZR_NULL && capacity == 1u);
    return ok;
}

#if !defined(ZR_FACT_CAPACITY_ORIGINAL)
static TZrBool test_alias_growth(EFactKind kind) {
    SZrExecIrAnalysisFacts facts;
    UFact expected[9];
    UFact input;
    TZrUInt32 i;
    TZrBool added;
    TZrBool ok = ZR_TRUE;
    ZrParser_ExecIr_AnalysisFactsInit(&facts);
    for (i = 0u; i < 8u; ++i) {
        input = make_fact(kind, i + 1u, i, 1u);
        memcpy(&expected[i], &input, sizeof(input));
        CHECK(add_fact(&facts, kind, &input));
    }
    CHECK(fact_count(&facts, kind) == 8u && fact_capacity(&facts, kind) == 8u);
    memcpy(&expected[8], &expected[0], sizeof(expected[0]));
    if (kind == FACT_RANGE) {
        added = ZrParser_ExecIr_AnalysisFacts_AddRange(&facts, &facts.ranges[0]);
    } else if (kind == FACT_SHAPE) {
        added = ZrParser_ExecIr_AnalysisFacts_AddShape(&facts, &facts.shapes[0]);
    } else {
        added = ZrParser_ExecIr_AnalysisFacts_AddNullability(&facts, &facts.nullability[0]);
    }
    CHECK(added && fact_count(&facts, kind) == 9u && fact_capacity(&facts, kind) >= 9u);
    for (i = 0u; i < 9u; ++i) CHECK(equal_fact(&facts, kind, i, &expected[i]));
cleanup:
    ZrParser_ExecIr_AnalysisFactsFree(&facts);
    return ok;
}
#endif
#endif

#if !defined(ZR_FACT_CAPACITY_ORIGINAL)
static TZrBool test_required_math(void) {
    TZrUInt32 required;
    TZrBool ok = ZR_TRUE;
    required = 23u;
    NUM_CHECK(fact_append_required(0u, 0u, &required) && required == 1u);
    NUM_CHECK(fact_append_required(7u, 8u, &required) && required == 8u);
    NUM_CHECK(fact_append_required(8u, 8u, &required) && required == 9u);
    NUM_CHECK(fact_append_required(UINT32_MAX - 1u, UINT32_MAX, &required) &&
              required == UINT32_MAX);
    required = 23u;
    NUM_CHECK(!fact_append_required(UINT32_MAX, UINT32_MAX, &required));
    NUM_CHECK(required == 23u);
    required = 23u;
    NUM_CHECK(!fact_append_required(9u, 8u, &required));
    NUM_CHECK(required == 23u);
#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
    NUM_CHECK(!fact_append_required(0u, 0u, ZR_NULL));
#endif
    return ok;
}

static TZrBool test_byte_math(void) {
    size_t bytes = 31u;
    TZrBool ok = ZR_TRUE;
    NUM_CHECK(fact_capacity_bytes(0u, 1u, &bytes) && bytes == 0u);
    NUM_CHECK(fact_capacity_bytes(1u, SIZE_MAX, &bytes) && bytes == SIZE_MAX);
    bytes = 31u;
    NUM_CHECK(!fact_capacity_bytes(2u, SIZE_MAX, &bytes));
    NUM_CHECK(bytes == 31u);
    NUM_CHECK(fact_capacity_bytes(2u, SIZE_MAX / 2u, &bytes) &&
              bytes == (SIZE_MAX / 2u) * 2u);
    bytes = 31u;
    NUM_CHECK(!fact_capacity_bytes(2u, SIZE_MAX / 2u + 1u, &bytes));
    NUM_CHECK(bytes == 31u);
    bytes = 31u;
    NUM_CHECK(!fact_capacity_bytes(1u, 0u, &bytes));
    NUM_CHECK(bytes == 31u);
#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
    NUM_CHECK(!fact_capacity_bytes(1u, 1u, ZR_NULL));
#endif
    return ok;
}

static TZrBool test_native_fact_sizes(void) {
    const size_t sizes[3] = {sizeof(SZrExecIrRangeFact), sizeof(SZrExecIrShapeFact),
                            sizeof(SZrExecIrNullabilityFact)};
    size_t bytes = 31u;
    size_t limit;
    unsigned int i;
    TZrBool ok = ZR_TRUE;
    for (i = 0u; i < 3u; ++i) {
        limit = SIZE_MAX / sizes[i];
        if (limit < (size_t)UINT32_MAX) {
            NUM_CHECK(fact_capacity_bytes((TZrUInt32)limit, sizes[i], &bytes));
            NUM_CHECK(bytes == limit * sizes[i]);
            bytes = 31u;
            NUM_CHECK(!fact_capacity_bytes((TZrUInt32)(limit + 1u), sizes[i], &bytes));
            NUM_CHECK(bytes == 31u);
            printf("NATIVE_BOUNDARY %s: representable UInt32 size bound; no allocation\n",
                   kind_name((EFactKind)i));
        } else {
            NUM_CHECK(fact_capacity_bytes(UINT32_MAX, sizes[i], &bytes));
            NUM_CHECK(bytes == (size_t)UINT32_MAX * sizes[i]);
            printf("NATIVE_BOUNDARY %s: UInt32 capacities fit host size_t; no 32-bit claim\n",
                   kind_name((EFactKind)i));
        }
    }
    return ok;
}

#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
static TZrBool test_checked_reserve_preflight(void) {
    unsigned char owned[8];
    void *items = owned;
    TZrUInt32 capacity = 8u;
    TZrBool ok = ZR_TRUE;
    size_t invalidElementSize = SIZE_MAX / 8u + 1u;
    /* Current capacity is unrepresentable in bytes. Checked reserve returns
     * before realloc and never dereferences/frees this local byte storage. */
    NUM_CHECK(!reserve(&items, &capacity, 1u, invalidElementSize));
    NUM_CHECK(items == owned && capacity == 8u);
    NUM_CHECK(!reserve(ZR_NULL, &capacity, 1u, 1u));
    NUM_CHECK(!reserve(&items, ZR_NULL, 1u, 1u));
    NUM_CHECK(!reserve(&items, &capacity, 1u, 0u));
    NUM_CHECK(items == owned && capacity == 8u);
    return ok;
}
#endif
#endif

static void record(const char *name, const char *kind, TZrBool ok,
                    unsigned int *cases, unsigned int *failed) {
    ++*cases;
    if (!ok) ++*failed;
    printf("CASE %s/%s %s\n", name, kind, ok ? "PASS" : "FAIL");
}

int main(void) {
    unsigned int cases = 0u;
    unsigned int failed = 0u;
#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
    unsigned int i;
#endif
    printf("FACT_LAYOUT size_t=%u range=%u shape=%u null=%u\n",
           (unsigned int)sizeof(size_t), (unsigned int)sizeof(SZrExecIrRangeFact),
           (unsigned int)sizeof(SZrExecIrShapeFact),
           (unsigned int)sizeof(SZrExecIrNullabilityFact));
#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
    for (i = 0u; i < 3u; ++i) {
        record("growth17", kind_name((EFactKind)i), test_growth((EFactKind)i), &cases, &failed);
        record("rollback", kind_name((EFactKind)i), test_rejection_rollback((EFactKind)i), &cases, &failed);
        record("generation_latest", kind_name((EFactKind)i), test_generation_latest((EFactKind)i), &cases, &failed);
    }
    record("reserve_storage", "private", test_reserve_storage(), &cases, &failed);
#endif
#if !defined(ZR_FACT_CAPACITY_ORIGINAL)
    record("required_math", "private", test_required_math(), &cases, &failed);
    record("byte_math", "private", test_byte_math(), &cases, &failed);
    record("native_sizes", "private", test_native_fact_sizes(), &cases, &failed);
#if !defined(ZR_FACT_CAPACITY_UNCHECKED)
    record("reserve_preflight", "private", test_checked_reserve_preflight(), &cases, &failed);
    for (i = 0u; i < 3u; ++i) {
        record("alias_growth", kind_name((EFactKind)i), test_alias_growth((EFactKind)i), &cases, &failed);
    }
#endif
#endif
    printf("RESULT cases=%u failures=%u\n", cases, failed);
    return failed == 0u ? EXIT_SUCCESS : EXIT_FAILURE;
}
