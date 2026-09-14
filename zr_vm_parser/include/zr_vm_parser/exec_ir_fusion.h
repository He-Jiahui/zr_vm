#ifndef ZR_VM_PARSER_EXEC_IR_FUSION_H
#define ZR_VM_PARSER_EXEC_IR_FUSION_H

/*
 * Parser-owned ExecBC fusion contract.
 *
 * Fusion is a projection of ExecIR.  It never mutates the source function and
 * it never stores a runtime pointer in the generated records.  The emitted
 * instruction stream is deliberately the same width as SZrInstruction
 * (operationCode/operandExtra/operand); operands which do not fit that word
 * are kept in the pointer-free side table.
 */

#include <stddef.h>
#include <stdint.h>

#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

/* Optional static-binding witness.  Keep this as a forward declaration so
 * consumers that do not use binding facts do not inherit the facts table's
 * producer-only declarations through the fusion ABI. */
typedef struct SZrExecIrBindingFacts SZrExecIrBindingFacts;

#define ZR_EXEC_BC_FUSION_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_BC_FUSION_STORAGE_TAG ((TZrUInt32)0x46555331u)
#define ZR_EXEC_BC_FUSION_INVALID_INDEX ((TZrUInt32)UINT32_MAX)
#define ZR_EXEC_BC_FUSION_MAX_OPERANDS ((TZrUInt32)8u)
#define ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS ((TZrUInt32)2u)
#define ZR_EXEC_BC_FUSION_MAX_SOURCE_EVENTS ((TZrUInt32)2u)

/* Existing instruction codes occupy the low part of the u16 space.  Fusion
 * codes are appended after that generated range; no existing code is
 * renumbered or widened. */
/* Keep this calculation wide until the capacity proof below.  Casting the
 * base to u16 first would turn a future dispatch-table overflow into an
 * apparently valid wrapped opcode range. */
#define ZR_EXEC_BC_FUSION_OPCODE_BASE \
    ((TZrUInt32)ZR_INSTRUCTION_ENUM(ENUM_MAX))

typedef SZrInstruction SZrExecBcFixedInstruction;
typedef SZrInstruction SZrExecBcFusionInstruction;

_Static_assert(sizeof(SZrExecBcFixedInstruction) == sizeof(SZrInstruction),
               "ExecBC fusion instruction must remain fixed width");
_Static_assert(offsetof(SZrInstruction, operationCode) == 0u &&
                       offsetof(SZrInstruction, operandExtra) == sizeof(TZrUInt16) &&
                       offsetof(SZrInstruction, operand) ==
                               sizeof(TZrUInt16) * 2u,
               "ExecBC fusion instruction layout must match SZrInstruction");

#include "zr_vm_parser/execbc_fusion_patterns_generated.h"

typedef enum EZrExecBcFusionPattern {
#define ZR_EXEC_BC_FUSION_PATTERN_ROW(name, head, tail, constraints, result, cost, boundary) \
    ZR_EXEC_BC_FUSION_PATTERN_##name,
ZR_EXEC_BC_FUSION_PATTERN_ROWS(ZR_EXEC_BC_FUSION_PATTERN_ROW)
#undef ZR_EXEC_BC_FUSION_PATTERN_ROW
    ZR_EXEC_BC_FUSION_PATTERN_COUNT
} EZrExecBcFusionPattern;

_Static_assert(ZR_EXEC_BC_FUSION_PATTERN_COUNT ==
                       ZR_EXEC_BC_FUSION_GENERATED_PATTERN_COUNT,
               "generated fusion pattern count must match the public enum");
_Static_assert(ZR_EXEC_BC_FUSION_PATTERN_COUNT != 0u,
               "generated fusion schema must contain at least one pattern");
_Static_assert((TZrUInt64)ZR_EXEC_BC_FUSION_OPCODE_BASE +
                               ZR_EXEC_BC_FUSION_PATTERN_COUNT <=
                       (TZrUInt64)UINT16_MAX + 1u,
               "fusion opcodes must fit the existing u16 instruction field");

/* The opcode is derived, never serialized as a pointer-sized value. */
#define ZR_EXEC_BC_FUSION_OPCODE(pattern) \
    ((TZrUInt16)((TZrUInt32)ZR_EXEC_BC_FUSION_OPCODE_BASE + \
                 (TZrUInt32)(pattern)))

/* Names used by the early guide and by downstream lowering prototypes.  They
 * are aliases for the canonical generated rows, not additional patterns. */
#define ZR_EXEC_BC_FUSION_PATTERN_CMP_BRANCH_INT \
    ZR_EXEC_BC_FUSION_PATTERN_COMPARE_BRANCH_INT
#define ZR_EXEC_BC_FUSION_PATTERN_INDEX_LOAD \
    ZR_EXEC_BC_FUSION_PATTERN_INDEX_LOAD_STORE
#define ZR_EXEC_BC_FUSION_PATTERN_MEMBER_CALL \
    ZR_EXEC_BC_FUSION_PATTERN_BINDING_CALL
#define ZR_EXEC_BC_FUSION_PATTERN_INCREMENT_BRANCH \
    ZR_EXEC_BC_FUSION_PATTERN_INCREMENT_LOOP_BRANCH

/* Constraint bits are data in the generated pattern table, not free-form
 * predicates.  This keeps matcher decisions deterministic and auditable. */
typedef enum EZrExecBcFusionConstraint {
    ZR_EXEC_BC_FUSION_CONSTRAINT_NONE = 0u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_TYPED_OPERANDS = (TZrUInt32)1u << 0u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_SAME_RESULT_OPERAND = (TZrUInt32)1u << 1u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_NO_INTERVENING_EFFECT = (TZrUInt32)1u << 2u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_RESULT_SINGLE_USE = (TZrUInt32)1u << 3u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_LAYOUT_PROVEN = (TZrUInt32)1u << 4u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_BINDING_RESOLVED = (TZrUInt32)1u << 5u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_BRANCH_TARGET = (TZrUInt32)1u << 6u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_PRESERVE_BOUNDARY = (TZrUInt32)1u << 7u,
    ZR_EXEC_BC_FUSION_CONSTRAINT_INDEX_STORE_VARIANT = (TZrUInt32)1u << 8u
} EZrExecBcFusionConstraint;

#define ZR_EXEC_BC_FUSION_CONSTRAINT_TYPED \
    ZR_EXEC_BC_FUSION_CONSTRAINT_TYPED_OPERANDS
#define ZR_EXEC_BC_FUSION_CONSTRAINT_SAME_RESULT \
    ZR_EXEC_BC_FUSION_CONSTRAINT_SAME_RESULT_OPERAND
#define ZR_EXEC_BC_FUSION_CONSTRAINT_NO_EFFECT \
    ZR_EXEC_BC_FUSION_CONSTRAINT_NO_INTERVENING_EFFECT
#define ZR_EXEC_BC_FUSION_CONSTRAINT_SINGLE_USE \
    ZR_EXEC_BC_FUSION_CONSTRAINT_RESULT_SINGLE_USE

typedef enum EZrExecBcFusionResultForm {
    ZR_EXEC_BC_FUSION_RESULT_NONE = 0,
    ZR_EXEC_BC_FUSION_RESULT_OF_HEAD,
    ZR_EXEC_BC_FUSION_RESULT_OF_TAIL,
    ZR_EXEC_BC_FUSION_RESULT_COUNT
} EZrExecBcFusionResultForm;

typedef enum EZrExecBcFusionBoundaryMask {
    ZR_EXEC_BC_FUSION_BOUNDARY_NONE = 0u,
    ZR_EXEC_BC_FUSION_BOUNDARY_DEBUG = (TZrUInt32)1u << 0u,
    ZR_EXEC_BC_FUSION_BOUNDARY_SAFEPOINT = (TZrUInt32)1u << 1u,
    ZR_EXEC_BC_FUSION_BOUNDARY_EXCEPTION = (TZrUInt32)1u << 2u,
    ZR_EXEC_BC_FUSION_BOUNDARY_REENTRANT = (TZrUInt32)1u << 3u
} EZrExecBcFusionBoundaryMask;

typedef struct SZrExecBcFusionPatternInfo {
    EZrExecBcFusionPattern pattern;
    EZrExecIrOpcode headOpcode;
    EZrExecIrOpcode tailOpcode;
    TZrUInt32 constraints;
    EZrExecBcFusionResultForm resultForm;
    TZrUInt32 dispatchBenefit;
    TZrUInt32 codeCostBytes;
    TZrUInt32 boundaryMask;
    const TZrChar *name;
} SZrExecBcFusionPatternInfo;

typedef enum EZrExecBcFusionFallbackReason {
    ZR_EXEC_BC_FUSION_FALLBACK_NONE = 0,
    ZR_EXEC_BC_FUSION_FALLBACK_NO_PATTERN,
    ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH,
    ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH,
    ZR_EXEC_BC_FUSION_FALLBACK_EFFECT_MISMATCH,
    ZR_EXEC_BC_FUSION_FALLBACK_MULTIPLE_USE,
    ZR_EXEC_BC_FUSION_FALLBACK_LAYOUT_UNKNOWN,
    ZR_EXEC_BC_FUSION_FALLBACK_BINDING_MISSING,
    ZR_EXEC_BC_FUSION_FALLBACK_BRANCH_TARGET,
    ZR_EXEC_BC_FUSION_FALLBACK_DEBUG_BOUNDARY,
    ZR_EXEC_BC_FUSION_FALLBACK_SAFEPOINT,
    ZR_EXEC_BC_FUSION_FALLBACK_EXCEPTION_BOUNDARY,
    ZR_EXEC_BC_FUSION_FALLBACK_REENTRANT_BOUNDARY,
    ZR_EXEC_BC_FUSION_FALLBACK_OPERAND_OVERFLOW,
    ZR_EXEC_BC_FUSION_FALLBACK_SIDE_TABLE,
    ZR_EXEC_BC_FUSION_FALLBACK_BUDGET,
    ZR_EXEC_BC_FUSION_FALLBACK_DISABLED,
    ZR_EXEC_BC_FUSION_FALLBACK_INVALID_INPUT,
    ZR_EXEC_BC_FUSION_FALLBACK_COUNT
} EZrExecBcFusionFallbackReason;

/* Short aliases used by generated clients. */
#define ZR_EXEC_BC_FUSION_FALLBACK_TYPE ZR_EXEC_BC_FUSION_FALLBACK_TYPE_MISMATCH
#define ZR_EXEC_BC_FUSION_FALLBACK_RESULT ZR_EXEC_BC_FUSION_FALLBACK_RESULT_MISMATCH
#define ZR_EXEC_BC_FUSION_FALLBACK_EFFECT ZR_EXEC_BC_FUSION_FALLBACK_EFFECT_MISMATCH
#define ZR_EXEC_BC_FUSION_FALLBACK_SIDE_TABLE_LIMIT ZR_EXEC_BC_FUSION_FALLBACK_SIDE_TABLE

typedef enum EZrExecBcFusionInvalidationReason {
    ZR_EXEC_BC_FUSION_INVALIDATION_NONE = 0,
    ZR_EXEC_BC_FUSION_INVALIDATION_GENERATION,
    ZR_EXEC_BC_FUSION_INVALIDATION_SIGNATURE,
    ZR_EXEC_BC_FUSION_INVALIDATION_MODULE,
    ZR_EXEC_BC_FUSION_INVALIDATION_LAYOUT,
    ZR_EXEC_BC_FUSION_INVALIDATION_INPUT_CHANGED,
    ZR_EXEC_BC_FUSION_INVALIDATION_GUARD,
    ZR_EXEC_BC_FUSION_INVALIDATION_EXPLICIT,
    ZR_EXEC_BC_FUSION_INVALIDATION_INVALID_PLAN,
    ZR_EXEC_BC_FUSION_INVALIDATION_COUNT
} EZrExecBcFusionInvalidationReason;

typedef enum EZrExecBcFusionStatus {
    ZR_EXEC_BC_FUSION_OK = 0,
    ZR_EXEC_BC_FUSION_INVALID_ARGUMENT,
    ZR_EXEC_BC_FUSION_INVALID_INPUT,
    ZR_EXEC_BC_FUSION_SEALED,
    ZR_EXEC_BC_FUSION_CONTRACT_MISMATCH,
    ZR_EXEC_BC_FUSION_STALE_GENERATION,
    ZR_EXEC_BC_FUSION_OUT_OF_MEMORY,
    ZR_EXEC_BC_FUSION_CAPACITY_OVERFLOW,
    ZR_EXEC_BC_FUSION_INVALID_PLAN,
    ZR_EXEC_BC_FUSION_STATUS_COUNT
} EZrExecBcFusionStatus;

typedef struct SZrExecBcFusionDiagnostic {
    EZrExecBcFusionStatus status;
    EZrExecBcFusionFallbackReason fallbackReason;
    EZrExecBcFusionInvalidationReason invalidationReason;
    EZrExecBcFusionPattern pattern;
    TZrUInt32 headInstructionId;
    TZrUInt32 tailInstructionId;
    TZrUInt32 sourceId;
    TZrUInt32 expected;
    TZrUInt32 actual;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrExecBcFusionDiagnostic;

typedef struct SZrExecBcFusionPlan SZrExecBcFusionPlan;

typedef struct SZrExecBcPatternOptions {
    TZrUInt32 schemaVersion;
    TZrUInt32 enabledPatternMask;
    TZrUInt32 maxFusedCount;
    TZrUInt32 maxSideTableEntries;
    TZrUInt32 maxFallbackEntries;
    TZrUInt32 maxCodeBytes;
    TZrUInt32 minDispatchBenefit;
    TZrUInt64 activeGeneration;
    TZrUInt64 expectedSignatureHash;
    TZrUInt64 expectedModuleHash;
    TZrUInt64 expectedLayoutHash;
    TZrBool preserveObservableBoundaries;
    TZrBool requireSealed;
    TZrBool allowUnresolvedBinding;
    TZrBool reserved;
    SZrExecBcFusionPlan *output;
    /* When supplied, this validated table can authorize row index zero.  The
     * scalar ExecIR field uses zero as its historical unpublished marker, so
     * fusion must never infer that the first facts row is resolved without an
     * explicit witness. */
    const SZrExecIrBindingFacts *bindingFacts;
} SZrExecBcPatternOptions;

typedef struct SZrExecBcFusionSideEntry {
    EZrExecBcFusionPattern pattern;
    EZrExecIrOpcode headOpcode;
    EZrExecIrOpcode tailOpcode;
    TZrUInt32 headInstructionId;
    TZrUInt32 tailInstructionId;
    TZrUInt32 headSourceId;
    TZrUInt32 tailSourceId;
    TZrUInt32 headResumeId;
    TZrUInt32 tailResumeId;
    TZrUInt32 operandCount;
    /* Flattened operands retain their instruction partition so a generated
     * handler can replay head and tail evaluation without consulting source
     * ranges or guessing variadic arity. */
    TZrUInt32 headOperandCount;
    TZrUInt32 tailOperandCount;
    TZrUInt32 operands[ZR_EXEC_BC_FUSION_MAX_OPERANDS];
    /* Result identities are retained alongside operands so a handler can
     * materialize the fused value without consulting a parser-owned pointer. */
    TZrUInt32 headResult;
    TZrUInt32 tailResult;
    /* Conditional ExecIR branches carry two explicit successors.  The
     * singular fields below are retained as aliases for older consumers and
     * always mirror element zero when branchTargetCount is non-zero. */
    TZrUInt32 branchTarget;
    TZrUInt32 branchTargetPc;
    TZrUInt32 branchTargetCount;
    TZrUInt32 branchTargets[ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS];
    TZrUInt32 branchTargetPcs[ZR_EXEC_BC_FUSION_MAX_BRANCH_TARGETS];
    TZrUInt32 bindingRow;
    TZrUInt32 guardMask;
    TZrUInt64 generation;
    TZrUInt64 signatureHash;
    TZrUInt64 moduleHash;
    TZrUInt64 layoutHash;
} SZrExecBcFusionSideEntry;

typedef struct SZrExecBcFusionSourceMap {
    TZrUInt32 fusedPc;
    TZrUInt32 originalInstructionId;
    TZrUInt32 sourceId;
    TZrUInt32 resumeId;
    TZrUInt32 ordinal;
    TZrUInt32 boundaryMask;
} SZrExecBcFusionSourceMap;

typedef struct SZrExecBcFusionFallback {
    TZrUInt32 headInstructionId;
    TZrUInt32 tailInstructionId;
    EZrExecBcFusionPattern pattern;
    EZrExecBcFusionFallbackReason reason;
    TZrUInt32 sourceId;
    TZrUInt32 expected;
    TZrUInt32 actual;
} SZrExecBcFusionFallback;

struct SZrExecBcFusionPlan {
    TZrUInt32 storageTag;
    TZrUInt32 schemaVersion;
    TZrMetadataToken functionToken;
    TZrUInt64 generation;
    TZrUInt64 signatureHash;
    TZrUInt64 moduleHash;
    TZrUInt64 layoutHash;
    TZrUInt64 inputHash;
    TZrUInt64 patternSchemaHash;
    TZrUInt64 generatedHash;
    TZrUInt32 originalInstructionCount;
    TZrUInt32 instructionCount;
    TZrUInt32 instructionCapacity;
    SZrExecBcFusionInstruction *instructions;
    SZrExecBcFusionInstruction *originalInstructions;
    TZrUInt32 fusedCount;
    TZrUInt32 dispatchesSaved;
    TZrUInt32 estimatedCodeBytes;
    TZrUInt32 codeBudgetBytes;
    SZrExecBcFusionSideEntry *sideEntries;
    TZrUInt32 sideEntryCount;
    TZrUInt32 sideEntryCapacity;
    SZrExecBcFusionSourceMap *sourceMaps;
    TZrUInt32 sourceMapCount;
    TZrUInt32 sourceMapCapacity;
    SZrExecBcFusionFallback *fallbacks;
    TZrUInt32 fallbackCount;
    TZrUInt32 fallbackCapacity;
    EZrExecBcFusionInvalidationReason invalidationReason;
    TZrBool valid;
    TZrBool reserved0;
    TZrUInt16 reserved1;
};

ZR_PARSER_API void ZrParser_ExecBcPatternOptions_Init(
        SZrExecBcPatternOptions *options);
ZR_PARSER_API void ZrParser_ExecBcFusionPlan_Init(
        SZrExecBcFusionPlan *plan);
ZR_PARSER_API void ZrParser_ExecBcFusionPlan_Free(
        SZrExecBcFusionPlan *plan);

ZR_PARSER_API const SZrExecBcFusionPatternInfo *
ZrParser_ExecBcFusion_PatternInfo(EZrExecBcFusionPattern pattern);
ZR_PARSER_API TZrUInt32 ZrParser_ExecBcFusion_PatternCount(void);
ZR_PARSER_API TZrUInt64 ZrParser_ExecBcFusion_PatternSchemaHash(void);
ZR_PARSER_API TZrUInt64 ZrParser_ExecBcFusion_InputHash(
        const SZrExecIrFunction *function);
ZR_PARSER_API TZrUInt64 ZrParser_ExecBcFusion_Hash(
        const SZrExecBcFusionPlan *plan);

ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildExecBcFusion(
        const SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        SZrExecBcFusionPlan *output,
        SZrExecIrDiagnostic *diagnostic);

/* Draft-plan spelling retained as the public selection entry.  The function
 * remains immutable; callers wanting records provide options->output. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_SelectExecBcPatterns(
        SZrExecIrFunction *function,
        const SZrExecBcPatternOptions *options,
        SZrExecIrDiagnostic *diagnostic);

ZR_PARSER_API TZrBool ZrParser_ExecBcFusion_Validate(
        const SZrExecBcFusionPlan *plan,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API EZrExecBcFusionInvalidationReason
ZrParser_ExecBcFusion_CheckValidity(
        const SZrExecBcFusionPlan *plan,
        const SZrExecIrFunction *function,
        TZrUInt64 activeGeneration,
        SZrExecBcFusionDiagnostic *diagnostic);
ZR_PARSER_API void ZrParser_ExecBcFusion_Invalidate(
        SZrExecBcFusionPlan *plan,
        EZrExecBcFusionInvalidationReason reason);

ZR_PARSER_API const TZrChar *ZrParser_ExecBcFusion_PatternName(
        EZrExecBcFusionPattern pattern);
ZR_PARSER_API const TZrChar *ZrParser_ExecBcFusion_FallbackReasonName(
        EZrExecBcFusionFallbackReason reason);
ZR_PARSER_API const TZrChar *ZrParser_ExecBcFusion_InvalidationReasonName(
        EZrExecBcFusionInvalidationReason reason);
ZR_PARSER_API const TZrChar *ZrParser_ExecBcFusion_StatusName(
        EZrExecBcFusionStatus status);

/* Compatibility spellings used by early lowering prototypes. */
#define ZrParser_ExecBcFusion_Init ZrParser_ExecBcFusionPlan_Init
#define ZrParser_ExecBcFusion_Free ZrParser_ExecBcFusionPlan_Free
#define ZrParser_ExecBcFusion_Build ZrParser_ExecIr_BuildExecBcFusion
#define ZrParser_ExecIr_GenerateExecBcFusion ZrParser_ExecIr_BuildExecBcFusion

#endif /* ZR_VM_PARSER_EXEC_IR_FUSION_H */
