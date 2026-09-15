#ifndef ZR_VM_PARSER_EXEC_IR_PROJECTIONS_H
#define ZR_VM_PARSER_EXEC_IR_PROJECTIONS_H

#include "zr_vm_core/exec_ir.h"
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
    TZrUInt32 layoutId;
    TZrExecIrSourceId sourceId;
    TZrExecIrDeoptId deoptId;
    TZrUInt32 bindingRow;
} SZrExecBcInstruction;

typedef struct SZrExecBcBlock {
    TZrExecIrBlockId id;
    SZrExecIrRange instructions;
    SZrExecIrRange predecessors;
    SZrExecIrRange successors;
    SZrExecIrRange phis;
    TZrExecIrInstructionId terminatorInstructionId;
} SZrExecBcBlock;

typedef struct SZrExecIrProjectionSourceMap {
    TZrExecIrSourceId sourceId;
    TZrUInt32 pc;
} SZrExecIrProjectionSourceMap;

typedef struct SZrExecBcProjection {
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    TZrExecIrBlockId entryBlockId;
    TZrUInt64 frameLayoutHash;
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
    SZrExecIrProjectionSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    TZrUInt32 gcMapCount;
    TZrUInt32 deoptStateCount;
    TZrBool stateMapPresent;
    TZrUInt32 unsupportedInstructionId;
    TZrBool runnable;
    TZrUInt32 ownershipTag;
} SZrExecBcProjection;

typedef struct SZrAotIrProjection {
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    TZrExecIrBlockId entryBlockId;
    TZrUInt64 frameLayoutHash;
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
    SZrExecutionContract contract;
    SZrExecIrProjectionSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    TZrUInt32 gcMapCount;
    TZrUInt32 deoptStateCount;
    TZrBool stateMapPresent;
    TZrUInt32 unsupportedInstructionId;
    TZrBool runnable; /* false until a backend emits executable C/LLVM */
    TZrUInt32 ownershipTag;
} SZrAotIrProjection;

#define ZR_EXEC_IR_PROJECTION_TAG ((TZrUInt32)0x50524a31u)

ZR_PARSER_API void ZrParser_ExecBcProjection_Free(SZrExecBcProjection *projection);
ZR_PARSER_API void ZrParser_AotIrProjection_Free(SZrAotIrProjection *projection);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerExecBc(
        const SZrExecIrFunction *function, SZrExecBcProjection *output,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerAot(
        const SZrExecIrFunction *function, SZrAotIrProjection *output,
        SZrExecIrDiagnostic *diagnostic);

/* Shared implementation seam; kept public for the two C translation units,
 * but it only transports stable IDs and owned arrays. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildProjection(
        const SZrExecIrFunction *function, SZrExecBcProjection *output,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_ExecIr_MoveProjectionToAot(
        SZrExecBcProjection *source, SZrAotIrProjection *destination);

#endif
