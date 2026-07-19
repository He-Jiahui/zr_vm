#include "dataflow_ownership_observations.h"

#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_parser/semantic_facts.h"

static TZrBool ownership_observation_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool ownership_observation_fact_exists(
        const SZrSemanticContext *context,
        SZrAstNode *node,
        EZrSemanticOwnershipFactKind kind) {
    TZrSize index;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->ownershipFacts.length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        index);
        if (fact != ZR_NULL && fact->node == node && fact->kind == kind) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool ownership_observation_same_range(const SZrFileRange *left,
                                                 const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left->source != ZR_NULL &&
        right->source != ZR_NULL &&
        left->source != right->source &&
        !ZrCore_String_Equal(left->source, right->source)) {
        return ZR_FALSE;
    }
    if (ownership_observation_has_offset(&left->start) ||
        ownership_observation_has_offset(&left->end) ||
        ownership_observation_has_offset(&right->start) ||
        ownership_observation_has_offset(&right->end)) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }
    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static SZrSemanticOwnershipFact *ownership_observation_find_fact(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        EZrSemanticOwnershipFactKind kind) {
    TZrSize index;

    if (context == ZR_NULL || reference == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->ownershipFacts.length; index++) {
        SZrSemanticOwnershipFact *fact =
                (SZrSemanticOwnershipFact *)ZrCore_Array_Get(&context->ownershipFacts, index);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            (fact->node == reference->node ||
             ownership_observation_same_range(&fact->range, &reference->range))) {
            return fact;
        }
    }
    return ZR_NULL;
}

static const SZrSemanticOwnershipSymbolEntry *ownership_observation_owner_entry(
        SZrSemanticOwnershipSymbolMap *symbols,
        const SZrSemanticOwnershipSymbolEntry *entry,
        TZrSize ownerIndex) {
    const SZrSemanticOwnershipSymbolEntry *ownerEntry = entry;

    if (ownerIndex != ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID &&
        ownerIndex < symbols->entries.length) {
        const SZrSemanticOwnershipSymbolEntry *candidate =
                ZrParser_DataflowOwnership_SymbolEntry(symbols, ownerIndex);
        if (candidate != ZR_NULL) {
            ownerEntry = candidate;
        }
    }
    return ownerEntry;
}

static TZrBool ownership_observation_append_transition(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        const SZrSemanticOwnershipSymbolEntry *entry,
        const SZrSemanticOwnershipSymbolEntry *ownerEntry,
        EZrSemanticOwnershipFactKind kind) {
    SZrSemanticOwnershipFact fact;

    if (ownership_observation_fact_exists(context, reference->node, kind)) {
        return ZR_TRUE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = reference->node;
    fact.range = reference->range;
    fact.kind = kind;
    fact.qualifier = entry->qualifier;
    fact.symbolId = reference->symbolId;
    fact.lifetimeRegionId = entry->regionId;
    fact.ownerLifetimeRegionId = ownerEntry->regionId;
    return ZrParser_SemanticFacts_AppendOwnership(context, &fact);
}

TZrBool ZrParser_DataflowOwnership_AppendObservedFacts(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *symbols,
        const SZrDataflowOwnershipObservations *observations) {
    TZrSize index;

    if (context == ZR_NULL ||
        symbols == ZR_NULL ||
        observations == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < observations->count; index++) {
        const SZrSemanticReferenceFact *reference =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts,
                        index);
        const SZrSemanticOwnershipSymbolEntry *entry;
        const SZrSemanticOwnershipSymbolEntry *ownerEntry;
        const SZrSemanticOwnershipSymbolEntry *violationOwnerEntry;
        SZrSemanticOwnershipFact fact;
        SZrSemanticOwnershipFact *existingError;
        TZrSize symbolIndex;

        if (reference == ZR_NULL ||
            !ZrParser_DataflowOwnership_SymbolFind(symbols,
                                                   reference->symbolId,
                                                   &symbolIndex)) {
            continue;
        }
        entry = ZrParser_DataflowOwnership_SymbolEntry(symbols, symbolIndex);
        if (entry == ZR_NULL) {
            continue;
        }
        ownerEntry = ownership_observation_owner_entry(symbols, entry, entry->ownerIndex);
        violationOwnerEntry = ownership_observation_owner_entry(
                symbols,
                ownerEntry,
                observations->violationOwnerIndices[index]);

        if (observations->moveSeen[index] &&
            !ownership_observation_append_transition(context,
                                                     reference,
                                                     entry,
                                                     ownerEntry,
                                                     ZR_SEMANTIC_OWNERSHIP_FACT_MOVE)) {
            return ZR_FALSE;
        }
        if (observations->releaseSeen[index] &&
            !ownership_observation_append_transition(context,
                                                     reference,
                                                     entry,
                                                     ownerEntry,
                                                     ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE)) {
            return ZR_FALSE;
        }

        existingError = entry->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK
                                ? ownership_observation_find_fact(
                                          context,
                                          reference,
                                          ZR_SEMANTIC_OWNERSHIP_FACT_ERROR)
                                : ZR_NULL;
        if (observations->violationSeen[index] &&
            observations->violationCauses[index] != ZR_NULL &&
            existingError != ZR_NULL) {
            existingError->qualifier = entry->qualifier;
            existingError->symbolId = reference->symbolId;
            existingError->lifetimeRegionId = entry->regionId;
            existingError->ownerLifetimeRegionId = violationOwnerEntry->regionId;
            existingError->relatedNode = observations->violationCauses[index];
            existingError->isViolation = ZR_TRUE;
        } else if (observations->violationSeen[index] &&
                   observations->violationCauses[index] != ZR_NULL &&
                   !ownership_observation_fact_exists(
                           context,
                           reference->node,
                           ZR_SEMANTIC_OWNERSHIP_FACT_ERROR)) {
            memset(&fact, 0, sizeof(fact));
            fact.node = reference->node;
            fact.range = reference->range;
            fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_ERROR;
            fact.qualifier = entry->qualifier;
            fact.symbolId = reference->symbolId;
            fact.lifetimeRegionId = entry->regionId;
            fact.ownerLifetimeRegionId = violationOwnerEntry->regionId;
            fact.relatedNode = observations->violationCauses[index];
            fact.isViolation = ZR_TRUE;
            if (!ZrParser_SemanticFacts_AppendOwnership(context, &fact)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DataflowOwnership_ObservationsAllocate(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations) {
    TZrSize boolBytes;
    TZrSize pointerBytes;
    TZrSize ownerIndexBytes;
    TZrSize index;

    if (context == ZR_NULL || context->state == ZR_NULL || observations == ZR_NULL) {
        return ZR_FALSE;
    }
    observations->count = context->referenceFacts.length;
    if (observations->count == 0) {
        return ZR_TRUE;
    }
    boolBytes = observations->count * sizeof(TZrBool);
    pointerBytes = observations->count * sizeof(SZrAstNode *);
    ownerIndexBytes = observations->count * sizeof(TZrSize);
    observations->moveSeen = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            context->state->global, boolBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->releaseSeen = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            context->state->global, boolBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->violationSeen = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            context->state->global, boolBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->violationCauses = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(
            context->state->global, pointerBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    observations->violationOwnerIndices = (TZrSize *)ZrCore_Memory_RawMallocWithType(
            context->state->global, ownerIndexBytes, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (observations->moveSeen == ZR_NULL ||
        observations->releaseSeen == ZR_NULL ||
        observations->violationSeen == ZR_NULL ||
        observations->violationCauses == ZR_NULL ||
        observations->violationOwnerIndices == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(observations->moveSeen, 0, boolBytes);
    ZrCore_Memory_RawSet(observations->releaseSeen, 0, boolBytes);
    ZrCore_Memory_RawSet(observations->violationSeen, 0, boolBytes);
    ZrCore_Memory_RawSet(observations->violationCauses, 0, pointerBytes);
    for (index = 0; index < observations->count; index++) {
        observations->violationOwnerIndices[index] =
                ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    }
    return ZR_TRUE;
}

void ZrParser_DataflowOwnership_ObservationsFree(
        SZrSemanticContext *context,
        SZrDataflowOwnershipObservations *observations) {
    TZrSize boolBytes;
    TZrSize pointerBytes;
    TZrSize ownerIndexBytes;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        observations == ZR_NULL ||
        observations->count == 0) {
        return;
    }
    boolBytes = observations->count * sizeof(TZrBool);
    pointerBytes = observations->count * sizeof(SZrAstNode *);
    ownerIndexBytes = observations->count * sizeof(TZrSize);
#define FREE_OBSERVATION(pointer, bytes) \
    do { \
        if ((pointer) != ZR_NULL) { \
            ZrCore_Memory_RawFreeWithType(context->state->global, \
                                         (pointer), \
                                         (bytes), \
                                         ZR_MEMORY_NATIVE_TYPE_ARRAY); \
        } \
    } while (0)
    FREE_OBSERVATION(observations->moveSeen, boolBytes);
    FREE_OBSERVATION(observations->releaseSeen, boolBytes);
    FREE_OBSERVATION(observations->violationSeen, boolBytes);
    FREE_OBSERVATION(observations->violationCauses, pointerBytes);
    FREE_OBSERVATION(observations->violationOwnerIndices, ownerIndexBytes);
#undef FREE_OBSERVATION
    memset(observations, 0, sizeof(*observations));
}
