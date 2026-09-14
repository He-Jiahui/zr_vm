/*
 * Portable SSA platform capability, ABI, artifact, and runtime-evidence
 * contract.  This header deliberately contains no host pointers, executable
 * addresses, or runtime handles so declarations and observations can be
 * retained in a test manifest or an artifact-side verifier.
 */

#ifndef ZR_VM_COMMON_SSA_PLATFORM_CONTRACT_H
#define ZR_VM_COMMON_SSA_PLATFORM_CONTRACT_H

#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_common/zr_common_conf.h"

#include <limits.h>

#define ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_SSA_PLATFORM_CONTRACT_MAGIC ((TZrUInt32)0x53504d31u)
#define ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY ((TZrSize)96u)
#define ZR_SSA_PLATFORM_COMPILER_CAPACITY ((TZrSize)96u)
#define ZR_SSA_PLATFORM_RUNTIME_CAPACITY ((TZrSize)128u)
#define ZR_SSA_PLATFORM_MAX_ALIGNMENT ((TZrUInt32)4096u)
#define ZR_SSA_PLATFORM_INT8_WIDTH_BITS ((TZrUInt32)8u)
#define ZR_SSA_PLATFORM_INT16_WIDTH_BITS ((TZrUInt32)16u)
#define ZR_SSA_PLATFORM_INT32_WIDTH_BITS ((TZrUInt32)32u)
#define ZR_SSA_PLATFORM_INT64_WIDTH_BITS ((TZrUInt32)64u)
#define ZR_SSA_PLATFORM_FLOAT32_WIDTH_BITS ((TZrUInt32)32u)
#define ZR_SSA_PLATFORM_FLOAT64_WIDTH_BITS ((TZrUInt32)64u)

/* These assert the C ABI facts which are invariant for every supported
 * target.  Per-target pointer width, alignment, return convention, and
 * callback ABI are carried by SZrSsaPlatformAbi and checked at load/run time. */
#if defined(__cplusplus)
#define ZR_SSA_PLATFORM_STATIC_ASSERT(CONDITION, MESSAGE) \
    static_assert((CONDITION), MESSAGE)
#else
#define ZR_SSA_PLATFORM_STATIC_ASSERT(CONDITION, MESSAGE) \
    _Static_assert((CONDITION), MESSAGE)
#endif
ZR_SSA_PLATFORM_STATIC_ASSERT(CHAR_BIT == 8,
                              "ZR platform contract requires eight-bit bytes");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(TZrUInt8) * CHAR_BIT == 8,
                              "TZrUInt8 must be eight bits");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(TZrUInt16) * CHAR_BIT == 16,
                              "TZrUInt16 must be sixteen bits");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(TZrUInt32) * CHAR_BIT == 32,
                              "TZrUInt32 must be thirty-two bits");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(TZrUInt64) * CHAR_BIT == 64,
                              "TZrUInt64 must be sixty-four bits");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(TZrFloat32) * CHAR_BIT == 32,
                              "TZrFloat32 must be IEEE-width compatible");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(TZrFloat64) * CHAR_BIT == 64,
                              "TZrFloat64 must be IEEE-width compatible");
ZR_SSA_PLATFORM_STATIC_ASSERT(sizeof(void *) * CHAR_BIT == 32 ||
                                      sizeof(void *) * CHAR_BIT == 64,
                              "supported C targets use 32-bit or 64-bit pointers");

typedef enum EZrSsaPlatformTarget {
    ZR_SSA_PLATFORM_TARGET_UNKNOWN = 0,
    ZR_SSA_PLATFORM_TARGET_DESKTOP_WINDOWS,
    ZR_SSA_PLATFORM_TARGET_DESKTOP_LINUX,
    ZR_SSA_PLATFORM_TARGET_DESKTOP_DARWIN,
    ZR_SSA_PLATFORM_TARGET_ANDROID,
    ZR_SSA_PLATFORM_TARGET_IOS,
    ZR_SSA_PLATFORM_TARGET_WASM,
    ZR_SSA_PLATFORM_TARGET_COUNT
} EZrSsaPlatformTarget;

typedef enum EZrSsaPlatformArchitecture {
    ZR_SSA_PLATFORM_ARCH_UNKNOWN = 0,
    ZR_SSA_PLATFORM_ARCH_X86_64,
    ZR_SSA_PLATFORM_ARCH_AARCH64,
    ZR_SSA_PLATFORM_ARCH_WASM32,
    ZR_SSA_PLATFORM_ARCH_WASM64,
    ZR_SSA_PLATFORM_ARCH_COUNT
} EZrSsaPlatformArchitecture;

typedef enum EZrSsaPlatformBackend {
    ZR_SSA_PLATFORM_BACKEND_NONE = 0,
    ZR_SSA_PLATFORM_BACKEND_EXECBC,
    ZR_SSA_PLATFORM_BACKEND_AOT_C,
    ZR_SSA_PLATFORM_BACKEND_AOT_LLVM,
    ZR_SSA_PLATFORM_BACKEND_HOST_JIT,
    ZR_SSA_PLATFORM_BACKEND_COUNT
} EZrSsaPlatformBackend;

typedef enum EZrSsaPlatformRunner {
    ZR_SSA_PLATFORM_RUNNER_NONE = 0,
    ZR_SSA_PLATFORM_RUNNER_HOST,
    ZR_SSA_PLATFORM_RUNNER_CROSS_COMPILE,
    ZR_SSA_PLATFORM_RUNNER_EMULATOR,
    ZR_SSA_PLATFORM_RUNNER_REAL_DEVICE,
    ZR_SSA_PLATFORM_RUNNER_BROWSER_RUNTIME,
    ZR_SSA_PLATFORM_RUNNER_WASM_RUNTIME,
    ZR_SSA_PLATFORM_RUNNER_COUNT
} EZrSsaPlatformRunner;

typedef enum EZrSsaPlatformOutcome {
    ZR_SSA_PLATFORM_OUTCOME_UNSET = 0,
    ZR_SSA_PLATFORM_OUTCOME_PASSED,
    ZR_SSA_PLATFORM_OUTCOME_UNAVAILABLE,
    ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED,
    ZR_SSA_PLATFORM_OUTCOME_FAILED,
    ZR_SSA_PLATFORM_OUTCOME_COUNT
} EZrSsaPlatformOutcome;

typedef enum EZrSsaPlatformEndianness {
    ZR_SSA_PLATFORM_ENDIAN_UNKNOWN = 0,
    ZR_SSA_PLATFORM_ENDIAN_LITTLE,
    ZR_SSA_PLATFORM_ENDIAN_BIG,
    ZR_SSA_PLATFORM_ENDIAN_COUNT
} EZrSsaPlatformEndianness;

typedef enum EZrSsaPlatformStructReturnKind {
    ZR_SSA_PLATFORM_STRUCT_RETURN_UNKNOWN = 0,
    ZR_SSA_PLATFORM_STRUCT_RETURN_REGISTER,
    ZR_SSA_PLATFORM_STRUCT_RETURN_MEMORY,
    ZR_SSA_PLATFORM_STRUCT_RETURN_MIXED,
    ZR_SSA_PLATFORM_STRUCT_RETURN_COUNT
} EZrSsaPlatformStructReturnKind;

typedef enum EZrSsaPlatformAbiField {
    ZR_SSA_PLATFORM_ABI_FIELD_NONE = 0,
    ZR_SSA_PLATFORM_ABI_FIELD_POINTER_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_ENDIANNESS,
    ZR_SSA_PLATFORM_ABI_FIELD_MAX_ALIGNMENT,
    ZR_SSA_PLATFORM_ABI_FIELD_INT8_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_INT16_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_INT32_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_INT64_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_FLOAT32_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_FLOAT64_WIDTH,
    ZR_SSA_PLATFORM_ABI_FIELD_STRUCT_RETURN,
    ZR_SSA_PLATFORM_ABI_FIELD_NATIVE_CALLBACK,
    ZR_SSA_PLATFORM_ABI_FIELD_HASH
} EZrSsaPlatformAbiField;

typedef enum EZrSsaPlatformDispatchKind {
    ZR_SSA_PLATFORM_DISPATCH_UNKNOWN = 0,
    ZR_SSA_PLATFORM_DISPATCH_SWITCH,
    ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO,
    ZR_SSA_PLATFORM_DISPATCH_COUNT
} EZrSsaPlatformDispatchKind;

typedef enum EZrSsaPlatformWitnessField {
    ZR_SSA_PLATFORM_WITNESS_FIELD_NONE = 0,
    ZR_SSA_PLATFORM_WITNESS_FIELD_RESULT,
    ZR_SSA_PLATFORM_WITNESS_FIELD_EXCEPTION,
    ZR_SSA_PLATFORM_WITNESS_FIELD_SOURCE_MAP
} EZrSsaPlatformWitnessField;

#define ZR_SSA_PLATFORM_FEATURE_EXECBC ((TZrUInt64)1u << 0u)
#define ZR_SSA_PLATFORM_FEATURE_AOT_C ((TZrUInt64)1u << 1u)
#define ZR_SSA_PLATFORM_FEATURE_AOT_LLVM ((TZrUInt64)1u << 2u)
#define ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT ((TZrUInt64)1u << 3u)
#define ZR_SSA_PLATFORM_FEATURE_THREADS ((TZrUInt64)1u << 4u)
#define ZR_SSA_PLATFORM_FEATURE_CONCURRENT_GC ((TZrUInt64)1u << 5u)
#define ZR_SSA_PLATFORM_FEATURE_PMU ((TZrUInt64)1u << 6u)
#define ZR_SSA_PLATFORM_FEATURE_UNWIND ((TZrUInt64)1u << 7u)
#define ZR_SSA_PLATFORM_FEATURE_DEBUG ((TZrUInt64)1u << 8u)
#define ZR_SSA_PLATFORM_FEATURE_NATIVE_CALLBACKS ((TZrUInt64)1u << 9u)
#define ZR_SSA_PLATFORM_FEATURE_RESTRICTED_PATCH ((TZrUInt64)1u << 10u)
#define ZR_SSA_PLATFORM_FEATURE_COMPUTED_GOTO ((TZrUInt64)1u << 11u)
#define ZR_SSA_PLATFORM_FEATURE_SWITCH_DISPATCH ((TZrUInt64)1u << 12u)

#define ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH ((TZrUInt32)1u << 0u)
#define ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO ((TZrUInt32)1u << 1u)
#define ZR_SSA_PLATFORM_DISPATCH_FLAG_KNOWN_MASK \
    ((TZrUInt32)(ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH | \
                 ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO))

#define ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK \
    ((TZrUInt64)(ZR_SSA_PLATFORM_FEATURE_EXECBC | \
                 ZR_SSA_PLATFORM_FEATURE_AOT_C | \
                 ZR_SSA_PLATFORM_FEATURE_AOT_LLVM | \
                 ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT | \
                 ZR_SSA_PLATFORM_FEATURE_THREADS | \
                 ZR_SSA_PLATFORM_FEATURE_CONCURRENT_GC | \
                 ZR_SSA_PLATFORM_FEATURE_PMU | \
                 ZR_SSA_PLATFORM_FEATURE_UNWIND | \
                 ZR_SSA_PLATFORM_FEATURE_DEBUG | \
                 ZR_SSA_PLATFORM_FEATURE_NATIVE_CALLBACKS | \
                 ZR_SSA_PLATFORM_FEATURE_RESTRICTED_PATCH | \
                 ZR_SSA_PLATFORM_FEATURE_COMPUTED_GOTO | \
                 ZR_SSA_PLATFORM_FEATURE_SWITCH_DISPATCH))

typedef enum EZrSsaPlatformStatus {
    ZR_SSA_PLATFORM_STATUS_OK = 0,
    ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT,
    ZR_SSA_PLATFORM_STATUS_SCHEMA_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_TARGET_INVALID,
    ZR_SSA_PLATFORM_STATUS_ARCHITECTURE_INVALID,
    ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISSING,
    ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID,
    ZR_SSA_PLATFORM_STATUS_BACKEND_INVALID,
    ZR_SSA_PLATFORM_STATUS_RUNNER_INVALID,
    ZR_SSA_PLATFORM_STATUS_OUTCOME_INVALID,
    ZR_SSA_PLATFORM_STATUS_ABI_INVALID,
    ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_NUMERIC_CONTRACT_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_LAYOUT_CONTRACT_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_ARTIFACT_TARGET_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_ARTIFACT_ARCHITECTURE_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_ARTIFACT_NUMERIC_CONTRACT_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_ARTIFACT_LAYOUT_CONTRACT_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED,
    ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED,
    ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN,
    ZR_SSA_PLATFORM_STATUS_RESTRICTED_PATCH_NATIVE_IMPORT,
    ZR_SSA_PLATFORM_STATUS_COMPILE_NOT_COMPLETED,
    ZR_SSA_PLATFORM_STATUS_RUNTIME_NOT_EXECUTED,
    ZR_SSA_PLATFORM_STATUS_SEMANTIC_FAILURE,
    ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID,
    ZR_SSA_PLATFORM_STATUS_RUNTIME_UNAVAILABLE,
    ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH,
    ZR_SSA_PLATFORM_STATUS_COUNT
} EZrSsaPlatformStatus;

typedef struct SZrSsaPlatformAbi {
    TZrUInt32 pointerWidthBits;
    EZrSsaPlatformEndianness endianness;
    TZrUInt32 maxAlignment;
    TZrUInt32 int8WidthBits;
    TZrUInt32 int16WidthBits;
    TZrUInt32 int32WidthBits;
    TZrUInt32 int64WidthBits;
    TZrUInt32 float32WidthBits;
    TZrUInt32 float64WidthBits;
    EZrSsaPlatformStructReturnKind structReturnKind;
    TZrUInt64 nativeCallbackAbiHash;
} SZrSsaPlatformAbi;

typedef struct SZrSsaPlatformCapability {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrSsaPlatformTarget target;
    EZrSsaPlatformArchitecture architecture;
    TZrChar targetTriple[ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY];
    SZrSsaPlatformAbi abi;
    TZrUInt64 abiHash;
    TZrUInt64 numericContractHash;
    TZrUInt64 layoutContractHash;
    TZrUInt64 semanticResultHash;
    TZrUInt64 exceptionContractHash;
    TZrUInt64 sourceMapHash;
    TZrUInt64 featureFlags;
    TZrUInt32 dispatchFlags;
} SZrSsaPlatformCapability;

typedef struct SZrSsaPlatformObservation {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrSsaPlatformTarget target;
    EZrSsaPlatformArchitecture architecture;
    EZrSsaPlatformBackend backend;
    EZrSsaPlatformRunner runner;
    EZrSsaPlatformOutcome outcome;
    TZrChar targetTriple[ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY];
    TZrChar compiler[ZR_SSA_PLATFORM_COMPILER_CAPACITY];
    TZrChar deviceOrRuntime[ZR_SSA_PLATFORM_RUNTIME_CAPACITY];
    SZrSsaPlatformAbi observedAbi;
    TZrUInt64 observedAbiHash;
    TZrUInt64 observedNumericContractHash;
    TZrUInt64 observedLayoutContractHash;
    TZrUInt64 semanticResultHash;
    TZrUInt64 exceptionContractHash;
    TZrUInt64 sourceMapHash;
    TZrUInt64 requiredFeatures;
    TZrUInt64 unsupportedFeatures;
    EZrSsaPlatformDispatchKind dispatchKind;
    TZrUInt32 dispatchFlags;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    TZrBool compiled;
    TZrBool executed;
    TZrBool semanticPassed;
    TZrBool machineCodeJitExecuted;
} SZrSsaPlatformObservation;

typedef struct SZrSsaPlatformArtifactContract {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrSsaPlatformTarget target;
    EZrSsaPlatformArchitecture architecture;
    TZrChar targetTriple[ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY];
    SZrSsaPlatformAbi abi;
    TZrUInt64 abiHash;
    TZrUInt64 numericContractHash;
    TZrUInt64 layoutContractHash;
    TZrUInt64 requiredFeatures;
    TZrUInt32 newNativeImportCount;
    TZrBool machineCodeJit;
    TZrBool restrictedPatch;
} SZrSsaPlatformArtifactContract;

typedef struct SZrSsaPlatformDiagnostic {
    EZrSsaPlatformStatus status;
    EZrSsaPlatformTarget target;
    EZrSsaPlatformArchitecture architecture;
    EZrSsaPlatformBackend backend;
    EZrSsaPlatformRunner runner;
    EZrSsaPlatformAbiField abiField;
    TZrUInt64 requiredFeatures;
    TZrUInt64 unsupportedFeatures;
    TZrUInt64 expected;
    TZrUInt64 actual;
    TZrUInt32 sourceId;
    TZrUInt32 instructionId;
    EZrSsaPlatformWitnessField witnessField;
} SZrSsaPlatformDiagnostic;

#ifdef __cplusplus
extern "C" {
#endif

ZR_API void ZrCommon_SsaPlatform_DiagnosticInit(
        SZrSsaPlatformDiagnostic *diagnostic);
ZR_API const TZrChar *ZrCommon_SsaPlatform_StatusName(
        EZrSsaPlatformStatus status);
ZR_API const TZrChar *ZrCommon_SsaPlatform_OutcomeName(
        EZrSsaPlatformOutcome outcome);

ZR_API void ZrCommon_SsaPlatform_AbiInit(SZrSsaPlatformAbi *abi);
ZR_API void ZrCommon_SsaPlatform_DetectHostAbi(SZrSsaPlatformAbi *abi);
ZR_API TZrUInt64 ZrCommon_SsaPlatform_ComputeAbiHash(
        const SZrSsaPlatformAbi *abi);
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateAbi(
        const SZrSsaPlatformAbi *abi,
        SZrSsaPlatformDiagnostic *diagnostic);
ZR_API TZrBool ZrCommon_SsaPlatform_AbiEqual(
        const SZrSsaPlatformAbi *expected,
        const SZrSsaPlatformAbi *actual,
        SZrSsaPlatformDiagnostic *diagnostic);

ZR_API void ZrCommon_SsaPlatform_CapabilityInit(
        SZrSsaPlatformCapability *capability);
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateCapability(
        const SZrSsaPlatformCapability *capability,
        SZrSsaPlatformDiagnostic *diagnostic);
ZR_API TZrBool ZrCommon_SsaPlatform_CapabilitySupports(
        const SZrSsaPlatformCapability *capability,
        TZrUInt64 feature);

ZR_API void ZrCommon_SsaPlatform_ObservationInit(
        SZrSsaPlatformObservation *observation);
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_Check(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformObservation *observed,
        SZrSsaPlatformDiagnostic *diagnostic);
/* Fail-closed convenience predicate: malformed schema/identity/ABI/dispatch
 * fields are never considered runtime acceptance. */
ZR_API TZrBool ZrCommon_SsaPlatform_IsRuntimeAcceptance(
        const SZrSsaPlatformObservation *observation);

ZR_API void ZrCommon_SsaPlatform_ArtifactContractInit(
        SZrSsaPlatformArtifactContract *artifact);
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateArtifact(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformArtifactContract *artifact,
        SZrSsaPlatformDiagnostic *diagnostic);

/* Narrow test-facing entry point from the 10.03 plan.  Production callers
 * should use ZrCommon_SsaPlatform_Check to retain structured diagnostics. */
ZR_API TZrBool ZrTests_Ssa_CheckPlatform(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformObservation *observed);

#ifdef __cplusplus
}
#endif

#undef ZR_SSA_PLATFORM_STATIC_ASSERT

#endif /* ZR_VM_COMMON_SSA_PLATFORM_CONTRACT_H */
