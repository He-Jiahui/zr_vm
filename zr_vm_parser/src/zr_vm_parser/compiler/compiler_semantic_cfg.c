#include "compiler_internal.h"

/* The source compiler owns block boundaries: ExecBC jump offsets never enter
 * this graph.  The first source control boundary promotes the current
 * straight-line prefix into an entry block, and later boundaries keep their
 * own ranges. */
static TZrBool compiler_semantic_cfg_emit_branch(SZrCompilerState *cs,
                                                 TZrUInt32 target,
                                                 TZrValueId condition,
                                                 SZrFileRange range) {
    SZrSemanticIrInstructionSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_BRANCH;
    spec.targetBlockId = target;
    spec.sourceRange = range;
    if (condition != ZR_VALUE_ID_INVALID) {
        const SZrSemanticIrValue *value =
                ZrParser_SemanticIr_Value(&cs->preSemanticIr, condition);
        if (value == ZR_NULL ||
            value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
            return ZR_FALSE;
        }
        spec.typeId = value->typeId;
        spec.operands = &condition;
        spec.operandCount = 1U;
    }
    cs->preSemanticIrValidated = ZR_FALSE;
    return ZrParser_SemanticIr_Emit(&cs->preSemanticIr, &spec) !=
           ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
}

static TZrBool compiler_semantic_cfg_bind_current(
        SZrCompilerState *cs, EZrParserCfgTerminatorKind kind) {
    return ZrParser_SemanticIr_BindBlockRange(
            &cs->preSemanticIr, &cs->preSemanticIr.cfg,
            cs->preSemanticIrCfgBlock, cs->preSemanticIrCfgStart,
            (TZrUInt32)cs->preSemanticIr.instructions.length -
                    cs->preSemanticIrCfgStart, kind);
}

TZrBool compiler_semantic_cfg_ensure_active(SZrCompilerState *cs) {
    SZrParserCfg *cfg;
    TZrUInt32 entry;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgActive) {
        return ZR_TRUE;
    }
    if (cs->preSemanticIrCfgStartupSuppressed ||
        cs->preSemanticIrCfgStartupBlocked) {
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    entry = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
    if (entry == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    cfg->entryBlockId = entry;
    cs->preSemanticIrCfgBlock = entry;
    cs->preSemanticIrCfgStart = 0U;
    cs->preSemanticIrCfgActive = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_expression_is_linear(
        const SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
        case ZR_AST_IDENTIFIER_LITERAL:
            return ZR_TRUE;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return (TZrBool)(
                    compiler_semantic_cfg_expression_is_linear(
                            node->data.assignmentExpression.left) &&
                    compiler_semantic_cfg_expression_is_linear(
                            node->data.assignmentExpression.right));
        default:
            return ZR_FALSE;
    }
}

TZrBool compiler_semantic_cfg_short_circuit_is_supported(
        const SZrAstNode *node) {
    const TZrChar *op;

    if (node == ZR_NULL || node->type != ZR_AST_LOGICAL_EXPRESSION) {
        return ZR_FALSE;
    }
    op = node->data.logicalExpression.op;
    return (TZrBool)(
            op != ZR_NULL &&
            (strcmp(op, "&&") == 0 || strcmp(op, "||") == 0) &&
            compiler_semantic_cfg_expression_is_linear(
                    node->data.logicalExpression.left) &&
            compiler_semantic_cfg_expression_is_linear(
                    node->data.logicalExpression.right));
}

/* Until cleanup and suspension edges are represented here, publish only a
 * deliberately small straight-line subset. */
TZrBool compiler_semantic_cfg_arm_falls_through(const SZrAstNode *node) {
    TZrSize index;
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (node->type) {
        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL) {
                for (index = 0U; index < node->data.block.body->count; index++) {
                    if (!compiler_semantic_cfg_arm_falls_through(
                                node->data.block.body->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_IF_EXPRESSION:
            return (TZrBool)(compiler_semantic_cfg_arm_falls_through(
                                     node->data.ifExpression.thenExpr) &&
                             compiler_semantic_cfg_arm_falls_through(
                                     node->data.ifExpression.elseExpr));
        case ZR_AST_WHILE_LOOP:
            return (TZrBool)(compiler_semantic_cfg_expression_is_linear(
                                     node->data.whileLoop.cond) &&
                             compiler_semantic_cfg_arm_falls_through(
                                     node->data.whileLoop.block));
        case ZR_AST_VARIABLE_DECLARATION:
            return (TZrBool)(compiler_semantic_cfg_expression_is_linear(
                                     node->data.variableDeclaration.value) ||
                             compiler_semantic_cfg_short_circuit_is_supported(
                                     node->data.variableDeclaration.value));
        case ZR_AST_EXPRESSION_STATEMENT:
            return (TZrBool)(compiler_semantic_cfg_expression_is_linear(
                                     node->data.expressionStatement.expr) ||
                             compiler_semantic_cfg_short_circuit_is_supported(
                                     node->data.expressionStatement.expr));
        default:
            return ZR_FALSE;
    }
}

typedef enum EZrCompilerSemanticCfgArmFlow {
    ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED = 0,
    ZR_COMPILER_SEMANTIC_CFG_ARM_FALLS_THROUGH,
    ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES
} EZrCompilerSemanticCfgArmFlow;

static EZrCompilerSemanticCfgArmFlow compiler_semantic_cfg_if_arm_flow(
        const SZrAstNode *node) {
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_COMPILER_SEMANTIC_CFG_ARM_FALLS_THROUGH;
    }
    if (node->type == ZR_AST_RETURN_STATEMENT) {
        return compiler_semantic_cfg_expression_is_linear(
                       node->data.returnStatement.expr)
                       ? ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES
                       : ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED;
    }
    if (node->type == ZR_AST_THROW_STATEMENT) {
        return node->data.throwStatement.expr != ZR_NULL &&
                       compiler_semantic_cfg_expression_is_linear(
                               node->data.throwStatement.expr)
                       ? ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES
                       : ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED;
    }
    if (node->type == ZR_AST_IF_EXPRESSION) {
        EZrCompilerSemanticCfgArmFlow thenFlow;
        EZrCompilerSemanticCfgArmFlow elseFlow;

        if (!node->data.ifExpression.isStatement) {
            return ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED;
        }
        thenFlow = compiler_semantic_cfg_if_arm_flow(
                node->data.ifExpression.thenExpr);
        elseFlow = compiler_semantic_cfg_if_arm_flow(
                node->data.ifExpression.elseExpr);
        if (thenFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED ||
            elseFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED) {
            return ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED;
        }
        if (thenFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES &&
            elseFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES) {
            return ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES;
        }
        return ZR_COMPILER_SEMANTIC_CFG_ARM_FALLS_THROUGH;
    }
    if (node->type != ZR_AST_BLOCK) {
        return compiler_semantic_cfg_arm_falls_through(node)
                       ? ZR_COMPILER_SEMANTIC_CFG_ARM_FALLS_THROUGH
                       : ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED;
    }
    if (node->data.block.body == ZR_NULL) {
        return ZR_COMPILER_SEMANTIC_CFG_ARM_FALLS_THROUGH;
    }
    for (index = 0U; index < node->data.block.body->count; index++) {
        const SZrAstNode *statement = node->data.block.body->nodes[index];
        EZrCompilerSemanticCfgArmFlow flow;
        TZrSize trailingIndex;

        if (statement == ZR_NULL) {
            continue;
        }
        flow = compiler_semantic_cfg_if_arm_flow(statement);
        if (flow == ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED) {
            return flow;
        }
        if (flow != ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES) {
            continue;
        }
        for (trailingIndex = index + 1U;
             trailingIndex < node->data.block.body->count;
             trailingIndex++) {
            if (node->data.block.body->nodes[trailingIndex] != ZR_NULL) {
                return ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED;
            }
        }
        return ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES;
    }
    return ZR_COMPILER_SEMANTIC_CFG_ARM_FALLS_THROUGH;
}

static TZrUInt32 compiler_semantic_cfg_retained_before(
        const SZrSemanticIrFunction *function, TZrUInt32 end) {
    TZrUInt32 readIndex;
    TZrUInt32 retained = 0U;
    for (readIndex = 0U;
         readIndex < end && readIndex < function->instructions.length;
         readIndex++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, readIndex);
        if (instruction != ZR_NULL &&
            instruction->opcode != ZR_SEMANTIC_IR_BRANCH) {
            retained++;
        }
    }
    return retained;
}

TZrBool compiler_semantic_cfg_abandon(SZrCompilerState *cs) {
    SZrSemanticIrFunction *function;
    SZrArray instructions;
    SZrArray operands;
    SZrArray sourceMap;
    TZrSize readIndex;

    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_TRUE;
    }
    function = &cs->preSemanticIr;
    if (!cs->preSemanticIrCfgActive ||
        function->sourceMap.length != function->instructions.length) {
        return ZR_FALSE;
    }
    for (readIndex = 0U; readIndex < function->cleanupScopes.length; readIndex++) {
        SZrSemanticIrCleanupScope *scope =
                (SZrSemanticIrCleanupScope *)ZrCore_Array_Get(
                        &function->cleanupScopes, readIndex);
        TZrUInt32 oldEnd = scope->firstInstructionIndex + scope->instructionCount;
        TZrUInt32 newFirst = compiler_semantic_cfg_retained_before(
                function, scope->firstInstructionIndex);
        scope->firstInstructionIndex = newFirst;
        scope->instructionCount =
                compiler_semantic_cfg_retained_before(function, oldEnd) - newFirst;
    }
    ZrCore_Array_Init(cs->state, &instructions,
                      sizeof(SZrSemanticIrInstruction),
                      function->instructions.length == 0U
                              ? 1U : function->instructions.length);
    ZrCore_Array_Init(cs->state, &operands, sizeof(TZrValueId),
                      function->valueOperands.length == 0U
                              ? 1U : function->valueOperands.length);
    ZrCore_Array_Init(cs->state, &sourceMap,
                      sizeof(SZrSemanticIrSourceMapEntry),
                      function->sourceMap.length == 0U
                              ? 1U : function->sourceMap.length);
    for (readIndex = 0U; readIndex < function->instructions.length; readIndex++) {
        SZrSemanticIrInstruction instruction =
                *(const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                        &function->instructions, readIndex);
        SZrSemanticIrSourceMapEntry entry =
                *(const SZrSemanticIrSourceMapEntry *)ZrCore_Array_Get(
                        &function->sourceMap, readIndex);
        TZrUInt32 operandIndex;
        if (instruction.opcode == ZR_SEMANTIC_IR_BRANCH) {
            continue;
        }
        instruction.id = (TZrSemanticInstructionId)(instructions.length + 1U);
        instruction.operandStart = (TZrUInt32)operands.length;
        for (operandIndex = 0U; operandIndex < instruction.operandCount;
             operandIndex++) {
            const TZrValueId *operand = (const TZrValueId *)ZrCore_Array_Get(
                    &function->valueOperands,
                    ((const SZrSemanticIrInstruction *)ZrCore_Array_Get(
                            &function->instructions, readIndex))->operandStart +
                            operandIndex);
            ZrCore_Array_Push(cs->state, &operands, (TZrPtr)operand);
        }
        entry.instructionId = instruction.id;
        ZrCore_Array_Push(cs->state, &instructions, &instruction);
        ZrCore_Array_Push(cs->state, &sourceMap, &entry);
        if (instruction.resultValueId != ZR_VALUE_ID_INVALID) {
            SZrSemanticIrValue *value = (SZrSemanticIrValue *)ZrCore_Array_Get(
                    &function->values, instruction.resultValueId - 1U);
            if (value != ZR_NULL) {
                value->definitionInstructionId = instruction.id;
            }
        }
    }
    ZrCore_Array_Free(cs->state, &function->instructions);
    ZrCore_Array_Free(cs->state, &function->valueOperands);
    ZrCore_Array_Free(cs->state, &function->sourceMap);
    function->instructions = instructions;
    function->valueOperands = operands;
    function->sourceMap = sourceMap;
    ZrParser_Cfg_Free(cs->state, &function->cfg);
    ZrParser_Cfg_Init(cs->state, &function->cfg);
    cs->preSemanticIrCfgActive = ZR_FALSE;
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgStart = 0U;
    cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_if(SZrCompilerState *cs,
                                      TZrUInt32 conditionSlot,
                                      SZrAstNode *node,
                                      TZrUInt32 *thenBlock,
                                      TZrUInt32 *elseBlock,
                                      TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;
    TZrValueId condition;
    EZrCompilerSemanticCfgArmFlow thenFlow;
    EZrCompilerSemanticCfgArmFlow elseFlow;
    TZrBool bothTerminate;
    if (cs == ZR_NULL || node == ZR_NULL || thenBlock == ZR_NULL ||
        elseBlock == ZR_NULL || joinBlock == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_FALSE;
    }
    thenFlow = compiler_semantic_cfg_if_arm_flow(
            node->data.ifExpression.thenExpr);
    elseFlow = compiler_semantic_cfg_if_arm_flow(
            node->data.ifExpression.elseExpr);
    if (thenFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED ||
        elseFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_UNSUPPORTED) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        return ZR_FALSE;
    }
    /* Declared callable control flow is compiled in a disposable SemanticIR
     * isolation, but its returns intentionally stay on the legacy callable
     * path. Keep an internal join there so an unclosed arm never targets the
     * entry sidecar's no-continuation sentinel. */
    bothTerminate = (TZrBool)(cs->currentFunctionNode == ZR_NULL &&
            thenFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES &&
            elseFlow == ZR_COMPILER_SEMANTIC_CFG_ARM_TERMINATES);
    condition = compiler_semantic_ir_slot_value(cs, conditionSlot);
    if (condition == ZR_VALUE_ID_INVALID) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    *thenBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.ifExpression.thenExpr);
    *elseBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.ifExpression.elseExpr);
    *joinBlock = bothTerminate
            ? ZR_PARSER_CFG_INVALID_BLOCK_ID
            : ZrParser_Cfg_AppendBlock(
                    cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    if (*thenBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *elseBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (!bothTerminate &&
         *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        !compiler_semantic_cfg_emit_branch(cs, *thenBlock,
                                           condition, node->location) ||
        !compiler_semantic_cfg_bind_current(cs, ZR_PARSER_CFG_TERMINATOR_BRANCH) ||
        !ZrParser_Cfg_Connect(cfg, cs->preSemanticIrCfgBlock,
                              *thenBlock, ZR_PARSER_CFG_EDGE_TRUE_BRANCH, node) ||
        !ZrParser_Cfg_Connect(cfg, cs->preSemanticIrCfgBlock,
                              *elseBlock, ZR_PARSER_CFG_EDGE_FALSE_BRANCH, node)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, *thenBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_while(SZrCompilerState *cs,
                                          SZrAstNode *node,
                                          TZrUInt32 *conditionBlock,
                                          TZrUInt32 *bodyBlock,
                                          TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;
    if (cs == ZR_NULL || node == ZR_NULL || conditionBlock == ZR_NULL ||
        bodyBlock == ZR_NULL || joinBlock == ZR_NULL ||
        node->type != ZR_AST_WHILE_LOOP) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_expression_is_linear(
                node->data.whileLoop.cond) ||
        !compiler_semantic_cfg_loop_body_analyze(
                node->data.whileLoop.block, ZR_TRUE, ZR_TRUE,
                ZR_NULL)) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    *conditionBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.whileLoop.cond);
    *bodyBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.whileLoop.block);
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    if (*conditionBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *bodyBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_jump(cs, *conditionBlock, node->location)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, *conditionBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_for(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        TZrUInt32 *conditionBlock,
                                        TZrUInt32 *bodyBlock,
                                        TZrUInt32 *stepBlock,
                                        TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;
    const SZrForLoop *loop;
    TZrBool bodyEndsWithBreak = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || conditionBlock == ZR_NULL ||
        bodyBlock == ZR_NULL || stepBlock == ZR_NULL ||
        joinBlock == ZR_NULL || node->type != ZR_AST_FOR_LOOP) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_for_is_supported(
                node, &bodyEndsWithBreak)) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        return ZR_FALSE;
    }
    loop = &node->data.forLoop;
    cfg = &cs->preSemanticIr.cfg;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    *conditionBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, loop->cond);
    *bodyBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, loop->block);
    *stepBlock = bodyEndsWithBreak
                         ? ZR_PARSER_CFG_INVALID_BLOCK_ID
                         : ZrParser_Cfg_AppendBlock(
                                   cs->state, cfg,
                                   ZR_PARSER_CFG_BLOCK_STATEMENT,
                                   loop->step);
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg,
            loop->cond == ZR_NULL && !bodyEndsWithBreak
                    ? ZR_PARSER_CFG_BLOCK_EXIT
                    : ZR_PARSER_CFG_BLOCK_JOIN,
            node);
    if (*conditionBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *bodyBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (!bodyEndsWithBreak &&
         *stepBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_jump(cs, *conditionBlock, node->location)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, *conditionBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_short_circuit(
        SZrCompilerState *cs,
        TZrUInt32 conditionSlot,
        SZrAstNode *node,
        TZrUInt32 *rightBlock,
        TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;
    TZrUInt32 trueBlock;
    TZrUInt32 falseBlock;
    TZrValueId condition;
    TZrBool isAnd;

    if (cs == ZR_NULL || node == ZR_NULL || rightBlock == ZR_NULL ||
        joinBlock == ZR_NULL || node->type != ZR_AST_LOGICAL_EXPRESSION) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_short_circuit_is_supported(node)) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        return ZR_FALSE;
    }
    condition = compiler_semantic_ir_slot_value(cs, conditionSlot);
    if (condition == ZR_VALUE_ID_INVALID) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    *rightBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.logicalExpression.right);
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    isAnd = (TZrBool)(strcmp(node->data.logicalExpression.op, "&&") == 0);
    trueBlock = isAnd ? *rightBlock : *joinBlock;
    falseBlock = isAnd ? *joinBlock : *rightBlock;
    if (*rightBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_emit_branch(
                cs, trueBlock, condition, node->location) ||
        !compiler_semantic_cfg_bind_current(
                cs, ZR_PARSER_CFG_TERMINATOR_BRANCH) ||
        !ZrParser_Cfg_Connect(
                cfg, cs->preSemanticIrCfgBlock, trueBlock,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH, node) ||
        !ZrParser_Cfg_Connect(
                cfg, cs->preSemanticIrCfgBlock, falseBlock,
                ZR_PARSER_CFG_EDGE_FALSE_BRANCH, node)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, *rightBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_optional_guard(
        SZrCompilerState *cs,
        TZrUInt32 receiverSlot,
        SZrAstNode *node,
        TZrUInt32 *presentBlock,
        TZrUInt32 *absentBlock,
        TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;
    TZrUInt32 falseBlock;
    TZrValueId receiver;

    if (cs == ZR_NULL || node == ZR_NULL || presentBlock == ZR_NULL ||
        joinBlock == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated) {
        return ZR_FALSE;
    }
    receiver = compiler_semantic_ir_slot_value(cs, receiverSlot);
    if (receiver == ZR_VALUE_ID_INVALID) {
        if (cs->preSemanticIrCfgActive && !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        return ZR_FALSE;
    }
    cfg = &cs->preSemanticIr.cfg;
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    *presentBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, node);
    if (absentBlock != ZR_NULL) {
        *absentBlock = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, node);
    }
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    falseBlock = absentBlock != ZR_NULL ? *absentBlock : *joinBlock;
    if (*presentBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (absentBlock != ZR_NULL &&
         *absentBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_emit_branch(
                cs, *presentBlock, receiver, node->location) ||
        !compiler_semantic_cfg_bind_current(
                cs, ZR_PARSER_CFG_TERMINATOR_BRANCH) ||
        !ZrParser_Cfg_Connect(
                cfg, cs->preSemanticIrCfgBlock, *presentBlock,
                ZR_PARSER_CFG_EDGE_TRUE_BRANCH, node) ||
        !ZrParser_Cfg_Connect(
                cfg, cs->preSemanticIrCfgBlock, falseBlock,
                ZR_PARSER_CFG_EDGE_FALSE_BRANCH, node)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, *presentBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_branch_while(SZrCompilerState *cs,
                                           TZrUInt32 conditionSlot,
                                           SZrAstNode *node,
                                           TZrUInt32 bodyBlock,
                                           TZrUInt32 joinBlock) {
    TZrValueId condition;
    if (cs == ZR_NULL || node == ZR_NULL ||
        !cs->preSemanticIrCfgActive) {
        return ZR_FALSE;
    }
    condition = compiler_semantic_ir_slot_value(cs, conditionSlot);
    if (condition == ZR_VALUE_ID_INVALID ||
        !compiler_semantic_cfg_emit_branch(
                cs, bodyBlock, condition, node->location) ||
        !compiler_semantic_cfg_bind_current(
                cs, ZR_PARSER_CFG_TERMINATOR_BRANCH) ||
        !ZrParser_Cfg_Connect(&cs->preSemanticIr.cfg,
                              cs->preSemanticIrCfgBlock, bodyBlock,
                              ZR_PARSER_CFG_EDGE_TRUE_BRANCH, node) ||
        !ZrParser_Cfg_Connect(&cs->preSemanticIr.cfg,
                              cs->preSemanticIrCfgBlock, joinBlock,
                              ZR_PARSER_CFG_EDGE_FALSE_BRANCH, node)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, bodyBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_branch_for(SZrCompilerState *cs,
                                         TZrUInt32 conditionSlot,
                                         SZrAstNode *node,
                                         TZrUInt32 bodyBlock,
                                         TZrUInt32 joinBlock) {
    if (node == ZR_NULL || node->type != ZR_AST_FOR_LOOP) {
        return ZR_FALSE;
    }
    return compiler_semantic_cfg_branch_while(
            cs, conditionSlot, node, bodyBlock, joinBlock);
}

TZrBool compiler_semantic_cfg_jump(SZrCompilerState *cs,
                                  TZrUInt32 target,
                                  SZrFileRange range) {
    if (cs == ZR_NULL || !cs->preSemanticIrCfgActive ||
        !compiler_semantic_cfg_emit_branch(cs, target, ZR_VALUE_ID_INVALID, range) ||
        !compiler_semantic_cfg_bind_current(cs, ZR_PARSER_CFG_TERMINATOR_BRANCH)) {
        return ZR_FALSE;
    }
    return ZrParser_Cfg_Connect(&cs->preSemanticIr.cfg,
                                cs->preSemanticIrCfgBlock, target,
                                ZR_PARSER_CFG_EDGE_NORMAL, ZR_NULL);
}

TZrBool compiler_semantic_cfg_jump_abrupt(SZrCompilerState *cs,
                                          TZrUInt32 target,
                                          SZrFileRange range) {
    if (!compiler_semantic_cfg_jump(cs, target, range)) {
        return ZR_FALSE;
    }
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgStart =
            (TZrUInt32)cs->preSemanticIr.instructions.length;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_invoke(
        SZrCompilerState *cs,
        SZrAstNode *callNode,
        SZrFileRange range) {
    TZrUInt32 invokeBlock;

    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }
    invokeBlock = ZrParser_Cfg_AppendBlock(
            cs->state,
            &cs->preSemanticIr.cfg,
            ZR_PARSER_CFG_BLOCK_STATEMENT,
            callNode);
    if (invokeBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_jump(cs, invokeBlock, range)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, invokeBlock);
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_split_invoke(
        SZrCompilerState *cs,
        SZrAstNode *callNode,
        SZrFileRange range) {
    SZrParserCfg *cfg;
    TZrUInt32 invokeBlock;
    TZrUInt32 normalBlock;
    TZrUInt32 exceptionBlock;

    if (cs == ZR_NULL || !cs->preSemanticIrCfgActive ||
        cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        cs->preSemanticIr.instructions.length == 0U) {
        return ZR_FALSE;
    }
    ZR_UNUSED_PARAMETER(range);
    invokeBlock = cs->preSemanticIrCfgBlock;
    cfg = &cs->preSemanticIr.cfg;
    normalBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, callNode);
    exceptionBlock = cs->preSemanticIrCfgCatchBlock !=
                             ZR_PARSER_CFG_INVALID_BLOCK_ID
            ? cs->preSemanticIrCfgCatchBlock
            : ZrParser_Cfg_AppendBlock(
                    cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT, callNode);
    if (normalBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        exceptionBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_bind_current(
                cs, ZR_PARSER_CFG_TERMINATOR_NONE) ||
        !ZrParser_Cfg_Connect(
                cfg, invokeBlock, normalBlock,
                ZR_PARSER_CFG_EDGE_NORMAL, callNode) ||
        !ZrParser_Cfg_Connect(
                cfg, invokeBlock, exceptionBlock,
                ZR_PARSER_CFG_EDGE_EXCEPTION, callNode)) {
        return ZR_FALSE;
    }

    if (exceptionBlock == cs->preSemanticIrCfgCatchBlock) {
        cs->preSemanticIrCfgCatchUsed = ZR_TRUE;
        compiler_semantic_cfg_enter(cs, normalBlock);
        return ZR_TRUE;
    }
    compiler_semantic_cfg_enter(cs, exceptionBlock);
    /* Calls outside a modeled catch still propagate through an explicit
     * exceptional sink. Handled calls are redirected above. */
    if (!compiler_semantic_cfg_bind_current(
                cs, ZR_PARSER_CFG_TERMINATOR_THROW)) {
        return ZR_FALSE;
    }
    compiler_semantic_cfg_enter(cs, normalBlock);
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_terminate_value(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range,
        EZrSemanticIrOpcode opcode,
        EZrParserCfgTerminatorKind terminatorKind) {
    SZrSemanticIrInstructionSpec spec;
    const SZrSemanticIrValue *value;
    TZrValueId valueId;

    if (cs == ZR_NULL || !cs->preSemanticIrInitialized) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgTerminated ||
        cs->preSemanticIrCfgStartupBlocked) {
        return ZR_TRUE;
    }
    if (cs->preSemanticIrCfgStartupSuppressed) {
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        return ZR_TRUE;
    }
    valueId = compiler_semantic_ir_slot_value(cs, valueSlot);
    value = ZrParser_SemanticIr_Value(&cs->preSemanticIr, valueId);
    if (value == ZR_NULL) {
        if (cs->preSemanticIrCfgActive &&
            !compiler_semantic_cfg_abandon(cs)) {
            return ZR_FALSE;
        }
        cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
        return ZR_TRUE;
    }
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        return ZR_FALSE;
    }

    memset(&spec, 0, sizeof(spec));
    spec.opcode = opcode;
    spec.typeId = value->typeId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.operands = &valueId;
    spec.operandCount = 1U;
    spec.sourceRange = range;
    if (!compiler_semantic_ir_emit(cs, &spec) ||
        !compiler_semantic_cfg_bind_current(cs, terminatorKind)) {
        return ZR_FALSE;
    }

    cs->preSemanticIr.cfg.exitBlockId = cs->preSemanticIrCfgBlock;
    if (!cs->preSemanticIrCfgAbruptIsLocal) {
        cs->preSemanticIrCfgTerminated = ZR_TRUE;
    }
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgStart =
            (TZrUInt32)cs->preSemanticIr.instructions.length;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_terminate_throw(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range) {
    return compiler_semantic_cfg_terminate_value(
            cs, valueSlot, range,
            ZR_SEMANTIC_IR_THROW,
            ZR_PARSER_CFG_TERMINATOR_THROW);
}

TZrBool compiler_semantic_cfg_terminate_return(
        SZrCompilerState *cs,
        TZrUInt32 valueSlot,
        SZrFileRange range) {
    return compiler_semantic_cfg_terminate_value(
            cs, valueSlot, range,
            ZR_SEMANTIC_IR_RETURN,
            ZR_PARSER_CFG_TERMINATOR_RETURN);
}

void compiler_semantic_cfg_enter(SZrCompilerState *cs, TZrUInt32 block) {
    cs->preSemanticIrCfgBlock = block;
    cs->preSemanticIrCfgStart = (TZrUInt32)cs->preSemanticIr.instructions.length;
}

TZrBool compiler_semantic_cfg_capture_slots(SZrCompilerState *cs,
                                            SZrArray *snapshot) {
    const SZrArray *slots;
    if (cs == ZR_NULL || snapshot == ZR_NULL) {
        return ZR_FALSE;
    }
    slots = &cs->preSemanticIrSlots;
    if (!slots->isValid || slots->head == ZR_NULL ||
        slots->elementSize == 0U || slots->length > slots->capacity) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(cs->state, snapshot, slots->elementSize,
                      slots->length == 0U ? 1U : slots->length);
    if (slots->length != 0U) {
        ZrCore_Array_Append(cs->state, snapshot, slots->head, slots->length);
    }
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_restore_slots(SZrCompilerState *cs,
                                            const SZrArray *snapshot) {
    SZrArray *slots;
    if (cs == ZR_NULL || snapshot == ZR_NULL || !snapshot->isValid ||
        snapshot->head == ZR_NULL || snapshot->elementSize == 0U ||
        snapshot->length > snapshot->capacity) {
        return ZR_FALSE;
    }
    slots = &cs->preSemanticIrSlots;
    if (!slots->isValid || slots->head == ZR_NULL ||
        slots->elementSize != snapshot->elementSize ||
        slots->capacity < snapshot->length) {
        return ZR_FALSE;
    }
    if (snapshot->length != 0U) {
        ZrCore_Memory_RawCopy(slots->head, snapshot->head,
                              snapshot->length * snapshot->elementSize);
    }
    slots->length = snapshot->length;
    return ZR_TRUE;
}

void compiler_semantic_cfg_free_slots(SZrCompilerState *cs,
                                      SZrArray *snapshot) {
    if (cs != ZR_NULL && snapshot != ZR_NULL) {
        ZrCore_Array_Free(cs->state, snapshot);
    }
}

TZrBool compiler_semantic_cfg_finish(SZrCompilerState *cs) {
    SZrSemanticIrInstructionSpec spec;
    SZrParserCfg *cfg;
    TZrUInt32 exitBlock;
    if (cs == ZR_NULL || !cs->preSemanticIrCfgActive) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_TRUE;
    }
    cfg = &cs->preSemanticIr.cfg;
    exitBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_EXIT, ZR_NULL);
    if (exitBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        !compiler_semantic_cfg_jump(cs, exitBlock, (SZrFileRange){0})) {
        return ZR_FALSE;
    }
    cfg->exitBlockId = exitBlock;
    compiler_semantic_cfg_enter(cs, exitBlock);
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_RETURN;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    if (ZrParser_SemanticIr_Emit(&cs->preSemanticIr, &spec) ==
                ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        !compiler_semantic_cfg_bind_current(cs, ZR_PARSER_CFG_TERMINATOR_RETURN)) {
        return ZR_FALSE;
    }
    cs->preSemanticIrCfgBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    return ZR_TRUE;
}
