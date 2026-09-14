#ifndef ZR_VM_PARSER_COMPILE_OPTIMIZATION_PROFILE_H
#define ZR_VM_PARSER_COMPILE_OPTIMIZATION_PROFILE_H

#include "zr_vm_parser/conf.h"

/*
 * A compile profile is deliberately made up of independent dimensions.  The
 * normalized form is immutable from the point of view of a compiler/cache
 * consumer and contains no process addresses or runtime generations.
 */
#define ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_COMPILE_OPTIMIZATION_PROFILE_DESCRIPTION_SIZE ((TZrSize)256u)

typedef enum EZrCompileBuildMode {
    ZR_COMPILE_BUILD_MODE_DEV = 1,
    ZR_COMPILE_BUILD_MODE_INTERACTIVE,
    ZR_COMPILE_BUILD_MODE_RELEASE,
    ZR_COMPILE_BUILD_MODE_COUNT
} EZrCompileBuildMode;

typedef enum EZrCompileNumericPermission {
    ZR_COMPILE_NUMERIC_STRICT = 1,
    ZR_COMPILE_NUMERIC_FAST_MATH,
    ZR_COMPILE_NUMERIC_PERMISSION_COUNT
} EZrCompileNumericPermission;

typedef enum EZrCompileTarget {
    ZR_COMPILE_TARGET_HOST = 1,
    ZR_COMPILE_TARGET_MOBILE,
    ZR_COMPILE_TARGET_WASM,
    ZR_COMPILE_TARGET_COUNT
} EZrCompileTarget;

typedef enum EZrCompileBackend {
    ZR_COMPILE_BACKEND_INTERPRETER = 1,
    ZR_COMPILE_BACKEND_HOST_JIT,
    ZR_COMPILE_BACKEND_AOT_C,
    ZR_COMPILE_BACKEND_AOT_LLVM,
    ZR_COMPILE_BACKEND_COUNT
} EZrCompileBackend;

/* Presets only expand to explicit dimensions; they never grant fast-math by
 * implication.  Callers may use ApplyPreset and then adjust dimensions. */
typedef enum EZrCompileOptimizationPreset {
    ZR_COMPILE_PRESET_DEFAULT = 0,
    ZR_COMPILE_PRESET_DEV,
    ZR_COMPILE_PRESET_INTERACTIVE,
    ZR_COMPILE_PRESET_RELEASE,
    ZR_COMPILE_PRESET_RELEASE_LTO,
    ZR_COMPILE_PRESET_RELEASE_PGO,
    ZR_COMPILE_PRESET_RELEASE_FAST_MATH,
    ZR_COMPILE_PRESET_FRAME_SAFE,
    ZR_COMPILE_PRESET_WASM,
    ZR_COMPILE_PRESET_ANDROID_AOT,
    ZR_COMPILE_PRESET_IOS_AOT,
    ZR_COMPILE_PRESET_HOST_JIT,
    ZR_COMPILE_PRESET_COUNT
} EZrCompileOptimizationPreset;

enum {
    ZR_COMPILE_PASS_BASIC = (TZrUInt32)1u << 0u,
    ZR_COMPILE_PASS_SCALAR = (TZrUInt32)1u << 1u,
    ZR_COMPILE_PASS_VECTOR = (TZrUInt32)1u << 2u,
    ZR_COMPILE_PASS_LTO = (TZrUInt32)1u << 3u,
    ZR_COMPILE_PASS_PGO = (TZrUInt32)1u << 4u
};

#define ZR_COMPILE_PASS_KNOWN_MASK \
    (ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR | ZR_COMPILE_PASS_VECTOR | \
     ZR_COMPILE_PASS_LTO | ZR_COMPILE_PASS_PGO)

typedef struct SZrCompileOptimizationProfile {
    TZrUInt32 schemaVersion;
    EZrCompileOptimizationPreset preset;
    EZrCompileBuildMode buildMode;
    EZrCompileNumericPermission numericPermission;
    EZrCompileTarget target;
    EZrCompileBackend backend;
    TZrUInt32 frameBudgetBytes;
    TZrUInt32 passMask;
    TZrUInt64 targetHash;
    TZrUInt64 hostAbiHash;
    TZrBool frameSafe;
    TZrBool requestFastMath;
    TZrBool enableLto;
    TZrBool enablePgo;
} SZrCompileOptimizationProfile;

typedef struct SZrCompileEffectivePolicy {
    TZrUInt32 schemaVersion;
    EZrCompileOptimizationPreset preset;
    EZrCompileBuildMode buildMode;
    EZrCompileNumericPermission numericPermission;
    EZrCompileTarget target;
    EZrCompileBackend backend;
    TZrUInt32 frameBudgetBytes;
    TZrUInt32 passMask;
    TZrUInt64 targetHash;
    TZrUInt64 hostAbiHash;
    TZrUInt64 profileHash;
    TZrBool frameSafe;
    TZrBool fastMathEnabled;
    TZrBool ltoEnabled;
    TZrBool pgoEnabled;
} SZrCompileEffectivePolicy;

typedef enum EZrCompileOptimizationDiagnosticCode {
    ZR_COMPILE_PROFILE_DIAGNOSTIC_NONE = 0,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_SCHEMA,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_BUILD_MODE,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_NUMERIC_PERMISSION,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_TARGET,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_BACKEND,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_TARGET_BACKEND,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_FRAME_BUDGET,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_LTO_BACKEND,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_PGO_BACKEND,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_LTO_PGO_CONFLICT,
    ZR_COMPILE_PROFILE_DIAGNOSTIC_PASS_PIPELINE
} EZrCompileOptimizationDiagnosticCode;

typedef struct SZrCompileOptimizationDiagnostic {
    EZrCompileOptimizationDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrCompileOptimizationDiagnostic;

ZR_PARSER_API void ZrParser_CompileOptimizationProfile_Init(
        SZrCompileOptimizationProfile *profile);
ZR_PARSER_API TZrBool ZrParser_CompileOptimizationProfile_ApplyPreset(
        SZrCompileOptimizationProfile *profile,
        EZrCompileOptimizationPreset preset,
        SZrCompileOptimizationDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_CompileOptimizationProfile_Normalize(
        const SZrCompileOptimizationProfile *profile,
        SZrCompileEffectivePolicy *effective,
        SZrCompileOptimizationDiagnostic *diagnostic);
ZR_PARSER_API TZrUInt64 ZrParser_CompileOptimizationProfile_Hash(
        const SZrCompileEffectivePolicy *effective);
ZR_PARSER_API TZrBool ZrParser_CompileOptimizationProfile_Describe(
        const SZrCompileEffectivePolicy *effective,
        TZrChar *buffer,
        TZrSize bufferSize);

#endif /* ZR_VM_PARSER_COMPILE_OPTIMIZATION_PROFILE_H */
