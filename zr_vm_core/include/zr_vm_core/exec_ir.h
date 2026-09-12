#ifndef ZR_VM_CORE_EXEC_IR_H
#define ZR_VM_CORE_EXEC_IR_H

#include <stddef.h>
#include <stdint.h>

#include "zr_vm_core/execution_contract.h"

#define ZR_EXEC_IR_MAX_OPERANDS ((TZrUInt32)UINT32_MAX)
#define ZR_EXEC_IR_VARIADIC ZR_EXEC_IR_MAX_OPERANDS

typedef TZrUInt32 TZrExecIrModuleId;
typedef TZrUInt32 TZrExecIrFunctionId;
typedef TZrUInt32 TZrExecIrBlockId;
typedef TZrUInt32 TZrExecIrInstructionId;
typedef TZrUInt32 TZrExecIrValueId;
typedef TZrUInt32 TZrExecIrMemoryTokenId;
typedef TZrUInt32 TZrExecIrEffectTokenId;
typedef TZrUInt32 TZrExecIrTypeToken;
typedef TZrUInt32 TZrExecIrSourceId;
typedef TZrUInt32 TZrExecIrDeoptId;

/* Optional logical state-map side table, defined in exec_ir_state_map.h. */
typedef struct SZrExecIrStateMap SZrExecIrStateMap;

#define ZR_EXEC_IR_MODULE_ID_INVALID ((TZrExecIrModuleId)0u)
#define ZR_EXEC_IR_FUNCTION_ID_INVALID ((TZrExecIrFunctionId)0u)
#define ZR_EXEC_IR_BLOCK_ID_INVALID ((TZrExecIrBlockId)0u)
#define ZR_EXEC_IR_BLOCK_ID_ENTRY ((TZrExecIrBlockId)1u)
#define ZR_EXEC_IR_INSTRUCTION_ID_INVALID ((TZrExecIrInstructionId)0u)
#define ZR_EXEC_IR_VALUE_ID_INVALID ((TZrExecIrValueId)0u)
#define ZR_EXEC_IR_MEMORY_TOKEN_ID_INVALID ((TZrExecIrMemoryTokenId)0u)
#define ZR_EXEC_IR_EFFECT_TOKEN_ID_INVALID ((TZrExecIrEffectTokenId)0u)

typedef struct SZrExecIrRange {
    union {
        TZrUInt32 offset;
        /* Compatibility spelling used by the initial builder prototype. */
        TZrUInt32 start;
    };
    TZrUInt32 count;
} SZrExecIrRange;

typedef struct SZrExecIrMemoryToken {
    TZrExecIrMemoryTokenId id;
} SZrExecIrMemoryToken;

typedef struct SZrExecIrEffectToken {
    TZrExecIrEffectTokenId id;
} SZrExecIrEffectToken;

typedef enum EZrExecIrOwnership {
    ZR_EXEC_IR_OWNERSHIP_UNKNOWN = 0,
    ZR_EXEC_IR_OWNERSHIP_BORROWED,
    ZR_EXEC_IR_OWNERSHIP_UNIQUE,
    ZR_EXEC_IR_OWNERSHIP_SHARED,
    ZR_EXEC_IR_OWNERSHIP_GC,
    ZR_EXEC_IR_OWNERSHIP_COUNT
} EZrExecIrOwnership;

typedef enum EZrExecIrNullability {
    ZR_EXEC_IR_NULLABILITY_UNKNOWN = 0,
    ZR_EXEC_IR_NULLABILITY_NONNULL,
    ZR_EXEC_IR_NULLABILITY_NULLABLE,
    ZR_EXEC_IR_NULLABILITY_COUNT
} EZrExecIrNullability;

typedef enum EZrExecIrInstructionFlags {
    ZR_EXEC_IR_FLAG_MAY_ALLOCATE = (TZrUInt16)1u << 0u,
    ZR_EXEC_IR_FLAG_MAY_THROW = (TZrUInt16)1u << 1u,
    ZR_EXEC_IR_FLAG_MAY_GC = (TZrUInt16)1u << 2u,
    ZR_EXEC_IR_FLAG_MAY_SUSPEND = (TZrUInt16)1u << 3u,
    /* Explicit safepoint/guard boundaries used by logical state maps. */
    ZR_EXEC_IR_FLAG_DEBUG_POLL = (TZrUInt16)1u << 4u,
    ZR_EXEC_IR_FLAG_GUARD_EXIT = (TZrUInt16)1u << 5u
} EZrExecIrInstructionFlags;

#define ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK \
    ((TZrUInt16)(ZR_EXEC_IR_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_FLAG_MAY_THROW | \
                 ZR_EXEC_IR_FLAG_MAY_GC | ZR_EXEC_IR_FLAG_MAY_SUSPEND | \
                 ZR_EXEC_IR_FLAG_DEBUG_POLL | ZR_EXEC_IR_FLAG_GUARD_EXIT))

/* Schema flags describe an opcode, while instruction flags describe the
 * dynamic properties carried by a concrete instruction.  Keep the old names
 * as aliases for source compatibility with early model consumers. */
#define ZR_EXEC_IR_SCHEMA_FLAG_PRODUCES_VALUE ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW ((TZrUInt32)1u << 3u)
#define ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC ((TZrUInt32)1u << 4u)
#define ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND ((TZrUInt32)1u << 5u)
#define ZR_EXEC_IR_SCHEMA_FLAG_MAY_DROP ((TZrUInt32)1u << 6u)
#define ZR_EXEC_IR_OPCODE_FLAG_PRODUCES_VALUE ZR_EXEC_IR_SCHEMA_FLAG_PRODUCES_VALUE
#define ZR_EXEC_IR_OPCODE_FLAG_TERMINATOR ZR_EXEC_IR_SCHEMA_FLAG_TERMINATOR
#define ZR_EXEC_IR_OPCODE_FLAG_MAY_ALLOCATE ZR_EXEC_IR_SCHEMA_FLAG_MAY_ALLOCATE
#define ZR_EXEC_IR_OPCODE_FLAG_MAY_THROW ZR_EXEC_IR_SCHEMA_FLAG_MAY_THROW
#define ZR_EXEC_IR_OPCODE_FLAG_MAY_GC ZR_EXEC_IR_SCHEMA_FLAG_MAY_GC
#define ZR_EXEC_IR_OPCODE_FLAG_MAY_SUSPEND ZR_EXEC_IR_SCHEMA_FLAG_MAY_SUSPEND
#define ZR_EXEC_IR_OPCODE_FLAG_KNOWN_MASK \
    (ZR_EXEC_IR_OPCODE_FLAG_PRODUCES_VALUE | ZR_EXEC_IR_OPCODE_FLAG_TERMINATOR | \
     ZR_EXEC_IR_OPCODE_FLAG_MAY_ALLOCATE | ZR_EXEC_IR_OPCODE_FLAG_MAY_THROW | \
     ZR_EXEC_IR_OPCODE_FLAG_MAY_GC | ZR_EXEC_IR_OPCODE_FLAG_MAY_SUSPEND | \
     ZR_EXEC_IR_SCHEMA_FLAG_MAY_DROP)

#define ZR_EXEC_IR_EFFECT_READ_MEMORY ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_EFFECT_WRITE_MEMORY ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_EFFECT_ALLOCATE ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_EFFECT_THROW ((TZrUInt32)1u << 3u)
#define ZR_EXEC_IR_EFFECT_SUSPEND ((TZrUInt32)1u << 4u)
#define ZR_EXEC_IR_EFFECT_DROP ((TZrUInt32)1u << 5u)
#define ZR_EXEC_IR_EFFECT_KNOWN_MASK \
    (ZR_EXEC_IR_EFFECT_READ_MEMORY | ZR_EXEC_IR_EFFECT_WRITE_MEMORY | \
     ZR_EXEC_IR_EFFECT_ALLOCATE | ZR_EXEC_IR_EFFECT_THROW | \
     ZR_EXEC_IR_EFFECT_SUSPEND | ZR_EXEC_IR_EFFECT_DROP)

typedef enum EZrExecIrMemoryClass {
    ZR_EXEC_IR_MEMORY_STACK_FRAME = 0,
    ZR_EXEC_IR_MEMORY_MANAGED_HEAP,
    ZR_EXEC_IR_MEMORY_MODULE_GLOBAL,
    ZR_EXEC_IR_MEMORY_NATIVE_FFI,
    ZR_EXEC_IR_MEMORY_GC,
    ZR_EXEC_IR_MEMORY_OWNERSHIP,
    ZR_EXEC_IR_MEMORY_SCHEDULER_TASK,
    ZR_EXEC_IR_MEMORY_IO,
    ZR_EXEC_IR_MEMORY_CLASS_COUNT
} EZrExecIrMemoryClass;

typedef enum EZrExecIrOpcode {
    ZR_EXEC_IR_OPCODE_INVALID = 0,
#define ZR_EXEC_IR_OP(name, resultArity, operandArity, memoryReads, memoryWrites, flags) \
    ZR_EXEC_IR_OPCODE_##name,
#include "zr_vm_core/exec_ir_opcode.def"
#undef ZR_EXEC_IR_OP
    ZR_EXEC_IR_OPCODE_COUNT
} EZrExecIrOpcode;

typedef struct SZrExecIrOpcodeInfo {
    EZrExecIrOpcode opcode;
    union {
        TZrUInt32 resultArity;
        TZrUInt32 minimumResults;
    };
    union {
        TZrUInt32 operandArity;
        TZrUInt32 maximumOperands;
    };
    TZrUInt32 minimumOperands;
    TZrUInt32 memoryReads;
    TZrUInt32 memoryWrites;
    TZrUInt32 flags;
    TZrUInt32 effects;
    const TZrChar *name;
} SZrExecIrOpcodeInfo;

typedef struct SZrExecIrValue {
    TZrExecIrValueId id;
    union {
        TZrExecIrInstructionId definition;
        TZrExecIrInstructionId definitionInstructionId;
    };
    TZrExecIrTypeToken typeToken;
    EZrExecIrOwnership ownership;
    EZrExecIrNullability nullability;
    TZrUInt32 flags;
} SZrExecIrValue;

typedef struct SZrExecIrPhiIncoming {
    TZrExecIrBlockId predecessor;
    TZrExecIrValueId value;
} SZrExecIrPhiIncoming;

typedef struct SZrExecIrPhi {
    TZrExecIrValueId result;
    SZrExecIrRange incomings;
} SZrExecIrPhi;

typedef struct SZrExecIrInstruction {
    TZrUInt16 opcode;
    TZrUInt16 flags;
    union {
        SZrExecIrRange results;
        SZrExecIrRange resultRange;
    };
    union {
        SZrExecIrRange operands;
        SZrExecIrRange operandRange;
    };
    SZrExecIrRange phiRange;
    SZrExecIrRange successorRange;
    TZrExecIrTypeToken typeToken;
    TZrUInt32 layoutId;
    SZrExecIrRange memoryIn;
    SZrExecIrRange memoryOut;
    TZrExecIrEffectTokenId effectIn;
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrSourceId sourceId;
    TZrExecIrDeoptId deoptId;
    TZrUInt32 bindingRow;
} SZrExecIrInstruction;

#define ZR_EXEC_IR_BLOCK_FLAG_ENTRY ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_BLOCK_FLAG_COLD ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_BLOCK_FLAG_CLEANUP ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION ((TZrUInt32)1u << 3u)

typedef struct SZrExecIrBlock {
    TZrExecIrBlockId id;
    TZrUInt32 flags;
    union {
        SZrExecIrRange instructions;
        SZrExecIrRange instructionRange;
    };
    union {
        SZrExecIrRange predecessors;
        SZrExecIrRange predecessorRange;
    };
    union {
        SZrExecIrRange successors;
        SZrExecIrRange successorRange;
    };
    SZrExecIrRange phis;
    TZrExecIrBlockId immediateDominator;
    TZrExecIrInstructionId terminatorInstructionId;
} SZrExecIrBlock;

typedef enum EZrExecIrFrameSlotKind {
    ZR_EXEC_IR_FRAME_SLOT_VALUE = 0,
    ZR_EXEC_IR_FRAME_SLOT_REFERENCE,
    ZR_EXEC_IR_FRAME_SLOT_SPILL,
    ZR_EXEC_IR_FRAME_SLOT_TEMPORARY,
    ZR_EXEC_IR_FRAME_SLOT_COUNT
} EZrExecIrFrameSlotKind;

typedef struct SZrExecIrFrameSlot {
    TZrUInt32 slotId;
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrMetadataToken typeToken;
    EZrExecIrFrameSlotKind kind;
} SZrExecIrFrameSlot;

typedef struct SZrExecIrFrameLayout {
    TZrUInt32 storageSlotCount;
    TZrUInt32 logicalSlotCount;
    TZrUInt32 parameterPrefixCount;
    TZrUInt32 returnBufferOffset;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    TZrUInt32 parameterCount;
    TZrUInt32 localCount;
    SZrExecIrFrameSlot *slots;
    TZrUInt32 slotCount;
    TZrUInt32 slotCapacity;
    TZrUInt64 layoutHash;
} SZrExecIrFrameLayout;

typedef struct SZrExecIrGcMapEntry {
    TZrExecIrInstructionId site;
    SZrExecIrRange liveRefSlots;
    SZrExecIrRange inlineRefOffsets;
} SZrExecIrGcMapEntry;

typedef struct SZrExecIrGcMap {
    SZrExecIrGcMapEntry *entries;
    TZrUInt32 entryCount;
    TZrUInt32 entryCapacity;
    TZrUInt32 *slotIndexPool;
    TZrUInt32 slotIndexCount;
    TZrUInt32 slotIndexCapacity;
    TZrUInt32 *inlineRefOffsetPool;
    TZrUInt32 inlineRefOffsetCount;
    TZrUInt32 inlineRefOffsetCapacity;
    /* Single-site fields retained for the lightweight model API. */
    TZrUInt32 safepointId;
    TZrExecIrSourceId sourceId;
    SZrExecIrRange rootRange;
} SZrExecIrGcMap;

typedef struct SZrExecIrDeoptState {
    union {
        TZrExecIrDeoptId id;
        TZrExecIrDeoptId deoptId;
    };
    union {
        TZrExecIrSourceId source;
        TZrExecIrSourceId sourceId;
    };
    TZrUInt32 resumeId;
    union {
        SZrExecIrRange reconstruction;
        SZrExecIrRange valueRange;
    };
    TZrUInt32 cleanupState;
} SZrExecIrDeoptState;

typedef struct SZrExecIrSourceMap {
    TZrExecIrSourceId sourceId;
    TZrExecIrInstructionId instructionId;
    TZrUInt32 startOffset;
    TZrUInt32 endOffset;
    TZrUInt32 startLine;
    TZrUInt32 startColumn;
    TZrUInt32 endLine;
    TZrUInt32 endColumn;
} SZrExecIrSourceMap;

typedef struct SZrExecIrConstant {
    TZrExecIrTypeToken typeToken;
    TZrUInt32 flags;
    TZrUInt64 bits;
} SZrExecIrConstant;

typedef struct SZrExecIrLayout {
    TZrUInt32 id;
    TZrExecIrTypeToken typeToken;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt64 layoutHash;
} SZrExecIrLayout;

typedef struct SZrExecIrFunction {
    TZrExecIrFunctionId id;
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    SZrExecutionContract contract;
    TZrExecIrBlockId entryBlockId;

    SZrExecIrValue *values;
    TZrUInt32 valueCount;
    TZrUInt32 valueCapacity;
    SZrExecIrInstruction *instructions;
    TZrUInt32 instructionCount;
    TZrUInt32 instructionCapacity;
    SZrExecIrBlock *blocks;
    TZrUInt32 blockCount;
    TZrUInt32 blockCapacity;

    union {
        TZrExecIrValueId *operandPool;
        TZrExecIrValueId *operands;
    };
    TZrUInt32 operandCount;
    TZrUInt32 operandCapacity;
    union {
        TZrExecIrValueId *resultPool;
        TZrExecIrValueId *results;
    };
    TZrUInt32 resultCount;
    TZrUInt32 resultCapacity;
    TZrExecIrMemoryTokenId *memoryTokenPool;
    TZrUInt32 memoryTokenCount;
    TZrUInt32 memoryTokenCapacity;
    SZrExecIrPhi *phiPool;
    TZrUInt32 phiCount;
    TZrUInt32 phiCapacity;
    SZrExecIrPhiIncoming *phiIncoming;
    TZrUInt32 phiIncomingCount;
    TZrUInt32 phiIncomingCapacity;
    TZrExecIrBlockId *predecessors;
    TZrUInt32 predecessorCount;
    TZrUInt32 predecessorCapacity;
    TZrExecIrBlockId *successors;
    TZrUInt32 successorCount;
    TZrUInt32 successorCapacity;

    SZrExecIrFrameLayout *frameLayout;
    union {
        SZrExecIrGcMap *gcMap;
        /* Compatibility spelling retained for the first model prototype. */
        SZrExecIrGcMap *gcMaps;
    };
    TZrUInt32 gcMapCount;
    TZrUInt32 gcMapCapacity;
    TZrExecIrValueId *gcRoots;
    TZrUInt32 gcRootCount;
    TZrUInt32 gcRootCapacity;
    SZrExecIrDeoptState *deoptStates;
    TZrUInt32 deoptStateCount;
    TZrUInt32 deoptStateCapacity;
    TZrExecIrValueId *deoptValues;
    TZrUInt32 deoptValueCount;
    TZrUInt32 deoptValueCapacity;
    SZrExecIrSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    TZrUInt32 sourceMapCapacity;
    union {
        SZrExecIrStateMap *stateMap;
        /* Plural spelling retained for callers that model the side table as
         * a collection of resume maps. */
        SZrExecIrStateMap *stateMaps;
    };
    TZrBool sealed;
} SZrExecIrFunction;

typedef struct SZrExecIrModule {
    TZrExecIrModuleId id;
    TZrMetadataToken moduleToken;
    TZrUInt64 moduleHash;
    SZrExecutionContract contract;
    SZrExecIrConstant *constants;
    TZrUInt32 constantCount;
    TZrUInt32 constantCapacity;
    SZrExecIrLayout *layouts;
    TZrUInt32 layoutCount;
    TZrUInt32 layoutCapacity;
    SZrExecIrSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    TZrUInt32 sourceMapCapacity;
    SZrExecIrFunction *functions;
    TZrUInt32 functionCount;
    TZrUInt32 functionCapacity;
} SZrExecIrModule;

typedef enum EZrExecIrVerifyLevel {
    ZR_EXEC_IR_VERIFY_STRUCTURE = 1u << 0u,
    ZR_EXEC_IR_VERIFY_SSA = 1u << 1u,
    ZR_EXEC_IR_VERIFY_EFFECT = 1u << 2u,
    ZR_EXEC_IR_VERIFY_ALL = ZR_EXEC_IR_VERIFY_STRUCTURE |
                            ZR_EXEC_IR_VERIFY_SSA |
                            ZR_EXEC_IR_VERIFY_EFFECT
} EZrExecIrVerifyLevel;

ZR_CORE_API void ZrCore_ExecIr_ModuleInit(SZrExecIrModule *module);
ZR_CORE_API void ZrCore_ExecIr_FunctionInit(SZrExecIrFunction *function);
ZR_CORE_API void ZrCore_ExecIr_FrameLayoutInit(SZrExecIrFrameLayout *layout);
ZR_CORE_API void ZrCore_ExecIr_FrameLayoutFree(SZrExecIrFrameLayout *layout);
ZR_CORE_API void ZrCore_ExecIr_GcMapInit(SZrExecIrGcMap *map);
ZR_CORE_API void ZrCore_ExecIr_GcMapFree(SZrExecIrGcMap *map);
ZR_CORE_API void ZrCore_ExecIr_FreeModule(SZrExecIrModule *module);
ZR_CORE_API void ZrCore_ExecIr_FreeFunction(SZrExecIrFunction *function);
ZR_CORE_API TZrBool ZrCore_ExecIr_CloneModule(const SZrExecIrModule *source,
                                              SZrExecIrModule *destination,
                                              SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_CloneFunction(const SZrExecIrFunction *source,
                                                SZrExecIrFunction *destination,
                                                SZrExecIrDiagnostic *diagnostic);

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
ZR_CORE_API const SZrExecIrFunction *ZrCore_ExecIr_ModuleFunctionAtConst(
        const SZrExecIrModule *module, TZrExecIrFunctionId id);

ZR_CORE_API TZrExecIrBlockId ZrCore_ExecIr_FunctionAddBlock(SZrExecIrFunction *function,
                                                             TZrUInt32 flags);
ZR_CORE_API TZrExecIrValueId ZrCore_ExecIr_FunctionAddValue(
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
ZR_CORE_API const SZrExecIrBlock *ZrCore_ExecIr_FunctionBlockAtConst(
        const SZrExecIrFunction *function, TZrExecIrBlockId id);
ZR_CORE_API const SZrExecIrOpcodeInfo *ZrCore_ExecIr_OpcodeInfo(EZrExecIrOpcode opcode);
ZR_CORE_API const TZrChar *ZrCore_ExecIr_OpcodeName(EZrExecIrOpcode opcode);
ZR_CORE_API TZrBool ZrCore_ExecIr_ValidateModule(const SZrExecIrModule *module,
                                                 SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_VerifyModule(const SZrExecIrModule *module,
                                               SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_VerifyFunction(const SZrExecIrFunction *function,
                                                 EZrExecIrVerifyLevel level,
                                                 SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_VerifyEffects(const SZrExecIrFunction *function,
                                                SZrExecIrDiagnostic *diagnostic);

#endif
