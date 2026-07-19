#include "dataflow_ownership_symbols.h"

#include <string.h>

#include "zr_vm_parser/semantic_facts.h"

TZrBool ZrParser_DataflowOwnership_SymbolFind(
        const SZrSemanticOwnershipSymbolMap *map,
        TZrSymbolId symbolId,
        TZrSize *outIndex) {
    TZrSize index;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (map == ZR_NULL ||
        !map->entries.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (index = 0; index < map->entries.length; index++) {
        const SZrSemanticOwnershipSymbolEntry *candidate =
                (const SZrSemanticOwnershipSymbolEntry *)ZrCore_Array_Get(
                        (SZrArray *)&map->entries,
                        index);
        if (candidate != ZR_NULL && candidate->symbolId == symbolId) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

SZrSemanticOwnershipSymbolEntry *ZrParser_DataflowOwnership_SymbolEntry(
        SZrSemanticOwnershipSymbolMap *map,
        TZrSize index) {
    if (map == ZR_NULL || !map->entries.isValid || index >= map->entries.length) {
        return ZR_NULL;
    }
    return (SZrSemanticOwnershipSymbolEntry *)ZrCore_Array_Get(&map->entries, index);
}

static EZrOwnershipQualifier ownership_symbol_type_qualifier(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    TZrSize index;

    if (context == ZR_NULL ||
        !context->types.isValid ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    for (index = 0; index < context->types.length; index++) {
        const SZrSemanticTypeRecord *type =
                (const SZrSemanticTypeRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->types,
                        index);
        if (type != ZR_NULL && type->id == typeId) {
            return type->ownershipQualifier;
        }
    }
    return ZR_OWNERSHIP_QUALIFIER_NONE;
}

static const SZrSemanticReferenceFact *ownership_symbol_declaration_reference(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            fact->isResolved &&
            fact->symbolId == symbolId) {
            return fact;
        }
    }
    return ZR_NULL;
}

static TZrLifetimeRegionId ownership_symbol_declaration_region(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->ownershipFacts.isValid) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (index = 0; index < context->ownershipFacts.length; index++) {
        const SZrSemanticOwnershipFact *fact =
                (const SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->ownershipFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION &&
            fact->symbolId == symbolId &&
            fact->lifetimeRegionId != ZR_SEMANTIC_ID_INVALID) {
            return fact->lifetimeRegionId;
        }
    }
    return ZR_SEMANTIC_ID_INVALID;
}

static TZrBool ownership_symbol_append_declaration_fact(
        SZrSemanticContext *context,
        const SZrSemanticReferenceFact *reference,
        EZrOwnershipQualifier qualifier,
        TZrLifetimeRegionId regionId) {
    SZrSemanticOwnershipFact fact;

    if (context == ZR_NULL || reference == ZR_NULL) {
        return ZR_TRUE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = reference->node;
    fact.range = reference->range;
    fact.kind = ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION;
    fact.qualifier = qualifier;
    fact.symbolId = reference->symbolId;
    fact.lifetimeRegionId = regionId;
    fact.ownerLifetimeRegionId = regionId;
    return ZrParser_SemanticFacts_AppendOwnership(context, &fact);
}

static TZrBool ownership_symbol_map_add(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map,
        TZrSymbolId symbolId,
        EZrOwnershipQualifier qualifier) {
    SZrSemanticOwnershipSymbolEntry entry;
    const SZrSemanticReferenceFact *declaration;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        map == ZR_NULL ||
        !map->entries.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        qualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
        return ZR_FALSE;
    }
    if (!ZrParser_DataflowOwnership_SymbolFind(map, symbolId, ZR_NULL)) {
        memset(&entry, 0, sizeof(entry));
        entry.symbolId = symbolId;
        entry.qualifier = qualifier;
        entry.ownerIndex = ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
        entry.regionId = ownership_symbol_declaration_region(context, symbolId);
        if (entry.regionId == ZR_SEMANTIC_ID_INVALID) {
            entry.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(context);
            declaration = ownership_symbol_declaration_reference(context, symbolId);
            if (!ownership_symbol_append_declaration_fact(context,
                                                          declaration,
                                                          qualifier,
                                                          entry.regionId)) {
                return ZR_FALSE;
            }
        }
        ZrCore_Array_Push(context->state, &map->entries, &entry);
    }
    return ZR_TRUE;
}

void ZrParser_DataflowOwnership_SymbolMapConstruct(SZrSemanticOwnershipSymbolMap *map) {
    if (map != ZR_NULL) {
        ZrCore_Array_Construct(&map->entries);
    }
}

TZrBool ZrParser_DataflowOwnership_SymbolMapBuild(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map) {
    TZrSize capacity;
    TZrSize index;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        map == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    capacity = context->referenceFacts.length > 0
                       ? context->referenceFacts.length
                       : ZR_PARSER_INITIAL_CAPACITY_TINY;
    ZrCore_Array_Init(context->state,
                      &map->entries,
                      sizeof(SZrSemanticOwnershipSymbolEntry),
                      capacity);

    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &context->referenceFacts,
                        index);
        EZrOwnershipQualifier qualifier;

        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        qualifier = ownership_symbol_type_qualifier(context, fact->typeId);
        if (qualifier == ZR_OWNERSHIP_QUALIFIER_NONE) {
            continue;
        }
        if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
            fact->kind == ZR_SEMANTIC_REFERENCE_READ ||
            fact->kind == ZR_SEMANTIC_REFERENCE_WRITE) {
            if (!ownership_symbol_map_add(context,
                                          map,
                                          fact->symbolId,
                                          qualifier)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

void ZrParser_DataflowOwnership_SymbolMapFree(
        SZrSemanticContext *context,
        SZrSemanticOwnershipSymbolMap *map) {
    if (context != ZR_NULL && context->state != ZR_NULL && map != ZR_NULL) {
        ZrCore_Array_Free(context->state, &map->entries);
    }
}
