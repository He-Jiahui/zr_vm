#ifndef ZR_VM_PARSER_EXEC_IR_BINDING_FACTS_H
#define ZR_VM_PARSER_EXEC_IR_BINDING_FACTS_H

/*
 * Parser-owned facts with pointer-free payload records for projecting static
 * member resolution to ExecIR.  The arrays referenced by this type remain
 * owned by the producer;
 * projection only copies row indices into ExecIR instructions.  In
 * particular, no AST/string/member-name pointer is carried across the parser
 * boundary.
 */

#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

#define ZR_EXEC_IR_BINDING_FACTS_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_BINDING_ROW_NONE ZR_CALL_BINDING_SLOT_NONE
#define ZR_EXEC_IR_BINDING_SEGMENT_INDEX_NONE ((TZrUInt32)0xffffffffu)
#define ZR_EXEC_IR_BINDING_MEMBER_NONE ((TZrUInt32)0xffffffffu)

/* A chain segment describes one evaluation step.  FIELD is deliberately
 * separate from ACCESSOR/META: a field still emits the receiver project/load
 * and therefore cannot be folded into the final call row. */
typedef enum EZrExecIrBindingSegmentKind {
    ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD = 0,
    ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR,
    ZR_EXEC_IR_BINDING_SEGMENT_META,
    ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE,
    ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL,
    ZR_EXEC_IR_BINDING_SEGMENT_KIND_COUNT
} EZrExecIrBindingSegmentKind;

/* Common spellings used by producers and tests.  They are aliases, not new
 * enum values, so the serialized kind remains stable. */
#define ZR_EXEC_IR_BINDING_SEGMENT_FIELD ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD
#define ZR_EXEC_IR_BINDING_SEGMENT_FIELD_LOAD ZR_EXEC_IR_BINDING_SEGMENT_RUNTIME_FIELD
#define ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR_CALL ZR_EXEC_IR_BINDING_SEGMENT_ACCESSOR
#define ZR_EXEC_IR_BINDING_SEGMENT_META_CALL ZR_EXEC_IR_BINDING_SEGMENT_META
#define ZR_EXEC_IR_BINDING_SEGMENT_CALLABLE ZR_EXEC_IR_BINDING_SEGMENT_TYPED_CALLABLE
#define ZR_EXEC_IR_BINDING_SEGMENT_CALL ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL
#define ZR_EXEC_IR_BINDING_SEGMENT_FINAL ZR_EXEC_IR_BINDING_SEGMENT_FINAL_CALL

typedef enum EZrExecIrBindingSegmentFlags {
    ZR_EXEC_IR_BINDING_SEGMENT_FLAG_OPTIONAL = (TZrUInt32)1u << 0u,
    ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK = (TZrUInt32)1u << 1u,
    ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL = (TZrUInt32)1u << 2u,
    ZR_EXEC_IR_BINDING_SEGMENT_FLAG_RUNTIME_READ = (TZrUInt32)1u << 3u,
    ZR_EXEC_IR_BINDING_SEGMENT_FLAG_RUNTIME_GUARD = (TZrUInt32)1u << 4u
} EZrExecIrBindingSegmentFlags;

#define ZR_EXEC_IR_BINDING_SEGMENT_FLAG_KNOWN_MASK \
    ((TZrUInt32)(ZR_EXEC_IR_BINDING_SEGMENT_FLAG_OPTIONAL | \
                 ZR_EXEC_IR_BINDING_SEGMENT_FLAG_WRITEBACK | \
                 ZR_EXEC_IR_BINDING_SEGMENT_FLAG_FINAL | \
                 ZR_EXEC_IR_BINDING_SEGMENT_FLAG_RUNTIME_READ | \
                 ZR_EXEC_IR_BINDING_SEGMENT_FLAG_RUNTIME_GUARD))

/* Stable status returned by the extended validator.  A bool-only wrapper is
 * also provided for callers that only need success/failure. */
typedef enum EZrExecIrBindingFactsStatus {
    ZR_EXEC_IR_BINDING_FACTS_OK = 0,
    ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT,
    ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER,
    ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER,
    ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT,
    ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN,
    ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH,
    ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH,
    ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH,
    ZR_EXEC_IR_BINDING_FACTS_INVALID_SLOT,
    ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION,
    ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN,
    ZR_EXEC_IR_BINDING_FACTS_SEALED,
    ZR_EXEC_IR_BINDING_FACTS_STATUS_COUNT
} EZrExecIrBindingFactsStatus;

#define ZR_EXEC_IR_BINDING_STATUS_OK ZR_EXEC_IR_BINDING_FACTS_OK
#define ZR_EXEC_IR_BINDING_STATUS_INVALID_ARGUMENT ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT
#define ZR_EXEC_IR_BINDING_STATUS_UNKNOWN_MEMBER ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER
#define ZR_EXEC_IR_BINDING_STATUS_AMBIGUOUS_MEMBER ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER
#define ZR_EXEC_IR_BINDING_STATUS_MISSING_CONTRACT ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT
#define ZR_EXEC_IR_BINDING_STATUS_INVALID_TOKEN ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN
#define ZR_EXEC_IR_BINDING_STATUS_SIGNATURE_MISMATCH ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH
#define ZR_EXEC_IR_BINDING_STATUS_MODULE_MISMATCH ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH
#define ZR_EXEC_IR_BINDING_STATUS_LAYOUT_MISMATCH ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH
#define ZR_EXEC_IR_BINDING_STATUS_INVALID_SLOT ZR_EXEC_IR_BINDING_FACTS_INVALID_SLOT
#define ZR_EXEC_IR_BINDING_STATUS_STALE_GENERATION ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION
#define ZR_EXEC_IR_BINDING_STATUS_INVALID_CHAIN ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN

/* Status-name aliases retain the longer spelling used by some generated
 * parser clients. */
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_OK ZR_EXEC_IR_BINDING_FACTS_OK
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_INVALID_ARGUMENT \
    ZR_EXEC_IR_BINDING_FACTS_INVALID_ARGUMENT
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_UNKNOWN_MEMBER \
    ZR_EXEC_IR_BINDING_FACTS_UNKNOWN_MEMBER
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_AMBIGUOUS_MEMBER \
    ZR_EXEC_IR_BINDING_FACTS_AMBIGUOUS_MEMBER
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_MISSING_CONTRACT \
    ZR_EXEC_IR_BINDING_FACTS_MISSING_CONTRACT
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_INVALID_TOKEN \
    ZR_EXEC_IR_BINDING_FACTS_INVALID_TOKEN
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_SIGNATURE_MISMATCH \
    ZR_EXEC_IR_BINDING_FACTS_SIGNATURE_MISMATCH
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_MODULE_MISMATCH \
    ZR_EXEC_IR_BINDING_FACTS_MODULE_MISMATCH
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_LAYOUT_MISMATCH \
    ZR_EXEC_IR_BINDING_FACTS_LAYOUT_MISMATCH
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_INVALID_SLOT \
    ZR_EXEC_IR_BINDING_FACTS_INVALID_SLOT
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_STALE_GENERATION \
    ZR_EXEC_IR_BINDING_FACTS_STALE_GENERATION
#define ZR_EXEC_IR_BINDING_FACTS_STATUS_INVALID_CHAIN \
    ZR_EXEC_IR_BINDING_FACTS_INVALID_CHAIN

/* One chain step.  IDs are local ExecIR identities; zero means “not
 * materialized by this producer”.  memberToken/memberId are stable identities
 * and intentionally contain no source/member-name text. */
typedef struct SZrExecIrBindingSegment {
    union {
        TZrUInt32 index;
        TZrUInt32 segmentIndex;
    };
    EZrExecIrBindingSegmentKind kind;
    TZrUInt32 flags;
    union {
        TZrExecIrInstructionId instructionId;
        TZrExecIrInstructionId operationInstructionId;
    };
    TZrExecIrInstructionId receiverInstructionId;
    TZrExecIrInstructionId writebackInstructionId;
    union {
        TZrExecIrValueId receiverValueId;
        TZrExecIrValueId receiverValue;
    };
    union {
        TZrExecIrValueId resultValueId;
        TZrExecIrValueId resultValue;
    };
    TZrMetadataToken memberToken;
    TZrExecIrTypeToken receiverTypeToken;
    TZrExecIrTypeToken resultTypeToken;
    TZrUInt32 memberId;
    TZrUInt32 layoutId;
    TZrUInt32 layoutVersion;
    TZrUInt64 layoutHash;
    TZrUInt32 operation;
    TZrUInt32 bindingRow;
    TZrExecIrSourceId sourceId;
} SZrExecIrBindingSegment;

/* A row is the only place where the complete call contract is stored.  The
 * ExecIR instruction stores rowIndex/bindingRow, keeping instruction data
 * fixed width and pointer-free. */
typedef struct SZrExecIrBindingRow {
    union {
        TZrUInt32 rowIndex;
        TZrUInt32 bindingRow;
    };
    union {
        TZrExecIrInstructionId instructionId;
        TZrExecIrInstructionId instructionIndex;
    };
    union {
        TZrUInt32 segmentIndex;
        TZrUInt32 chainSegmentIndex;
    };
    SZrCallBindingContract contract;
    SZrCallBindingLocation location;
    TZrExecIrSourceId sourceId;
} SZrExecIrBindingRow;

/* The producer may leave arrays borrowed for the duration of a projection;
 * no pointer is retained in the destination function.  expectedHash, when
 * non-zero, pins the exact facts payload and catches stale/reordered rows. */
typedef struct SZrExecIrBindingFacts {
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrMetadataToken functionToken;
    union {
        TZrUInt64 signatureHash;
        TZrUInt64 functionSignatureHash;
    };
    TZrUInt64 moduleHash;
    TZrUInt64 generation;
    TZrUInt64 expectedHash;
    const SZrExecIrBindingSegment *segments;
    TZrUInt32 segmentCount;
    union {
        const SZrExecIrBindingRow *rows;
        const SZrExecIrBindingRow *bindingRows;
    };
    union {
        TZrUInt32 rowCount;
        TZrUInt32 bindingRowCount;
    };
    union {
        TZrUInt32 finalSegmentIndex;
        TZrUInt32 finalCallSegmentIndex;
    };
} SZrExecIrBindingFacts;

ZR_PARSER_API void ZrParser_ExecIr_BindingFacts_Init(
        SZrExecIrBindingFacts *facts);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_BindingFacts_Hash(
        const SZrExecIrBindingFacts *facts);
ZR_PARSER_API EZrExecIrBindingFactsStatus ZrParser_ExecIr_BindingFacts_ValidateEx(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BindingFacts_Validate(
        const SZrExecIrBindingFacts *facts,
        const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ProjectBindingFacts(
        const SZrExecIrBindingFacts *facts,
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

/* Alias with the noun-first spelling used by a few parser clients. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_BindingFacts_Project(
        const SZrExecIrBindingFacts *facts,
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

ZR_PARSER_API const SZrExecIrBindingRow *ZrParser_ExecIr_BindingFacts_RowAt(
        const SZrExecIrBindingFacts *facts, TZrUInt32 rowIndex);
ZR_PARSER_API const SZrExecIrBindingSegment *ZrParser_ExecIr_BindingFacts_SegmentAt(
        const SZrExecIrBindingFacts *facts, TZrUInt32 segmentIndex);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_BindingFacts_StatusName(
        EZrExecIrBindingFactsStatus status);

#endif /* ZR_VM_PARSER_EXEC_IR_BINDING_FACTS_H */
