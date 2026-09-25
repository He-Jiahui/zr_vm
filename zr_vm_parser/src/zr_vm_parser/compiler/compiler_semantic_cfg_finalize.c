#include "compiler_internal.h"
#include <stdlib.h>

/* Starting a graph at the end of a source body must not turn missing
 * SemanticIR producers into executable external/default values. This is a
 * conservative capability preflight for previously inactive entry bodies,
 * not an AST-to-CFG reconstruction. Existing active CFG producers retain
 * their own control-flow preflights. */
typedef struct SZrSemanticCfgSourceWorklist {
    const SZrAstNode **nodes;
    TZrBool *initializedPlaces;
    size_t length;
    size_t capacity;
    TZrSize scannedInstruction;
    TZrSize nextWriteInstruction;
    TZrBool failed;
} SZrSemanticCfgSourceWorklist;

static void compiler_semantic_cfg_queue_node(
        SZrCompilerState *cs, SZrSemanticCfgSourceWorklist *pending,
        const SZrAstNode *node) {
    size_t capacity;
    const SZrAstNode **nodes;
    (void)cs;
    if (node == ZR_NULL || pending->failed) return;
    if (pending->length == pending->capacity) {
        capacity = pending->capacity == 0U ? ZR_PARSER_INITIAL_CAPACITY_SMALL
                                          : pending->capacity * 2U;
        if (capacity < pending->capacity || capacity > SIZE_MAX / sizeof(*nodes)) {
            pending->failed = ZR_TRUE;
            return;
        }
        nodes = (const SZrAstNode **)realloc(pending->nodes, capacity * sizeof(*nodes));
        if (nodes == ZR_NULL) {
            pending->failed = ZR_TRUE;
            return;
        }
        pending->nodes = nodes;
        pending->capacity = capacity;
    }
    pending->nodes[pending->length++] = node;
}

static TZrBool compiler_semantic_cfg_empty_nodes(const SZrAstNodeArray *nodes) {
    return (TZrBool)(nodes == ZR_NULL || nodes->count == 0U);
}

static TZrBool compiler_semantic_cfg_same_source(
        const SZrSemanticIrInstruction *instruction, const SZrAstNode *node) {
    return (TZrBool)(instruction->sourceRange.source == node->location.source &&
            instruction->sourceRange.start.offset == node->location.start.offset &&
            instruction->sourceRange.end.offset == node->location.end.offset);
}

static TZrBool compiler_semantic_cfg_has_source_store(
        const SZrCompilerState *cs, const SZrAstNode *node,
        TZrSize *nextWriteInstruction) {
    TZrSize index;
    for (index = *nextWriteInstruction;
         index < cs->preSemanticIr.instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        if (instruction != ZR_NULL && instruction->opcode == ZR_SEMANTIC_IR_STORE &&
            compiler_semantic_cfg_same_source(instruction, node)) {
            *nextWriteInstruction = index + 1U;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_semantic_cfg_has_complete_value_types(
        const SZrCompilerState *cs) {
    TZrSize index;
    for (index = 0U; index < cs->preSemanticIr.instructions.length; ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        const SZrSemanticIrValue *input;
        if (instruction == ZR_NULL ||
            (instruction->opcode != ZR_SEMANTIC_IR_CONVERT &&
             instruction->opcode != ZR_SEMANTIC_IR_STORE)) continue;
        input = ZrParser_SemanticIr_Value(&cs->preSemanticIr, instruction->valueId);
        /* Cross-type CONVERT is not executable yet; an implicit assignment
         * conversion only appears in ExecBC, leaving STORE's input unchanged. */
        if (input == ZR_NULL || input->typeId != instruction->typeId) return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_place_initialized(
        const SZrCompilerState *cs, SZrSemanticCfgSourceWorklist *pending,
        TZrSize beforeInstruction, TZrPlaceId placeId) {
    TZrSize placeCount = cs->preSemanticIr.places.places.length;
    if (placeId == ZR_PLACE_ID_INVALID || placeId > placeCount ||
        placeCount > SIZE_MAX / sizeof(*pending->initializedPlaces)) return ZR_FALSE;
    if (pending->initializedPlaces == ZR_NULL) {
        pending->initializedPlaces = (TZrBool *)realloc(
                ZR_NULL, placeCount * sizeof(*pending->initializedPlaces));
        if (pending->initializedPlaces == ZR_NULL) {
            pending->failed = ZR_TRUE;
            return ZR_FALSE;
        }
        memset(pending->initializedPlaces, 0,
               placeCount * sizeof(*pending->initializedPlaces));
    }
    for (; pending->scannedInstruction < beforeInstruction;
         ++pending->scannedInstruction) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(
                        &cs->preSemanticIr, pending->scannedInstruction);
        if (instruction != ZR_NULL &&
            instruction->placeId != ZR_PLACE_ID_INVALID &&
            instruction->placeId <= placeCount &&
            (instruction->opcode == ZR_SEMANTIC_IR_INITIALIZE ||
             instruction->opcode == ZR_SEMANTIC_IR_STORE))
            pending->initializedPlaces[instruction->placeId - 1U] = ZR_TRUE;
    }
    return pending->initializedPlaces[placeId - 1U];
}

static TZrBool compiler_semantic_cfg_has_source_load(
        const SZrCompilerState *cs, const SZrAstNode *node,
        TZrSize *nextInstruction, SZrSemanticCfgSourceWorklist *pending) {
    TZrSize index;
    for (index = *nextInstruction; index < cs->preSemanticIr.instructions.length;
         ++index) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(&cs->preSemanticIr, index);
        if (instruction != ZR_NULL && instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
            compiler_semantic_cfg_same_source(instruction, node)) {
            *nextInstruction = index + 1U;
            /* A closure declaration can provide a local LOAD while writing
             * only legacy ExecBC. Do not treat its place as initialized. */
            return compiler_semantic_cfg_place_initialized(
                    cs, pending, index, instruction->placeId);
        }
    }
    return ZR_FALSE;
}

static TZrBool compiler_semantic_cfg_straight_line_is_supported(
        SZrCompilerState *cs, TZrBool *outSupported) {
    SZrSemanticCfgSourceWorklist pending = {0};
    TZrSize nextReadInstruction = 0U;
    TZrBool supported = ZR_TRUE;
    *outSupported = ZR_FALSE;
    if (cs->currentAst == ZR_NULL || cs->currentAst->type != ZR_AST_SCRIPT ||
        cs->currentFunctionNode != ZR_NULL) {
        return ZR_TRUE;
    }
    if (!compiler_semantic_cfg_has_complete_value_types(cs)) return ZR_TRUE;
    /* An explicit worklist avoids introducing another recursive walk over
     * user-controlled expression depth. No ExecBC instruction is consulted. */
    compiler_semantic_cfg_queue_node(cs, &pending, cs->currentAst);
    while (supported && !pending.failed && pending.length != 0U) {
        const SZrAstNode *node = pending.nodes[--pending.length];
        switch (node->type) {
            case ZR_AST_SCRIPT: {
                const SZrAstNodeArray *statements = node->data.script.statements;
                TZrSize index;
                if (statements != ZR_NULL) {
                    for (index = statements->count; index > 0U; --index)
                        compiler_semantic_cfg_queue_node(
                                cs, &pending, statements->nodes[index - 1U]);
                }
                break;
            }
            case ZR_AST_VARIABLE_DECLARATION:
                supported = (TZrBool)(node->data.variableDeclaration.pattern != ZR_NULL &&
                        node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
                        node->data.variableDeclaration.value != ZR_NULL);
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.variableDeclaration.value);
                break;
            case ZR_AST_EXPRESSION_STATEMENT:
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.expressionStatement.expr);
                break;
            case ZR_AST_BOOLEAN_LITERAL:
            case ZR_AST_INTEGER_LITERAL:
            case ZR_AST_FLOAT_LITERAL:
            case ZR_AST_STRING_LITERAL:
            case ZR_AST_CHAR_LITERAL:
            case ZR_AST_NULL_LITERAL:
                break;
            case ZR_AST_IDENTIFIER_LITERAL:
                /* Only a source read with a real local LOAD is complete.
                 * Global, closure, child-function and type identifier reads
                 * currently emit legacy bytecode without a SemanticIR load. */
                supported = compiler_semantic_cfg_has_source_load(
                        cs, node, &nextReadInstruction, &pending);
                break;
            case ZR_AST_PRIMARY_EXPRESSION:
                supported = compiler_semantic_cfg_empty_nodes(
                        node->data.primaryExpression.members);
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.primaryExpression.property);
                break;
            case ZR_AST_ASSIGNMENT_EXPRESSION:
                supported = (TZrBool)(node->data.assignmentExpression.op.op != ZR_NULL &&
                        strcmp(node->data.assignmentExpression.op.op, "=") == 0 &&
                        node->data.assignmentExpression.left != ZR_NULL &&
                        node->data.assignmentExpression.left->type == ZR_AST_IDENTIFIER_LITERAL &&
                        compiler_semantic_cfg_has_source_store(
                                cs, node->data.assignmentExpression.left,
                                &pending.nextWriteInstruction));
                compiler_semantic_cfg_queue_node(
                        cs, &pending, node->data.assignmentExpression.right);
                break;
            case ZR_AST_CONSTRUCT_EXPRESSION:
                /* An ownership wrapper over an existing local has a producer;
                 * new/resource construction first emits an instance and may
                 * invoke an initializer only in the legacy instruction path. */
                supported = (TZrBool)(
                        node->data.constructExpression.builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_NONE &&
                        !node->data.constructExpression.isNew &&
                        !node->data.constructExpression.isResourceSurface &&
                        compiler_semantic_cfg_empty_nodes(node->data.constructExpression.args) &&
                        node->data.constructExpression.target != ZR_NULL &&
                        node->data.constructExpression.target->type == ZR_AST_IDENTIFIER_LITERAL);
                break;
            case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
                /* The intrinsic consumes its canonical identifier place;
                 * it does not compile the argument as an expression read. */
                supported = (TZrBool)(
                        node->data.ownershipIntrinsicExpression.argument != ZR_NULL &&
                        node->data.ownershipIntrinsicExpression.argument->type ==
                                ZR_AST_IDENTIFIER_LITERAL);
                break;
            case ZR_AST_FUNCTION_DECLARATION:
                /* The child body is isolated, but a named declaration still
                 * emits CREATE_CLOSURE and SET_STACK in the parent ExecBC. */
                supported = ZR_FALSE;
                break;
            case ZR_AST_CLASS_DECLARATION:
                /* Empty type declarations have no entry runtime initializer.
                 * Never silently skip static fields, decorators or members. */
                supported = (TZrBool)(
                        node->data.classDeclaration.generic == ZR_NULL &&
                        compiler_semantic_cfg_empty_nodes(node->data.classDeclaration.members) &&
                        compiler_semantic_cfg_empty_nodes(node->data.classDeclaration.inherits) &&
                        compiler_semantic_cfg_empty_nodes(node->data.classDeclaration.decorators));
                break;
            default:
                supported = ZR_FALSE;
                break;
        }
    }
    free(pending.nodes);
    free(pending.initializedPlaces);
    *outSupported = (TZrBool)(supported && !pending.failed);
    return (TZrBool)!pending.failed;
}

/* This fallback is deliberately analysis-only. Its synthetic RETURN edge is
 * not an executable terminator; the strict ExecIR builder must reject it. */
static TZrBool compiler_semantic_cfg_build_analysis_graph(SZrCompilerState *cs) {
    SZrSemanticIrFunction *function = &cs->preSemanticIr;
    TZrUInt32 entryBlock, exitBlock;
    ZrParser_Cfg_Free(cs->state, &function->cfg);
    ZrParser_Cfg_Init(cs->state, &function->cfg);
    entryBlock = ZrParser_Cfg_AppendBlock(
            cs->state, &function->cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
    exitBlock = ZrParser_Cfg_AppendBlock(
            cs->state, &function->cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL);
    if (entryBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        exitBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    function->cfg.entryBlockId = entryBlock;
    function->cfg.exitBlockId = exitBlock;
    return (TZrBool)(ZrParser_Cfg_Connect(
                &function->cfg, entryBlock, exitBlock,
                ZR_PARSER_CFG_EDGE_RETURN, ZR_NULL) &&
            ZrParser_SemanticIr_BindBlockRange(
                function, &function->cfg, entryBlock, 0U,
                (TZrUInt32)function->instructions.length,
                ZR_PARSER_CFG_TERMINATOR_RETURN) &&
            ZrParser_SemanticIr_BindBlockRange(
                function, &function->cfg, exitBlock,
                (TZrUInt32)function->instructions.length, 0U,
                ZR_PARSER_CFG_TERMINATOR_EXIT));
}

static TZrBool compiler_semantic_cfg_promote_straight_line(SZrCompilerState *cs) {
    SZrSemanticIrFunction *function = &cs->preSemanticIr;
    SZrParserCfg previousCfg = function->cfg;
    TZrSize previousInstructions = function->instructions.length;
    TZrSize previousSourceMap = function->sourceMap.length;
    TZrSize previousOperands = function->valueOperands.length;
    TZrUInt32 previousBlock = cs->preSemanticIrCfgBlock;
    TZrUInt32 previousStart = cs->preSemanticIrCfgStart;
    TZrBool previousValidated = cs->preSemanticIrValidated;

    ZrParser_Cfg_Init(cs->state, &function->cfg);
    if (!compiler_semantic_cfg_ensure_active(cs) ||
        !compiler_semantic_cfg_finish(cs)) {
        ZrParser_Cfg_Free(cs->state, &function->cfg);
        function->cfg = previousCfg;
        function->instructions.length = previousInstructions;
        function->sourceMap.length = previousSourceMap;
        function->valueOperands.length = previousOperands;
        cs->preSemanticIrCfgBlock = previousBlock;
        cs->preSemanticIrCfgStart = previousStart;
        cs->preSemanticIrCfgActive = ZR_FALSE;
        cs->preSemanticIrValidated = previousValidated;
        return ZR_FALSE;
    }
    ZrParser_Cfg_Free(cs->state, &previousCfg);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_finalize(SZrCompilerState *cs) {
    TZrBool supported = ZR_FALSE;
    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) return ZR_FALSE;
    if (cs->preSemanticIrCfgActive) return compiler_semantic_cfg_finish(cs);
    if (!cs->preSemanticIrCfgStartupBlocked &&
        !cs->preSemanticIrCfgStartupSuppressed &&
        !cs->preSemanticIrCfgTerminated) {
        if (!compiler_semantic_cfg_straight_line_is_supported(cs, &supported))
            return ZR_FALSE;
    }
    if (supported) {
        return compiler_semantic_cfg_promote_straight_line(cs);
    }
    return compiler_semantic_cfg_build_analysis_graph(cs);
}
