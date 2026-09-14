#ifndef ZR_VM_PARSER_EXEC_IR_LOOPS_H
#define ZR_VM_PARSER_EXEC_IR_LOOPS_H

#include <stdint.h>

#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

/*
 * Loop analysis is parser-owned and pointer-free at the contract boundary.
 * A loop record refers to its block and induction rows through offsets into
 * the owning SZrExecIrLoopInfo side arrays.  This makes the result safe to
 * retain in an analysis cache and easy to inspect in optimization remarks.
 */
#define ZR_EXEC_IR_LOOP_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_LOOP_INFO_MAGIC ((TZrUInt32)0x4c4f4f50u)
#define ZR_EXEC_IR_LOOP_TRIP_COUNT_UNKNOWN UINT64_MAX

typedef TZrUInt32 TZrExecIrLoopId;

typedef enum EZrExecIrLoopReason {
    ZR_EXEC_IR_LOOP_REASON_NONE = 0,
    ZR_EXEC_IR_LOOP_REASON_NO_PREHEADER,
    ZR_EXEC_IR_LOOP_REASON_MULTIPLE_ENTRY,
    ZR_EXEC_IR_LOOP_REASON_IRREDUCIBLE,
    ZR_EXEC_IR_LOOP_REASON_ZERO_TRIP,
    ZR_EXEC_IR_LOOP_REASON_TRAPPING,
    ZR_EXEC_IR_LOOP_REASON_EFFECT,
    ZR_EXEC_IR_LOOP_REASON_NOT_INVARIANT,
    ZR_EXEC_IR_LOOP_REASON_OVERFLOW,
    ZR_EXEC_IR_LOOP_REASON_BUDGET,
    ZR_EXEC_IR_LOOP_REASON_SEALED,
    ZR_EXEC_IR_LOOP_REASON_INVALID
} EZrExecIrLoopReason;

typedef enum EZrExecIrInductionOperation {
    ZR_EXEC_IR_INDUCTION_NONE = 0,
    ZR_EXEC_IR_INDUCTION_ADD,
    ZR_EXEC_IR_INDUCTION_SUB,
    ZR_EXEC_IR_INDUCTION_MUL
} EZrExecIrInductionOperation;

typedef struct SZrExecIrInduction {
    TZrExecIrLoopId loopId;
    TZrExecIrValueId valueId;
    TZrExecIrValueId baseValueId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrSourceId sourceId;
    EZrExecIrInductionOperation operation;
    TZrInt64 step;
    TZrInt64 factor;
    TZrBool overflowSafe;
    TZrBool strengthReducible;
} SZrExecIrInduction;

typedef struct SZrExecIrLoop {
    TZrExecIrLoopId id;
    TZrExecIrBlockId headerBlockId;
    TZrExecIrBlockId preheaderBlockId;
    TZrExecIrBlockId latchBlockId;
    TZrUInt32 memberOffset;
    TZrUInt32 memberCount;
    TZrUInt32 inductionOffset;
    TZrUInt32 inductionCount;
    TZrUInt64 tripCountMin;
    TZrUInt64 tripCountMax;
    EZrExecIrLoopReason blockedReason;
    TZrExecIrInstructionId blockedInstructionId;
    TZrBool reducible;
    TZrBool multipleEntry;
    TZrBool hasPreheader;
    TZrBool tripCountKnown;
    TZrBool zeroTrip;
} SZrExecIrLoop;

typedef struct SZrExecIrLoopInfo {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt64 irHash;
    SZrExecIrLoop *loops;
    TZrUInt32 loopCount;
    TZrUInt32 loopCapacity;
    TZrExecIrBlockId *memberBlocks;
    TZrUInt32 memberCount;
    TZrUInt32 memberCapacity;
    SZrExecIrInduction *inductions;
    TZrUInt32 inductionCount;
    TZrUInt32 inductionCapacity;
    TZrUInt32 hoistedInstructionCount;
    TZrUInt32 strengthReducedCount;
    TZrUInt32 blockedInstructionCount;
    TZrBool changed;
} SZrExecIrLoopInfo;

ZR_PARSER_API void ZrParser_ExecIr_LoopInfoInit(SZrExecIrLoopInfo *info);
ZR_PARSER_API void ZrParser_ExecIr_LoopInfoFree(SZrExecIrLoopInfo *info);
ZR_PARSER_API TZrBool ZrParser_ExecIr_AnalyzeLoops(
        const SZrExecIrFunction *function,
        SZrExecIrLoopInfo *info,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API const SZrExecIrLoop *ZrParser_ExecIr_LoopAt(
        const SZrExecIrLoopInfo *info, TZrExecIrLoopId loopId);
ZR_PARSER_API const TZrExecIrBlockId *ZrParser_ExecIr_LoopMembers(
        const SZrExecIrLoopInfo *info, const SZrExecIrLoop *loop);
ZR_PARSER_API const SZrExecIrInduction *ZrParser_ExecIr_InductionAt(
        const SZrExecIrLoopInfo *info, TZrUInt32 index);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_LoopReasonName(
        EZrExecIrLoopReason reason);

/*
 * OptimizeLoops performs only transformations that can be checked locally:
 * an existing single-predecessor preheader receives a pure, non-trapping,
 * memory-free invariant instruction.  It never invents a preheader or
 * rewrites a potentially overflowing recurrence.  The Ex form optionally
 * appends one remark per blocked/hoisted decision.
 */
struct SZrExecIrRemarkSink;
ZR_PARSER_API TZrBool ZrParser_ExecIr_OptimizeLoops(
        SZrExecIrFunction *function,
        SZrExecIrLoopInfo *info,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_OptimizeLoopsEx(
        SZrExecIrFunction *function,
        SZrExecIrLoopInfo *info,
        struct SZrExecIrRemarkSink *remarks,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_StrengthReduce(
        SZrExecIrFunction *function,
        SZrExecIrLoopInfo *info,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_StrengthReduceEx(
        SZrExecIrFunction *function,
        SZrExecIrLoopInfo *info,
        struct SZrExecIrRemarkSink *remarks,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_RunLicm(
        SZrExecIrFunction *function,
        SZrExecIrLoopInfo *info,
        SZrExecIrDiagnostic *diagnostic);

/* Noun-first spellings match the other parser-owned analysis contracts. */
#define ZrParser_ExecIr_LoopInfo_Init ZrParser_ExecIr_LoopInfoInit
#define ZrParser_ExecIr_LoopInfo_Free ZrParser_ExecIr_LoopInfoFree
#define ZrParser_ExecIr_LoopInfo_Analyze ZrParser_ExecIr_AnalyzeLoops
#define ZrParser_ExecIr_LoopInfo_Optimize ZrParser_ExecIr_OptimizeLoops
#define ZrParser_ExecIr_LoopInfo_StrengthReduce ZrParser_ExecIr_StrengthReduce
#define ZrParser_ExecIr_LoopInfo_LoopAt ZrParser_ExecIr_LoopAt
#define ZrParser_ExecIr_LoopInfo_InductionAt ZrParser_ExecIr_InductionAt

#endif /* ZR_VM_PARSER_EXEC_IR_LOOPS_H */
