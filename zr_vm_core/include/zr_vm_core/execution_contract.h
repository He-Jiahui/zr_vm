#ifndef ZR_VM_CORE_EXECUTION_CONTRACT_H
#define ZR_VM_CORE_EXECUTION_CONTRACT_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/metadata_token.h"

/*
 * The legacy artifact/call-binding/AOT versions remain readable by their
 * existing consumers.  These values are the candidate version of the shared
 * execution contract; publishing them does not silently reinterpret an old
 * on-disk artifact.
 */
#define ZR_EXECUTION_CONTRACT_SCHEMA_VERSION ((TZrUInt32)6u)
#define ZR_EXECUTION_CONTRACT_ABI_VERSION ((TZrUInt32)17u)
#define ZR_EXECUTION_CONTRACT_LOGICAL_VERSION ((TZrUInt32)1u)

#define ZR_EXECUTION_CAPABILITY_ARITHMETIC ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_CAPABILITY_MEMORY ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_CAPABILITY_NATIVE_CALL ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_CAPABILITY_SUSPEND ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_CAPABILITY_GC ((TZrUInt32)1u << 4u)
#define ZR_EXECUTION_CAPABILITY_KNOWN_MASK \
    (ZR_EXECUTION_CAPABILITY_ARITHMETIC | ZR_EXECUTION_CAPABILITY_MEMORY | \
     ZR_EXECUTION_CAPABILITY_NATIVE_CALL | ZR_EXECUTION_CAPABILITY_SUSPEND | \
     ZR_EXECUTION_CAPABILITY_GC)

#define ZR_EXECUTION_EFFECT_READ_MEMORY ((TZrUInt32)1u << 0u)
#define ZR_EXECUTION_EFFECT_WRITE_MEMORY ((TZrUInt32)1u << 1u)
#define ZR_EXECUTION_EFFECT_ALLOCATE ((TZrUInt32)1u << 2u)
#define ZR_EXECUTION_EFFECT_THROW ((TZrUInt32)1u << 3u)
#define ZR_EXECUTION_EFFECT_SUSPEND ((TZrUInt32)1u << 4u)
#define ZR_EXECUTION_EFFECT_KNOWN_MASK \
    (ZR_EXECUTION_EFFECT_READ_MEMORY | ZR_EXECUTION_EFFECT_WRITE_MEMORY | \
     ZR_EXECUTION_EFFECT_ALLOCATE | ZR_EXECUTION_EFFECT_THROW | \
     ZR_EXECUTION_EFFECT_SUSPEND)

typedef enum EZrExecutionContractStatus {
    ZR_EXECUTION_CONTRACT_OK = 0,
    ZR_EXECUTION_CONTRACT_INVALID_ARGUMENT,
    ZR_EXECUTION_CONTRACT_RECOMPILE_REQUIRED,
    ZR_EXECUTION_CONTRACT_TARGET_MISMATCH,
    ZR_EXECUTION_CONTRACT_SIGNATURE_MISMATCH,
    ZR_EXECUTION_CONTRACT_LAYOUT_MISMATCH,
    ZR_EXECUTION_CONTRACT_MODULE_MISMATCH,
    ZR_EXECUTION_CONTRACT_CAPABILITY_MISMATCH,
    ZR_EXECUTION_CONTRACT_EFFECT_MISMATCH,
    ZR_EXECUTION_CONTRACT_STALE_GENERATION,
    ZR_EXECUTION_CONTRACT_UNSUPPORTED,
    ZR_EXECUTION_CONTRACT_STATUS_COUNT
} EZrExecutionContractStatus;

typedef enum EZrExecutionDiagnosticCode {
    ZR_EXECUTION_DIAGNOSTIC_NONE = 0,
    ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_CAPABILITY_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_EFFECT_MISMATCH,
    ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION,
    ZR_EXEC_IR_DIAGNOSTIC_UNKNOWN_OPCODE,
    ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE,
    ZR_EXEC_IR_DIAGNOSTIC_INVALID_BLOCK,
    ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE,
    ZR_EXEC_IR_DIAGNOSTIC_DUPLICATE_DEFINITION,
    ZR_EXEC_IR_DIAGNOSTIC_PHI_PREDECESSOR_MISMATCH,
    ZR_EXEC_IR_DIAGNOSTIC_MISSING_TERMINATOR,
    ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE,
    ZR_EXEC_IR_DIAGNOSTIC_MEMORY_TOKEN,
    ZR_EXEC_IR_DIAGNOSTIC_EFFECT_TOKEN,
    ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE,
    ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW,
    ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY,
    ZR_EXEC_IR_DIAGNOSTIC_SEALED,
    /* State-map and resume diagnostics (01.04). */
    ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID,
    ZR_EXEC_IR_DIAGNOSTIC_RESUME_NOT_FOUND,
    ZR_EXEC_IR_DIAGNOSTIC_BORROWED_ACROSS_SUSPEND,
    ZR_EXEC_IR_DIAGNOSTIC_MATERIALIZATION_FAILED,
    /* 01.05 oracle/projection diagnostics. */
    ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED,
    ZR_EXEC_IR_DIAGNOSTIC_CRITICAL_EDGE,
    ZR_EXEC_IR_DIAGNOSTIC_INVALID_PROJECTION,
    ZR_EXEC_IR_DIAGNOSTIC_ORACLE_STEP_LIMIT,
    ZR_EXEC_IR_DIAGNOSTIC_ARITHMETIC_ERROR,
    /* A reference-oracle memory provider rejected a load/store operation. */
    ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR,
    /* A reference-oracle allocation provider rejected an allocation. */
    ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ALLOCATION_ERROR,
    /* A reference-oracle type-test provider rejected a membership query. */
    ZR_EXEC_IR_DIAGNOSTIC_ORACLE_TYPE_TEST_ERROR
} EZrExecutionDiagnosticCode;

/*
 * This diagnostic is intentionally made only of stable scalar identities.
 * It can be copied into a report without exposing an AST, runtime pointer, or
 * host address.
 */
typedef struct SZrExecIrDiagnostic {
    EZrExecutionDiagnosticCode code;
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    TZrUInt32 expectedVersion;
    TZrUInt32 actualVersion;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
} SZrExecIrDiagnostic;

/* Stable identity fields shared by ExecIR, artifact links, and backends. */
typedef struct SZrExecutionContract {
    TZrUInt32 schemaVersion;
    TZrUInt32 abiVersion;
    TZrUInt32 logicalVersion;
    TZrUInt32 reserved0;
    TZrUInt64 generation;
    TZrMetadataToken targetToken;
    TZrUInt32 reserved1;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 moduleHash;
    TZrUInt32 requiredCapabilities;
    TZrUInt32 declaredEffects;
} SZrExecutionContract;

ZR_CORE_API EZrExecutionContractStatus ZrCore_ExecutionContract_Check(
        const SZrExecutionContract *expected,
        const SZrExecutionContract *actual,
        SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API const TZrChar *ZrCore_ExecutionContract_StatusName(
        EZrExecutionContractStatus status);

#endif
