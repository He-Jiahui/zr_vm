/* Compile-only declaration boundary; no main and no evaluated API calls.
 * Default: public model, lifecycle and const queries remain available.
 * PRIVATE_HEADER: every mutation declaration is available to Core consumers.
 * PUBLIC_PRIVATE_SYMBOL=<name>: that one mutation must be undeclared publicly.
 */
#if defined(PRIVATE_HEADER) && defined(PUBLIC_PRIVATE_SYMBOL)
#error Header boundary probe modes are mutually exclusive
#endif

#if defined(PRIVATE_HEADER)
#include "exec_ir/exec_ir_mutation_internal.h"
#else
#include "zr_vm_core/exec_ir.h"
#endif

void ZrTests_ExecIr_HeaderBoundaryProbe(void) {
#if defined(PRIVATE_HEADER)
    (void)sizeof(&ZrCore_ExecIr_FunctionSetBindingRows);
    (void)sizeof(&ZrCore_ExecIr_ModuleAddFunction);
    (void)sizeof(&ZrCore_ExecIr_ModuleAppendConstant);
    (void)sizeof(&ZrCore_ExecIr_ModuleAppendLayout);
    (void)sizeof(&ZrCore_ExecIr_ModuleFunctionAt);
    (void)sizeof(&ZrCore_ExecIr_FunctionAddBlock);
    (void)sizeof(&ZrCore_ExecIr_FunctionAddValue);
    (void)sizeof(&ZrCore_ExecIr_FunctionAddExternalValue);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendOperands);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendResults);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendMemoryTokens);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendPhis);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendPredecessors);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendSuccessors);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendPhiIncoming);
    (void)sizeof(&ZrCore_ExecIr_FunctionSetEffectPhi);
    (void)sizeof(&ZrCore_ExecIr_FunctionSetMemoryPhi);
    (void)sizeof(&ZrCore_ExecIr_FunctionAppendInstruction);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveOperands);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveOperandsEx);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveResults);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveResultsEx);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveBlocks);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveInstructions);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveMemoryTokens);
    (void)sizeof(&ZrCore_ExecIr_FunctionReservePhis);
    (void)sizeof(&ZrCore_ExecIr_FunctionReservePredecessors);
    (void)sizeof(&ZrCore_ExecIr_FunctionReserveSuccessors);
    (void)sizeof(&ZrCore_ExecIr_FunctionSeal);
    (void)sizeof(&ZrCore_ExecIr_FunctionBlockAt);
#elif defined(PUBLIC_PRIVATE_SYMBOL)
    (void)sizeof(&PUBLIC_PRIVATE_SYMBOL);
#else
    (void)sizeof(SZrExecIrModule);
    (void)sizeof(SZrExecIrFunction);
    (void)sizeof(SZrExecIrBlock);
    (void)sizeof(SZrExecIrInstruction);
    (void)sizeof(SZrExecIrFrameLayout);
    (void)sizeof(SZrExecIrGcMap);
    (void)sizeof(SZrExecIrDiagnostic);

    (void)sizeof(&ZrCore_ExecIr_ModuleInit);
    (void)sizeof(&ZrCore_ExecIr_FunctionInit);
    (void)sizeof(&ZrCore_ExecIr_FrameLayoutInit);
    (void)sizeof(&ZrCore_ExecIr_FrameLayoutFree);
    (void)sizeof(&ZrCore_ExecIr_GcMapInit);
    (void)sizeof(&ZrCore_ExecIr_GcMapFree);
    (void)sizeof(&ZrCore_ExecIr_FreeModule);
    (void)sizeof(&ZrCore_ExecIr_FreeFunction);
    (void)sizeof(&ZrCore_ExecIr_CloneModule);
    (void)sizeof(&ZrCore_ExecIr_CloneFunction);

    (void)sizeof(&ZrCore_ExecIr_ModuleFunctionAtConst);
    (void)sizeof(&ZrCore_ExecIr_FunctionBlockAtConst);
    (void)sizeof(&ZrCore_ExecIr_FunctionBindingRowAt);
    (void)sizeof(&ZrCore_ExecIr_FunctionValidateBindingRows);
    (void)sizeof(&ZrCore_ExecIr_FunctionBindingRowsHash);
    (void)sizeof(&ZrCore_ExecIr_OpcodeInfo);
    (void)sizeof(&ZrCore_ExecIr_OpcodeName);
    (void)sizeof(&ZrCore_ExecIr_ValidateModule);
    (void)sizeof(&ZrCore_ExecIr_VerifyModule);
    (void)sizeof(&ZrCore_ExecIr_VerifyFunction);
    (void)sizeof(&ZrCore_ExecIr_VerifyEffects);

    (void)sizeof(ZrCore_ExecIr_ModuleFunctionAtConst(
            (const SZrExecIrModule *)0, (TZrExecIrFunctionId)0));
    (void)sizeof(ZrCore_ExecIr_FunctionBlockAtConst(
            (const SZrExecIrFunction *)0, (TZrExecIrBlockId)0));
    (void)sizeof(ZrCore_ExecIr_FunctionBindingRowAt(
            (const SZrExecIrFunction *)0, (TZrUInt32)0));
#endif
}
