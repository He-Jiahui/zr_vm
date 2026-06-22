#include "dataflow.h"
#include "dataflow_definite_assignment.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_facts.h"

typedef struct SZrSemanticDaSymbolMap {
    SZrArray symbolIds;
} SZrSemanticDaSymbolMap;

typedef struct SZrSemanticDaAnalysis {
    SZrSemanticContext *context;
    const SZrSemanticDaSymbolMap *symbols;
    EZrParserDefiniteAssignmentState *readStates;
    TZrBool *readStateSeen;
    TZrSize readStateCount;
} SZrSemanticDaAnalysis;

static TZrBool semantic_da_same_source(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_TRUE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_da_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

static TZrBool semantic_da_range_is_known(const SZrFileRange *range) {
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

static TZrBool semantic_da_range_contains_range(const SZrFileRange *outer,
                                                const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !semantic_da_range_is_known(outer) ||
        !semantic_da_range_is_known(inner) ||
        !semantic_da_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }

    if ((semantic_da_has_offset(&outer->start) ||
         semantic_da_has_offset(&outer->end)) &&
        (semantic_da_has_offset(&inner->start) ||
         semantic_da_has_offset(&inner->end))) {
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

static TZrBool semantic_da_node_contains_fact(SZrAstNode *node,
                                              const SZrSemanticReferenceFact *fact) {
    if (node == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fact->node == node) {
        return ZR_TRUE;
    }
    return semantic_da_range_contains_range(&node->location, &fact->range);
}

static TZrBool semantic_da_symbol_map_find(const SZrSemanticDaSymbolMap *map,
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

static TZrBool semantic_da_symbol_map_add(SZrState *state,
                                          SZrSemanticDaSymbolMap *map,
                                          TZrSymbolId symbolId) {
    if (state == ZR_NULL ||
        map == ZR_NULL ||
        !map->symbolIds.isValid ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (semantic_da_symbol_map_find(map, symbolId, ZR_NULL)) {
        return ZR_TRUE;
    }

    ZrCore_Array_Push(state, &map->symbolIds, &symbolId);
    return ZR_TRUE;
}

static TZrBool semantic_da_build_symbol_map(SZrSemanticContext *context,
                                            SZrSemanticDaSymbolMap *map) {
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
            if (!semantic_da_symbol_map_add(context->state, map, fact->symbolId)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

static void semantic_da_free_symbol_map(SZrSemanticContext *context,
                                        SZrSemanticDaSymbolMap *map) {
    if (context == ZR_NULL || context->state == ZR_NULL || map == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(context->state, &map->symbolIds);
}

static TZrBool semantic_da_names_equal(SZrString *left, SZrString *right) {
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    if (left == right) {
        return ZR_TRUE;
    }
    return ZrCore_String_Equal(left, right);
}

static TZrBool semantic_da_find_symbol_by_name(const SZrSemanticDaAnalysis *analysis,
                                               SZrString *name,
                                               TZrSymbolId *outSymbolId,
                                               TZrSize *outSymbolIndex) {
    TZrSize index;

    if (outSymbolId != ZR_NULL) {
        *outSymbolId = ZR_SEMANTIC_ID_INVALID;
    }
    if (outSymbolIndex != ZR_NULL) {
        *outSymbolIndex = 0;
    }
    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid ||
        name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&analysis->context->referenceFacts, index);
        TZrSize symbolIndex;
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_da_names_equal(fact->name, name) ||
            !semantic_da_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
            continue;
        }

        if (outSymbolId != ZR_NULL) {
            *outSymbolId = fact->symbolId;
        }
        if (outSymbolIndex != ZR_NULL) {
            *outSymbolIndex = symbolIndex;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static EZrSemanticDefiniteAssignmentState semantic_da_to_reference_state(
        EZrParserDefiniteAssignmentState state) {
    switch (state) {
        case ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT:
            return ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT;
        case ZR_PARSER_DEFINITE_ASSIGNMENT_INIT:
            return ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT;
        case ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT:
        default:
            return ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT;
    }
}

static EZrParserDefiniteAssignmentState semantic_da_from_reference_state(
        EZrSemanticDefiniteAssignmentState state) {
    switch (state) {
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNINIT:
            return ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT;
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_INIT:
            return ZR_PARSER_DEFINITE_ASSIGNMENT_INIT;
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_MAYBE_INIT:
        case ZR_SEMANTIC_DEFINITE_ASSIGNMENT_UNKNOWN:
        default:
            return ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
    }
}

static EZrParserDefiniteAssignmentState semantic_da_join_state(
        EZrParserDefiniteAssignmentState left,
        EZrParserDefiniteAssignmentState right) {
    return left == right ? left : ZR_PARSER_DEFINITE_ASSIGNMENT_MAYBE_INIT;
}

static EZrParserDefiniteAssignmentState semantic_da_declaration_state(SZrAstNode *statement) {
    if (statement != ZR_NULL &&
        statement->type == ZR_AST_VARIABLE_DECLARATION &&
        statement->data.variableDeclaration.value == ZR_NULL) {
        return ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT;
    }
    return ZR_PARSER_DEFINITE_ASSIGNMENT_INIT;
}

static EZrParserDefiniteAssignmentState semantic_da_declaration_fact_state(SZrAstNode *statement) {
    if (statement != ZR_NULL && statement->type == ZR_AST_VARIABLE_DECLARATION) {
        return ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT;
    }
    return semantic_da_declaration_state(statement);
}

static void semantic_da_set_slot(void *state,
                                 const SZrSemanticDaAnalysis *analysis,
                                 TZrSize symbolIndex,
                                 EZrParserDefiniteAssignmentState value) {
    if (analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return;
    }
    ZrParser_DefiniteAssignment_Set(state,
                                    analysis->symbols->symbolIds.length,
                                    symbolIndex,
                                    value);
}

static void semantic_da_apply_fact_state(void *state,
                                         const SZrSemanticDaAnalysis *analysis,
                                         SZrSemanticReferenceFact *fact,
                                         EZrParserDefiniteAssignmentState value) {
    TZrSize symbolIndex;

    if (state == ZR_NULL ||
        analysis == ZR_NULL ||
        fact == ZR_NULL ||
        !semantic_da_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
        return;
    }

    fact->definiteAssignmentState = semantic_da_to_reference_state(value);
    fact->hasDefiniteAssignmentState = ZR_TRUE;
    semantic_da_set_slot(state, analysis, symbolIndex, value);
}

static void semantic_da_read_fact_state(void *state,
                                        const SZrSemanticDaAnalysis *analysis,
                                        SZrSemanticReferenceFact *fact,
                                        TZrSize factIndex) {
    TZrSize symbolIndex;
    EZrParserDefiniteAssignmentState value;

    if (state == ZR_NULL ||
        analysis == ZR_NULL ||
        fact == ZR_NULL ||
        !semantic_da_symbol_map_find(analysis->symbols, fact->symbolId, &symbolIndex)) {
        return;
    }

    value = ZrParser_DefiniteAssignment_Get(state,
                                            analysis->symbols->symbolIds.length,
                                            symbolIndex);
    if (analysis->readStates != ZR_NULL &&
        analysis->readStateSeen != ZR_NULL &&
        factIndex < analysis->readStateCount) {
        if (analysis->readStateSeen[factIndex]) {
            analysis->readStates[factIndex] =
                    semantic_da_join_state(analysis->readStates[factIndex], value);
        } else {
            analysis->readStates[factIndex] = value;
            analysis->readStateSeen[factIndex] = ZR_TRUE;
        }
    }
    fact->definiteAssignmentState = semantic_da_to_reference_state(value);
    fact->hasDefiniteAssignmentState = ZR_TRUE;
}

static void semantic_da_apply_read_states(const SZrSemanticDaAnalysis *analysis) {
    TZrSize index;

    if (analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid ||
        analysis->readStates == ZR_NULL ||
        analysis->readStateSeen == ZR_NULL) {
        return;
    }

    for (index = 0;
         index < analysis->readStateCount && index < analysis->context->referenceFacts.length;
         index++) {
        SZrSemanticReferenceFact *fact;

        if (!analysis->readStateSeen[index]) {
            continue;
        }

        fact = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &analysis->context->referenceFacts,
                index);
        if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_READ) {
            continue;
        }

        fact->definiteAssignmentState = semantic_da_to_reference_state(analysis->readStates[index]);
        fact->hasDefiniteAssignmentState = ZR_TRUE;
    }
}

static SZrString *semantic_da_variable_declaration_name(SZrAstNode *statement) {
    SZrAstNode *pattern;

    if (statement == ZR_NULL || statement->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_NULL;
    }

    pattern = statement->data.variableDeclaration.pattern;
    if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    return pattern->data.identifier.name;
}

static TZrBool semantic_da_variable_declaration_slot(SZrAstNode *statement,
                                                     const SZrSemanticDaAnalysis *analysis,
                                                     TZrSize *outSymbolIndex) {
    SZrString *name = semantic_da_variable_declaration_name(statement);
    TZrSymbolId symbolId;

    if (outSymbolIndex != ZR_NULL) {
        *outSymbolIndex = 0;
    }
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_da_find_symbol_by_name(analysis, name, &symbolId, outSymbolIndex);
}

static TZrBool semantic_da_fact_in_statement(SZrAstNode *statement,
                                             const SZrSemanticReferenceFact *fact) {
    if (statement == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (statement->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_da_node_contains_fact(statement->data.variableDeclaration.pattern, fact) ||
                   semantic_da_node_contains_fact(statement->data.variableDeclaration.value, fact);
        case ZR_AST_EXPRESSION_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.expressionStatement.expr, fact);
        case ZR_AST_RETURN_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.returnStatement.expr, fact);
        case ZR_AST_THROW_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.throwStatement.expr, fact);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return semantic_da_node_contains_fact(statement->data.breakContinueStatement.expr, fact);
        case ZR_AST_IF_EXPRESSION:
            return semantic_da_node_contains_fact(statement->data.ifExpression.condition, fact);
        case ZR_AST_WHILE_LOOP:
            return semantic_da_node_contains_fact(statement->data.whileLoop.cond, fact);
        case ZR_AST_FOR_LOOP:
            return semantic_da_node_contains_fact(statement->data.forLoop.cond, fact);
        case ZR_AST_FOREACH_LOOP:
            return semantic_da_node_contains_fact(statement->data.foreachLoop.expr, fact) ||
                   semantic_da_node_contains_fact(statement->data.foreachLoop.pattern, fact);
        case ZR_AST_SWITCH_EXPRESSION:
            return semantic_da_node_contains_fact(statement->data.switchExpression.expr, fact);
        case ZR_AST_SWITCH_CASE:
            return semantic_da_node_contains_fact(statement->data.switchCase.value, fact);
        case ZR_AST_BLOCK:
        case ZR_AST_CATCH_CLAUSE:
        case ZR_AST_SWITCH_DEFAULT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return ZR_FALSE;
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_da_range_contains_range(&statement->data.functionDeclaration.nameLocation,
                                                    &fact->range);
        default:
            return semantic_da_node_contains_fact(statement, fact);
    }
}

static void semantic_da_transfer_statement(SZrAstNode *statement, void *state, void *userData) {
    SZrSemanticDaAnalysis *analysis = (SZrSemanticDaAnalysis *)userData;
    TZrBool hasDeclarationSlot = ZR_FALSE;
    TZrSize declarationSlot = 0;
    TZrSize index;

    if (statement == ZR_NULL ||
        state == ZR_NULL ||
        analysis == ZR_NULL ||
        analysis->context == ZR_NULL ||
        !analysis->context->referenceFacts.isValid) {
        return;
    }

    hasDeclarationSlot = semantic_da_variable_declaration_slot(statement,
                                                               analysis,
                                                               &declarationSlot);
    if (hasDeclarationSlot) {
        semantic_da_set_slot(state,
                             analysis,
                             declarationSlot,
                             ZR_PARSER_DEFINITE_ASSIGNMENT_UNINIT);
    }

    for (index = 0; index < analysis->context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact =
                (SZrSemanticReferenceFact *)ZrCore_Array_Get(&analysis->context->referenceFacts, index);
        if (fact == ZR_NULL ||
            !fact->isResolved ||
            fact->symbolId == ZR_SEMANTIC_ID_INVALID ||
            !semantic_da_fact_in_statement(statement, fact)) {
            continue;
        }

        switch (fact->kind) {
            case ZR_SEMANTIC_REFERENCE_READ:
                semantic_da_read_fact_state(state, analysis, fact, index);
                break;
            case ZR_SEMANTIC_REFERENCE_WRITE:
                semantic_da_apply_fact_state(state,
                                             analysis,
                                             fact,
                                             ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
                break;
            case ZR_SEMANTIC_REFERENCE_DECLARATION:
                semantic_da_apply_fact_state(state,
                                             analysis,
                                             fact,
                                             semantic_da_declaration_fact_state(statement));
                break;
            default:
                break;
        }
    }

    if (hasDeclarationSlot && statement->data.variableDeclaration.value != ZR_NULL) {
        semantic_da_set_slot(state,
                             analysis,
                             declarationSlot,
                             ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
    }
}

static void semantic_da_init_entry(void *state, void *userData) {
    SZrSemanticDaAnalysis *analysis = (SZrSemanticDaAnalysis *)userData;

    if (analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return;
    }

    ZrParser_DefiniteAssignment_InitState(state,
                                          analysis->symbols->symbolIds.length,
                                          ZR_PARSER_DEFINITE_ASSIGNMENT_INIT);
}

static TZrBool semantic_da_join(void *dst, const void *src, void *userData) {
    SZrSemanticDaAnalysis *analysis = (SZrSemanticDaAnalysis *)userData;

    if (analysis == ZR_NULL || analysis->symbols == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrParser_DefiniteAssignment_Join(dst,
                                            src,
                                            analysis->symbols->symbolIds.length);
}

static TZrBool semantic_da_run_cfg_for_root(SZrSemanticContext *context,
                                            const SZrSemanticDaSymbolMap *symbols,
                                            SZrAstNode *root) {
    SZrParserCfg cfg;
    SZrParserDataflowResult result;
    SZrParserDataflowAnalysis analysis;
    SZrSemanticDaAnalysis semanticAnalysis;
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
    semanticAnalysis.readStateCount = context->referenceFacts.length;
    semanticAnalysis.readStates = ZR_NULL;
    semanticAnalysis.readStateSeen = ZR_NULL;

    if (semanticAnalysis.readStateCount > 0) {
        semanticAnalysis.readStates =
                (EZrParserDefiniteAssignmentState *)ZrCore_Memory_RawMallocWithType(
                        context->state->global,
                        semanticAnalysis.readStateCount *
                                sizeof(EZrParserDefiniteAssignmentState),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        semanticAnalysis.readStateSeen =
                (TZrBool *)ZrCore_Memory_RawMallocWithType(
                        context->state->global,
                        semanticAnalysis.readStateCount * sizeof(TZrBool),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (semanticAnalysis.readStates == ZR_NULL ||
            semanticAnalysis.readStateSeen == ZR_NULL) {
            if (semanticAnalysis.readStates != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        semanticAnalysis.readStates,
                        semanticAnalysis.readStateCount *
                                sizeof(EZrParserDefiniteAssignmentState),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            if (semanticAnalysis.readStateSeen != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        context->state->global,
                        semanticAnalysis.readStateSeen,
                        semanticAnalysis.readStateCount * sizeof(TZrBool),
                        ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(semanticAnalysis.readStates,
                             0,
                             semanticAnalysis.readStateCount *
                                     sizeof(EZrParserDefiniteAssignmentState));
        ZrCore_Memory_RawSet(semanticAnalysis.readStateSeen,
                             0,
                             semanticAnalysis.readStateCount * sizeof(TZrBool));
    }

    analysis.direction = ZR_PARSER_DATAFLOW_FORWARD;
    analysis.stateSize = ZrParser_DefiniteAssignment_StateSize(symbols->symbolIds.length);
    analysis.initEntry = semantic_da_init_entry;
    analysis.join = semantic_da_join;
    analysis.transferStatement = semantic_da_transfer_statement;
    analysis.userData = &semanticAnalysis;

    ZrParser_Cfg_Init(context->state, &cfg);
    ZrParser_DataflowResult_Init(&result);
    ok = ZrParser_Cfg_Build(context->state, &cfg, root) &&
         ZrParser_Dataflow_Run(context->state, &cfg, &analysis, &result);
    if (ok) {
        semantic_da_apply_read_states(&semanticAnalysis);
    }
    ZrParser_DataflowResult_Free(context->state, &result);
    ZrParser_Cfg_Free(context->state, &cfg);
    if (semanticAnalysis.readStates != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                semanticAnalysis.readStates,
                semanticAnalysis.readStateCount * sizeof(EZrParserDefiniteAssignmentState),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (semanticAnalysis.readStateSeen != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                context->state->global,
                semanticAnalysis.readStateSeen,
                semanticAnalysis.readStateCount * sizeof(TZrBool),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return ok;
}

static TZrBool semantic_da_resolve_node(SZrSemanticContext *context,
                                        const SZrSemanticDaSymbolMap *symbols,
                                        SZrAstNode *node);

static TZrBool semantic_da_resolve_function_like_body(SZrSemanticContext *context,
                                                      const SZrSemanticDaSymbolMap *symbols,
                                                      SZrAstNode *body) {
    TZrSize index;

    if (body == ZR_NULL || body->type != ZR_AST_BLOCK || body->data.block.body == ZR_NULL) {
        return ZR_TRUE;
    }

    for (index = 0; index < body->data.block.body->count; index++) {
        if (!semantic_da_resolve_node(context, symbols, body->data.block.body->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_da_resolve_node(SZrSemanticContext *context,
                                        const SZrSemanticDaSymbolMap *symbols,
                                        SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (!semantic_da_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            if (node->data.script.statements == ZR_NULL) {
                return ZR_TRUE;
            }
            for (index = 0; index < node->data.script.statements->count; index++) {
                if (!semantic_da_resolve_node(context,
                                              symbols,
                                              node->data.script.statements->nodes[index])) {
                    return ZR_FALSE;
                }
            }
            return ZR_TRUE;
        case ZR_AST_FUNCTION_DECLARATION:
            if (!semantic_da_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            return semantic_da_resolve_function_like_body(context,
                                                         symbols,
                                                         node->data.functionDeclaration.body);
        case ZR_AST_TEST_DECLARATION:
            if (!semantic_da_run_cfg_for_root(context, symbols, node)) {
                return ZR_FALSE;
            }
            return semantic_da_resolve_function_like_body(context,
                                                         symbols,
                                                         node->data.testDeclaration.body);
        case ZR_AST_BLOCK:
            return semantic_da_resolve_function_like_body(context, symbols, node);
        default:
            return ZR_TRUE;
    }
}

TZrBool ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    SZrSemanticDaSymbolMap symbols;
    TZrBool ok;

    if (context == ZR_NULL ||
        context->state == ZR_NULL ||
        root == ZR_NULL ||
        !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&symbols.symbolIds);
    if (!semantic_da_build_symbol_map(context, &symbols)) {
        semantic_da_free_symbol_map(context, &symbols);
        return ZR_FALSE;
    }

    ok = semantic_da_resolve_node(context, &symbols, root);
    semantic_da_free_symbol_map(context, &symbols);
    return ok;
}
