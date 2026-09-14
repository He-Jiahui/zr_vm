#ifndef ZR_VM_PARSER_EXEC_IR_CONTAINER_SPECIALIZE_H
#define ZR_VM_PARSER_EXEC_IR_CONTAINER_SPECIALIZE_H

/*
 * Parser-side admission contract for map and string storage candidates.
 *
 * The parser does not own a map entry, string object, callback, or allocator
 * here.  Facts and plans contain only fixed-width scalar values (including
 * the scalar core storage witnesses), so a plan can be copied into an IR
 * side table and discarded at a safepoint.  The pass is deliberately an
 * admission boundary: this ExecIR revision has no container-specific opcode
 * or payload slot to rewrite.  A rejected/unknown witness therefore keeps
 * the existing generic operation and its equality/exception ordering.
 */

#include "zr_vm_core/container_storage_contract.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_parser/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAGIC ((TZrUInt32)0x31505343u) /* CSP1 */

/* Facts flags are optional metadata; the explicit known booleans below are
 * authoritative and keep a zero-initialized/partial producer conservative. */
#define ZR_EXEC_IR_CONTAINER_FACT_ALLOW_GENERIC ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_CONTAINER_FACT_REQUIRE_MAP ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_CONTAINER_FACT_REQUIRE_STRING ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_CONTAINER_FACT_KNOWN_MASK \
    (ZR_EXEC_IR_CONTAINER_FACT_ALLOW_GENERIC | \
     ZR_EXEC_IR_CONTAINER_FACT_REQUIRE_MAP | \
     ZR_EXEC_IR_CONTAINER_FACT_REQUIRE_STRING)

typedef enum EZrExecIrContainerKind {
    ZR_EXEC_IR_CONTAINER_KIND_NONE = 0,
    ZR_EXEC_IR_CONTAINER_KIND_MAP = 1,
    ZR_EXEC_IR_CONTAINER_KIND_STRING = 2
} EZrExecIrContainerKind;

typedef enum EZrExecIrContainerMapStrategy {
    ZR_EXEC_IR_CONTAINER_MAP_GENERIC = 0,
    ZR_EXEC_IR_CONTAINER_MAP_COMPACT = 1
} EZrExecIrContainerMapStrategy;

typedef enum EZrExecIrContainerSpecializationStatus {
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_OK = 0,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_INVALID_ARGUMENT,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SCHEMA_MISMATCH,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SEALED,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_INVALID,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SCOPE_MISMATCH,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERATION_MISMATCH,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_IR_HASH_MISMATCH,
    /* Unknown facts are not language errors.  They produce a valid generic
     * plan and this reason is retained for optimization remarks. */
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_CONTRACT_INVALID,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_HASH_UNSTABLE,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_EQUALITY_UNSTABLE,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_OWNERSHIP_UNSAFE,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_ITERATION_UNSAFE,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_CONTRACT_INVALID,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_IDENTITY_OBSERVED,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_ESCAPE,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_EFFECT_MISMATCH,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_BUDGET,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_GENERIC_FALLBACK,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_PLAN_HASH_MISMATCH,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNSUPPORTED,
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STATUS_COUNT
} EZrExecIrContainerSpecializationStatus;

/* Compatibility spellings used by plan notes and downstream diagnostics. */
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_MAP \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_STRING \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_MAP_LAYOUT_UNKNOWN \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_LAYOUT_UNKNOWN \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_UNKNOWN_LAYOUT
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_ESCAPE_OBSERVED \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_ESCAPE
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_EFFECT_MISMATCH \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_STRING_EFFECT_MISMATCH
#define ZR_EXEC_IR_CONTAINER_SPECIALIZATION_SEALED \
    ZR_EXEC_IR_CONTAINER_SPECIALIZATION_FUNCTION_SEALED

typedef struct SZrContainerSpecializationDiagnostic {
    EZrExecIrContainerSpecializationStatus status;
    EZrExecIrContainerKind kind;
    SZrExecIrDiagnostic execution;
    SZrContainerStorageDiagnostic storage;
} SZrContainerSpecializationDiagnostic;

/* A producer may leave mapKnown/stringKnown false when layout or escape facts
 * are unavailable.  No pointer or callback field is permitted in this type. */
typedef struct SZrContainerSpecializationFacts {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    TZrUInt64 generation;
    TZrUInt64 irHash;
    union {
        TZrBool mapKnown;
        TZrBool mapFactsKnown;
    };
    union {
        TZrBool stringKnown;
        TZrBool stringFactsKnown;
    };
    TZrBool allowGenericFallback;
    TZrBool reservedBool;
    union {
        SZrCompactMapCandidate mapCandidate;
        SZrCompactMapCandidate map;
    };
    union {
        SZrStringStorageFacts stringFacts;
        SZrStringStorageFacts string;
    };
} SZrContainerSpecializationFacts;

/* The plan is a value-only snapshot.  It records generic preservation even
 * when one candidate is accepted, so callers cannot accidentally bypass
 * collision equality or exception ordering. */
typedef struct SZrContainerSpecializationPlan {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrExecIrContainerSpecializationStatus status;
    EZrExecIrContainerSpecializationStatus fallbackReason;
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    TZrUInt64 generation;
    TZrUInt64 beforeHash;
    TZrUInt64 afterHash;
    EZrExecIrContainerMapStrategy mapStrategy;
    EZrStringStorageStrategy stringStrategy;
    TZrBool mapKnown;
    TZrBool stringKnown;
    TZrBool cacheHash;
    TZrBool preservesGenericEquality;
    TZrBool irUnchanged;
    TZrBool specialized;
    TZrBool reservedBool;
    SZrCompactMapCandidate mapCandidate;
    SZrStringStorageCandidate stringCandidate;
    TZrUInt64 planHash;
} SZrContainerSpecializationPlan;

/* Type aliases keep generated/pass-side code that prefixes ExecIR source
 * facts source-compatible without introducing a second representation. */
typedef SZrContainerSpecializationFacts SZrExecIrContainerSpecializationFacts;
typedef SZrContainerSpecializationPlan SZrExecIrContainerSpecializationPlan;
typedef SZrContainerSpecializationDiagnostic
        SZrExecIrContainerSpecializationDiagnostic;

ZR_PARSER_API void ZrParser_ExecIr_ContainerSpecializationDiagnosticInit(
        SZrContainerSpecializationDiagnostic *diagnostic);
ZR_PARSER_API const TZrChar *ZrParser_ExecIr_ContainerSpecializationStatusName(
        EZrExecIrContainerSpecializationStatus status);

ZR_PARSER_API void ZrParser_ExecIr_ContainerSpecializationFactsInit(
        SZrContainerSpecializationFacts *facts);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ContainerSpecializationFactsValidate(
        const SZrContainerSpecializationFacts *facts,
        SZrContainerSpecializationDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_ContainerSpecializationFactsHash(
        const SZrContainerSpecializationFacts *facts);

ZR_PARSER_API void ZrParser_ExecIr_ContainerSpecializationPlanInit(
        SZrContainerSpecializationPlan *plan);
ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildContainerSpecialization(
        const SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        SZrContainerSpecializationPlan *plan,
        SZrContainerSpecializationDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_ExecIr_ValidateContainerSpecializationPlan(
        const SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        const SZrContainerSpecializationPlan *plan,
        SZrContainerSpecializationDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_ExecIr_ContainerSpecializationPlanHash(
        const SZrContainerSpecializationPlan *plan);

/* Draft interface retained verbatim from 05.03.  It returns true when at
 * least one non-generic candidate can be admitted.  Unknown/unsafe facts are
 * reported through diagnostic and leave the generic implementation intact. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_SpecializeContainers(
        SZrExecIrFunction *function,
        const SZrContainerSpecializationFacts *facts,
        SZrExecIrDiagnostic *diagnostic);

/* Additional descriptive spellings used by pass callers. */
#define ZrParser_ExecIr_ContainerSpecializationFacts_Init \
    ZrParser_ExecIr_ContainerSpecializationFactsInit
#define ZrParser_ExecIr_ContainerSpecializationFacts_Validate \
    ZrParser_ExecIr_ContainerSpecializationFactsValidate
#define ZrParser_ExecIr_ContainerSpecializationFacts_Hash \
    ZrParser_ExecIr_ContainerSpecializationFactsHash
#define ZrParser_ExecIr_ContainerSpecializationPlan_Init \
    ZrParser_ExecIr_ContainerSpecializationPlanInit
#define ZrParser_ExecIr_ContainerSpecialization_BuildPlan \
    ZrParser_ExecIr_BuildContainerSpecialization
#define ZrParser_ExecIr_ContainerSpecializationBuildPlan \
    ZrParser_ExecIr_BuildContainerSpecialization
#define ZrParser_ExecIr_ContainerSpecializationPlan_Validate \
    ZrParser_ExecIr_ValidateContainerSpecializationPlan
#define ZrParser_ExecIr_ContainerSpecializationValidatePlan \
    ZrParser_ExecIr_ValidateContainerSpecializationPlan
#define ZrParser_ExecIr_ContainerSpecializationPlan_Hash \
    ZrParser_ExecIr_ContainerSpecializationPlanHash
#define ZrParser_ExecIr_SpecializeContainer \
    ZrParser_ExecIr_SpecializeContainers

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_PARSER_EXEC_IR_CONTAINER_SPECIALIZE_H */
