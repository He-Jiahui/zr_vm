#ifndef ZR_VM_CORE_AOT_IR_H
#define ZR_VM_CORE_AOT_IR_H

/*
 * Shared, pointer-free-in-content AOT intermediate representation.
 *
 * The C structs below use pointers only as views over caller-owned arrays;
 * those pointers are never hashed, serialized, or consumed by a backend as
 * semantic data.  All references between records are stable numeric IDs or
 * bounded ranges.  C and LLVM emitters therefore receive the same semantic
 * input and only legalize the target ABI.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"

#define ZR_AOT_IR_SCHEMA_VERSION ((TZrUInt32)4u)
#define ZR_AOT_IR_TARGET_ABI_VERSION ((TZrUInt32)1u)
#define ZR_AOT_IR_ID_INVALID ((TZrUInt32)0u)

typedef enum EZrAotIrStatus {
    ZR_AOT_IR_OK = 0,
    ZR_AOT_IR_INVALID_ARGUMENT,
    ZR_AOT_IR_VERSION_MISMATCH,
    ZR_AOT_IR_INVALID_TARGET,
    ZR_AOT_IR_INVALID_CONTRACT,
    ZR_AOT_IR_INVALID_ID,
    ZR_AOT_IR_DUPLICATE_ID,
    ZR_AOT_IR_INVALID_RANGE,
    ZR_AOT_IR_INVALID_CFG,
    ZR_AOT_IR_INVALID_OPCODE,
    ZR_AOT_IR_INVALID_EFFECT,
    ZR_AOT_IR_INVALID_LAYOUT,
    ZR_AOT_IR_INVALID_SIGNATURE,
    ZR_AOT_IR_RELOCATION,
    ZR_AOT_IR_UNSUPPORTED
} EZrAotIrStatus;

typedef struct SZrAotIrDiagnostic {
    EZrAotIrStatus status;
    TZrUInt32 functionId;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrAotIrDiagnostic;

typedef struct SZrAotIrTargetContract {
    TZrUInt32 abiVersion;
    TZrUInt32 pointerSize;
    TZrUInt32 endianness;
    TZrUInt32 requiredCapabilities;
    TZrUInt64 targetTripleHash;
    TZrUInt64 abiHash;
} SZrAotIrTargetContract;

typedef struct SZrAotIrRange {
    TZrUInt32 offset;
    TZrUInt32 count;
} SZrAotIrRange;

typedef struct SZrAotIrFrameLayout {
    TZrUInt32 logicalSlotCount;
    TZrUInt32 storageSlotCount;
    TZrUInt32 parameterPrefixBytes;
    TZrUInt32 returnAreaOffset;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    TZrUInt64 layoutHash;
} SZrAotIrFrameLayout;

typedef struct SZrAotIrFrameSlot {
    TZrUInt32 slotId;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrExecIrTypeToken typeToken;
    TZrUInt32 kind;
} SZrAotIrFrameSlot;

typedef struct SZrAotIrSourceMap {
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrUInt32 startOffset;
    TZrUInt32 endOffset;
    TZrUInt32 startLine;
    TZrUInt32 startColumn;
    TZrUInt32 endLine;
    TZrUInt32 endColumn;
} SZrAotIrSourceMap;

typedef struct SZrAotIrInstruction {
    TZrUInt32 id;
    TZrUInt32 opcode;
    TZrUInt32 flags;
    SZrAotIrRange results;
    SZrAotIrRange operands;
    SZrAotIrRange successors;
    SZrAotIrRange phiIncoming;
    TZrUInt32 effectIn;
    TZrUInt32 effectOut;
    TZrUInt32 sourceId;
    TZrUInt32 deoptId;
    TZrUInt32 layoutId;
    TZrUInt32 bindingRow;
    TZrExecIrTypeToken typeToken;
    TZrExecIrTypeToken matchTypeToken;
    SZrAotIrRange memoryIn;
    SZrAotIrRange memoryOut;
} SZrAotIrInstruction;

typedef struct SZrAotIrBlock {
    TZrUInt32 id;
    TZrUInt32 flags;
    SZrAotIrRange instructions;
    SZrAotIrRange predecessors;
    SZrAotIrRange successors;
    TZrUInt32 terminatorInstructionId;
} SZrAotIrBlock;

typedef struct SZrAotIrPhiIncoming {
    TZrUInt32 predecessorBlockId;
    TZrUInt32 valueId;
} SZrAotIrPhiIncoming;

typedef struct SZrAotIrFunction {
    TZrUInt32 id;
    TZrMetadataToken functionToken;
    SZrExecutionContract contract;
    TZrUInt64 signatureHash;
    SZrAotIrFrameLayout frameLayout;
    const SZrAotIrFrameSlot *frameSlots;
    TZrUInt32 frameSlotCount;
    const TZrUInt32 *valueSlotPool;
    TZrUInt32 valueSlotCount;
    const SZrAotIrBlock *blocks;
    TZrUInt32 blockCount;
    const SZrAotIrInstruction *instructions;
    TZrUInt32 instructionCount;
    const TZrUInt32 *operandPool;
    TZrUInt32 operandCount;
    const TZrUInt32 *resultPool;
    TZrUInt32 resultCount;
    const SZrAotIrPhiIncoming *phiIncomingPool;
    TZrUInt32 phiIncomingCount;
    const TZrUInt32 *successorPool;
    TZrUInt32 successorCount;
    const TZrExecIrMemoryTokenId *memoryTokenPool;
    TZrUInt32 memoryTokenCount;
    const SZrAotIrSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    const SZrExecIrGcMap *gcMap;
    const TZrExecIrValueId *gcRootPool;
    TZrUInt32 gcRootCount;
    const SZrExecIrDeoptState *deoptStates;
    TZrUInt32 deoptStateCount;
    const TZrExecIrValueId *deoptValuePool;
    TZrUInt32 deoptValueCount;
    const SZrExecIrDeoptAggregate *deoptAggregates;
    TZrUInt32 deoptAggregateCount;
    const SZrExecIrDeoptAggregateField *deoptAggregateFields;
    TZrUInt32 deoptAggregateFieldCount;
    const SZrExecIrStateMap *logicalStateMap; /* borrowed complete checkpoint table */
    TZrUInt64 gcMapHash;
    TZrUInt64 exceptionMapHash;
    TZrUInt64 debugMapHash;
    TZrUInt32 relocationCount;
} SZrAotIrFunction;

typedef struct SZrAotIrModule {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    SZrAotIrTargetContract target;
    SZrExecutionContract contract;
    TZrUInt64 moduleHash;
    const SZrAotIrFunction *functions;
    TZrUInt32 functionCount;
    TZrUInt32 relocationCount;
} SZrAotIrModule;

ZR_CORE_API EZrAotIrStatus ZrCore_AotIr_ValidateTarget(
        const SZrAotIrTargetContract *target,
        SZrAotIrDiagnostic *diagnostic);
ZR_CORE_API EZrAotIrStatus ZrCore_AotIr_ValidateModule(
        const SZrAotIrModule *module,
        SZrAotIrDiagnostic *diagnostic);
ZR_CORE_API TZrUInt64 ZrCore_AotIr_HashModule(
        const SZrAotIrModule *module);
ZR_CORE_API TZrBool ZrCore_AotIr_IsRelocationFree(
        const SZrAotIrModule *module,
        SZrAotIrDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_AotIr_StatusName(EZrAotIrStatus status);

#endif /* ZR_VM_CORE_AOT_IR_H */
