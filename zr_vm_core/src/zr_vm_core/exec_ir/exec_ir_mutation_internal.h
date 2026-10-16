#ifndef ZR_VM_CORE_EXEC_IR_MUTATION_INTERNAL_H
#define ZR_VM_CORE_EXEC_IR_MUTATION_INTERNAL_H

#include "zr_vm_core/exec_ir.h"

/* Construction helpers for internal Core and Parser consumers.
 * Declarations remain exported; this private header does not change the ABI. */

ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionSetBindingRows(
        SZrExecIrFunction *function, const SZrExecIrBindingRow *rows,
        TZrUInt32 rowCount, SZrExecIrDiagnostic *diagnostic);

ZR_CORE_API TZrBool ZrCore_ExecIr_ModuleAddFunction(SZrExecIrModule *module,
                                                     TZrMetadataToken functionToken,
                                                     TZrUInt64 signatureHash,
                                                     TZrExecIrFunctionId *outId);
ZR_CORE_API TZrBool ZrCore_ExecIr_ModuleAppendConstant(
        SZrExecIrModule *module,
        const SZrExecIrConstant *constant,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_ModuleAppendLayout(
        SZrExecIrModule *module,
        const SZrExecIrLayout *layout,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API SZrExecIrFunction *ZrCore_ExecIr_ModuleFunctionAt(
        SZrExecIrModule *module, TZrExecIrFunctionId id);

ZR_CORE_API TZrExecIrBlockId ZrCore_ExecIr_FunctionAddBlock(SZrExecIrFunction *function,
                                                             TZrUInt32 flags);
ZR_CORE_API TZrExecIrValueId ZrCore_ExecIr_FunctionAddValue(
        SZrExecIrFunction *function,
        TZrMetadataToken typeToken,
        EZrExecIrOwnership ownership,
        EZrExecIrNullability nullability);
ZR_CORE_API TZrExecIrValueId ZrCore_ExecIr_FunctionAddExternalValue(
        SZrExecIrFunction *function,
        TZrMetadataToken typeToken,
        EZrExecIrOwnership ownership,
        EZrExecIrNullability nullability);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendOperands(
        SZrExecIrFunction *function,
        const TZrExecIrValueId *operands,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendResults(
        SZrExecIrFunction *function,
        const TZrExecIrValueId *results,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendMemoryTokens(
        SZrExecIrFunction *function,
        const TZrExecIrMemoryTokenId *tokens,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendPhis(
        SZrExecIrFunction *function,
        const SZrExecIrPhi *phis,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendPredecessors(
        SZrExecIrFunction *function,
        const TZrExecIrBlockId *blocks,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendSuccessors(
        SZrExecIrFunction *function,
        const TZrExecIrBlockId *blocks,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendPhiIncoming(
        SZrExecIrFunction *function,
        const SZrExecIrPhiIncoming *incoming,
        TZrSize count,
        SZrExecIrRange *outRange);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionSetEffectPhi(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        TZrExecIrEffectTokenId result,
        const SZrExecIrPhiIncoming *incoming,
        TZrSize count);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionSetMemoryPhi(
        SZrExecIrFunction *function,
        TZrExecIrBlockId blockId,
        EZrExecIrMemoryClass region,
        TZrExecIrMemoryTokenId result,
        const SZrExecIrPhiIncoming *incoming,
        TZrSize count);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionAppendInstruction(
        SZrExecIrFunction *function,
        const SZrExecIrInstruction *instruction,
        TZrExecIrInstructionId *outId);

ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveOperands(SZrExecIrFunction *function,
                                                           TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveOperandsEx(
        SZrExecIrFunction *function,
        TZrSize capacity,
        SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveResults(SZrExecIrFunction *function,
                                                          TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveResultsEx(
        SZrExecIrFunction *function,
        TZrSize capacity,
        SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveBlocks(SZrExecIrFunction *function,
                                                         TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveInstructions(SZrExecIrFunction *function,
                                                               TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveMemoryTokens(SZrExecIrFunction *function,
                                                                TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReservePhis(SZrExecIrFunction *function,
                                                       TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReservePredecessors(SZrExecIrFunction *function,
                                                               TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionReserveSuccessors(SZrExecIrFunction *function,
                                                             TZrSize capacity);
ZR_CORE_API TZrBool ZrCore_ExecIr_FunctionSeal(SZrExecIrFunction *function,
                                                SZrExecIrDiagnostic *diagnostic);

ZR_CORE_API SZrExecIrBlock *ZrCore_ExecIr_FunctionBlockAt(
        SZrExecIrFunction *function, TZrExecIrBlockId id);

#endif /* ZR_VM_CORE_EXEC_IR_MUTATION_INTERNAL_H */
