#include "compiler_script_callable_return.h"

#include <stdint.h>
#include <string.h>

typedef enum EZrScriptReturnProof {
    ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED = 0,
    ZR_SCRIPT_RETURN_PROOF_PROVEN,
    ZR_SCRIPT_RETURN_PROOF_FAILED
} EZrScriptReturnProof;

typedef enum EZrScriptReturnVisit {
    ZR_SCRIPT_RETURN_UNSEEN = 0,
    ZR_SCRIPT_RETURN_ACTIVE,
    ZR_SCRIPT_RETURN_FINISHED
} EZrScriptReturnVisit;

typedef struct SZrScriptReturnVisit {
    EZrScriptReturnVisit color;
    TZrUInt32 parent;
    TZrUInt32 nextEdge;
} SZrScriptReturnVisit;

static TZrBool script_return_is_int64(
        const SZrSemanticContext *context, TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(context, typeId);
    return (TZrBool)(type != ZR_NULL && type->id == typeId &&
            type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
}

/* Finite value/place/branch producers only. Effects and ownership operations
 * are outside this metadata slice even when their result happens to be i64. */
static TZrBool script_return_opcode_is_supported(EZrSemanticIrOpcode opcode) {
    switch (opcode) {
        case ZR_SEMANTIC_IR_CONSTANT:
        case ZR_SEMANTIC_IR_PLACE_BASE:
        case ZR_SEMANTIC_IR_LOAD:
        case ZR_SEMANTIC_IR_STORE:
        case ZR_SEMANTIC_IR_INITIALIZE:
        case ZR_SEMANTIC_IR_CONVERT:
        case ZR_SEMANTIC_IR_ADD:
        case ZR_SEMANTIC_IR_SUB:
        case ZR_SEMANTIC_IR_MUL:
        case ZR_SEMANTIC_IR_COMPARE:
        case ZR_SEMANTIC_IR_BRANCH:
        case ZR_SEMANTIC_IR_RETURN:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static EZrScriptReturnProof script_return_check_terminal(
        const SZrCompilerState *cs, const SZrSemanticIrInstruction *instruction,
        TZrTypeId *returnTypeId) {
    const TZrValueId *operand;
    const SZrSemanticIrValue *value;
    const SZrSemanticIrInstruction *definition;
    const SZrSemanticIrFunction *ir = &cs->preSemanticIr;
    if (instruction->opcode != ZR_SEMANTIC_IR_RETURN ||
        instruction->operandCount != 1U ||
        !script_return_is_int64(cs->semanticContext, instruction->typeId))
        return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
    if (instruction->operandStart >= ir->valueOperands.length)
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    operand = (const TZrValueId *)ZrCore_Array_Get(
            (SZrArray *)&ir->valueOperands, instruction->operandStart);
    value = operand != ZR_NULL ? ZrParser_SemanticIr_Value(ir, *operand) : ZR_NULL;
    if (value == ZR_NULL || value->typeId != instruction->typeId ||
        !script_return_is_int64(cs->semanticContext, value->typeId) ||
        value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        value->definitionInstructionId > ir->instructions.length)
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    definition = ZrParser_SemanticIr_InstructionAt(
            ir, value->definitionInstructionId - 1U);
    if (definition == ZR_NULL || definition->resultValueId != value->id ||
        definition->typeId != value->typeId || definition->id >= instruction->id)
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    if (*returnTypeId != ZR_SEMANTIC_ID_INVALID && *returnTypeId != value->typeId)
        return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
    *returnTypeId = value->typeId;
    return ZR_SCRIPT_RETURN_PROOF_PROVEN;
}

static EZrScriptReturnProof script_return_check_block(
        const SZrCompilerState *cs, const SZrParserCfgBlock *block,
        TZrTypeId *returnTypeId) {
    const SZrSemanticIrFunction *ir = &cs->preSemanticIr;
    const SZrSemanticIrInstruction *last = ZR_NULL;
    TZrSize offset;
    if (block->firstInstructionIndex > ir->instructions.length ||
        block->instructionCount > ir->instructions.length - block->firstInstructionIndex)
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    if (!block->outgoingEdges.isValid ||
        block->outgoingEdges.length != block->successorCount)
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    if (block->kind == ZR_PARSER_CFG_BLOCK_CLEANUP ||
        block->kind == ZR_PARSER_CFG_BLOCK_SUSPENSION)
        return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
    for (offset = 0U; offset < block->instructionCount; ++offset) {
        last = ZrParser_SemanticIr_InstructionAt(ir, block->firstInstructionIndex + offset);
        if (last == ZR_NULL) return ZR_SCRIPT_RETURN_PROOF_FAILED;
        if (!script_return_opcode_is_supported(last->opcode))
            return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
        if ((last->opcode == ZR_SEMANTIC_IR_RETURN || last->opcode == ZR_SEMANTIC_IR_BRANCH) &&
            offset + 1U != block->instructionCount)
            return ZR_SCRIPT_RETURN_PROOF_FAILED;
    }
    if (block->successorCount == 0U) {
        if (last == ZR_NULL || block->terminatorKind != ZR_PARSER_CFG_TERMINATOR_RETURN)
            return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
        return script_return_check_terminal(cs, last, returnTypeId);
    }
    if (last == ZR_NULL || last->opcode != ZR_SEMANTIC_IR_BRANCH ||
        block->terminatorKind != ZR_PARSER_CFG_TERMINATOR_BRANCH)
        return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
    if ((last->operandCount == 0U && block->successorCount != 1U) ||
        (last->operandCount == 1U && block->successorCount != 2U) ||
        last->operandCount > 1U)
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    return ZR_SCRIPT_RETURN_PROOF_PROVEN;
}

/* Iterative DFS uses at most one record per actual block, and visits each
 * reachable edge once. A gray target proves a cycle; black joins are shared.
 * Entry-unreachable blocks are excluded only by this traversal's proof. */
static EZrScriptReturnProof script_return_prove_cfg(
        SZrCompilerState *cs, TZrTypeId *returnTypeId) {
    const SZrParserCfg *cfg = &cs->preSemanticIr.cfg;
    SZrScriptReturnVisit *visits;
    TZrUInt32 current;
    TZrSize byteCount;
    EZrScriptReturnProof proof = ZR_SCRIPT_RETURN_PROOF_PROVEN;
    if (!cfg->blocks.isValid || cfg->blocks.length == 0U ||
        cfg->blocks.length > ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        cfg->entryBlockId >= cfg->blocks.length ||
        cfg->blocks.length > SIZE_MAX / sizeof(*visits))
        return ZR_SCRIPT_RETURN_PROOF_FAILED;
    byteCount = cfg->blocks.length * sizeof(*visits);
    visits = (SZrScriptReturnVisit *)ZrCore_Memory_RawMallocWithType(
            cs->state->global, byteCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (visits == ZR_NULL) return ZR_SCRIPT_RETURN_PROOF_FAILED;
    memset(visits, 0, byteCount);
    current = cfg->entryBlockId;
    visits[current].parent = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    while (current != ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        const SZrParserCfgBlock *block = (const SZrParserCfgBlock *)ZrCore_Array_Get(
                (SZrArray *)&cfg->blocks, current);
        const SZrParserCfgEdge *edge;
        if (block == ZR_NULL || block->id != current) {
            proof = ZR_SCRIPT_RETURN_PROOF_FAILED;
            break;
        }
        if (visits[current].color == ZR_SCRIPT_RETURN_UNSEEN) {
            proof = script_return_check_block(cs, block, returnTypeId);
            if (proof != ZR_SCRIPT_RETURN_PROOF_PROVEN) break;
            visits[current].color = ZR_SCRIPT_RETURN_ACTIVE;
        }
        if (visits[current].nextEdge == block->successorCount) {
            visits[current].color = ZR_SCRIPT_RETURN_FINISHED;
            current = visits[current].parent;
            continue;
        }
        edge = ZrParser_Cfg_BlockEdgeAt(block, visits[current].nextEdge++);
        if (edge == ZR_NULL || edge->fromBlockId != current || edge->toBlockId >= cfg->blocks.length) {
            proof = ZR_SCRIPT_RETURN_PROOF_FAILED;
            break;
        }
        if (edge->kind != ZR_PARSER_CFG_EDGE_NORMAL &&
            edge->kind != ZR_PARSER_CFG_EDGE_TRUE_BRANCH &&
            edge->kind != ZR_PARSER_CFG_EDGE_FALSE_BRANCH) {
            proof = ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
            break;
        }
        if (visits[edge->toBlockId].color == ZR_SCRIPT_RETURN_ACTIVE) {
            proof = ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
            break;
        }
        if (visits[edge->toBlockId].color == ZR_SCRIPT_RETURN_UNSEEN) {
            visits[edge->toBlockId].parent = current;
            current = edge->toBlockId;
        }
    }
    ZrCore_Memory_RawFreeWithType(cs->state->global, visits, byteCount, ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (proof == ZR_SCRIPT_RETURN_PROOF_PROVEN && *returnTypeId == ZR_SEMANTIC_ID_INVALID)
        return ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED;
    return proof;
}

TZrBool compiler_script_callable_return_publish(
        SZrCompilerState *cs, SZrFunction *function, const SZrAstNode *script) {
    EZrScriptReturnProof proof;
    TZrTypeId returnTypeId = ZR_SEMANTIC_ID_INVALID;
    SZrInferredType inferred;
    SZrFunctionTypedTypeRef metadata;
    if (cs == ZR_NULL || function == ZR_NULL || script == ZR_NULL) return ZR_FALSE;
    if (script->type != ZR_AST_SCRIPT || script != cs->scriptAst ||
        script != cs->currentAst || function != cs->currentFunction ||
        cs->topLevelFunction != ZR_NULL || cs->currentFunctionNode != ZR_NULL ||
        cs->submissionContext != ZR_NULL || cs->submissionEntryFunction != ZR_NULL ||
        function->parameterCount != 0U || function->hasVariableArguments ||
        function->staticImportLength != 0U || function->moduleEntryEffectLength != 0U ||
        function->childFunctionLength != 0U || function->closureValueLength != 0U ||
        function->typedClosureBindingLength != 0U || cs->closureVars.length != 0U ||
        function->exportedVariableLength != 0U || function->typedExportedSymbolLength != 0U ||
        !cs->preSemanticIrInitialized || !cs->preSemanticIrValidated ||
        !cs->preSemanticIrCfgActive || cs->semanticContext == ZR_NULL)
        return ZR_TRUE;
    if (cs->state == ZR_NULL || cs->state->global == ZR_NULL) return ZR_FALSE;
    proof = script_return_prove_cfg(cs, &returnTypeId);
    if (proof == ZR_SCRIPT_RETURN_PROOF_FAILED) return ZR_FALSE;
    if (proof == ZR_SCRIPT_RETURN_PROOF_UNSUPPORTED) return ZR_TRUE;
    ZrParser_InferredType_Init(cs->state, &inferred, ZR_VALUE_TYPE_INT64);
    compiler_typed_type_ref_from_inferred(&metadata, &inferred);
    ZrParser_InferredType_Free(cs->state, &inferred);
    function->callableReturnType = metadata;
    function->hasCallableReturnType = ZR_TRUE;
    return ZR_TRUE;
}
