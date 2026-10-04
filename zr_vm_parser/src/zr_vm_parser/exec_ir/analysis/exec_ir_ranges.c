#include "zr_vm_parser/exec_ir_ranges.h"

#include <stdlib.h>
#include <string.h>

static TZrBool fact_append_required(TZrUInt32 count, TZrUInt32 capacity,
                                   TZrUInt32 *required) {
    if (required == ZR_NULL || count > capacity || count == UINT32_MAX) {
        return ZR_FALSE;
    }
    *required = count + 1u;
    return ZR_TRUE;
}

static TZrBool fact_capacity_bytes(TZrUInt32 capacity, size_t elementSize,
                                  size_t *bytes) {
    if (bytes == ZR_NULL || elementSize == 0u ||
        (size_t)capacity > SIZE_MAX / elementSize) {
        return ZR_FALSE;
    }
    *bytes = (size_t)capacity * elementSize;
    return ZR_TRUE;
}

static TZrBool reserve(void **items, TZrUInt32 *capacity, TZrUInt32 count,
                       size_t elementSize) {
    TZrUInt32 next;
    size_t bytes;
    void *memory;
    if (items == ZR_NULL || capacity == ZR_NULL ||
        (*capacity != 0u && *items == ZR_NULL) ||
        !fact_capacity_bytes(*capacity, elementSize, &bytes)) {
        return ZR_FALSE;
    }
    if (count <= *capacity) {
        return ZR_TRUE;
    }
    next = *capacity == 0u ? 8u : *capacity;
    while (next < count) {
        if (next > UINT32_MAX / 2u) {
            next = count;
            break;
        }
        next *= 2u;
    }
    if (!fact_capacity_bytes(next, elementSize, &bytes)) {
        return ZR_FALSE;
    }
    memory = realloc(*items, bytes);
    if (memory == ZR_NULL) {
        return ZR_FALSE;
    }
    *items = memory;
    *capacity = next;
    return ZR_TRUE;
}

void ZrParser_ExecIr_AnalysisFactsInit(SZrExecIrAnalysisFacts *facts) {
    if (facts != ZR_NULL) {
        memset(facts, 0, sizeof(*facts));
        facts->generation = 1u;
    }
}

void ZrParser_ExecIr_AnalysisFactsFree(SZrExecIrAnalysisFacts *facts) {
    if (facts != ZR_NULL) {
        free(facts->ranges);
        free(facts->shapes);
        free(facts->nullability);
        memset(facts, 0, sizeof(*facts));
    }
}

void ZrParser_ExecIr_AnalysisFacts_InvalidateGeneration(
        SZrExecIrAnalysisFacts *facts, TZrUInt64 generation) {
    if (facts == ZR_NULL) {
        return;
    }
    facts->generation = generation == 0u ? facts->generation + 1u : generation;
    facts->rangeCount = 0u;
    facts->shapeCount = 0u;
    facts->nullabilityCount = 0u;
}

TZrBool ZrParser_ExecIr_AnalysisFacts_AddRange(
        SZrExecIrAnalysisFacts *facts, const SZrExecIrRangeFact *range) {
    if (facts == ZR_NULL || range == ZR_NULL || range->valueId == 0u ||
        range->generation == 0u || range->generation != facts->generation ||
        (range->hasLower && range->hasUpper && range->lower > range->upper)) {
        return ZR_FALSE;
    }
    SZrExecIrRangeFact value = *range;
    TZrUInt32 required;
    if (!fact_append_required(facts->rangeCount, facts->rangeCapacity, &required) ||
        !reserve((void **)&facts->ranges, &facts->rangeCapacity,
                 required, sizeof(*facts->ranges))) {
        return ZR_FALSE;
    }
    facts->ranges[facts->rangeCount] = value;
    facts->rangeCount = required;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_AnalysisFacts_AddShape(
        SZrExecIrAnalysisFacts *facts, const SZrExecIrShapeFact *shape) {
    if (facts == ZR_NULL || shape == ZR_NULL || shape->valueId == 0u ||
        shape->typeToken == 0u || shape->layoutId == 0u || shape->shapeId == 0u ||
        shape->generation == 0u || shape->generation != facts->generation) {
        return ZR_FALSE;
    }
    SZrExecIrShapeFact value = *shape;
    TZrUInt32 required;
    if (!fact_append_required(facts->shapeCount, facts->shapeCapacity, &required) ||
        !reserve((void **)&facts->shapes, &facts->shapeCapacity,
                 required, sizeof(*facts->shapes))) {
        return ZR_FALSE;
    }
    facts->shapes[facts->shapeCount] = value;
    facts->shapeCount = required;
    return ZR_TRUE;
}

TZrBool ZrParser_ExecIr_AnalysisFacts_AddNullability(
        SZrExecIrAnalysisFacts *facts,
        const SZrExecIrNullabilityFact *nullability) {
    if (facts == ZR_NULL || nullability == ZR_NULL || nullability->valueId == 0u ||
        nullability->state > ZR_EXEC_IR_NULL_FACT_NULL ||
        nullability->generation == 0u ||
        nullability->generation != facts->generation) {
        return ZR_FALSE;
    }
    SZrExecIrNullabilityFact value = *nullability;
    TZrUInt32 required;
    if (!fact_append_required(facts->nullabilityCount, facts->nullabilityCapacity, &required) ||
        !reserve((void **)&facts->nullability, &facts->nullabilityCapacity,
                 required, sizeof(*facts->nullability))) {
        return ZR_FALSE;
    }
    facts->nullability[facts->nullabilityCount] = value;
    facts->nullabilityCount = required;
    return ZR_TRUE;
}

const SZrExecIrNullabilityFact *ZrParser_ExecIr_AnalysisFacts_FindNullability(
        const SZrExecIrAnalysisFacts *facts, TZrExecIrValueId valueId) {
    TZrUInt32 index;
    if (facts == ZR_NULL || valueId == 0u) {
        return ZR_NULL;
    }
    for (index = facts->nullabilityCount; index != 0u; --index) {
        const SZrExecIrNullabilityFact *fact = &facts->nullability[index - 1u];
        if (fact->valueId == valueId && fact->generation == facts->generation) {
            return fact;
        }
    }
    return ZR_NULL;
}

const SZrExecIrShapeFact *ZrParser_ExecIr_AnalysisFacts_FindShape(
        const SZrExecIrAnalysisFacts *facts, TZrExecIrValueId valueId) {
    TZrUInt32 index;
    if (facts == ZR_NULL || valueId == 0u) {
        return ZR_NULL;
    }
    for (index = facts->shapeCount; index != 0u; --index) {
        const SZrExecIrShapeFact *shape = &facts->shapes[index - 1u];
        if (shape->valueId == valueId && shape->generation == facts->generation) {
            return shape;
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_ExecIr_RangeProvesBounds(
        const SZrExecIrRangeFact *index, const SZrExecIrRangeFact *length) {
    if (index == ZR_NULL || length == ZR_NULL || !index->hasLower ||
        !index->hasUpper || !length->hasLower || !length->hasUpper ||
        index->overflowed || length->overflowed || index->lengthMutable ||
        length->lengthMutable || index->lower > index->upper ||
        length->lower > length->upper ||
        index->generation == 0u || index->generation != length->generation ||
        index->lower < 0 || length->lower < 0) {
        return ZR_FALSE;
    }
    return (TZrBool)(index->upper < length->lower);
}
