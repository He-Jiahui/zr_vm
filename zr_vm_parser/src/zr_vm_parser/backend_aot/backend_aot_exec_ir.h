#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_H

#include "zr_vm_parser/writer.h"

typedef enum EZrAotRuntimeContract {
    ZR_AOT_RUNTIME_CONTRACT_NONE = 0,
    ZR_AOT_RUNTIME_CONTRACT_REFLECTION_TYPEOF = 1 << 0,
    ZR_AOT_RUNTIME_CONTRACT_FUNCTION_PRECALL = 1 << 1,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_BORROW = 1 << 2,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_LOAN = 1 << 3,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_SHARE = 1 << 4,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_WEAK = 1 << 5,
    ZR_AOT_RUNTIME_CONTRACT_ITER_INIT = 1 << 6,
    ZR_AOT_RUNTIME_CONTRACT_ITER_MOVE_NEXT = 1 << 7,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_DETACH = 1 << 8,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_UPGRADE = 1 << 9,
    ZR_AOT_RUNTIME_CONTRACT_OWNERSHIP_RELEASE = 1 << 10
} EZrAotRuntimeContract;

typedef enum EZrAotExecIrCallsiteKind {
    ZR_AOT_EXEC_IR_CALLSITE_KIND_NONE = 0,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_STATIC_DIRECT = 1,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_DIRECT_PROBE = 2,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_META = 3,
    ZR_AOT_EXEC_IR_CALLSITE_KIND_GENERIC = 4
} EZrAotExecIrCallsiteKind;

typedef enum EZrAotExecIrTerminatorKind {
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_NONE = 0,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_FALLTHROUGH = 1,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_BRANCH = 2,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_CONDITIONAL_BRANCH = 3,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_RETURN = 4,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_TAIL_RETURN = 5,
    ZR_AOT_EXEC_IR_TERMINATOR_KIND_EH_RESUME = 6
} EZrAotExecIrTerminatorKind;

typedef struct SZrAotExecIrInstruction {
    TZrUInt32 functionIndex;
    TZrUInt32 blockIndex;
    TZrUInt32 semIrOpcode;
    TZrUInt32 execInstructionIndex;
    TZrUInt32 typeTableIndex;
    TZrUInt32 effectTableIndex;
    TZrUInt32 destinationSlot;
    TZrUInt32 operand0;
    TZrUInt32 operand1;
    TZrUInt32 deoptId;
    TZrUInt32 debugLine;
    TZrUInt32 callsiteKind;
} SZrAotExecIrInstruction;

typedef struct SZrAotExecIrFrameLayout {
    TZrUInt32 parameterCount;
    TZrUInt32 stackSlotCount;
    TZrUInt32 generatedFrameSlotCount;
    TZrUInt32 closureValueCount;
    TZrUInt32 localVariableCount;
    TZrUInt32 exportedValueCount;
} SZrAotExecIrFrameLayout;

typedef struct SZrAotExecIrBasicBlock {
    TZrUInt32 blockId;
    TZrUInt32 firstExecInstructionIndex;
    TZrUInt32 instructionCount;
    TZrUInt32 firstInstructionOffset;
    TZrUInt32 semIrInstructionCount;
    TZrUInt32 terminatorInstructionIndex;
    TZrUInt32 terminatorKind;
    TZrUInt32 successorCount;
    TZrUInt32 successorBlockIndices[2];
} SZrAotExecIrBasicBlock;

typedef struct SZrAotExecIrFunction {
    const SZrFunction *function;
    TZrUInt32 flatIndex;
    TZrUInt32 parentFunctionIndex;
    TZrUInt32 runtimeContracts;
    TZrUInt32 firstInstructionOffset;
    TZrUInt32 instructionCount;
    TZrUInt32 execInstructionCount;
    SZrAotExecIrFrameLayout frameLayout;
    SZrAotExecIrBasicBlock *basicBlocks;
    TZrUInt32 basicBlockCount;
} SZrAotExecIrFunction;

typedef struct SZrAotExecIrModule {
    SZrAotExecIrInstruction *instructions;
    TZrUInt32 instructionCount;
    TZrUInt32 runtimeContracts;
    SZrAotExecIrFunction *functions;
    TZrUInt32 functionCount;
} SZrAotExecIrModule;

const TZrChar *backend_aot_exec_ir_semir_opcode_name(TZrUInt32 opcode);
const TZrChar *backend_aot_exec_ir_runtime_contract_name(TZrUInt32 contractBit);
TZrUInt32 backend_aot_exec_ir_runtime_contract_count(TZrUInt32 runtimeContracts);
const TZrChar *backend_aot_exec_ir_callsite_kind_name(TZrUInt32 callsiteKind);
const TZrChar *backend_aot_exec_ir_terminator_kind_name(TZrUInt32 terminatorKind);

TZrBool backend_aot_exec_ir_build_module(SZrState *state, SZrFunction *function, SZrAotExecIrModule *outModule);
void backend_aot_exec_ir_release_module(SZrState *state, SZrAotExecIrModule *module);
const SZrAotExecIrFunction *backend_aot_exec_ir_find_function(const SZrAotExecIrModule *module, TZrUInt32 functionIndex);

#endif
