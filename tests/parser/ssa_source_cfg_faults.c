#include "ssa_source_cfg_faults.h"

#include <stdlib.h>

#include "zr_vm_parser/cfg.h"
#include "zr_vm_parser/semantic_ir.h"

extern TZrBool compiler_semantic_cfg_ensure_active(SZrCompilerState *compiler);
extern TZrBool compiler_semantic_cfg_finish(SZrCompilerState *compiler);
extern TZrBool ssa_test_compiler_semantic_cfg_finish(
        SZrCompilerState *compiler);

static size_t failureOrdinal, allocationOrdinal, outstanding;
static TZrBool failed;
static ESsaSourceCfgPromotionFault promotionFault;
static ESsaSourceCfgFinishFault finishFault;
static TZrBool finishFaultEnabled, finishFaultTriggered;

static TZrBool (*source_cfg_real_ensure_active)(SZrCompilerState *) =
        compiler_semantic_cfg_ensure_active;
static TZrBool (*source_cfg_real_finish)(SZrCompilerState *) =
        compiler_semantic_cfg_finish;
static TZrUInt32 (*source_cfg_real_append_block)(
        SZrState *, SZrParserCfg *, EZrParserCfgBlockKind, SZrAstNode *) =
        ZrParser_Cfg_AppendBlock;
static TZrBool (*source_cfg_real_connect)(
        SZrParserCfg *, TZrUInt32, TZrUInt32, EZrParserCfgEdgeKind,
        SZrAstNode *) = ZrParser_Cfg_Connect;
static TZrSemanticInstructionId (*source_cfg_real_emit)(
        SZrSemanticIrFunction *, const SZrSemanticIrInstructionSpec *) =
        ZrParser_SemanticIr_Emit;
static TZrBool (*source_cfg_real_bind_block_range)(
        const SZrSemanticIrFunction *, SZrParserCfg *, TZrUInt32,
        TZrUInt32, TZrUInt32, EZrParserCfgTerminatorKind) =
        ZrParser_SemanticIr_BindBlockRange;

void ssa_source_cfg_fail_promotion(ESsaSourceCfgPromotionFault fault) {
    promotionFault = fault;
}

void ssa_source_cfg_fail_finish(ESsaSourceCfgFinishFault fault) {
    finishFault = fault;
    finishFaultTriggered = ZR_FALSE;
}

TZrBool ssa_source_cfg_finish_fault_triggered(void) {
    return finishFaultTriggered;
}

static TZrBool source_cfg_ensure_active(SZrCompilerState *compiler) {
    TZrBool activated = source_cfg_real_ensure_active(compiler);
    return (TZrBool)(activated &&
            promotionFault != SSA_SOURCE_CFG_PROMOTION_AFTER_ACTIVATION);
}

static TZrBool source_cfg_finish(SZrCompilerState *compiler) {
    TZrBool finished;
    if (finishFault != SSA_SOURCE_CFG_FINISH_NO_FAULT) {
        finishFaultEnabled = ZR_TRUE;
        finished = ssa_test_compiler_semantic_cfg_finish(compiler);
        finishFaultEnabled = ZR_FALSE;
        return finished;
    }
    finished = source_cfg_real_finish(compiler);
    return (TZrBool)(finished &&
            promotionFault != SSA_SOURCE_CFG_PROMOTION_AFTER_FINISH);
}

static TZrUInt32 source_cfg_append_block(
        SZrState *state, SZrParserCfg *cfg, EZrParserCfgBlockKind kind,
        SZrAstNode *statement) {
    if (finishFaultEnabled &&
        finishFault == SSA_SOURCE_CFG_FINISH_EXIT_BLOCK_APPEND &&
        kind == ZR_PARSER_CFG_BLOCK_EXIT) {
        finishFaultTriggered = ZR_TRUE;
        return ZR_PARSER_CFG_INVALID_BLOCK_ID;
    }
    return source_cfg_real_append_block(state, cfg, kind, statement);
}

static TZrBool source_cfg_connect(
        SZrParserCfg *cfg, TZrUInt32 fromBlockId, TZrUInt32 toBlockId,
        EZrParserCfgEdgeKind kind, SZrAstNode *sourceNode) {
    if (finishFaultEnabled &&
        finishFault == SSA_SOURCE_CFG_FINISH_NORMAL_EDGE_APPEND &&
        kind == ZR_PARSER_CFG_EDGE_NORMAL) {
        finishFaultTriggered = ZR_TRUE;
        return ZR_FALSE;
    }
    return source_cfg_real_connect(cfg, fromBlockId, toBlockId, kind,
                                   sourceNode);
}

static TZrSemanticInstructionId source_cfg_emit(
        SZrSemanticIrFunction *function,
        const SZrSemanticIrInstructionSpec *spec) {
    if (finishFaultEnabled && spec != ZR_NULL &&
        ((finishFault == SSA_SOURCE_CFG_FINISH_BRANCH_INSTRUCTION_APPEND &&
          spec->opcode == ZR_SEMANTIC_IR_BRANCH) ||
         (finishFault == SSA_SOURCE_CFG_FINISH_RETURN_INSTRUCTION_APPEND &&
          spec->opcode == ZR_SEMANTIC_IR_RETURN))) {
        finishFaultTriggered = ZR_TRUE;
        return ZR_SEMANTIC_INSTRUCTION_ID_INVALID;
    }
    return source_cfg_real_emit(function, spec);
}

static TZrBool source_cfg_bind_block_range(
        const SZrSemanticIrFunction *function, SZrParserCfg *cfg,
        TZrUInt32 blockId, TZrUInt32 firstInstructionIndex,
        TZrUInt32 instructionCount, EZrParserCfgTerminatorKind terminatorKind) {
    if (finishFaultEnabled &&
        ((finishFault == SSA_SOURCE_CFG_FINISH_BRANCH_RANGE_BIND &&
          terminatorKind == ZR_PARSER_CFG_TERMINATOR_BRANCH) ||
         (finishFault == SSA_SOURCE_CFG_FINISH_RETURN_RANGE_BIND &&
          terminatorKind == ZR_PARSER_CFG_TERMINATOR_RETURN))) {
        finishFaultTriggered = ZR_TRUE;
        return ZR_FALSE;
    }
    return source_cfg_real_bind_block_range(
            function, cfg, blockId, firstInstructionIndex,
            instructionCount, terminatorKind);
}

/* Compile the production CFG helper body under private names.  This gives the
 * fault suite API-level failures inside finish without adding hooks to the
 * parser implementation or intercepting unrelated parser work. */
#define ZrParser_Cfg_AppendBlock source_cfg_append_block
#define ZrParser_Cfg_Connect source_cfg_connect
#define ZrParser_SemanticIr_Emit source_cfg_emit
#define ZrParser_SemanticIr_BindBlockRange source_cfg_bind_block_range
#define compiler_semantic_cfg_ensure_active ssa_test_compiler_semantic_cfg_ensure_active
#define compiler_semantic_cfg_expression_is_linear ssa_test_compiler_semantic_cfg_expression_is_linear
#define compiler_semantic_cfg_short_circuit_is_supported ssa_test_compiler_semantic_cfg_short_circuit_is_supported
#define compiler_semantic_cfg_arm_falls_through ssa_test_compiler_semantic_cfg_arm_falls_through
#define compiler_semantic_cfg_abandon ssa_test_compiler_semantic_cfg_abandon
#define compiler_semantic_cfg_begin_if ssa_test_compiler_semantic_cfg_begin_if
#define compiler_semantic_cfg_begin_while ssa_test_compiler_semantic_cfg_begin_while
#define compiler_semantic_cfg_begin_for ssa_test_compiler_semantic_cfg_begin_for
#define compiler_semantic_cfg_begin_short_circuit ssa_test_compiler_semantic_cfg_begin_short_circuit
#define compiler_semantic_cfg_begin_optional_guard ssa_test_compiler_semantic_cfg_begin_optional_guard
#define compiler_semantic_cfg_branch_while ssa_test_compiler_semantic_cfg_branch_while
#define compiler_semantic_cfg_branch_for ssa_test_compiler_semantic_cfg_branch_for
#define compiler_semantic_cfg_jump ssa_test_compiler_semantic_cfg_jump
#define compiler_semantic_cfg_jump_edge ssa_test_compiler_semantic_cfg_jump_edge
#define compiler_semantic_cfg_jump_abrupt ssa_test_compiler_semantic_cfg_jump_abrupt
#define compiler_semantic_cfg_branch_value ssa_test_compiler_semantic_cfg_branch_value
#define compiler_semantic_cfg_cleanup_dispatch ssa_test_compiler_semantic_cfg_cleanup_dispatch
#define compiler_semantic_cfg_begin_invoke ssa_test_compiler_semantic_cfg_begin_invoke
#define compiler_semantic_cfg_split_invoke ssa_test_compiler_semantic_cfg_split_invoke
#define compiler_semantic_cfg_terminate_throw ssa_test_compiler_semantic_cfg_terminate_throw
#define compiler_semantic_cfg_terminate_throw_value ssa_test_compiler_semantic_cfg_terminate_throw_value
#define compiler_semantic_cfg_terminate_return ssa_test_compiler_semantic_cfg_terminate_return
#define compiler_semantic_cfg_terminate_return_value ssa_test_compiler_semantic_cfg_terminate_return_value
#define compiler_semantic_cfg_enter ssa_test_compiler_semantic_cfg_enter
#define compiler_semantic_cfg_capture_slots ssa_test_compiler_semantic_cfg_capture_slots
#define compiler_semantic_cfg_restore_slots ssa_test_compiler_semantic_cfg_restore_slots
#define compiler_semantic_cfg_free_slots ssa_test_compiler_semantic_cfg_free_slots
#define compiler_semantic_cfg_finish ssa_test_compiler_semantic_cfg_finish
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c"
#undef ZrParser_Cfg_AppendBlock
#undef ZrParser_Cfg_Connect
#undef ZrParser_SemanticIr_Emit
#undef ZrParser_SemanticIr_BindBlockRange
#undef compiler_semantic_cfg_ensure_active
#undef compiler_semantic_cfg_expression_is_linear
#undef compiler_semantic_cfg_short_circuit_is_supported
#undef compiler_semantic_cfg_arm_falls_through
#undef compiler_semantic_cfg_abandon
#undef compiler_semantic_cfg_begin_if
#undef compiler_semantic_cfg_begin_while
#undef compiler_semantic_cfg_begin_for
#undef compiler_semantic_cfg_begin_short_circuit
#undef compiler_semantic_cfg_begin_optional_guard
#undef compiler_semantic_cfg_branch_while
#undef compiler_semantic_cfg_branch_for
#undef compiler_semantic_cfg_jump
#undef compiler_semantic_cfg_jump_edge
#undef compiler_semantic_cfg_jump_abrupt
#undef compiler_semantic_cfg_branch_value
#undef compiler_semantic_cfg_cleanup_dispatch
#undef compiler_semantic_cfg_begin_invoke
#undef compiler_semantic_cfg_split_invoke
#undef compiler_semantic_cfg_terminate_throw
#undef compiler_semantic_cfg_terminate_throw_value
#undef compiler_semantic_cfg_terminate_return
#undef compiler_semantic_cfg_terminate_return_value
#undef compiler_semantic_cfg_enter
#undef compiler_semantic_cfg_capture_slots
#undef compiler_semantic_cfg_restore_slots
#undef compiler_semantic_cfg_free_slots
#undef compiler_semantic_cfg_finish

void ssa_source_cfg_fail_allocation(size_t ordinal) {
    failureOrdinal = ordinal;
    allocationOrdinal = 0u;
    failed = ZR_FALSE;
}

TZrBool ssa_source_cfg_allocation_failed(void) { return failed; }
size_t ssa_source_cfg_outstanding_allocations(void) { return outstanding; }

static void *source_cfg_realloc(void *memory, size_t bytes) {
    TZrBool newAllocation = (TZrBool)(memory == NULL);
    void *result;
    ++allocationOrdinal;
    if (failureOrdinal != 0u && allocationOrdinal == failureOrdinal) {
        failed = ZR_TRUE;
        return NULL;
    }
    result = realloc(memory, bytes);
    if (result != NULL && newAllocation) ++outstanding;
    return result;
}

static void source_cfg_free(void *memory) {
    if (memory != NULL) --outstanding;
    free(memory);
}

/* Intercept preflight scratch allocations and report failures after the real
 * promotion helpers mutate state. Compile the actual finalizer in this test
 * target, keeping its private helper boundary visible to the hooks. */
#define compiler_semantic_cfg_finalize ssa_source_cfg_finalize
#define compiler_semantic_cfg_ensure_active source_cfg_ensure_active
#define compiler_semantic_cfg_finish source_cfg_finish
#define realloc source_cfg_realloc
#define free source_cfg_free
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_finalize.c"
#undef compiler_semantic_cfg_finalize
#undef compiler_semantic_cfg_ensure_active
#undef compiler_semantic_cfg_finish
#undef realloc
#undef free
