#include "dataflow.h"
#include "dataflow_ownership_moves.h"
#include "dataflow_ownership_observations.h"
#include "dataflow_ownership_owner_sets.h"
#include "dataflow_ownership_regions.h"
#include "dataflow_ownership_statements.h"
#include "dataflow_ownership_symbols.h"

#include <string.h>

#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

enum EZrSemanticOwnershipFlowState {
    ZR_SEMANTIC_OWNERSHIP_FLOW_OWNED = 1 << 0,
    ZR_SEMANTIC_OWNERSHIP_FLOW_MOVED = 1 << 1,
    ZR_SEMANTIC_OWNERSHIP_FLOW_BORROWED = 1 << 2,
    ZR_SEMANTIC_OWNERSHIP_FLOW_RELEASED = 1 << 3,
};

typedef struct SZrSemanticOwnershipFlowSlot {
    TZrUInt8 states;
    TZrSize ownerSetId;
    SZrAstNode *moveNode;
    SZrAstNode *releaseNode;
} SZrSemanticOwnershipFlowSlot;

typedef struct SZrSemanticOwnershipAnalysis {
    SZrSemanticContext *context;
    SZrSemanticOwnershipSymbolMap *symbols;
    SZrDataflowOwnershipObservations observations;
    SZrDataflowOwnershipOwnerSetPool ownerSets;
} SZrSemanticOwnershipAnalysis;

static TZrBool semantic_ownership_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_ownership_using_releases(
        SZrAstNode *statement,
        const SZrSemanticOwnershipSymbolEntry *entry) {
    if (statement == ZR_NULL || statement->type != ZR_AST_USING_STATEMENT) {
        return ZR_TRUE;
    }
    return entry != ZR_NULL &&
           (entry->qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
            entry->qualifier == ZR_OWNERSHIP_QUALIFIER_SHARED ||
            entry->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
            entry->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED);
}

static SZrAstNode *semantic_ownership_earlier_node(SZrAstNode *left, SZrAstNode *right) {
    if (left == ZR_NULL) {
        return right;
    }
    if (right == ZR_NULL) {
        return left;
    }
    if ((semantic_ownership_has_offset(&left->location.start) ||
         semantic_ownership_has_offset(&right->location.start)) &&
        left->location.start.offset != right->location.start.offset) {
        return left->location.start.offset < right->location.start.offset ? left : right;
    }
    if (left->location.start.line != right->location.start.line) {
        return left->location.start.line < right->location.start.line ? left : right;
    }
    return left->location.start.column <= right->location.start.column ? left : right;
}

static TZrSize semantic_ownership_entry_owner_set(
        SZrSemanticOwnershipAnalysis *analysis,
        const SZrSemanticOwnershipSymbolEntry *entry) {
    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        entry == ZR_NULL) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    if (entry->ownerIndex != ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID &&
        analysis->symbols != ZR_NULL &&
        entry->ownerIndex < analysis->symbols->entries.length) {
        return ZrParser_DataflowOwnership_OwnerSetSingleton(
                analysis->context->state,
                &analysis->ownerSets,
                entry->ownerIndex);
    }
    if (entry->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
        entry->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED ||
        entry->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
        return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
    }
    return ZR_DATAFLOW_OWNERSHIP_OWNER_SET_EMPTY;
}

static void semantic_ownership_init_entry(void *state, void *userData) {
    SZrSemanticOwnershipAnalysis *analysis = (SZrSemanticOwnershipAnalysis *)userData;
    SZrSemanticOwnershipFlowSlot *slots = (SZrSemanticOwnershipFlowSlot *)state;
    TZrSize index;

    if (slots == ZR_NULL || analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return;
    }
    for (index = 0; index < analysis->symbols->entries.length; index++) {
        const SZrSemanticOwnershipSymbolEntry *entry =
                ZrParser_DataflowOwnership_SymbolEntry(analysis->symbols, index);
        slots[index].states = entry != ZR_NULL &&
                                      (entry->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
                                       entry->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED)
                                       ? ZR_SEMANTIC_OWNERSHIP_FLOW_BORROWED
                                       : ZR_SEMANTIC_OWNERSHIP_FLOW_OWNED;
        slots[index].ownerSetId = semantic_ownership_entry_owner_set(
                analysis,
                entry);
        slots[index].moveNode = ZR_NULL;
        slots[index].releaseNode = ZR_NULL;
    }
}

static TZrBool semantic_ownership_join(void *dst, const void *src, void *userData) {
    SZrSemanticOwnershipAnalysis *analysis = (SZrSemanticOwnershipAnalysis *)userData;
    SZrSemanticOwnershipFlowSlot *dstSlots = (SZrSemanticOwnershipFlowSlot *)dst;
    const SZrSemanticOwnershipFlowSlot *srcSlots =
            (const SZrSemanticOwnershipFlowSlot *)src;
    TZrBool changed = ZR_FALSE;
    TZrSize index;

    if (dstSlots == ZR_NULL ||
        srcSlots == ZR_NULL ||
        analysis == ZR_NULL ||
        analysis->symbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < analysis->symbols->entries.length; index++) {
        TZrUInt8 joinedStates = (TZrUInt8)(dstSlots[index].states | srcSlots[index].states);
        SZrAstNode *joinedMove = semantic_ownership_earlier_node(dstSlots[index].moveNode,
                                                                 srcSlots[index].moveNode);
        SZrAstNode *joinedRelease = semantic_ownership_earlier_node(
                dstSlots[index].releaseNode,
                srcSlots[index].releaseNode);
        TZrSize joinedOwnerSet = ZrParser_DataflowOwnership_OwnerSetUnion(
                analysis->context->state,
                &analysis->ownerSets,
                dstSlots[index].ownerSetId,
                srcSlots[index].ownerSetId);
        if (dstSlots[index].states != joinedStates) {
            dstSlots[index].states = joinedStates;
            changed = ZR_TRUE;
        }
        if (dstSlots[index].moveNode != joinedMove) {
            dstSlots[index].moveNode = joinedMove;
            changed = ZR_TRUE;
        }
        if (dstSlots[index].releaseNode != joinedRelease) {
            dstSlots[index].releaseNode = joinedRelease;
            changed = ZR_TRUE;
        }
        if (dstSlots[index].ownerSetId != joinedOwnerSet) {
            dstSlots[index].ownerSetId = joinedOwnerSet;
            changed = ZR_TRUE;
        }
    }
    return changed;
}

static void semantic_ownership_record_violation(SZrSemanticOwnershipAnalysis *analysis,
                                                 TZrSize factIndex,
                                                 SZrAstNode *cause,
                                                 TZrSize ownerIndex) {
    SZrAstNode *earlierCause;

    if (analysis == ZR_NULL || factIndex >= analysis->observations.count) {
        return;
    }
    earlierCause = semantic_ownership_earlier_node(
            analysis->observations.violationCauses[factIndex],
            cause);
    analysis->observations.violationSeen[factIndex] = ZR_TRUE;
    if (analysis->observations.violationCauses[factIndex] == ZR_NULL ||
        earlierCause != analysis->observations.violationCauses[factIndex]) {
        analysis->observations.violationOwnerIndices[factIndex] = ownerIndex;
    }
    analysis->observations.violationCauses[factIndex] = earlierCause;
}

static SZrAstNode *semantic_ownership_owner_set_release(
        const SZrSemanticOwnershipAnalysis *analysis,
        const SZrSemanticOwnershipFlowSlot *slots,
        TZrSize ownerSetId,
        TZrSize *outOwnerIndex) {
    SZrAstNode *releaseCause = ZR_NULL;
    TZrSize ownerCount;
    TZrSize index;

    if (outOwnerIndex != ZR_NULL) {
        *outOwnerIndex = ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    }
    if (analysis == ZR_NULL ||
        analysis->symbols == ZR_NULL ||
        slots == ZR_NULL ||
        ZrParser_DataflowOwnership_OwnerSetIsUnknown(
                &analysis->ownerSets,
                ownerSetId)) {
        return ZR_NULL;
    }

    ownerCount = ZrParser_DataflowOwnership_OwnerSetCount(
            &analysis->ownerSets,
            ownerSetId);
    for (index = 0; index < ownerCount; index++) {
        TZrSize ownerIndex = ZrParser_DataflowOwnership_OwnerSetAt(
                &analysis->ownerSets,
                ownerSetId,
                index);
        SZrAstNode *joinedCause;

        if (ownerIndex >= analysis->symbols->entries.length ||
            (slots[ownerIndex].states & ZR_SEMANTIC_OWNERSHIP_FLOW_RELEASED) == 0 ||
            slots[ownerIndex].releaseNode == ZR_NULL) {
            continue;
        }
        joinedCause = semantic_ownership_earlier_node(
                releaseCause,
                slots[ownerIndex].releaseNode);
        if (releaseCause == ZR_NULL || joinedCause != releaseCause) {
            if (outOwnerIndex != ZR_NULL) {
                *outOwnerIndex = ownerIndex;
            }
        }
        releaseCause = joinedCause;
    }
    return releaseCause;
}

static TZrBool semantic_ownership_read_seen_before(
        const SZrSemanticOwnershipAnalysis *analysis,
        TZrSize factIndex,
        const SZrSemanticReferenceFact *fact) {
    TZrSize index;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        fact == ZR_NULL ||
        !analysis->context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < factIndex; index++) {
        const SZrSemanticReferenceFact *candidate =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &analysis->context->referenceFacts,
                        index);
        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_REFERENCE_READ &&
            candidate->node == fact->node &&
            candidate->symbolId == fact->symbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void semantic_ownership_transfer_statement(SZrAstNode *statement,
                                                   void *state,
                                                   void *userData) {
    SZrSemanticOwnershipAnalysis *analysis = (SZrSemanticOwnershipAnalysis *)userData;
    SZrSemanticOwnershipFlowSlot *slots = (SZrSemanticOwnershipFlowSlot *)state;
    SZrDataflowOwnershipRegionBinding statementBinding;
    TZrSize bindingAliasIndex = ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    TZrSize bindingOwnerIndex = ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;
    TZrBool hasStatementBinding;
    TZrSize index;

    if (statement == ZR_NULL ||
        slots == ZR_NULL ||
        analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid) {
        return;
    }

    hasStatementBinding = ZrParser_DataflowOwnership_StatementRegionBinding(
                                  analysis->context,
                                  statement,
                                  &statementBinding) &&
                          ZrParser_DataflowOwnership_SymbolFind(
                                  analysis->symbols,
                                  statementBinding.aliasReference->symbolId,
                                  &bindingAliasIndex) &&
                          ZrParser_DataflowOwnership_SymbolFind(
                                  analysis->symbols,
                                  statementBinding.ownerReference->symbolId,
                                  &bindingOwnerIndex);

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &analysis->context->referenceFacts,
                        index);
        TZrSize symbolIndex;
        SZrSemanticOwnershipFlowSlot *slot;
        const SZrSemanticOwnershipSymbolEntry *entry;
        TZrBool moves;
        TZrBool releases;
        TZrBool weakRequiresUpgrade;
        TZrBool belongsToStatement;
        SZrAstNode *releaseCause = ZR_NULL;
        TZrSize violationOwnerIndex = ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID;

        if (fact == ZR_NULL ||
            fact->kind != ZR_SEMANTIC_REFERENCE_READ ||
            !fact->isResolved ||
            !ZrParser_DataflowOwnership_SymbolFind(
                    analysis->symbols,
                    fact->symbolId,
                    &symbolIndex) ||
            semantic_ownership_read_seen_before(analysis, index, fact)) {
            continue;
        }

        moves = ZrParser_DataflowOwnership_StatementMovesRead(
                analysis->context,
                statement,
                fact);
        releases = ZrParser_DataflowOwnership_StatementReleasesRead(statement, fact);
        weakRequiresUpgrade = ZrParser_DataflowOwnership_StatementWeakReadRequiresUpgrade(
                analysis->context,
                statement,
                fact);
        belongsToStatement = ZrParser_DataflowOwnership_FactInStatement(statement, fact) ||
                              moves ||
                              releases ||
                              weakRequiresUpgrade;
        if (!belongsToStatement) {
            continue;
        }

        slot = &slots[symbolIndex];
        entry = ZrParser_DataflowOwnership_SymbolEntry(analysis->symbols, symbolIndex);
        if (entry == ZR_NULL) {
            continue;
        }
        if (releases && !semantic_ownership_using_releases(statement, entry)) {
            releases = ZR_FALSE;
        }
        if ((entry->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
             entry->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED)) {
            TZrSize ownerReleaseIndex;
            SZrAstNode *ownerReleaseCause;

            violationOwnerIndex = ZrParser_DataflowOwnership_OwnerSetAt(
                    &analysis->ownerSets,
                    slot->ownerSetId,
                    0);
            if ((slot->states & ZR_SEMANTIC_OWNERSHIP_FLOW_RELEASED) != 0) {
                releaseCause = slot->releaseNode;
            }
            ownerReleaseCause = semantic_ownership_owner_set_release(
                    analysis,
                    slots,
                    slot->ownerSetId,
                    &ownerReleaseIndex);
            if (ownerReleaseCause != ZR_NULL) {
                SZrAstNode *joinedCause = semantic_ownership_earlier_node(
                        releaseCause,
                        ownerReleaseCause);
                if (releaseCause == ZR_NULL || joinedCause != releaseCause) {
                    violationOwnerIndex = ownerReleaseIndex;
                }
                releaseCause = joinedCause;
            }
            if (releaseCause != ZR_NULL) {
                semantic_ownership_record_violation(
                        analysis,
                        index,
                        releaseCause,
                        violationOwnerIndex);
            }
        } else if (entry->qualifier == ZR_OWNERSHIP_QUALIFIER_WEAK) {
            releaseCause = semantic_ownership_owner_set_release(
                    analysis,
                    slots,
                    slot->ownerSetId,
                    &violationOwnerIndex);
            if (releaseCause != ZR_NULL && weakRequiresUpgrade) {
                semantic_ownership_record_violation(analysis,
                                                    index,
                                                    releaseCause,
                                                    violationOwnerIndex);
            }
        } else if (entry->qualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE &&
                   (slot->states & ZR_SEMANTIC_OWNERSHIP_FLOW_MOVED) != 0) {
            semantic_ownership_record_violation(
                    analysis,
                    index,
                    slot->moveNode,
                    ZR_SEMANTIC_OWNERSHIP_SYMBOL_INDEX_INVALID);
        }

        if (releases) {
            analysis->observations.releaseSeen[index] = ZR_TRUE;
            slot->states = ZR_SEMANTIC_OWNERSHIP_FLOW_RELEASED;
            slot->releaseNode = semantic_ownership_earlier_node(slot->releaseNode, fact->node);
            continue;
        }
        if (!moves || entry->qualifier != ZR_OWNERSHIP_QUALIFIER_UNIQUE) {
            continue;
        }
        if ((slot->states & ZR_SEMANTIC_OWNERSHIP_FLOW_OWNED) != 0) {
            analysis->observations.moveSeen[index] = ZR_TRUE;
            slot->moveNode = semantic_ownership_earlier_node(slot->moveNode, fact->node);
        }
        slot->states = ZR_SEMANTIC_OWNERSHIP_FLOW_MOVED;
    }

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        &analysis->context->referenceFacts,
                        index);
        TZrSize symbolIndex;
        const SZrSemanticOwnershipSymbolEntry *entry;

        if (fact == ZR_NULL ||
            (fact->kind != ZR_SEMANTIC_REFERENCE_WRITE &&
             fact->kind != ZR_SEMANTIC_REFERENCE_DECLARATION) ||
            !ZrParser_DataflowOwnership_SymbolFind(
                    analysis->symbols,
                    fact->symbolId,
                    &symbolIndex) ||
            !ZrParser_DataflowOwnership_FactInStatement(statement, fact)) {
            continue;
        }
        entry = ZrParser_DataflowOwnership_SymbolEntry(analysis->symbols, symbolIndex);
        slots[symbolIndex].states = entry != ZR_NULL &&
                                            (entry->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED ||
                                             entry->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED)
                                            ? ZR_SEMANTIC_OWNERSHIP_FLOW_BORROWED
                                            : ZR_SEMANTIC_OWNERSHIP_FLOW_OWNED;
        if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION) {
            slots[symbolIndex].ownerSetId = semantic_ownership_entry_owner_set(
                    analysis,
                    entry);
        } else if (hasStatementBinding && symbolIndex == bindingAliasIndex) {
            slots[symbolIndex].ownerSetId =
                    ZrParser_DataflowOwnership_OwnerSetSingleton(
                            analysis->context->state,
                            &analysis->ownerSets,
                            bindingOwnerIndex);
        } else {
            slots[symbolIndex].ownerSetId =
                    ZR_DATAFLOW_OWNERSHIP_OWNER_SET_UNKNOWN;
        }
        slots[symbolIndex].moveNode = ZR_NULL;
        slots[symbolIndex].releaseNode = ZR_NULL;
    }
}

static void semantic_ownership_update_region_facts(
        SZrSemanticOwnershipAnalysis *analysis,
        const SZrDataflowOwnershipRegionBinding *binding,
        const SZrSemanticOwnershipSymbolEntry *aliasEntry,
        const SZrSemanticOwnershipSymbolEntry *ownerEntry) {
    TZrSize index;
    EZrSemanticOwnershipFactKind constructKind;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        binding == ZR_NULL ||
        aliasEntry == ZR_NULL ||
        ownerEntry == ZR_NULL ||
        !analysis->context->ownershipFacts.isValid) {
        return;
    }
    if (binding->qualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) {
        constructKind = ZR_SEMANTIC_OWNERSHIP_FACT_BORROW;
    } else if (binding->qualifier == ZR_OWNERSHIP_QUALIFIER_LOANED) {
        constructKind = ZR_SEMANTIC_OWNERSHIP_FACT_MOVE;
    } else {
        constructKind = ZR_SEMANTIC_OWNERSHIP_FACT_COPY;
    }

    for (index = 0; index < analysis->context->ownershipFacts.length; index++) {
        SZrSemanticOwnershipFact *fact =
                (SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        &analysis->context->ownershipFacts,
                        index);
        if (fact == ZR_NULL) {
            continue;
        }
        if (fact->node == binding->constructNode && fact->kind == constructKind) {
            fact->qualifier = binding->qualifier;
            fact->symbolId = aliasEntry->symbolId;
            fact->lifetimeRegionId = aliasEntry->regionId;
            fact->ownerLifetimeRegionId = ownerEntry->regionId;
            fact->relatedNode = binding->ownerReference->node;
        } else if (binding->isDeclaration &&
                   fact->kind == ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION &&
                   fact->symbolId == aliasEntry->symbolId) {
            fact->ownerLifetimeRegionId = ownerEntry->regionId;
        }
    }
}

static TZrBool semantic_ownership_bind_cfg_regions(
        SZrSemanticOwnershipAnalysis *analysis,
        const SZrParserCfg *cfg) {
    TZrSize blockIndex;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        analysis->symbols == ZR_NULL ||
        cfg == ZR_NULL ||
        !cfg->blocks.isValid) {
        return ZR_FALSE;
    }
    for (blockIndex = 0; blockIndex < cfg->blocks.length; blockIndex++) {
        const SZrParserCfgBlock *block =
                (const SZrParserCfgBlock *)ZrCore_Array_Get(
                        (SZrArray *)&cfg->blocks,
                        blockIndex);
        SZrDataflowOwnershipRegionBinding binding;
        TZrSize aliasIndex;
        TZrSize ownerIndex;
        SZrSemanticOwnershipSymbolEntry *aliasEntry;
        SZrSemanticOwnershipSymbolEntry *ownerEntry;

        if (block == ZR_NULL ||
            !ZrParser_DataflowOwnership_StatementRegionBinding(
                    analysis->context,
                    block->statement,
                    &binding)) {
            continue;
        }
        if (!ZrParser_DataflowOwnership_SymbolFind(analysis->symbols,
                                                binding.aliasReference->symbolId,
                                                &aliasIndex) ||
            !ZrParser_DataflowOwnership_SymbolFind(analysis->symbols,
                                                binding.ownerReference->symbolId,
                                                &ownerIndex)) {
            return ZR_FALSE;
        }
        aliasEntry = ZrParser_DataflowOwnership_SymbolEntry(analysis->symbols, aliasIndex);
        ownerEntry = ZrParser_DataflowOwnership_SymbolEntry(analysis->symbols, ownerIndex);
        if (aliasEntry == ZR_NULL || ownerEntry == ZR_NULL) {
            return ZR_FALSE;
        }
        if (binding.isDeclaration) {
            aliasEntry->ownerIndex = ownerIndex;
        }
        semantic_ownership_update_region_facts(analysis,
                                               &binding,
                                               aliasEntry,
                                               ownerEntry);
    }
    return ZR_TRUE;
}

static TZrBool semantic_ownership_seed_unbound_builtin_regions(
        SZrSemanticOwnershipAnalysis *analysis) {
    TZrSize index;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        analysis->symbols == ZR_NULL ||
        !analysis->context->ownershipFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < analysis->context->ownershipFacts.length; index++) {
        SZrSemanticOwnershipFact *fact =
                (SZrSemanticOwnershipFact *)ZrCore_Array_Get(
                        &analysis->context->ownershipFacts,
                        index);
        const SZrSemanticReferenceFact *ownerReference;
        TZrSize ownerIndex;
        const SZrSemanticOwnershipSymbolEntry *ownerEntry;

        if (fact == ZR_NULL ||
            fact->node == ZR_NULL ||
            fact->node->type != ZR_AST_CONSTRUCT_EXPRESSION ||
            (fact->node->data.constructExpression.builtinKind !=
                     ZR_OWNERSHIP_BUILTIN_KIND_BORROW &&
             fact->node->data.constructExpression.builtinKind !=
                     ZR_OWNERSHIP_BUILTIN_KIND_LOAN &&
             fact->node->data.constructExpression.builtinKind !=
                     ZR_OWNERSHIP_BUILTIN_KIND_WEAK)) {
            continue;
        }
        ownerReference = ZrParser_DataflowOwnership_ConstructTargetRead(
                analysis->context,
                fact->node);
        if (ownerReference == ZR_NULL ||
            !ZrParser_DataflowOwnership_SymbolFind(analysis->symbols,
                                                ownerReference->symbolId,
                                                &ownerIndex)) {
            continue;
        }
        ownerEntry = ZrParser_DataflowOwnership_SymbolEntry(analysis->symbols, ownerIndex);
        if (ownerEntry == ZR_NULL) {
            continue;
        }
        if (fact->lifetimeRegionId == ZR_SEMANTIC_ID_INVALID) {
            fact->lifetimeRegionId = ZrParser_Semantic_ReserveLifetimeRegionId(
                    analysis->context);
        }
        fact->ownerLifetimeRegionId = ownerEntry->regionId;
        fact->relatedNode = ownerReference->node;
    }
    return ZR_TRUE;
}

static TZrBool semantic_ownership_run_cfg(SZrSemanticOwnershipAnalysis *semanticAnalysis,
                                          SZrAstNode *root) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    TZrBool ok;

    if (semanticAnalysis == ZR_NULL ||
        semanticAnalysis->context == ZR_NULL ||
        semanticAnalysis->symbols == ZR_NULL ||
        root == ZR_NULL ||
        semanticAnalysis->symbols->entries.length == 0) {
        return ZR_TRUE;
    }

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = semanticAnalysis->symbols->entries.length *
                         sizeof(SZrSemanticOwnershipFlowSlot);
    analysis.initEntry = semantic_ownership_init_entry;
    analysis.join = semantic_ownership_join;
    analysis.transferStatement = semantic_ownership_transfer_statement;
    analysis.userData = semanticAnalysis;

    ZrParser_Cfg_Init(semanticAnalysis->context->state, &cfg);
    ZrParser_DataflowResult_Init(&result);
    ok = ZrParser_Cfg_Build(semanticAnalysis->context->state, &cfg, root) &&
         semantic_ownership_bind_cfg_regions(semanticAnalysis, &cfg) &&
         ZrParser_Dataflow_Run(semanticAnalysis->context->state, &cfg, &analysis, &result);
    ZrParser_DataflowResult_Free(semanticAnalysis->context->state, &result);
    ZrParser_Cfg_Free(semanticAnalysis->context->state, &cfg);
    return ok;
}

static TZrBool semantic_ownership_resolve_node(SZrSemanticOwnershipAnalysis *analysis,
                                               SZrAstNode *node);

static TZrBool semantic_ownership_resolve_array(SZrSemanticOwnershipAnalysis *analysis,
                                                SZrAstNodeArray *nodes) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0; index < nodes->count; index++) {
        if (!semantic_ownership_resolve_node(analysis, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_ownership_resolve_node(SZrSemanticOwnershipAnalysis *analysis,
                                               SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return semantic_ownership_run_cfg(analysis, node) &&
                   semantic_ownership_resolve_array(analysis, node->data.script.statements);
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_ownership_run_cfg(analysis, node->data.functionDeclaration.body);
        case ZR_AST_TEST_DECLARATION:
            return semantic_ownership_run_cfg(analysis, node->data.testDeclaration.body);
        case ZR_AST_STRUCT_DECLARATION:
            return semantic_ownership_resolve_array(analysis, node->data.structDeclaration.members);
        case ZR_AST_CLASS_DECLARATION:
            return semantic_ownership_resolve_array(analysis, node->data.classDeclaration.members);
        case ZR_AST_STRUCT_METHOD:
            return semantic_ownership_run_cfg(analysis, node->data.structMethod.body);
        case ZR_AST_STRUCT_META_FUNCTION:
            return semantic_ownership_run_cfg(analysis, node->data.structMetaFunction.body);
        case ZR_AST_CLASS_METHOD:
            return semantic_ownership_run_cfg(analysis, node->data.classMethod.body);
        case ZR_AST_CLASS_META_FUNCTION:
            return semantic_ownership_run_cfg(analysis, node->data.classMetaFunction.body);
        case ZR_AST_PROPERTY_GET:
            return semantic_ownership_run_cfg(analysis, node->data.propertyGet.body);
        case ZR_AST_PROPERTY_SET:
            return semantic_ownership_run_cfg(analysis, node->data.propertySet.body);
        case ZR_AST_BLOCK:
            return semantic_ownership_run_cfg(analysis, node);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_SemanticFacts_ResolveControlFlowOwnership(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    SZrSemanticOwnershipSymbolMap symbols;
    SZrSemanticOwnershipAnalysis analysis;
    TZrBool ok;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        root == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    ZrParser_DataflowOwnership_SymbolMapConstruct(&symbols);
    if (!ZrParser_DataflowOwnership_SymbolMapBuild(context, &symbols)) {
        ZrParser_DataflowOwnership_SymbolMapFree(context, &symbols);
        return ZR_FALSE;
    }
    if (symbols.entries.length == 0) {
        ZrParser_DataflowOwnership_SymbolMapFree(context, &symbols);
        return ZR_TRUE;
    }

    memset(&analysis, 0, sizeof(analysis));
    analysis.context = context;
    analysis.symbols = &symbols;
    ZrParser_DataflowOwnership_OwnerSetPoolConstruct(&analysis.ownerSets);
    if (!ZrParser_DataflowOwnership_OwnerSetPoolInit(
                context->state,
                &analysis.ownerSets)) {
        ZrParser_DataflowOwnership_SymbolMapFree(context, &symbols);
        return ZR_FALSE;
    }
    if (!ZrParser_DataflowOwnership_ObservationsAllocate(
                context,
                &analysis.observations)) {
        ZrParser_DataflowOwnership_ObservationsFree(
                context,
                &analysis.observations);
        ZrParser_DataflowOwnership_OwnerSetPoolFree(
                context->state,
                &analysis.ownerSets);
        ZrParser_DataflowOwnership_SymbolMapFree(context, &symbols);
        return ZR_FALSE;
    }

    ok = semantic_ownership_resolve_node(&analysis, root) &&
         semantic_ownership_seed_unbound_builtin_regions(&analysis) &&
         ZrParser_DataflowOwnership_AppendObservedFacts(
                 context,
                 &symbols,
                 &analysis.observations);
    ZrParser_DataflowOwnership_ObservationsFree(
            context,
            &analysis.observations);
    ZrParser_DataflowOwnership_OwnerSetPoolFree(
            context->state,
            &analysis.ownerSets);
    ZrParser_DataflowOwnership_SymbolMapFree(context, &symbols);
    return ok;
}
