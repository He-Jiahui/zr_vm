#ifndef ZR_TEST_SSA_FUNCTION_AOT_OWNER_FIXTURE_H
#define ZR_TEST_SSA_FUNCTION_AOT_OWNER_FIXTURE_H

#include "zr_vm_core/aot_ir.h"

/* Ordinary CRT producer; no VM object or source compiler participates. */
typedef struct SZrTestsFunctionAotProducer {
    SZrAotIrModule module;
    SZrAotIrFunction function;
    SZrExecIrConstant constant;
    SZrAotIrFrameSlot slot;
    TZrUInt32 valueSlot, result, operand;
    SZrAotIrBlock block;
    SZrAotIrInstruction instructions[2];
    SZrAotIrSourceMap sourceMaps[2];
    SZrExecIrStateMap logicalMap;
} SZrTestsFunctionAotProducer;

/* All pointer members of these copied metadata records are cleared. */
typedef struct SZrTestsFunctionAotWitness {
    TZrUInt64 semanticHash;
    SZrAotIrModule module;
    SZrAotIrFunction function;
    SZrExecIrConstant constant;
    SZrAotIrFrameSlot slot;
    TZrUInt32 valueSlot, result, operand;
    SZrAotIrBlock block;
    SZrAotIrInstruction instructions[2];
    SZrAotIrSourceMap sourceMaps[2];
    SZrExecIrStateMap logicalMap;
} SZrTestsFunctionAotWitness;

SZrTestsFunctionAotProducer *ZrTests_FunctionAotProducer_New(TZrInt64 literal);
void ZrTests_FunctionAotProducer_Free(SZrTestsFunctionAotProducer **slot);
void ZrTests_FunctionAotWitness_Capture(const SZrAotIrModule *module,
                                       SZrTestsFunctionAotWitness *witness);
void ZrTests_FunctionAot_AssertView(const SZrAotIrModule *module,
                                  const SZrTestsFunctionAotWitness *witness);
void ZrTests_FunctionAot_AssertIndependent(const SZrAotIrModule *source,
                                         const SZrAotIrModule *copy);

#endif
