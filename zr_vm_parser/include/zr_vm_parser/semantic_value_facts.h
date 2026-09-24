#ifndef ZR_VM_PARSER_SEMANTIC_VALUE_FACTS_H
#define ZR_VM_PARSER_SEMANTIC_VALUE_FACTS_H

#include "zr_vm_parser/canonical_type.h"

typedef enum EZrSemanticValueOwnership {
    ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN = 0,
    ZR_SEMANTIC_VALUE_OWNERSHIP_VALUE,
    ZR_SEMANTIC_VALUE_OWNERSHIP_BORROWED,
    ZR_SEMANTIC_VALUE_OWNERSHIP_UNIQUE,
    ZR_SEMANTIC_VALUE_OWNERSHIP_SHARED,
    ZR_SEMANTIC_VALUE_OWNERSHIP_ATOMIC_SHARED,
    ZR_SEMANTIC_VALUE_OWNERSHIP_WEAK,
    ZR_SEMANTIC_VALUE_OWNERSHIP_GC,
    ZR_SEMANTIC_VALUE_OWNERSHIP_COUNT
} EZrSemanticValueOwnership;

typedef enum EZrSemanticValueNullability {
    ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN = 0,
    ZR_SEMANTIC_VALUE_NULLABILITY_NONNULL,
    ZR_SEMANTIC_VALUE_NULLABILITY_NULLABLE,
    ZR_SEMANTIC_VALUE_NULLABILITY_COUNT
} EZrSemanticValueNullability;

/* Pointer-free snapshot. A zero witness carries no asserted facts; a nonzero
 * witness must match the current SemIR value's canonical type identity. */
typedef struct SZrSemanticValueFacts {
    TZrTypeId typeId;
    EZrSemanticValueOwnership ownership;
    EZrSemanticValueNullability nullability;
} SZrSemanticValueFacts;

static ZR_FORCE_INLINE TZrBool ZrParser_SemanticValueFacts_Validate(
        const SZrSemanticValueFacts *facts, TZrTypeId typeId) {
    if (facts == ZR_NULL ||
        (TZrUInt32)facts->ownership >= ZR_SEMANTIC_VALUE_OWNERSHIP_COUNT ||
        (TZrUInt32)facts->nullability >= ZR_SEMANTIC_VALUE_NULLABILITY_COUNT) {
        return ZR_FALSE;
    }
    if (facts->typeId == 0u) {
        return (TZrBool)(facts->ownership == ZR_SEMANTIC_VALUE_OWNERSHIP_UNKNOWN &&
                         facts->nullability == ZR_SEMANTIC_VALUE_NULLABILITY_UNKNOWN);
    }
    return (TZrBool)(facts->typeId == typeId);
}

struct SZrSemanticIrFunction;

/* Atomically refresh every value from its reachable canonical type graph.
 * Failure leaves every prior snapshot untouched, including stale snapshots. */
ZR_PARSER_API TZrBool ZrParser_SemanticIr_ResolveValueFacts(
        struct SZrSemanticIrFunction *function,
        const struct SZrSemanticContext *context);

#endif /* ZR_VM_PARSER_SEMANTIC_VALUE_FACTS_H */
