#include "zr_vm_common/ssa_platform_contract.h"

#include <string.h>

#define ZR_SSA_PLATFORM_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_SSA_PLATFORM_FNV_PRIME UINT64_C(1099511628211)

static TZrUInt64 ssa_platform_hash_u32(TZrUInt64 hash, TZrUInt32 value) {
    TZrUInt32 index;

    for (index = 0u; index < 4u; ++index) {
        hash ^= (TZrUInt8)(value >> (index * 8u));
        hash *= ZR_SSA_PLATFORM_FNV_PRIME;
    }
    return hash;
}

static TZrUInt64 ssa_platform_hash_u64(TZrUInt64 hash, TZrUInt64 value) {
    TZrUInt32 index;

    for (index = 0u; index < 8u; ++index) {
        hash ^= (TZrUInt8)(value >> (index * 8u));
        hash *= ZR_SSA_PLATFORM_FNV_PRIME;
    }
    return hash;
}

static TZrUInt64 ssa_platform_hash_abi_without_callback(
        const SZrSsaPlatformAbi *abi) {
    TZrUInt64 hash = ZR_SSA_PLATFORM_FNV_OFFSET;

    hash = ssa_platform_hash_u32(hash, abi->pointerWidthBits);
    hash = ssa_platform_hash_u32(hash, (TZrUInt32)abi->endianness);
    hash = ssa_platform_hash_u32(hash, abi->maxAlignment);
    hash = ssa_platform_hash_u32(hash, abi->int8WidthBits);
    hash = ssa_platform_hash_u32(hash, abi->int16WidthBits);
    hash = ssa_platform_hash_u32(hash, abi->int32WidthBits);
    hash = ssa_platform_hash_u32(hash, abi->int64WidthBits);
    hash = ssa_platform_hash_u32(hash, abi->float32WidthBits);
    hash = ssa_platform_hash_u32(hash, abi->float64WidthBits);
    hash = ssa_platform_hash_u32(hash, (TZrUInt32)abi->structReturnKind);
    return hash;
}

static TZrBool ssa_platform_valid_target(EZrSsaPlatformTarget target) {
    return target > ZR_SSA_PLATFORM_TARGET_UNKNOWN &&
           target < ZR_SSA_PLATFORM_TARGET_COUNT;
}

static TZrBool ssa_platform_valid_architecture(
        EZrSsaPlatformArchitecture architecture) {
    return architecture > ZR_SSA_PLATFORM_ARCH_UNKNOWN &&
           architecture < ZR_SSA_PLATFORM_ARCH_COUNT;
}

static TZrBool ssa_platform_valid_backend(EZrSsaPlatformBackend backend) {
    return backend > ZR_SSA_PLATFORM_BACKEND_NONE &&
           backend < ZR_SSA_PLATFORM_BACKEND_COUNT;
}

static TZrBool ssa_platform_valid_runner(EZrSsaPlatformRunner runner) {
    return runner > ZR_SSA_PLATFORM_RUNNER_NONE &&
           runner < ZR_SSA_PLATFORM_RUNNER_COUNT;
}

static TZrBool ssa_platform_valid_outcome(EZrSsaPlatformOutcome outcome) {
    return outcome > ZR_SSA_PLATFORM_OUTCOME_UNSET &&
           outcome < ZR_SSA_PLATFORM_OUTCOME_COUNT;
}

static TZrBool ssa_platform_valid_dispatch(EZrSsaPlatformDispatchKind dispatch) {
    return dispatch == ZR_SSA_PLATFORM_DISPATCH_UNKNOWN ||
           dispatch == ZR_SSA_PLATFORM_DISPATCH_SWITCH ||
           dispatch == ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO;
}

static TZrBool ssa_platform_target_architecture_compatible(
        EZrSsaPlatformTarget target,
        EZrSsaPlatformArchitecture architecture) {
    if (target == ZR_SSA_PLATFORM_TARGET_IOS) {
        return architecture == ZR_SSA_PLATFORM_ARCH_AARCH64;
    }
    if (target == ZR_SSA_PLATFORM_TARGET_WASM) {
        return architecture == ZR_SSA_PLATFORM_ARCH_WASM32 ||
               architecture == ZR_SSA_PLATFORM_ARCH_WASM64;
    }
    return architecture == ZR_SSA_PLATFORM_ARCH_X86_64 ||
           architecture == ZR_SSA_PLATFORM_ARCH_AARCH64;
}

static TZrBool ssa_platform_string_present(const TZrChar *value,
                                            TZrSize capacity) {
    TZrSize index;

    if (value == ZR_NULL || capacity == 0u) {
        return ZR_FALSE;
    }
    for (index = 0u; index < capacity; ++index) {
        if (value[index] == '\0') {
            return index != 0u;
        }
    }
    return ZR_FALSE;
}

static void ssa_platform_set_status(SZrSsaPlatformDiagnostic *diagnostic,
                                     EZrSsaPlatformStatus status) {
    if (diagnostic != ZR_NULL) {
        diagnostic->status = status;
    }
}

static void ssa_platform_copy_observation_context(
        SZrSsaPlatformDiagnostic *diagnostic,
        const SZrSsaPlatformObservation *observation) {
    if (diagnostic == ZR_NULL || observation == ZR_NULL) {
        return;
    }
    diagnostic->target = observation->target;
    diagnostic->architecture = observation->architecture;
    diagnostic->backend = observation->backend;
    diagnostic->runner = observation->runner;
    diagnostic->requiredFeatures = observation->requiredFeatures;
    diagnostic->unsupportedFeatures = observation->unsupportedFeatures;
    diagnostic->sourceId = observation->sourceId;
    diagnostic->instructionId = observation->instructionId;
}

static void ssa_platform_copy_artifact_context(
        SZrSsaPlatformDiagnostic *diagnostic,
        const SZrSsaPlatformArtifactContract *artifact) {
    if (diagnostic == ZR_NULL || artifact == ZR_NULL) {
        return;
    }
    diagnostic->target = artifact->target;
    diagnostic->architecture = artifact->architecture;
    diagnostic->requiredFeatures = artifact->requiredFeatures;
}

static EZrSsaPlatformStatus ssa_platform_validate_identity(
        TZrUInt32 magic,
        TZrUInt32 schemaVersion,
        EZrSsaPlatformTarget target,
        EZrSsaPlatformArchitecture architecture,
        const TZrChar *targetTriple,
        SZrSsaPlatformDiagnostic *diagnostic) {
    if (magic != ZR_SSA_PLATFORM_CONTRACT_MAGIC ||
        schemaVersion != ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_SCHEMA_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_SCHEMA_MISMATCH;
    }
    if (!ssa_platform_valid_target(target)) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_TARGET_INVALID);
        return ZR_SSA_PLATFORM_STATUS_TARGET_INVALID;
    }
    if (!ssa_platform_valid_architecture(architecture)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_ARCHITECTURE_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ARCHITECTURE_INVALID;
    }
    if (!ssa_platform_target_architecture_compatible(target, architecture)) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_ARCHITECTURE_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ARCHITECTURE_INVALID;
    }
    if (!ssa_platform_string_present(
                targetTriple, ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISSING);
        return ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISSING;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
    return ZR_SSA_PLATFORM_STATUS_OK;
}

static TZrBool ssa_platform_is_machine_code_forbidden(
        EZrSsaPlatformTarget target) {
    return target == ZR_SSA_PLATFORM_TARGET_ANDROID ||
           target == ZR_SSA_PLATFORM_TARGET_IOS ||
           target == ZR_SSA_PLATFORM_TARGET_WASM;
}

static TZrBool ssa_platform_feature_allowed(
        EZrSsaPlatformTarget target,
        TZrUInt64 feature) {
    if (ssa_platform_is_machine_code_forbidden(target) &&
        feature == ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) {
        return ZR_FALSE;
    }
    /* Browser/WASM runtimes have no portable PMU contract.  A future threaded
     * WASM profile may opt into THREADS explicitly; this base contract never
     * infers that capability from the target name. */
    if (target == ZR_SSA_PLATFORM_TARGET_WASM &&
        feature == ZR_SSA_PLATFORM_FEATURE_PMU) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrUInt64 ssa_platform_backend_feature(EZrSsaPlatformBackend backend) {
    switch (backend) {
        case ZR_SSA_PLATFORM_BACKEND_EXECBC:
            return ZR_SSA_PLATFORM_FEATURE_EXECBC;
        case ZR_SSA_PLATFORM_BACKEND_AOT_C:
            return ZR_SSA_PLATFORM_FEATURE_AOT_C;
        case ZR_SSA_PLATFORM_BACKEND_AOT_LLVM:
            return ZR_SSA_PLATFORM_FEATURE_AOT_LLVM;
        case ZR_SSA_PLATFORM_BACKEND_HOST_JIT:
            return ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT;
        default:
            return 0u;
    }
}

void ZrCommon_SsaPlatform_DiagnosticInit(
        SZrSsaPlatformDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->status = ZR_SSA_PLATFORM_STATUS_OK;
    }
}

const TZrChar *ZrCommon_SsaPlatform_StatusName(
        EZrSsaPlatformStatus status) {
    switch (status) {
        case ZR_SSA_PLATFORM_STATUS_OK:
            return "ok";
        case ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_SSA_PLATFORM_STATUS_SCHEMA_MISMATCH:
            return "schema-mismatch";
        case ZR_SSA_PLATFORM_STATUS_TARGET_INVALID:
            return "target-invalid";
        case ZR_SSA_PLATFORM_STATUS_ARCHITECTURE_INVALID:
            return "architecture-invalid";
        case ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISSING:
            return "target-triple-missing";
        case ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH:
            return "target-triple-mismatch";
        case ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID:
            return "feature-flags-invalid";
        case ZR_SSA_PLATFORM_STATUS_BACKEND_INVALID:
            return "backend-invalid";
        case ZR_SSA_PLATFORM_STATUS_RUNNER_INVALID:
            return "runner-invalid";
        case ZR_SSA_PLATFORM_STATUS_OUTCOME_INVALID:
            return "outcome-invalid";
        case ZR_SSA_PLATFORM_STATUS_ABI_INVALID:
            return "abi-invalid";
        case ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH:
            return "abi-mismatch";
        case ZR_SSA_PLATFORM_STATUS_NUMERIC_CONTRACT_MISMATCH:
            return "numeric-contract-mismatch";
        case ZR_SSA_PLATFORM_STATUS_LAYOUT_CONTRACT_MISMATCH:
            return "layout-contract-mismatch";
        case ZR_SSA_PLATFORM_STATUS_ARTIFACT_TARGET_MISMATCH:
            return "artifact-target-mismatch";
        case ZR_SSA_PLATFORM_STATUS_ARTIFACT_ARCHITECTURE_MISMATCH:
            return "artifact-architecture-mismatch";
        case ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH:
            return "artifact-abi-mismatch";
        case ZR_SSA_PLATFORM_STATUS_ARTIFACT_NUMERIC_CONTRACT_MISMATCH:
            return "artifact-numeric-contract-mismatch";
        case ZR_SSA_PLATFORM_STATUS_ARTIFACT_LAYOUT_CONTRACT_MISMATCH:
            return "artifact-layout-contract-mismatch";
        case ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED:
            return "backend-unsupported";
        case ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED:
            return "required-feature-unsupported";
        case ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN:
            return "machine-code-jit-forbidden";
        case ZR_SSA_PLATFORM_STATUS_RESTRICTED_PATCH_NATIVE_IMPORT:
            return "restricted-patch-native-import";
        case ZR_SSA_PLATFORM_STATUS_COMPILE_NOT_COMPLETED:
            return "compile-not-completed";
        case ZR_SSA_PLATFORM_STATUS_RUNTIME_NOT_EXECUTED:
            return "runtime-not-executed";
        case ZR_SSA_PLATFORM_STATUS_SEMANTIC_FAILURE:
            return "semantic-failure";
        case ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID:
            return "observation-invalid";
        case ZR_SSA_PLATFORM_STATUS_RUNTIME_UNAVAILABLE:
            return "runtime-unavailable";
        case ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH:
            return "semantic-witness-mismatch";
        default:
            return "invalid-platform-status";
    }
}

const TZrChar *ZrCommon_SsaPlatform_OutcomeName(
        EZrSsaPlatformOutcome outcome) {
    switch (outcome) {
        case ZR_SSA_PLATFORM_OUTCOME_PASSED:
            return "passed";
        case ZR_SSA_PLATFORM_OUTCOME_UNAVAILABLE:
            return "unavailable";
        case ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED:
            return "unsupported";
        case ZR_SSA_PLATFORM_OUTCOME_FAILED:
            return "failed";
        default:
            return "unset";
    }
}

void ZrCommon_SsaPlatform_AbiInit(SZrSsaPlatformAbi *abi) {
    if (abi != ZR_NULL) {
        memset(abi, 0, sizeof(*abi));
        abi->endianness = ZR_SSA_PLATFORM_ENDIAN_UNKNOWN;
        abi->structReturnKind = ZR_SSA_PLATFORM_STRUCT_RETURN_UNKNOWN;
    }
}

void ZrCommon_SsaPlatform_DetectHostAbi(SZrSsaPlatformAbi *abi) {
    const TZrUInt16 endianProbe = 1u;
    const TZrUInt8 *endianBytes = (const TZrUInt8 *)(const void *)&endianProbe;

    if (abi == ZR_NULL) {
        return;
    }
    ZrCommon_SsaPlatform_AbiInit(abi);
    abi->pointerWidthBits = (TZrUInt32)(sizeof(void *) * CHAR_BIT);
    abi->endianness = endianBytes[0] == 1u
                          ? ZR_SSA_PLATFORM_ENDIAN_LITTLE
                          : ZR_SSA_PLATFORM_ENDIAN_BIG;
#if defined(_MSC_VER)
    abi->maxAlignment = (TZrUInt32)ZR_ALIGN_SIZE;
#else
    abi->maxAlignment = (TZrUInt32)alignof(max_align_t);
#endif
    abi->int8WidthBits = (TZrUInt32)(sizeof(TZrInt8) * CHAR_BIT);
    abi->int16WidthBits = (TZrUInt32)(sizeof(TZrInt16) * CHAR_BIT);
    abi->int32WidthBits = (TZrUInt32)(sizeof(TZrInt32) * CHAR_BIT);
    abi->int64WidthBits = (TZrUInt32)(sizeof(TZrInt64) * CHAR_BIT);
    abi->float32WidthBits = (TZrUInt32)(sizeof(TZrFloat32) * CHAR_BIT);
    abi->float64WidthBits = (TZrUInt32)(sizeof(TZrFloat64) * CHAR_BIT);
    /* A portable probe cannot infer every aggregate return class.  MIXED is a
     * valid conservative witness and target-specific probes may replace it. */
    abi->structReturnKind = ZR_SSA_PLATFORM_STRUCT_RETURN_MIXED;
    abi->nativeCallbackAbiHash =
            ssa_platform_hash_abi_without_callback(abi) ^ UINT64_C(0x9e3779b97f4a7c15);
    if (abi->nativeCallbackAbiHash == 0u) {
        abi->nativeCallbackAbiHash = 1u;
    }
}

TZrUInt64 ZrCommon_SsaPlatform_ComputeAbiHash(
        const SZrSsaPlatformAbi *abi) {
    TZrUInt64 hash;

    if (abi == ZR_NULL) {
        return 0u;
    }
    hash = ssa_platform_hash_abi_without_callback(abi);
    return ssa_platform_hash_u64(hash, abi->nativeCallbackAbiHash);
}

EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateAbi(
        const SZrSsaPlatformAbi *abi,
        SZrSsaPlatformDiagnostic *diagnostic) {
    if (abi == ZR_NULL) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT);
        return ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT;
    }
    if (abi->pointerWidthBits != 32u && abi->pointerWidthBits != 64u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_POINTER_WIDTH;
            diagnostic->actual = abi->pointerWidthBits;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (abi->endianness <= ZR_SSA_PLATFORM_ENDIAN_UNKNOWN ||
        abi->endianness >= ZR_SSA_PLATFORM_ENDIAN_COUNT) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_ENDIANNESS;
            diagnostic->actual = (TZrUInt64)abi->endianness;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (abi->maxAlignment == 0u ||
        abi->maxAlignment > ZR_SSA_PLATFORM_MAX_ALIGNMENT ||
        (abi->maxAlignment & (abi->maxAlignment - 1u)) != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_MAX_ALIGNMENT;
            diagnostic->actual = abi->maxAlignment;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (abi->int8WidthBits != ZR_SSA_PLATFORM_INT8_WIDTH_BITS ||
        abi->int16WidthBits != ZR_SSA_PLATFORM_INT16_WIDTH_BITS ||
        abi->int32WidthBits != ZR_SSA_PLATFORM_INT32_WIDTH_BITS ||
        abi->int64WidthBits != ZR_SSA_PLATFORM_INT64_WIDTH_BITS) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = abi->int8WidthBits != 8u
                                       ? ZR_SSA_PLATFORM_ABI_FIELD_INT8_WIDTH
                                       : abi->int16WidthBits != 16u
                                             ? ZR_SSA_PLATFORM_ABI_FIELD_INT16_WIDTH
                                             : abi->int32WidthBits != 32u
                                                   ? ZR_SSA_PLATFORM_ABI_FIELD_INT32_WIDTH
                                                   : ZR_SSA_PLATFORM_ABI_FIELD_INT64_WIDTH;
            diagnostic->actual = abi->int8WidthBits != 8u
                                      ? abi->int8WidthBits
                                      : abi->int16WidthBits != 16u
                                            ? abi->int16WidthBits
                                            : abi->int32WidthBits != 32u
                                                  ? abi->int32WidthBits
                                                  : abi->int64WidthBits;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (abi->float32WidthBits != ZR_SSA_PLATFORM_FLOAT32_WIDTH_BITS ||
        abi->float64WidthBits != ZR_SSA_PLATFORM_FLOAT64_WIDTH_BITS) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = abi->float32WidthBits != 32u
                                       ? ZR_SSA_PLATFORM_ABI_FIELD_FLOAT32_WIDTH
                                       : ZR_SSA_PLATFORM_ABI_FIELD_FLOAT64_WIDTH;
            diagnostic->actual = abi->float32WidthBits != 32u
                                      ? abi->float32WidthBits
                                      : abi->float64WidthBits;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (abi->structReturnKind <= ZR_SSA_PLATFORM_STRUCT_RETURN_UNKNOWN ||
        abi->structReturnKind >= ZR_SSA_PLATFORM_STRUCT_RETURN_COUNT) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_STRUCT_RETURN;
            diagnostic->actual = (TZrUInt64)abi->structReturnKind;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (abi->nativeCallbackAbiHash == 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_NATIVE_CALLBACK;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
    return ZR_SSA_PLATFORM_STATUS_OK;
}

TZrBool ZrCommon_SsaPlatform_AbiEqual(
        const SZrSsaPlatformAbi *expected,
        const SZrSsaPlatformAbi *actual,
        SZrSsaPlatformDiagnostic *diagnostic) {
    if (expected == ZR_NULL || actual == ZR_NULL) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT);
        return ZR_FALSE;
    }
    if (ZrCommon_SsaPlatform_ValidateAbi(expected, diagnostic) !=
            ZR_SSA_PLATFORM_STATUS_OK ||
        ZrCommon_SsaPlatform_ValidateAbi(actual, diagnostic) !=
            ZR_SSA_PLATFORM_STATUS_OK) {
        return ZR_FALSE;
    }
    if (expected->pointerWidthBits != actual->pointerWidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_POINTER_WIDTH;
            diagnostic->expected = expected->pointerWidthBits;
            diagnostic->actual = actual->pointerWidthBits;
        }
    } else if (expected->endianness != actual->endianness) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_ENDIANNESS;
            diagnostic->expected = expected->endianness;
            diagnostic->actual = actual->endianness;
        }
    } else if (expected->maxAlignment != actual->maxAlignment) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_MAX_ALIGNMENT;
            diagnostic->expected = expected->maxAlignment;
            diagnostic->actual = actual->maxAlignment;
        }
    } else if (expected->int8WidthBits != actual->int8WidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_INT8_WIDTH;
            diagnostic->expected = expected->int8WidthBits;
            diagnostic->actual = actual->int8WidthBits;
        }
    } else if (expected->int16WidthBits != actual->int16WidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_INT16_WIDTH;
            diagnostic->expected = expected->int16WidthBits;
            diagnostic->actual = actual->int16WidthBits;
        }
    } else if (expected->int32WidthBits != actual->int32WidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_INT32_WIDTH;
            diagnostic->expected = expected->int32WidthBits;
            diagnostic->actual = actual->int32WidthBits;
        }
    } else if (expected->int64WidthBits != actual->int64WidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_INT64_WIDTH;
            diagnostic->expected = expected->int64WidthBits;
            diagnostic->actual = actual->int64WidthBits;
        }
    } else if (expected->float32WidthBits != actual->float32WidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_FLOAT32_WIDTH;
            diagnostic->expected = expected->float32WidthBits;
            diagnostic->actual = actual->float32WidthBits;
        }
    } else if (expected->float64WidthBits != actual->float64WidthBits) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_FLOAT64_WIDTH;
            diagnostic->expected = expected->float64WidthBits;
            diagnostic->actual = actual->float64WidthBits;
        }
    } else if (expected->structReturnKind != actual->structReturnKind) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_STRUCT_RETURN;
            diagnostic->expected = expected->structReturnKind;
            diagnostic->actual = actual->structReturnKind;
        }
    } else if (expected->nativeCallbackAbiHash != actual->nativeCallbackAbiHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_NATIVE_CALLBACK;
            diagnostic->expected = expected->nativeCallbackAbiHash;
            diagnostic->actual = actual->nativeCallbackAbiHash;
        }
    } else {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
        return ZR_TRUE;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH);
    return ZR_FALSE;
}

void ZrCommon_SsaPlatform_CapabilityInit(
        SZrSsaPlatformCapability *capability) {
    if (capability != ZR_NULL) {
        memset(capability, 0, sizeof(*capability));
        capability->magic = ZR_SSA_PLATFORM_CONTRACT_MAGIC;
        capability->schemaVersion = ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION;
        capability->dispatchFlags = ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH |
                                    ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO;
        ZrCommon_SsaPlatform_AbiInit(&capability->abi);
    }
}

EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateCapability(
        const SZrSsaPlatformCapability *capability,
        SZrSsaPlatformDiagnostic *diagnostic) {
    EZrSsaPlatformStatus status;

    if (capability == ZR_NULL) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT);
        return ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT;
    }
    status = ssa_platform_validate_identity(
            capability->magic, capability->schemaVersion, capability->target,
            capability->architecture, capability->targetTriple, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    if ((capability->featureFlags & ~ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK) != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->actual = capability->featureFlags;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID);
        return ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID;
    }
    if ((capability->dispatchFlags & ~ZR_SSA_PLATFORM_DISPATCH_FLAG_KNOWN_MASK) != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->actual = capability->dispatchFlags;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID);
        return ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID;
    }
    status = ZrCommon_SsaPlatform_ValidateAbi(&capability->abi, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    if (capability->target == ZR_SSA_PLATFORM_TARGET_WASM &&
        ((capability->architecture == ZR_SSA_PLATFORM_ARCH_WASM32 &&
          capability->abi.pointerWidthBits != 32u) ||
         (capability->architecture == ZR_SSA_PLATFORM_ARCH_WASM64 &&
          capability->abi.pointerWidthBits != 64u))) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_POINTER_WIDTH;
            diagnostic->expected = capability->architecture ==
                                           ZR_SSA_PLATFORM_ARCH_WASM32
                                       ? 32u
                                       : 64u;
            diagnostic->actual = capability->abi.pointerWidthBits;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    if (!ssa_platform_feature_allowed(
                capability->target,
                ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) &&
        (capability->featureFlags & ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) != 0u) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
        return ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN;
    }
    if (!ssa_platform_feature_allowed(
                capability->target, ZR_SSA_PLATFORM_FEATURE_PMU) &&
        (capability->featureFlags & ZR_SSA_PLATFORM_FEATURE_PMU) != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures = ZR_SSA_PLATFORM_FEATURE_PMU;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED);
        return ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED;
    }
    if (capability->abiHash == 0u || capability->numericContractHash == 0u ||
        capability->layoutContractHash == 0u) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_INVALID);
        return ZR_SSA_PLATFORM_STATUS_ABI_INVALID;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
    return ZR_SSA_PLATFORM_STATUS_OK;
}

TZrBool ZrCommon_SsaPlatform_CapabilitySupports(
        const SZrSsaPlatformCapability *capability,
        TZrUInt64 feature) {
    if (capability == ZR_NULL || feature == 0u ||
        (feature & (feature - 1u)) != 0u ||
        (feature & ~ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK) != 0u ||
        !ssa_platform_feature_allowed(capability->target, feature)) {
        return ZR_FALSE;
    }
    return (capability->featureFlags & feature) != 0u;
}

void ZrCommon_SsaPlatform_ObservationInit(
        SZrSsaPlatformObservation *observation) {
    if (observation != ZR_NULL) {
        memset(observation, 0, sizeof(*observation));
        observation->magic = ZR_SSA_PLATFORM_CONTRACT_MAGIC;
        observation->schemaVersion = ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION;
        observation->outcome = ZR_SSA_PLATFORM_OUTCOME_UNSET;
        ZrCommon_SsaPlatform_AbiInit(&observation->observedAbi);
    }
}

EZrSsaPlatformStatus ZrCommon_SsaPlatform_Check(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformObservation *observed,
        SZrSsaPlatformDiagnostic *diagnostic) {
    EZrSsaPlatformStatus status;
    TZrUInt64 unsupported;
    TZrUInt64 backendFeature;

    ZrCommon_SsaPlatform_DiagnosticInit(diagnostic);
    ssa_platform_copy_observation_context(diagnostic, observed);
    if (declared == ZR_NULL || observed == ZR_NULL) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT);
        return ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT;
    }
    status = ZrCommon_SsaPlatform_ValidateCapability(declared, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    status = ssa_platform_validate_identity(
            observed->magic, observed->schemaVersion, observed->target,
            observed->architecture, observed->targetTriple, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    if (!ssa_platform_valid_backend(observed->backend)) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_BACKEND_INVALID);
        return ZR_SSA_PLATFORM_STATUS_BACKEND_INVALID;
    }
    if (!ssa_platform_valid_runner(observed->runner)) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_RUNNER_INVALID);
        return ZR_SSA_PLATFORM_STATUS_RUNNER_INVALID;
    }
    if (!ssa_platform_valid_outcome(observed->outcome)) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OUTCOME_INVALID);
        return ZR_SSA_PLATFORM_STATUS_OUTCOME_INVALID;
    }
    if (!ssa_platform_valid_dispatch(observed->dispatchKind)) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    if ((observed->dispatchFlags & ~ZR_SSA_PLATFORM_DISPATCH_FLAG_KNOWN_MASK) != 0u ||
        (observed->dispatchKind == ZR_SSA_PLATFORM_DISPATCH_SWITCH &&
         (observed->dispatchFlags & ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH) == 0u) ||
        (observed->dispatchKind == ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO &&
         (observed->dispatchFlags &
          ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO) == 0u)) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    if (observed->target != declared->target) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->target;
            diagnostic->actual = observed->target;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_ARTIFACT_TARGET_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_TARGET_MISMATCH;
    }
    if (observed->architecture != declared->architecture) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->architecture;
            diagnostic->actual = observed->architecture;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_ARTIFACT_ARCHITECTURE_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_ARCHITECTURE_MISMATCH;
    }
    if (strcmp(declared->targetTriple, observed->targetTriple) != 0) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH;
    }
    status = ZrCommon_SsaPlatform_ValidateAbi(&observed->observedAbi, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    if (!ZrCommon_SsaPlatform_AbiEqual(
                &declared->abi, &observed->observedAbi, diagnostic)) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH;
    }
    if (observed->observedAbiHash != declared->abiHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->abiField = ZR_SSA_PLATFORM_ABI_FIELD_HASH;
            diagnostic->expected = declared->abiHash;
            diagnostic->actual = observed->observedAbiHash;
        }
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ABI_MISMATCH;
    }
    if (observed->observedNumericContractHash != declared->numericContractHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->numericContractHash;
            diagnostic->actual = observed->observedNumericContractHash;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_NUMERIC_CONTRACT_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_NUMERIC_CONTRACT_MISMATCH;
    }
    if (observed->observedLayoutContractHash != declared->layoutContractHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->layoutContractHash;
            diagnostic->actual = observed->observedLayoutContractHash;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_LAYOUT_CONTRACT_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_LAYOUT_CONTRACT_MISMATCH;
    }
    if ((observed->requiredFeatures & ~ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK) != 0u) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID);
        return ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID;
    }
    if ((observed->unsupportedFeatures & ~observed->requiredFeatures) != 0u) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    if (ssa_platform_is_machine_code_forbidden(declared->target) &&
        (observed->backend == ZR_SSA_PLATFORM_BACKEND_HOST_JIT ||
         observed->machineCodeJitExecuted ||
         (observed->requiredFeatures & ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) != 0u)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
        return ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN;
    }
    unsupported = observed->requiredFeatures & ~declared->featureFlags;
    backendFeature = ssa_platform_backend_feature(observed->backend);
    if ((backendFeature & ~declared->featureFlags) != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures = backendFeature;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED);
        return ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED;
    }
    if (unsupported != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures = unsupported;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED);
        return ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED;
    }
    if ((observed->dispatchFlags & ~declared->dispatchFlags) != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->dispatchFlags;
            diagnostic->actual = observed->dispatchFlags;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    if (observed->outcome == ZR_SSA_PLATFORM_OUTCOME_UNAVAILABLE) {
        if (observed->executed || observed->semanticPassed) {
            ssa_platform_set_status(diagnostic,
                                     ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
            return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_RUNTIME_UNAVAILABLE);
        return ZR_SSA_PLATFORM_STATUS_RUNTIME_UNAVAILABLE;
    }
    if (observed->outcome == ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED) {
        if (observed->unsupportedFeatures == 0u) {
            ssa_platform_set_status(
                    diagnostic, ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
            return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
        }
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures = observed->unsupportedFeatures;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED);
        return ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED;
    }
    /* A passed row must not carry an unsupported-feature witness.  Without
     * this check a producer could set outcome=PASSED while leaving a required
     * feature unavailable; the Boolean test entry point would then report a
     * false success even though IsRuntimeAcceptance() rejects the row. */
    if (observed->outcome == ZR_SSA_PLATFORM_OUTCOME_PASSED &&
        observed->unsupportedFeatures != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures = observed->unsupportedFeatures;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    if (!observed->compiled) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_COMPILE_NOT_COMPLETED);
        return ZR_SSA_PLATFORM_STATUS_COMPILE_NOT_COMPLETED;
    }
    if (!observed->executed) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_RUNTIME_NOT_EXECUTED);
        return ZR_SSA_PLATFORM_STATUS_RUNTIME_NOT_EXECUTED;
    }
    if (!observed->semanticPassed) {
        ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_SEMANTIC_FAILURE);
        return ZR_SSA_PLATFORM_STATUS_SEMANTIC_FAILURE;
    }
    if (declared->semanticResultHash != 0u || observed->semanticResultHash != 0u) {
        if (declared->semanticResultHash != observed->semanticResultHash) {
            if (diagnostic != ZR_NULL) {
                diagnostic->witnessField = ZR_SSA_PLATFORM_WITNESS_FIELD_RESULT;
                diagnostic->expected = declared->semanticResultHash;
                diagnostic->actual = observed->semanticResultHash;
            }
            ssa_platform_set_status(
                    diagnostic, ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH);
            return ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH;
        }
    }
    if (declared->exceptionContractHash != 0u ||
        observed->exceptionContractHash != 0u) {
        if (declared->exceptionContractHash != observed->exceptionContractHash) {
            if (diagnostic != ZR_NULL) {
                diagnostic->witnessField = ZR_SSA_PLATFORM_WITNESS_FIELD_EXCEPTION;
                diagnostic->expected = declared->exceptionContractHash;
                diagnostic->actual = observed->exceptionContractHash;
            }
            ssa_platform_set_status(
                    diagnostic, ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH);
            return ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH;
        }
    }
    if (declared->sourceMapHash != 0u || observed->sourceMapHash != 0u) {
        if (declared->sourceMapHash != observed->sourceMapHash) {
            if (diagnostic != ZR_NULL) {
                diagnostic->witnessField = ZR_SSA_PLATFORM_WITNESS_FIELD_SOURCE_MAP;
                diagnostic->expected = declared->sourceMapHash;
                diagnostic->actual = observed->sourceMapHash;
            }
            ssa_platform_set_status(
                    diagnostic, ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH);
            return ZR_SSA_PLATFORM_STATUS_SEMANTIC_WITNESS_MISMATCH;
        }
    }
    if (observed->outcome != ZR_SSA_PLATFORM_OUTCOME_PASSED) {
        ssa_platform_set_status(diagnostic,
                                 observed->outcome == ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED
                                     ? ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED
                                     : ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID);
        return observed->outcome == ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED
                   ? ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED
                   : ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
    return ZR_SSA_PLATFORM_STATUS_OK;
}

TZrBool ZrCommon_SsaPlatform_IsRuntimeAcceptance(
        const SZrSsaPlatformObservation *observation) {
    if (observation == ZR_NULL ||
        observation->magic != ZR_SSA_PLATFORM_CONTRACT_MAGIC ||
        observation->schemaVersion != ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION ||
        !ssa_platform_valid_target(observation->target) ||
        !ssa_platform_valid_architecture(observation->architecture) ||
        !ssa_platform_target_architecture_compatible(
                observation->target, observation->architecture) ||
        !ssa_platform_string_present(
                observation->targetTriple,
                ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY) ||
        !ssa_platform_valid_backend(observation->backend) ||
        !ssa_platform_valid_runner(observation->runner) ||
        !ssa_platform_valid_outcome(observation->outcome) ||
        !ssa_platform_valid_dispatch(observation->dispatchKind) ||
        (observation->requiredFeatures &
         ~ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK) != 0u ||
        (observation->unsupportedFeatures &
         ~ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK) != 0u ||
        (observation->unsupportedFeatures &
         ~observation->requiredFeatures) != 0u ||
        (observation->dispatchFlags &
         ~ZR_SSA_PLATFORM_DISPATCH_FLAG_KNOWN_MASK) != 0u ||
        (observation->dispatchKind == ZR_SSA_PLATFORM_DISPATCH_SWITCH &&
         (observation->dispatchFlags & ZR_SSA_PLATFORM_DISPATCH_FLAG_SWITCH) ==
                 0u) ||
        (observation->dispatchKind == ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO &&
         (observation->dispatchFlags &
          ZR_SSA_PLATFORM_DISPATCH_FLAG_COMPUTED_GOTO) == 0u) ||
        ZrCommon_SsaPlatform_ValidateAbi(&observation->observedAbi, ZR_NULL) !=
                ZR_SSA_PLATFORM_STATUS_OK) {
        return ZR_FALSE;
    }
    return observation->compiled && observation->executed &&
           observation->semanticPassed &&
           observation->outcome == ZR_SSA_PLATFORM_OUTCOME_PASSED &&
           observation->unsupportedFeatures == 0u &&
           !(observation->machineCodeJitExecuted &&
             ssa_platform_is_machine_code_forbidden(observation->target));
}

void ZrCommon_SsaPlatform_ArtifactContractInit(
        SZrSsaPlatformArtifactContract *artifact) {
    if (artifact != ZR_NULL) {
        memset(artifact, 0, sizeof(*artifact));
        artifact->magic = ZR_SSA_PLATFORM_CONTRACT_MAGIC;
        artifact->schemaVersion = ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION;
        ZrCommon_SsaPlatform_AbiInit(&artifact->abi);
    }
}

EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateArtifact(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformArtifactContract *artifact,
        SZrSsaPlatformDiagnostic *diagnostic) {
    EZrSsaPlatformStatus status;
    TZrUInt64 unsupported;

    ZrCommon_SsaPlatform_DiagnosticInit(diagnostic);
    ssa_platform_copy_artifact_context(diagnostic, artifact);
    if (declared == ZR_NULL || artifact == ZR_NULL) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT);
        return ZR_SSA_PLATFORM_STATUS_INVALID_ARGUMENT;
    }
    status = ZrCommon_SsaPlatform_ValidateCapability(declared, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    status = ssa_platform_validate_identity(
            artifact->magic, artifact->schemaVersion, artifact->target,
            artifact->architecture, artifact->targetTriple, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    if (artifact->target != declared->target) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->target;
            diagnostic->actual = artifact->target;
        }
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_ARTIFACT_TARGET_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_TARGET_MISMATCH;
    }
    if (artifact->architecture != declared->architecture) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->architecture;
            diagnostic->actual = artifact->architecture;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_ARTIFACT_ARCHITECTURE_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_ARCHITECTURE_MISMATCH;
    }
    if (strcmp(declared->targetTriple, artifact->targetTriple) != 0) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISMATCH;
    }
    status = ZrCommon_SsaPlatform_ValidateAbi(&artifact->abi, diagnostic);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        return status;
    }
    if (!ZrCommon_SsaPlatform_AbiEqual(
                &declared->abi, &artifact->abi, diagnostic)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH;
    }
    if (artifact->abiHash != declared->abiHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->abiHash;
            diagnostic->actual = artifact->abiHash;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_ABI_MISMATCH;
    }
    if (artifact->numericContractHash != declared->numericContractHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->numericContractHash;
            diagnostic->actual = artifact->numericContractHash;
        }
        ssa_platform_set_status(
                diagnostic,
                ZR_SSA_PLATFORM_STATUS_ARTIFACT_NUMERIC_CONTRACT_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_NUMERIC_CONTRACT_MISMATCH;
    }
    if (artifact->layoutContractHash != declared->layoutContractHash) {
        if (diagnostic != ZR_NULL) {
            diagnostic->expected = declared->layoutContractHash;
            diagnostic->actual = artifact->layoutContractHash;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_ARTIFACT_LAYOUT_CONTRACT_MISMATCH);
        return ZR_SSA_PLATFORM_STATUS_ARTIFACT_LAYOUT_CONTRACT_MISMATCH;
    }
    if ((artifact->requiredFeatures & ~ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK) != 0u) {
        ssa_platform_set_status(diagnostic,
                                 ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID);
        return ZR_SSA_PLATFORM_STATUS_FEATURE_FLAGS_INVALID;
    }
    unsupported = artifact->requiredFeatures & ~declared->featureFlags;
    if (unsupported != 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures = unsupported;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED);
        return ZR_SSA_PLATFORM_STATUS_REQUIRED_FEATURE_UNSUPPORTED;
    }
    if (artifact->restrictedPatch &&
        artifact->target == ZR_SSA_PLATFORM_TARGET_IOS &&
        artifact->newNativeImportCount != 0u) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_RESTRICTED_PATCH_NATIVE_IMPORT);
        return ZR_SSA_PLATFORM_STATUS_RESTRICTED_PATCH_NATIVE_IMPORT;
    }
    if ((artifact->requiredFeatures &
         ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) != 0u &&
        ssa_platform_is_machine_code_forbidden(declared->target)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
        return ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN;
    }
    if (artifact->machineCodeJit &&
        ssa_platform_is_machine_code_forbidden(declared->target)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN);
        return ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN;
    }
    if (artifact->machineCodeJit &&
        (declared->featureFlags & ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) == 0u) {
        if (diagnostic != ZR_NULL) {
            diagnostic->unsupportedFeatures =
                    ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT;
        }
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED);
        return ZR_SSA_PLATFORM_STATUS_BACKEND_UNSUPPORTED;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
    return ZR_SSA_PLATFORM_STATUS_OK;
}

TZrBool ZrTests_Ssa_CheckPlatform(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformObservation *observed) {
    SZrSsaPlatformDiagnostic diagnostic;

    return ZrCommon_SsaPlatform_Check(declared, observed, &diagnostic) ==
           ZR_SSA_PLATFORM_STATUS_OK;
}
