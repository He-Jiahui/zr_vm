#include "compiler_internal.h"

/* The source compiler owns block boundaries: ExecBC jump offsets never enter
 * this graph.  The first branch promotes the current straight-line prefix
 * into an entry block, and subsequent branches keep their own ranges. */
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

static TZrBool compiler_semantic_cfg_expression_is_linear(
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

/* Until return, calls, cleanup, suspension, short-circuit and loop edges are
 * represented here, publish only a deliberately small straight-line subset. */
static TZrBool compiler_semantic_cfg_arm_falls_through(const SZrAstNode *node) {
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
        case ZR_AST_VARIABLE_DECLARATION:
            return compiler_semantic_cfg_expression_is_linear(
                    node->data.variableDeclaration.value);
        case ZR_AST_EXPRESSION_STATEMENT:
            return compiler_semantic_cfg_expression_is_linear(
                    node->data.expressionStatement.expr);
        default:
            return ZR_FALSE;
    }
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

static TZrBool compiler_semantic_cfg_abandon(SZrCompilerState *cs) {
    SZrSemanticIrFunction *function = &cs->preSemanticIr;
    SZrArray instructions;
    SZrArray operands;
    SZrArray sourceMap;
    TZrSize readIndex;

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
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_begin_if(SZrCompilerState *cs,
                                      TZrUInt32 conditionSlot,
                                      SZrAstNode *node,
                                      TZrUInt32 *thenBlock,
                                      TZrUInt32 *elseBlock,
                                      TZrUInt32 *joinBlock) {
    SZrParserCfg *cfg;
    TZrUInt32 entry;
    TZrValueId condition;
    if (cs == ZR_NULL || node == ZR_NULL || thenBlock == ZR_NULL ||
        elseBlock == ZR_NULL || joinBlock == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!compiler_semantic_cfg_arm_falls_through(
                node->data.ifExpression.thenExpr) ||
        !compiler_semantic_cfg_arm_falls_through(
                node->data.ifExpression.elseExpr)) {
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
    if (!cs->preSemanticIrCfgActive) {
        entry = ZrParser_Cfg_AppendBlock(
                cs->state, cfg, ZR_PARSER_CFG_BLOCK_ENTRY, ZR_NULL);
        if (entry == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            return ZR_FALSE;
        }
        cfg->entryBlockId = entry;
        cs->preSemanticIrCfgBlock = entry;
        cs->preSemanticIrCfgStart = 0U;
        cs->preSemanticIrCfgActive = ZR_TRUE;
    }
    *thenBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.ifExpression.thenExpr);
    *elseBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
            node->data.ifExpression.elseExpr);
    *joinBlock = ZrParser_Cfg_AppendBlock(
            cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    if (*thenBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *elseBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        *joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
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
