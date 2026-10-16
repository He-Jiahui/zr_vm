#ifndef ZR_VM_PARSER_EXEC_IR_PROJECTIONS_H
#define ZR_VM_PARSER_EXEC_IR_PROJECTIONS_H

#include "zr_vm_core/aot_ir.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/conf.h"

/* Fixed-width, pointer-free instruction representation used by both initial
 * projections. Ranges refer to the copied side pools in the projection. */
typedef struct SZrExecBcInstruction {
    TZrUInt16 opcode;
    TZrUInt16 flags;
    TZrUInt32 pc;
    SZrExecIrRange operands;
    SZrExecIrRange results;
    SZrExecIrRange phiRange;
    SZrExecIrRange successorRange;
    SZrExecIrRange memoryIn;
    SZrExecIrRange memoryOut;
    TZrExecIrEffectTokenId effectIn;
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrTypeToken typeToken;
    TZrExecIrTypeToken matchTypeToken;
    TZrUInt32 layoutId;
    TZrExecIrSourceId sourceId;
    TZrExecIrDeoptId deoptId;
    TZrUInt32 bindingRow;
} SZrExecBcInstruction;

typedef struct SZrExecBcBlock {
    TZrExecIrBlockId id;
    TZrUInt32 flags;
    SZrExecIrRange instructions;
    SZrExecIrRange predecessors;
    SZrExecIrRange successors;
    SZrExecIrRange phis;
    TZrExecIrInstructionId terminatorInstructionId;
} SZrExecBcBlock;

typedef struct SZrExecIrProjectionSourceMap {
    TZrExecIrSourceId sourceId;
    TZrUInt32 pc;
    TZrUInt32 startOffset;
    TZrUInt32 endOffset;
    TZrUInt32 startLine;
    TZrUInt32 startColumn;
    TZrUInt32 endLine;
    TZrUInt32 endColumn;
} SZrExecIrProjectionSourceMap;

/* Ordered physical-slot move performed when entering the successor of edge.
 * A split critical edge uses its synthetic predecessor block as edge. */
typedef struct SZrExecBcPhiMove {
    TZrExecIrBlockId edge;
    TZrUInt32 sourceSlot;
    TZrUInt32 destinationSlot;
} SZrExecBcPhiMove;

typedef TZrBool (*FZrExecBcPhiMoveConsumer)(void *userData,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 sourceSlot);

typedef struct SZrExecBcProjection {
    TZrExecIrFunctionId functionId;
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    TZrExecIrBlockId entryBlockId;
    TZrUInt64 frameLayoutHash;
    TZrUInt32 logicalSlotCount;
    TZrUInt32 storageSlotCount;
    TZrUInt32 parameterPrefixBytes;
    TZrUInt32 returnAreaOffset;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    SZrExecIrFrameSlot *frameSlots;
    TZrUInt32 frameSlotCount;
    TZrUInt32 instructionCount;
    TZrUInt32 *opcodes; /* compatibility view; same order as instructions */
    SZrExecBcInstruction *instructions;
    TZrExecIrValueId *operands;
    TZrUInt32 operandCount;
    TZrExecIrValueId *results;
    TZrUInt32 resultCount;
    /* Owns the token pool referenced by instruction memoryIn/memoryOut. */
    TZrExecIrMemoryTokenId *memoryTokens;
    TZrUInt32 memoryTokenCount;
    TZrUInt32 *valueSlots; /* valueId -> physical slot, no slot reuse */
    TZrUInt32 valueSlotCount;
    TZrUInt32 physicalSlotCount; /* includes reserved sparse frame slots */
    SZrExecIrValue *slotValues; /* physical slot -> owned value metadata */
    SZrExecBcBlock *blocks;
    TZrUInt32 blockCount;
    TZrUInt32 syntheticBlockCount;
    TZrExecIrBlockId *predecessors;
    TZrUInt32 predecessorCount;
    TZrExecIrBlockId *successors;
    TZrUInt32 successorCount;
    SZrExecIrPhi *phis;
    TZrUInt32 phiCount;
    SZrExecIrPhiIncoming *phiIncomings;
    TZrUInt32 phiIncomingCount;
    TZrUInt32 phiCopyCount;
    TZrExecIrValueId *phiCopySources;
    TZrExecIrValueId *phiCopyDestinations;
    TZrExecIrBlockId *phiCopyEdges;
    TZrUInt32 temporarySlotCount;
    TZrUInt32 phiTemporarySlot;
    TZrUInt32 phiMoveCount;
    SZrExecBcPhiMove *phiMoves;
    SZrExecIrProjectionSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    SZrExecIrConstant *constants;
    TZrUInt32 constantCount;
    SZrExecIrLayout *layouts;
    TZrUInt32 layoutCount;
    TZrUInt32 gcMapCount;
    SZrExecIrGcMap gcMap;
    TZrExecIrValueId *gcRoots;
    TZrUInt32 gcRootCount;
    TZrBool gcMapPresent;
    TZrUInt32 deoptStateCount;
    SZrExecIrDeoptState *deoptStates;
    TZrExecIrValueId *deoptValues;
    TZrUInt32 deoptValueCount;
    SZrExecIrDeoptAggregate *deoptAggregates;
    TZrUInt32 deoptAggregateCount;
    SZrExecIrDeoptAggregateField *deoptAggregateFields;
    TZrUInt32 deoptAggregateFieldCount;
    TZrBool stateMapPresent;
    /* An identity-matched map with no recovery entries or side-pool values. */
    TZrBool stateMapEmpty;
    TZrUInt32 unsupportedInstructionId;
    TZrBool runnable;
    TZrUInt32 ownershipTag;
} SZrExecBcProjection;

typedef struct SZrAotIrProjection {
    TZrExecIrFunctionId functionId;
    TZrMetadataToken functionToken;
    /* Preserve the source function's binding-row schema across APIs that
     * consume this projection without the owning ExecIR function. */
    TZrUInt32 bindingRowsSchemaVersion;
    TZrUInt64 signatureHash;
    SZrAotIrCallableAbi callableAbi;
    TZrExecIrBlockId entryBlockId;
    TZrUInt64 frameLayoutHash;
    TZrUInt32 logicalSlotCount;
    TZrUInt32 storageSlotCount;
    TZrUInt32 parameterPrefixBytes;
    TZrUInt32 returnAreaOffset;
    TZrUInt32 frameByteSize;
    TZrUInt32 frameByteAlign;
    SZrExecIrFrameSlot *frameSlots;
    TZrUInt32 frameSlotCount;
    TZrUInt32 instructionCount;
    TZrUInt32 *opcodes;
    SZrExecBcInstruction *instructions;
    TZrExecIrValueId *operands;
    TZrUInt32 operandCount;
    TZrExecIrValueId *results;
    TZrUInt32 resultCount;
    /* Owns the token pool referenced by instruction memoryIn/memoryOut. */
    TZrExecIrMemoryTokenId *memoryTokens;
    TZrUInt32 memoryTokenCount;
    TZrUInt32 *valueSlots;
    TZrUInt32 valueSlotCount;
    TZrUInt32 physicalSlotCount;
    SZrExecIrValue *slotValues;
    SZrExecBcBlock *blocks;
    TZrUInt32 blockCount;
    TZrUInt32 syntheticBlockCount;
    TZrExecIrBlockId *predecessors;
    TZrUInt32 predecessorCount;
    TZrExecIrBlockId *successors;
    TZrUInt32 successorCount;
    SZrExecIrPhi *phis;
    TZrUInt32 phiCount;
    SZrExecIrPhiIncoming *phiIncomings;
    TZrUInt32 phiIncomingCount;
    TZrUInt32 phiCopyCount;
    TZrExecIrValueId *phiCopySources;
    TZrExecIrValueId *phiCopyDestinations;
    TZrExecIrBlockId *phiCopyEdges;
    TZrUInt32 temporarySlotCount;
    TZrUInt32 phiTemporarySlot;
    TZrUInt32 phiMoveCount;
    SZrExecBcPhiMove *phiMoves;
    SZrExecutionContract contract;
    SZrExecIrProjectionSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    SZrExecIrConstant *constants;
    TZrUInt32 constantCount;
    SZrExecIrLayout *layouts;
    TZrUInt32 layoutCount;
    TZrUInt32 gcMapCount;
    SZrExecIrGcMap gcMap;
    TZrExecIrValueId *gcRoots;
    TZrUInt32 gcRootCount;
    TZrBool gcMapPresent;
    TZrUInt32 deoptStateCount;
    SZrExecIrDeoptState *deoptStates;
    TZrExecIrValueId *deoptValues;
    TZrUInt32 deoptValueCount;
    SZrExecIrDeoptAggregate *deoptAggregates;
    TZrUInt32 deoptAggregateCount;
    SZrExecIrDeoptAggregateField *deoptAggregateFields;
    TZrUInt32 deoptAggregateFieldCount;
    TZrBool stateMapPresent;
    TZrUInt32 unsupportedInstructionId;
    TZrBool runnable; /* false until a backend emits executable C/LLVM */
    SZrExecIrStateMap stateMap; /* owned logical recovery entries and side pools */
    TZrUInt32 ownershipTag;
} SZrAotIrProjection;

#define ZR_EXEC_IR_PROJECTION_TAG ((TZrUInt32)0x50524a31u)

ZR_PARSER_API void ZrParser_ExecBcProjection_Free(SZrExecBcProjection *projection);
ZR_PARSER_API TZrBool ZrParser_ExecBcProjection_ExecutePhiMoves(
        const SZrExecBcProjection *projection, TZrExecIrBlockId edge,
        void *slots, TZrUInt32 slotCount,
        FZrExecBcPhiMoveConsumer consumer, void *userData,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_AotIrProjection_Free(SZrAotIrProjection *projection);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerExecBc(
        const SZrExecIrFunction *function, SZrExecBcProjection *output,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerAotWithConstants(
        const SZrExecIrFunction *function,
        const SZrExecIrConstant *constants, TZrUInt32 constantCount,
        SZrAotIrProjection *output, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerAotWithConstantsAndLayouts(
        const SZrExecIrFunction *function,
        const SZrExecIrConstant *constants, TZrUInt32 constantCount,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        SZrAotIrProjection *output, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerAotWithCanonicalCallable(
        const SZrExecIrFunction *function,
        const SZrExecIrConstant *constants, TZrUInt32 constantCount,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        const struct SZrSemanticContext *context, TZrTypeId callableTypeId,
        SZrAotIrProjection *output, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerAot(
        const SZrExecIrFunction *function, SZrAotIrProjection *output,
        SZrExecIrDiagnostic *diagnostic);

/* Shared implementation seam; kept public for the two C translation units,
 * but it only transports stable IDs and owned arrays. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildProjection(
        const SZrExecIrFunction *function, SZrExecBcProjection *output,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildProjectionWithConstants(
        const SZrExecIrFunction *function,
        const SZrExecIrConstant *constants, TZrUInt32 constantCount,
        SZrExecBcProjection *output, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildProjectionWithConstantsAndLayouts(
        const SZrExecIrFunction *function,
        const SZrExecIrConstant *constants, TZrUInt32 constantCount,
        const SZrExecIrLayout *layouts, TZrUInt32 layoutCount,
        SZrExecBcProjection *output, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_ExecIr_MoveProjectionToAot(
        SZrExecBcProjection *source, SZrAotIrProjection *destination);

#endif
