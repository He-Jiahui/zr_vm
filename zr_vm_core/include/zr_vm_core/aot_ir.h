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

#define ZR_AOT_IR_SCHEMA_VERSION ((TZrUInt32)1u)
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

typedef struct SZrAotIrStateMapEntry {
    TZrUInt32 resumeId;
    TZrUInt32 instructionId;
    TZrUInt64 stateHash;
} SZrAotIrStateMapEntry;

typedef struct SZrAotIrFunction {
    TZrUInt32 id;
    TZrMetadataToken functionToken;
    SZrExecutionContract contract;
    TZrUInt64 signatureHash;
    SZrAotIrFrameLayout frameLayout;
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
    const SZrAotIrStateMapEntry *stateMaps;
    TZrUInt32 stateMapCount;
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
