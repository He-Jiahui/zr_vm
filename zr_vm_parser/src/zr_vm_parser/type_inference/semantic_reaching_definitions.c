#include "dataflow.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

#define ZR_SEMANTIC_RD_MAX_DEFINITION_RANGES 8U

typedef enum EZrSemanticRdSlotState {
    ZR_SEMANTIC_RD_SLOT_UNKNOWN = 0,
    ZR_SEMANTIC_RD_SLOT_SINGLE,
    ZR_SEMANTIC_RD_SLOT_AMBIGUOUS
} EZrSemanticRdSlotState;

typedef struct SZrSemanticRdSlot {
    EZrSemanticRdSlotState state;
    SZrFileRange range;
    SZrFileRange ranges[ZR_SEMANTIC_RD_MAX_DEFINITION_RANGES];
    TZrSize rangeCount;
    TZrBool rangeOverflow;
} SZrSemanticRdSlot;

typedef struct SZrSemanticRdSymbolMap {
    SZrArray symbolIds;
    SZrArray declarationRanges;
} SZrSemanticRdSymbolMap;

typedef struct SZrSemanticRdAnalysis {
    SZrSemanticContext *context;
    const SZrSemanticRdSymbolMap *symbols;
    SZrSemanticRdSlot *readSlots;
    TZrBool *readSlotSeen;
    TZrSize readSlotCount;
} SZrSemanticRdAnalysis;

static TZrBool semantic_rd_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_rd_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_rd_range_is_known(const SZrFileRange *range) {
    if (range == ZR_NULL) {
        return ZR_FALSE;
    }

    return range->source != ZR_NULL ||
           range->start.line != 0 ||
           range->start.column != 0 ||
           range->start.offset != 0 ||
           range->end.line != 0 ||
           range->end.column != 0 ||
           range->end.offset != 0;
}

static TZrBool semantic_rd_ranges_equal(const SZrFileRange *left,
                                        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        !semantic_rd_same_source(left->source, right->source)) {
        return ZR_FALSE;
    }

    if (semantic_rd_has_offset(&left->start) ||
        semantic_rd_has_offset(&left->end) ||
        semantic_rd_has_offset(&right->start) ||
        semantic_rd_has_offset(&right->end)) {
        return left->start.offset == right->start.offset &&
               left->end.offset == right->end.offset;
    }

    return left->start.line == right->start.line &&
           left->start.column == right->start.column &&
           left->end.line == right->end.line &&
           left->end.column == right->end.column;
}

static TZrBool semantic_rd_range_contains_range(const SZrFileRange *outer,
                                                const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !semantic_rd_range_is_known(outer) ||
        !semantic_rd_range_is_known(inner) ||
        !semantic_rd_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }

    if ((semantic_rd_has_offset(&outer->start) ||
         semantic_rd_has_offset(&outer->end)) &&
        (semantic_rd_has_offset(&inner->start) ||
         semantic_rd_has_offset(&inner->end))) {
        return inner->start.offset >= outer->start.offset &&
               inner->end.offset <= outer->end.offset;
    }

    if (inner->start.line < outer->start.line ||
        inner->end.line > outer->end.line) {
        return ZR_FALSE;
    }
    if (inner->start.line == outer->start.line &&
        inner->start.column < outer->start.column) {
        return ZR_FALSE;
    }
    if (inner->end.line == outer->end.line &&
        inner->end.column > outer->end.column) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool semantic_rd_node_contains_fact(SZrAstNode *node,
                                              const SZrSemanticReferenceFact *fact) {
    if (node == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fact->node == node) {
        return ZR_TRUE;
    }
    return semantic_rd_range_contains_range(&node->location, &fact->range);
}

static TZrBool semantic_rd_reference_is_symbol_definition(
        const SZrSemanticReferenceFact *fact) {
    if (fact == ZR_NULL ||
        !fact->isResolved ||
        fact->symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    return fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
           fact->kind == ZR_SEMANTIC_REFERENCE_WRITE;
}

static void semantic_rd_reference_set_own_definition(SZrSemanticReferenceFact *fact) {
    if (!semantic_rd_reference_is_symbol_definition(fact)) {
        return;
    }

    if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
        semantic_rd_range_is_known(&fact->declarationRange)) {
        fact->definitionRange = fact->declarationRange;
    } else {
        fact->definitionRange = fact->range;
    }
    fact->hasDefinitionRange = semantic_rd_range_is_known(&fact->definitionRange);
}

static TZrBool semantic_rd_symbol_map_find(const SZrSemanticRdSymbolMap *map,
                                           TZrSymbolId symbolId,
                                           TZrSize *outIndex) {
    TZrSize index;

    if (outIndex != ZR_NULL) {
        *outIndex = 0;
    }
    if (map == ZR_NULL ||
        !map->symbolIds.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (index = 0; index < map->symbolIds.length; index++) {
        TZrSymbolId *candidate =
                (TZrSymbolId *)ZrCore_Array_Get((SZrArray *)&map->symbolIds, index);
        if (candidate != ZR_NULL && *candidate == symbolId) {
            if (outIndex != ZR_NULL) {
                *outIndex = index;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrFileRange *semantic_rd_symbol_map_range_at(const SZrSemanticRdSymbolMap *map,
                                                     TZrSize index) {
    if (map == ZR_NULL || !map->declarationRanges.isValid ||
        index >= map->declarationRanges.length) {
        return ZR_NULL;
    }
    return (SZrFileRange *)ZrCore_Array_Get((SZrArray *)&map->declarationRanges, index);
}

static TZrBool semantic_rd_symbol_map_add_or_update(SZrState *state,
                                                    SZrSemanticRdSymbolMap *map,
                                                    TZrSymbolId symbolId,
                                                    const SZrFileRange *declarationRange) {
    TZrSize index;
    SZrFileRange emptyRange;
    SZrFileRange rangeCopy;

    if (state == ZR_NULL ||
        map == ZR_NULL ||
        !map->symbolIds.isValid ||
        !map->declarationRanges.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    if (semantic_rd_symbol_map_find(map, symbolId, &index)) {
        SZrFileRange *storedRange = semantic_rd_symbol_map_range_at(map, index);
        if (storedRange != ZR_NULL &&
            !semantic_rd_range_is_known(storedRange) &&
            semantic_rd_range_is_known(declarationRange)) {
            *storedRange = *declarationRange;
        }
        return ZR_TRUE;
    }

    ZrCore_Memory_RawSet(&emptyRange, 0, sizeof(emptyRange));
    rangeCopy = semantic_rd_range_is_known(declarationRange)
                        ? *declarationRange
                        : emptyRange;
    ZrCore_Array_Push(state, &map->symbolIds, &symbolId);
    ZrCore_Array_Push(state, &map->declarationRanges, &rangeCopy);
    return ZR_TRUE;
}

static const SZrFileRange *semantic_rd_fact_declaration_range(
        const SZrSemanticReferenceFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_NULL;
    }
    if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
        fact->hasDefinitionRange &&
        semantic_rd_range_is_known(&fact->definitionRange)) {
        return &fact->definitionRange;
    }
    if (semantic_rd_range_is_known(&fact->declarationRange)) {
        return &fact->declarationRange;
    }
    if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION) {
        return &fact->range;
    }
    return ZR_NULL;
}

static TZrBool semantic_rd_build_symbol_map(SZrSemanticContext *context,
                                            SZrSemanticRdSymbolMap *map) {
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
    ZrCore_Array_Init(context->state, &map->symbolIds, sizeof(TZrSymbolId), capacity);
    ZrCore_Array_Init(context->state, &map->declarationRanges, sizeof(SZrFileRange), capacity);

    for (index = 0; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&context->referenceFacts, index);
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID) {
            continue;
        }
        if (fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
            fact->kind == ZR_SEMANTIC_REFERENCE_READ ||
            fact->kind == ZR_SEMANTIC_REFERENCE_WRITE) {
            if (!semantic_rd_symbol_map_add_or_update(context->state,
                                                      map,
                                                      fact->symbolId,
                                                      semantic_rd_fact_declaration_range(fact))) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

static void semantic_rd_free_symbol_map(SZrSemanticContext *context,
                                        SZrSemanticRdSymbolMap *map) {
    if (context == ZR_NULL || context->state == ZR_NULL || map == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(context->state, &map->symbolIds);
    ZrCore_Array_Free(context->state, &map->declarationRanges);
}

static TZrSize semantic_rd_state_size(TZrSize symbolCount) {
    return sizeof(SZrSemanticRdSlot) * symbolCount;
}

static SZrSemanticRdSlot *semantic_rd_slot(void *state, TZrSize symbolIndex) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    return &((SZrSemanticRdSlot *)state)[symbolIndex];
}

static void semantic_rd_set_slot(void *state,
                                 TZrSize symbolIndex,
                                 EZrSemanticRdSlotState slotState,
                                 const SZrFileRange *range) {
    SZrSemanticRdSlot *slot = semantic_rd_slot(state, symbolIndex);

    if (slot == ZR_NULL) {
        return;
    }

    slot->state = slotState;
    slot->rangeCount = 0;
    slot->rangeOverflow = ZR_FALSE;
    if (range != ZR_NULL && semantic_rd_range_is_known(range)) {
        slot->range = *range;
        slot->ranges[0] = *range;
        slot->rangeCount = 1;
    } else {
        ZrCore_Memory_RawSet(&slot->range, 0, sizeof(slot->range));
    }
}

static TZrBool semantic_rd_slot_has_range(const SZrSemanticRdSlot *slot,
                                          const SZrFileRange *range) {
    TZrSize index;

    if (slot == ZR_NULL || range == ZR_NULL || !semantic_rd_range_is_known(range)) {
        return ZR_FALSE;
    }

    for (index = 0; index < slot->rangeCount; index++) {
        if (semantic_rd_ranges_equal(&slot->ranges[index], range)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool semantic_rd_slot_add_range(SZrSemanticRdSlot *slot,
                                          const SZrFileRange *range) {
    if (slot == ZR_NULL || range == ZR_NULL || !semantic_rd_range_is_known(range)) {
        return ZR_FALSE;
    }
    if (semantic_rd_slot_has_range(slot, range)) {
        return ZR_FALSE;
    }
    if (slot->rangeCount >= ZR_SEMANTIC_RD_MAX_DEFINITION_RANGES) {
        slot->rangeOverflow = ZR_TRUE;
        return ZR_FALSE;
    }

    slot->ranges[slot->rangeCount] = *range;
    slot->rangeCount++;
    return ZR_TRUE;
}

static TZrBool semantic_rd_slot_add_ranges(SZrSemanticRdSlot *dst,
                                           const SZrSemanticRdSlot *src) {
    TZrSize index;
    TZrBool changed = ZR_FALSE;

    if (dst == ZR_NULL || src == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < src->rangeCount; index++) {
        if (semantic_rd_slot_add_range(dst, &src->ranges[index])) {
            changed = ZR_TRUE;
        }
    }
    if (src->rangeOverflow) {
        dst->rangeOverflow = ZR_TRUE;
    }
    return changed;
}

static TZrBool semantic_rd_merge_slot(SZrSemanticRdSlot *dst,
                                      const SZrSemanticRdSlot *src) {
    TZrBool changed;

    if (dst == ZR_NULL || src == ZR_NULL) {
        return ZR_FALSE;
    }

    if (dst->state == src->state) {
        if (dst->state != ZR_SEMANTIC_RD_SLOT_SINGLE ||
            semantic_rd_ranges_equal(&dst->range, &src->range)) {
            return semantic_rd_slot_add_ranges(dst, src);
        }
    }

    if (dst->state == ZR_SEMANTIC_RD_SLOT_UNKNOWN &&
        src->state == ZR_SEMANTIC_RD_SLOT_UNKNOWN) {
        return ZR_FALSE;
    }

    if (dst->state == ZR_SEMANTIC_RD_SLOT_SINGLE &&
        src->state == ZR_SEMANTIC_RD_SLOT_SINGLE &&
        semantic_rd_ranges_equal(&dst->range, &src->range)) {
        return semantic_rd_slot_add_ranges(dst, src);
    }

    changed = dst->state != ZR_SEMANTIC_RD_SLOT_AMBIGUOUS;
    if (dst->state == ZR_SEMANTIC_RD_SLOT_SINGLE) {
        (void)semantic_rd_slot_add_range(dst, &dst->range);
    }
    if (src->state == ZR_SEMANTIC_RD_SLOT_SINGLE) {
        changed = semantic_rd_slot_add_range(dst, &src->range) || changed;
    }
    changed = semantic_rd_slot_add_ranges(dst, src) || changed;

    dst->state = ZR_SEMANTIC_RD_SLOT_AMBIGUOUS;
    ZrCore_Memory_RawSet(&dst->range, 0, sizeof(dst->range));
    return changed;
}

static void semantic_rd_apply_definition_fact(void *state,
                                              const SZrSemanticRdAnalysis *analysis,
                                              SZrSemanticReferenceFact *fact) {
    TZrSize symbolIndex;

    if (state == ZR_NULL ||
        analysis == ZR_NULL ||
        fact == ZR_NULL ||
        !semantic_rd_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
        return;
    }

    semantic_rd_reference_set_own_definition(fact);
    if (fact->hasDefinitionRange) {
        semantic_rd_set_slot(state,
                             symbolIndex,
                             ZR_SEMANTIC_RD_SLOT_SINGLE,
                             &fact->definitionRange);
    }
}

static void semantic_rd_clear_fact_definition_ranges(SZrSemanticContext *context,
                                                     SZrSemanticReferenceFact *fact) {
    if (context == ZR_NULL || context->state == ZR_NULL || fact == ZR_NULL) {
        return;
    }

    if (fact->definitionRanges.isValid) {
        ZrCore_Array_Free(context->state, &fact->definitionRanges);
    }
    ZrCore_Array_Construct(&fact->definitionRanges);
}

static TZrBool semantic_rd_append_fact_definition_range(SZrSemanticContext *context,
                                                        SZrSemanticReferenceFact *fact,
                                                        const SZrFileRange *range) {
    SZrFileRange rangeCopy;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        fact == ZR_NULL ||
        range == ZR_NULL ||
        !semantic_rd_range_is_known(range)) {
        return ZR_FALSE;
    }

    if (!fact->definitionRanges.isValid) {
        ZrCore_Array_Init(context->state,
                          &fact->definitionRanges,
                          sizeof(SZrFileRange),
                          ZR_PARSER_INITIAL_CAPACITY_TINY);
    }
    rangeCopy = *range;
    ZrCore_Array_Push(context->state, &fact->definitionRanges, &rangeCopy);
    return ZR_TRUE;
}

static void semantic_rd_apply_read_slot_to_fact(SZrSemanticContext *context,
                                                SZrSemanticReferenceFact *fact,
                                                const SZrSemanticRdSlot *slot) {
    TZrSize index;

    if (fact == ZR_NULL || slot == ZR_NULL) {
        return;
    }

    semantic_rd_clear_fact_definition_ranges(context, fact);
    for (index = 0; index < slot->rangeCount; index++) {
        (void)semantic_rd_append_fact_definition_range(context, fact, &slot->ranges[index]);
    }

    if (slot->state == ZR_SEMANTIC_RD_SLOT_SINGLE &&
        semantic_rd_range_is_known(&slot->range)) {
        fact->definitionRange = slot->range;
        fact->hasDefinitionRange = ZR_TRUE;
        return;
    }

    fact->hasDefinitionRange = ZR_FALSE;
}

static void semantic_rd_join_read_slot(SZrSemanticRdSlot *dst,
                                       const SZrSemanticRdSlot *src) {
    (void)semantic_rd_merge_slot(dst, src);
}

static void semantic_rd_record_read_slot(SZrSemanticRdAnalysis *analysis,
                                         TZrSize factIndex,
                                         const SZrSemanticRdSlot *value) {
    if (analysis == ZR_NULL ||
        analysis->readSlots == ZR_NULL ||
        analysis->readSlotSeen == ZR_NULL ||
        value == ZR_NULL ||
        factIndex >= analysis->readSlotCount) {
        return;
    }

    if (analysis->readSlotSeen[factIndex]) {
        semantic_rd_join_read_slot(&analysis->readSlots[factIndex], value);
        return;
    }

    analysis->readSlots[factIndex] = *value;
    analysis->readSlotSeen[factIndex] = ZR_TRUE;
}

static void semantic_rd_apply_read_fact(void *state,
                                        SZrSemanticRdAnalysis *analysis,
                                        SZrSemanticReferenceFact *fact,
                                        TZrSize factIndex) {
    TZrSize symbolIndex;
    SZrSemanticRdSlot *slot;
    SZrSemanticRdSlot value;

    if (state == ZR_NULL ||
        analysis == ZR_NULL ||
        fact == ZR_NULL ||
        !semantic_rd_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
        return;
    }

    semantic_rd_set_slot(&value, 0, ZR_SEMANTIC_RD_SLOT_UNKNOWN, ZR_NULL);
    slot = semantic_rd_slot(state, symbolIndex);
    if (slot != ZR_NULL &&
        slot->state == ZR_SEMANTIC_RD_SLOT_SINGLE &&
        semantic_rd_range_is_known(&slot->range)) {
        semantic_rd_set_slot(&value, 0, ZR_SEMANTIC_RD_SLOT_SINGLE, &slot->range);
    } else if (slot != ZR_NULL && slot->state == ZR_SEMANTIC_RD_SLOT_AMBIGUOUS) {
        value = *slot;
    }

    semantic_rd_record_read_slot(analysis, factIndex, &value);
    semantic_rd_apply_read_slot_to_fact(analysis->context, fact, &value);
}

static void semantic_rd_apply_read_slots(SZrSemanticRdAnalysis *analysis) {
    TZrSize index;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid ||
        analysis->readSlots == ZR_NULL ||
        analysis->readSlotSeen == ZR_NULL) {
        return;
    }

    for (index = 0;
         index < analysis->readSlotCount && index < analysis->context->referenceFacts.length;
         index++) {
        SZrSemanticReferenceFact *fact;

        if (!analysis->readSlotSeen[index]) {
            continue;
        }

        fact = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &analysis->context->referenceFacts,
                index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_READ) {
            continue;
        }

        semantic_rd_apply_read_slot_to_fact(analysis->context, fact, &analysis->readSlots[index]);
    }
}

static TZrBool semantic_rd_fact_in_statement(SZrAstNode *statement,
                                             const SZrSemanticReferenceFact *fact) {
    if (statement == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (statement->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_rd_node_contains_fact(statement->data.variableDeclaration.pattern, fact) ||
                   semantic_rd_node_contains_fact(statement->data.variableDeclaration.value, fact);
        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_rd_node_contains_fact(statement->data.expressionStatement.expr, fact);
        case ZR_AST_RETURN_STATEMENT:
            return semantic_rd_node_contains_fact(statement->data.returnStatement.expr, fact);
        case ZR_AST_THROW_STATEMENT:
            return semantic_rd_node_contains_fact(statement->data.throwStatement.expr, fact);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_rd_node_contains_fact(statement->data.breakContinueStatement.expr, fact);
        case ZR_AST_IF_EXPRESSION:
            return semantic_rd_node_contains_fact(statement->data.ifExpression.condition, fact);
        case ZR_AST_WHILE_LOOP:
            return semantic_rd_node_contains_fact(statement->data.whileLoop.cond, fact);
        case ZR_AST_FOR_LOOP:
            return semantic_rd_node_contains_fact(statement->data.forLoop.cond, fact);
        case ZR_AST_FOREACH_LOOP:
            return semantic_rd_node_contains_fact(statement->data.foreachLoop.expr, fact) ||
                   semantic_rd_node_contains_fact(statement->data.foreachLoop.pattern, fact);
        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_rd_node_contains_fact(statement->data.switchExpression.expr, fact);
        case ZR_AST_SWITCH_CASE:
            return semantic_rd_node_contains_fact(statement->data.switchCase.value, fact);
        case ZR_AST_BLOCK:
        case ZR_AST_CATCH_CLAUSE:
        case ZR_AST_SWITCH_DEFAULT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return ZR_FALSE;
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_rd_range_contains_range(&statement->data.functionDeclaration.nameLocation,
                                                    &fact->range);
        default:
            return semantic_rd_node_contains_fact(statement, fact);
    }
}

static void semantic_rd_transfer_statement(SZrAstNode *statement, void *state, void *userData) {
    SZrSemanticRdAnalysis *analysis = (SZrSemanticRdAnalysis *)userData;
    TZrSize index;

    if (statement == ZR_NULL ||
        state == ZR_NULL ||
        analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid) {
        return;
    }

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&analysis->context->referenceFacts, index);
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_rd_fact_in_statement(statement, fact)) {
            continue;
        }

        switch (fact->kind) {
            case ZR_SEMANTIC_REFERENCE_READ:
                semantic_rd_apply_read_fact(state, analysis, fact, index);
                break;
            case ZR_SEMANTIC_REFERENCE_DECLARATION:
            case ZR_SEMANTIC_REFERENCE_WRITE:
                semantic_rd_apply_definition_fact(state, analysis, fact);
                break;
            default:
                break;
        }
    }
}

static void semantic_rd_init_entry(void *state, void *userData) {
    SZrSemanticRdAnalysis *analysis = (SZrSemanticRdAnalysis *)userData;
    TZrSize index;

    if (state == ZR_NULL || analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(state,
                         0,
                         semantic_rd_state_size(analysis->symbols->symbolIds.length));
    for (index = 0; index < analysis->symbols->symbolIds.length; index++) {
        const SZrFileRange *declarationRange =
                semantic_rd_symbol_map_range_at(analysis->symbols, index);
        if (semantic_rd_range_is_known(declarationRange)) {
            semantic_rd_set_slot(state,
                                 index,
                                 ZR_SEMANTIC_RD_SLOT_SINGLE,
                                 declarationRange);
        }
    }
}

static TZrBool semantic_rd_join(void *dst, const void *src, void *userData) {
    SZrSemanticRdAnalysis *analysis = (SZrSemanticRdAnalysis *)userData;
    TZrSize index;
    TZrBool changed = ZR_FALSE;

    if (dst == ZR_NULL ||
        src == ZR_NULL ||
        analysis == ZR_NULL ||
        analysis->symbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < analysis->symbols->symbolIds.length; index++) {
        SZrSemanticRdSlot *dstSlot = &((SZrSemanticRdSlot *)dst)[index];
        const SZrSemanticRdSlot *srcSlot = &((const SZrSemanticRdSlot *)src)[index];

        if (semantic_rd_merge_slot(dstSlot, srcSlot)) {
            changed = ZR_TRUE;
        }
    }

    return changed;
}

static TZrBool semantic_rd_run_cfg_for_root(SZrSemanticContext *context,
                                            const SZrSemanticRdSymbolMap *symbols,
                                            SZrAstNode *root) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SZrSemanticRdAnalysis semanticAnalysis;
    TZrBool ok;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        symbols == ZR_NULL ||
        root == ZR_NULL ||
        symbols->symbolIds.length == 0) {
        return ZR_TRUE;
    }

    semanticAnalysis.context = context;
    semanticAnalysis.symbols = symbols;
    semanticAnalysis.readSlotCount = context->referenceFacts.length;
    semanticAnalysis.readSlots = ZR_NULL;
    semanticAnalysis.readSlotSeen = ZR_NULL;

    if (semanticAnalysis.readSlotCount > 0) {
        semanticAnalysis.readSlots =
                (SZrSemanticRdSlot *)ZrCore_Memory_RawMallocWithType(
                        context->state->global,
                        semanticAnalysis.readSlotCount * sizeof(SZrSemanticRdSlot),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        semanticAnalysis.readSlotSeen =
                (TZrBool *)ZrCore_Memory_RawMallocWithType(
                        context->state->global,
                        semanticAnalysis.readSlotCount * sizeof(TZrBool),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (semanticAnalysis.readSlots == ZR_NULL ||
            semanticAnalysis.readSlotSeen == ZR_NULL) {
            if (semanticAnalysis.readSlots != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        semanticAnalysis.readSlots,
                        semanticAnalysis.readSlotCount * sizeof(SZrSemanticRdSlot),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (semanticAnalysis.readSlotSeen != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        semanticAnalysis.readSlotSeen,
                        semanticAnalysis.readSlotCount * sizeof(TZrBool),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(semanticAnalysis.readSlots,
                             0,
                             semanticAnalysis.readSlotCount * sizeof(SZrSemanticRdSlot));
        ZrCore_Memory_RawSet(semanticAnalysis.readSlotSeen,
                             0,
                             semanticAnalysis.readSlotCount * sizeof(TZrBool));
    }

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = semantic_rd_state_size(symbols->symbolIds.length);
    analysis.initEntry = semantic_rd_init_entry;
    analysis.join = semantic_rd_join;
    analysis.transferStatement = semantic_rd_transfer_statement;
    analysis.userData = &semanticAnalysis;

    ZrParser_Cfg_Init(context->state, &cfg);
    ZrParser_DataflowResult_Init(&result);
    ok = ZrParser_Cfg_Build(context->state, &cfg, root) &&
         ZrParser_Dataflow_Run(context->state, &cfg, &analysis, &result);
    if (ok) {
        semantic_rd_apply_read_slots(&semanticAnalysis);
    }
    ZrParser_DataflowResult_Free(context->state, &result);
    ZrParser_Cfg_Free(context->state, &cfg);
    if (semanticAnalysis.readSlots != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                semanticAnalysis.readSlots,
                semanticAnalysis.readSlotCount * sizeof(SZrSemanticRdSlot),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (semanticAnalysis.readSlotSeen != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                semanticAnalysis.readSlotSeen,
                semanticAnalysis.readSlotCount * sizeof(TZrBool),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ok;
}

static TZrBool semantic_rd_resolve_node(SZrSemanticContext *context,
                                        const SZrSemanticRdSymbolMap *symbols,
                                        SZrAstNode *node);

static TZrBool semantic_rd_resolve_function_like_body(SZrSemanticContext *context,
                                                      const SZrSemanticRdSymbolMap *symbols,
                                                      SZrAstNode *body) {
    TZrSize index;

    if (body == ZR_NULL || body->type != ZR_AST_BLOCK || body->data.block.body == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < body->data.block.body->count; index++) {
        if (!semantic_rd_resolve_node(context, symbols, body->data.block.body->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_rd_resolve_node(SZrSemanticContext *context,
                                        const SZrSemanticRdSymbolMap *symbols,
                                        SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (!semantic_rd_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            if (node->data.script.statements == ZR_NULL) {
                return ZR_TRUE;
            }
            for (index = 0; index < node->data.script.statements->count; index++) {
                if (!semantic_rd_resolve_node(context,
                                              symbols,
                                              node->data.script.statements->nodes[index])) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_FUNCTION_DECLARATION:
            if (!semantic_rd_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            return semantic_rd_resolve_function_like_body(context,
                                                         symbols,
                                                         node->data.functionDeclaration.body);
        case ZR_AST_TEST_DECLARATION:
            if (!semantic_rd_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            return semantic_rd_resolve_function_like_body(context,
                                                         symbols,
                                                         node->data.testDeclaration.body);
        case ZR_AST_BLOCK:
            return semantic_rd_resolve_function_like_body(context, symbols, node);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    SZrSemanticRdSymbolMap symbols;
    TZrBool ok;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        root == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&symbols.symbolIds);
    ZrCore_Array_Construct(&symbols.declarationRanges);
    if (!semantic_rd_build_symbol_map(context, &symbols)) {
        semantic_rd_free_symbol_map(context, &symbols);
        return ZR_FALSE;
    }

    ok = semantic_rd_resolve_node(context, &symbols, root);
    semantic_rd_free_symbol_map(context, &symbols);
    return ok;
}
