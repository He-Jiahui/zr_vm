#ifndef ZR_VM_PARSER_EXEC_IR_FUSION_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_FUSION_INTERNAL_H

/*
 * Private seams between the generated-fusion projection units.  This header
 * is intentionally kept below src/: callers consume only exec_ir_fusion.h.
 * The seams use stable scalar records and never expose implementation-owned
 * pointers beyond the lifetime of a call.
 */

#include "zr_vm_parser/exec_ir_fusion.h"
#include "zr_vm_core/exec_ir_state_map.h"

TZrBool zr_fusion_range_valid(SZrExecIrRange range, TZrUInt32 count);
TZrBool zr_fusion_pointer_count_valid(const void *pointer,
                                      TZrUInt32 count,
                                      TZrUInt32 capacity);
void zr_fusion_diag_clear(SZrExecIrDiagnostic *diagnostic);
void zr_fusion_diag_set(SZrExecIrDiagnostic *diagnostic,
                        EZrExecutionDiagnosticCode code,
                        const SZrExecIrFunction *function,
                        TZrUInt32 instructionId,
                        TZrUInt32 blockId,
                        TZrUInt32 expected,
                        TZrUInt32 actual);
void zr_fusion_diag_set_rich(SZrExecBcFusionDiagnostic *diagnostic,
                             EZrExecBcFusionStatus status,
                             EZrExecBcFusionFallbackReason reason,
                             EZrExecBcFusionPattern pattern,
                             TZrUInt32 head,
                             TZrUInt32 tail,
                             TZrUInt32 source,
                             TZrUInt32 expected,
                             TZrUInt32 actual);

TZrBool zr_fusion_plan_storage_is_valid(const SZrExecBcFusionPlan *plan);
TZrBool zr_fusion_function_storage_is_valid(const SZrExecIrFunction *function);
TZrBool zr_fusion_state_map_is_valid(const SZrExecIrFunction *function);

const SZrExecIrInstruction *zr_fusion_instruction(
        const SZrExecIrFunction *function, TZrUInt32 instructionId);
EZrExecBcFusionFallbackReason zr_fusion_reason_for_pair(
        const SZrExecIrFunction *function,
        TZrUInt32 headId,
        TZrUInt32 tailId,
        const SZrExecBcPatternOptions *options,
        EZrExecBcFusionPattern *matchedPattern);

void zr_fusion_set_contract_diagnostic(
        SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code,
        const SZrExecIrFunction *function,
        TZrUInt64 expected,
        TZrUInt64 actual);
TZrBool zr_fusion_options_valid(const SZrExecBcPatternOptions *options);
TZrUInt32 zr_fusion_option_or_default(TZrUInt32 value,
                                      TZrUInt32 defaultValue);
TZrBool zr_fusion_budget_allows(const SZrExecBcFusionPlan *plan,
                                const SZrExecBcPatternOptions *options,
                                const SZrExecBcFusionPatternInfo *info);

TZrBool zr_fusion_reserve(void **storage,
                          TZrUInt32 *capacity,
                          TZrUInt32 requested,
                          size_t elementSize);
TZrBool zr_fusion_append_instruction(SZrExecBcFusionPlan *plan,
                                     SZrExecBcFusionInstruction word);
TZrBool zr_fusion_append_side(SZrExecBcFusionPlan *plan,
                              const SZrExecBcFusionSideEntry *entry);
TZrBool zr_fusion_append_source(SZrExecBcFusionPlan *plan,
                                const SZrExecBcFusionSourceMap *map);
TZrBool zr_fusion_append_fallback(SZrExecBcFusionPlan *plan,
                                  const SZrExecBcFusionFallback *fallback,
                                  TZrUInt32 maxEntries);
SZrExecBcFusionInstruction zr_fusion_encode_original(
        const SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction);
SZrExecBcFusionInstruction zr_fusion_encode_fused(
        EZrExecBcFusionPattern pattern, TZrUInt32 sideIndex);
TZrUInt32 zr_fusion_find_output_pc(const TZrUInt32 *instructionToPc,
                                   TZrUInt32 count,
                                   TZrUInt32 instructionId);
TZrBool zr_fusion_append_source_event(
        SZrExecBcFusionPlan *plan,
        TZrUInt32 fusedPc,
        TZrUInt32 instructionId,
        const SZrExecIrInstruction *instruction,
        TZrUInt32 ordinal,
        const SZrExecIrFunction *function);
TZrBool zr_fusion_fill_side_entry(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        EZrExecBcFusionPattern pattern,
        TZrUInt32 headId,
        TZrUInt32 tailId,
        SZrExecBcFusionSideEntry *entry);
TZrUInt32 zr_fusion_block_target_instruction(
        const SZrExecIrFunction *function, TZrUInt32 blockId);

#endif /* ZR_VM_PARSER_EXEC_IR_FUSION_INTERNAL_H */
