#include "zr_vm_common/ssa_platform_contract.h"

#include <string.h>

/* ABI 摘要按字段顺序和固定字节序编码，避免结构体填充和宿主字节序影响
 * 能力声明、产物及运行观察之间的比对。 */
#define ZR_SSA_PLATFORM_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_SSA_PLATFORM_FNV_PRIME UINT64_C(1099511628211)

/* 由 ComputeAbiHash 和宿主回调占位见证共用，不能改动编码顺序而不升级 schema。 */
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

/* UNKNOWN/COUNT 是记录边界，不是可验收的平台或执行结果。 */
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

/* UNKNOWN 分派可用于不依赖 switch/computed-goto 的后端观察。 */
static TZrBool ssa_platform_valid_dispatch(EZrSsaPlatformDispatchKind dispatch) {
    return dispatch == ZR_SSA_PLATFORM_DISPATCH_UNKNOWN ||
           dispatch == ZR_SSA_PLATFORM_DISPATCH_SWITCH ||
           dispatch == ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO;
}

/* 在读取三元组之前先排除平台/架构的显然矛盾；非 WASM 平台可共享桌面架构。 */
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

/* 三元组来自固定容量记录；比较前要求非空且在数组界内结束。 */
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

/* 失败发生在完整校验前也保留观察来源，便于矩阵定位设备与指令。 */
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

/* 能力、观察和产物共用同一身份入口，保证版本与目标组合一致解释。 */
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
    /* TODO: 此处只验证三元组存在，未核对其文本与 target/architecture 是否
     * 相符；当前测试只比对两份相同三元组。后续应查目标适配器是否另有
     * 规范化注册表，再决定是否在此拒绝矛盾的声明。 */
    if (!ssa_platform_string_present(
                targetTriple, ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY)) {
        ssa_platform_set_status(
                diagnostic, ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISSING);
        return ZR_SSA_PLATFORM_STATUS_TARGET_TRIPLE_MISSING;
    }
    ssa_platform_set_status(diagnostic, ZR_SSA_PLATFORM_STATUS_OK);
    return ZR_SSA_PLATFORM_STATUS_OK;
}

/* 目标策略在能力声明、观察和产物三个入口复用，防止只在某一阶段禁用 JIT。 */
static TZrBool ssa_platform_is_machine_code_forbidden(
        EZrSsaPlatformTarget target) {
    return target == ZR_SSA_PLATFORM_TARGET_ANDROID ||
           target == ZR_SSA_PLATFORM_TARGET_IOS ||
           target == ZR_SSA_PLATFORM_TARGET_WASM;
}

/* Both observation entry points use the same execution provenance policy. */
static EZrSsaPlatformStatus ssa_platform_validate_observation_policy(
        const SZrSsaPlatformObservation *observation) {
    if (ssa_platform_is_machine_code_forbidden(observation->target) &&
        (observation->backend == ZR_SSA_PLATFORM_BACKEND_HOST_JIT ||
         observation->machineCodeJitExecuted ||
         (observation->requiredFeatures &
          ZR_SSA_PLATFORM_FEATURE_MACHINE_CODE_JIT) != 0u)) {
        return ZR_SSA_PLATFORM_STATUS_MACHINE_CODE_JIT_FORBIDDEN;
    }
    if (observation->runner == ZR_SSA_PLATFORM_RUNNER_CROSS_COMPILE &&
        (observation->executed || observation->semanticPassed ||
         observation->outcome == ZR_SSA_PLATFORM_OUTCOME_PASSED)) {
        return ZR_SSA_PLATFORM_STATUS_OBSERVATION_INVALID;
    }
    return ZR_SSA_PLATFORM_STATUS_OK;
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

/* 观察中的后端必须先映射为能力位，才能与声明的 featureFlags 比较。 */
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
    /* 通用探针无法确定聚合返回类别，MIXED 只作保守占位；发布产物时应由
     * 目标专用探针替换，不可把宿主探针直接用于交叉目标。 */
    abi->structReturnKind = ZR_SSA_PLATFORM_STRUCT_RETURN_MIXED;
    /* TODO: 此值仅由通用 ABI 字段推得，并未调用 native callback 验证真实
     * 调用约定。现有测试只检查哈希变化；接入实际 FFI 适配器时需核对
     * 回调参数/返回布局并提供目标专用见证。 */
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
    /* 首个差异决定诊断字段，使目标矩阵可稳定归因；调用方需要完整差异时
     * 应保留两份 ABI 记录，而不能仅依赖此单字段诊断。 */
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
    /* WASM 架构名称蕴含指针宽度，其余平台仍以显式 ABI 见证为准。 */
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
    /* 此接口只查询单个位；组合需求由 Check/ValidateArtifact 做集合比较。 */
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

    /* 先重置，再复制原始观察上下文；即使后续身份验证失败仍可报告来源。 */
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
    status = ssa_platform_validate_observation_policy(observed);
    if (status != ZR_SSA_PLATFORM_STATUS_OK) {
        ssa_platform_set_status(diagnostic, status);
        return status;
    }
    /* 后端能力、显式需求及分派许可是三个独立门槛；任一缺失都不能把
     * 编译或运行观察提升为平台验收。 */
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
    /* 不可用/不支持先于阶段成功判断，确保跳过的运行环境不会成为通过行。 */
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
    /* 已通过的观察不得同时报告需求不支持；否则测试布尔入口与独立验收
     * 谓词会对同一行给出相反结论。 */
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
    /* 三类语义见证独立于分派形式；只要一侧声明了哈希，另一侧缺失也算
     * 不匹配，从而避免把未记录异常或源码映射误作一致。 */
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
           ssa_platform_validate_observation_policy(observation) ==
                   ZR_SSA_PLATFORM_STATUS_OK;
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

    /* 产物校验不消费运行观察；在加载前先比对 ABI 和契约哈希，再核对
     * 后端需求及受限平台策略。 */
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

/**
 * @fn TZrUInt64 ssa_platform_hash_u32(TZrUInt64 hash, TZrUInt32 value)
 *
 * ## Summary
 * 将一个32位字段按低字节在前的固定顺序混入ABI摘要，使摘要不依赖宿主字节序或结构体填充。
 *
 * ## Parameters
 * @param[in] hash 任意现有64位摘要状态。
 * @param[in] value 待编码的32位字段。
 *
 * ## Returns
 * @return 返回混入四个字节后的64位摘要；无错误码，摘要可碰撞且不承诺非零。
 *
 * ## Constraints and lifecycle
 * - 索引只取0..3，右移只取0/8/16/24；无移位越界。64位无符号乘法按模2^64回绕。
 * - 当前偏移和乘数属于本schema编码，不声称是标准FNV参数、加密或真实性证明。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:11
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:7
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:8
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:14
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:35
 */

/**
 * @fn TZrUInt64 ssa_platform_hash_u64(TZrUInt64 hash, TZrUInt64 value)
 *
 * ## Summary
 * 将回调ABI字段的64位值接入同一字节序明确的摘要编码。
 *
 * ## Parameters
 * @param[in] hash 已有64位摘要。
 * @param[in] value 待混入的64位字段。
 *
 * ## Returns
 * @return 返回八个字节更新后的摘要；无有效性判断、非零或无碰撞保证。
 *
 * ## Constraints and lifecycle
 * - 索引0..7对应右移0..56；移位未超出位宽，乘法为无符号模运算。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:21
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:24
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:392
 */

/**
 * @fn TZrUInt64 ssa_platform_hash_abi_without_callback( const SZrSsaPlatformAbi *abi)
 *
 * ## Summary
 * 按固定字段次序编码ABI的通用部分，供完整ABI哈希及宿主回调占位见证复用。
 *
 * ## Parameters
 * @param[in] abi 非空、稳定可读的ABI记录；本助手不验证字段合法性。
 *
 * ## Returns
 * @return 返回尚未混入nativeCallbackAbiHash的通用字段摘要。
 *
 * ## Constraints and lifecycle
 * - 依次编码指针宽度、端序、对齐、四种整数宽度、两种浮点宽度和聚合返回类别；不编码target、triple或结构体填充。
 * - 枚举转换为32位值后编码，不从摘要推导目标ABI是否真实。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:31
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:35
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:44
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:378
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:391
 */

/**
 * @fn TZrBool ssa_platform_target_architecture_compatible( EZrSsaPlatformTarget target, EZrSsaPlatformArchitecture architecture)
 *
 * ## Summary
 * 在身份解析中排除本模型不接受的目标/架构组合。
 *
 * ## Parameters
 * @param[in] target 调用方已另行验证的目标枚举。
 * @param[in] architecture 调用方已另行验证的架构枚举。
 *
 * ## Returns
 * @return iOS仅AArch64、WASM仅WASM32/64、其余分支仅x86_64/AArch64时返回真。
 *
 * ## Constraints and lifecycle
 * - 本助手不独立拒绝未知target；所有当前调用均先做枚举有效性检查。
 * - 非WASM指针宽度仍由显式ABI记录给出；此组合检查不是完整机器ABI或三元组解析。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:83
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:86
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:89
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:171
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:957
 */

/**
 * @fn TZrBool ssa_platform_string_present(const TZrChar *value, TZrSize capacity)
 *
 * ## Summary
 * 在固定容量记录内检查三元组是否非空且以NUL结束，使后续strcmp仅用于已结束的字符串。
 *
 * ## Parameters
 * @param[in] value 可空指针；非空时须能读取直到首个NUL或capacity界内的字节。
 * @param[in] capacity 可读上界；当前身份调用使用96字节固定容量。
 *
 * ## Returns
 * @return NULL、零容量、首字符NUL或容量内无NUL返回假；存在非首位NUL返回真。
 *
 * ## Constraints and lifecycle
 * - 仅检查存在和边界内结束，不解析语法、架构或目标含义，不认证底层地址及分配来源。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:98
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:102
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:105
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:179
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:959
 */

/**
 * @fn void ssa_platform_set_status(SZrSsaPlatformDiagnostic *diagnostic, EZrSsaPlatformStatus status)
 *
 * ## Summary
 * 向可选诊断发布当前校验阶段的状态，同时保留已收集的来源和细分字段。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空的诊断对象；非空须独占写status成员。
 * @param[in] status 本次要发布的状态值。
 *
 * ## Returns
 * void：完成可选status赋值；没有失败返回码或资源返回。
 *
 * ## Constraints and lifecycle
 * - 此函数不是DiagnosticInit，不清理旧abiField、expected/actual、witness或上下文。
 * - 只写status；其余字段的初始化和意义由具体调用分支承担。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:113
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:158
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:482
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:946
 */

/**
 * @fn void ssa_platform_copy_observation_context( SZrSsaPlatformDiagnostic *diagnostic, const SZrSsaPlatformObservation *observation)
 *
 * ## Summary
 * 在完整验证前把原始观察的来源坐标复制到诊断，支持早退定位。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空诊断输出；非空与observation不得重叠。
 * @param[in] observation 可空、可读的观察；字段尚未被验证。
 *
 * ## Returns
 * void：任一指针为空则不写；否则仅复制target/architecture/backend/runner、需求/不支持位及sourceId/instructionId。
 *
 * ## Constraints and lifecycle
 * - 不复制字符串或保存观察指针；复制的上下文仍可能是畸形输入，不能当作身份已验证。
 * - 不设置status或清空其余诊断；当前Check先DiagnosticInit。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:121
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:124
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:127
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:134
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:702
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:703
 */

/**
 * @fn void ssa_platform_copy_artifact_context( SZrSsaPlatformDiagnostic *diagnostic, const SZrSsaPlatformArtifactContract *artifact)
 *
 * ## Summary
 * 为产物预校验的早退诊断保留原始目标、架构和需求位。
 *
 * ## Parameters
 * @param[in,out] diagnostic 可空、独占写入且与artifact不重叠的诊断。
 * @param[in] artifact 可空、稳定可读的产物值对象。
 *
 * ## Returns
 * void：任一指针为空则不写；否则只复制三个上下文字段。
 *
 * ## Constraints and lifecycle
 * - 尚未验证被复制值，不修改status、不清空其余字段或保留artifact指针。
 * - 当前ValidateArtifact在本助手前先清空整份诊断。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:137
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:143
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1011
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1012
 */

/**
 * @fn EZrSsaPlatformStatus ssa_platform_validate_identity( TZrUInt32 magic, TZrUInt32 schemaVersion, EZrSsaPlatformTarget target, EZrSsaPlatformArchitecture architecture, const TZrChar *targetTriple, SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 把能力、观察和产物的版本身份及目标组合按同一顺序解释，阻止无效记录进入字段比较。
 *
 * ## Parameters
 * @param[in] magic 记录魔数。
 * @param[in] schemaVersion 记录schema版本。
 * @param[in] target 声明的目标枚举。
 * @param[in] architecture 声明的架构枚举。
 * @param[in] targetTriple 可空的三元组地址；非空时需满足96字节有界读取前提。
 * @param[in,out] diagnostic 可空诊断；仅更新本次状态，不能与被读字符串重叠。
 *
 * ## Returns
 * @return 依次返回SCHEMA_MISMATCH、TARGET_INVALID、ARCHITECTURE_INVALID或TARGET_TRIPLE_MISSING；全部通过返回OK。
 *
 * ## Constraints and lifecycle
 * - diagnostic 可空；非空输出须与所有被读输入记录及其内嵌数组不重叠。低层函数不清空整份诊断，仅更新当前分支字段和status，调用前可先 DiagnosticInit，返回后只解释本次status对应字段。
 * - 目标组合仅按iOS/WASM/其它三分支检查，三元组只要求非空且界内NUL结束。
 * - TODO: zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:176 此入口不比对triple文本与target/architecture的语义；现有tests/core/test_ssa_platform_matrix.c:38 等为人工固定字符串，须核查目标适配器注册/规范化责任后再决定更强合同。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:149
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:156
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:171
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:176
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:179
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:599
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:713
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1022
 */

/**
 * @fn TZrBool ssa_platform_is_machine_code_forbidden( EZrSsaPlatformTarget target)
 *
 * ## Summary
 * 集中表达本值契约将Android、iOS与WASM列为机器码JIT禁用目标的策略。
 *
 * ## Parameters
 * @param[in] target 待查询的目标；本助手不单独验证枚举。
 *
 * ## Returns
 * @return 仅三个列举目标返回真，其余值返回假。
 *
 * ## Constraints and lifecycle
 * - 是本模块固定策略，不是查询系统权限、驱动或宿主可执行内存状态。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:190
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:200
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:218
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1112
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1118
 */

/**
 * @fn EZrSsaPlatformStatus ssa_platform_validate_observation_policy( const SZrSsaPlatformObservation *observation)
 *
 * ## Summary
 * 使完整声明比对与单观察验收共享来源约束，拒绝交叉编译冒充运行及受限目标的JIT触发。
 *
 * ## Parameters
 * @param[in] observation 非空、稳定可读的观察；当前两调用方已处理各自更早的记录门禁。
 *
 * ## Returns
 * @return 受限目标出现HOST_JIT、machineCodeJitExecuted或JIT需求位时先返回MACHINE_CODE_JIT_FORBIDDEN；随后CROSS_COMPILE携带executed、semanticPassed或PASSED时返回OBSERVATION_INVALID；其余返回OK。
 *
 * ## Constraints and lifecycle
 * - 不写诊断；Check负责把本返回值写入诊断。
 * - 自身不验证magic/schema、枚举、ABI、声明能力或哈希，不把记录当成实际设备执行证据。
 * - IsRuntimeAcceptance使用短路&&，只有更早阶段条件均为真才会调用本助手；某些无效观察可更早返回假。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:198
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:200
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:207
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 * - tests/core/test_ssa_platform_matrix.c:193
 * - tests/core/test_ssa_platform_matrix.c:200
 * - tests/core/test_ssa_platform_matrix.c:209
 * - tests/core/test_ssa_platform_matrix.c:246
 * - tests/core/test_ssa_platform_matrix.c:260
 * - tests/core/test_ssa_platform_matrix.c:266
 */

/**
 * @fn TZrBool ssa_platform_feature_allowed( EZrSsaPlatformTarget target, TZrUInt64 feature)
 *
 * ## Summary
 * 向能力校验与单特性查询提供目标特定的两个禁用规则。
 *
 * ## Parameters
 * @param[in] target 已由上层解释的目标枚举。
 * @param[in] feature 当前调用方传入的单个能力位。
 *
 * ## Returns
 * @return Android/iOS/WASM的机器码JIT位或WASM的PMU位返回假，其余组合返回真。
 *
 * ## Constraints and lifecycle
 * - 不检查feature是否单比特、已知或已声明；这些检查分别由CapabilitySupports或ValidateCapability承担。
 * - 不从WASM目标名推定线程支持，也不检测实际PMU/线程环境。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:215
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:218
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:225
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:642
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:650
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:676
 */

/**
 * @fn TZrUInt64 ssa_platform_backend_feature(EZrSsaPlatformBackend backend)
 *
 * ## Summary
 * 把已识别的四种执行后端映射到声明能力位，供Check独立核验后端可用性。
 *
 * ## Parameters
 * @param[in] backend 待映射的后端枚举。
 *
 * ## Returns
 * @return EXECBC/AOT_C/AOT_LLVM/HOST_JIT返回对应单能力位，其余返回零。
 *
 * ## Constraints and lifecycle
 * - Check在本调用前已拒绝无效backend；零不是已支持后端证明。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:233
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:235
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:241
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:719
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:823
 */

/**
 * @anchor ssa_platform_contract_ZrCommon_SsaPlatform_Check_backend_feature_dispatch
 *
 * ## Summary
 * 在观察身份/ABI/来源策略已通过后，独立检查后端能力、显式需求和分派许可。
 *
 * ## Constraints and lifecycle
 * - 顺序是backendFeature缺失、requiredFeatures缺失、观察dispatchFlags超出声明，首个失败即返回。
 * - 不把后端bit存在等同真实实现或执行；声明与观察的读取/诊断别名及生命周期沿用Check。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:822
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:824
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:832
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:840
 * - tests/core/test_ssa_platform_matrix.c:365
 * - tests/core/test_ssa_platform_matrix.c:367
 */

/**
 * @anchor ssa_platform_contract_ZrCommon_SsaPlatform_Check_outcome_stages
 *
 * ## Summary
 * 在能力集合通过后，把不可用、不支持及各阶段未成功保留为不同状态。
 *
 * ## Constraints and lifecycle
 * - UNAVAILABLE携带executed或semanticPassed时拒为OBSERVATION_INVALID，否则RUNTIME_UNAVAILABLE；UNSUPPORTED须有不支持位。
 * - PASSED不能携带不支持位；随后分别检查compiled、executed、semanticPassed。更早需求缺失可先拒绝，不能仅按case名归到本块。
 * - 不执行编译或后端，不为失败记录修复字段；只写本分支诊断。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:850
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:860
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:875
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:884
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:889
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:894
 * - tests/core/test_ssa_platform_matrix.c:96
 * - tests/core/test_ssa_platform_matrix.c:142
 * - tests/core/test_ssa_platform_matrix.c:180
 * - tests/core/test_ssa_platform_matrix.c:382
 */

/**
 * @anchor ssa_platform_contract_ZrCommon_SsaPlatform_Check_semantic_witness
 *
 * ## Summary
 * 在所有运行阶段均满足后按结果、异常、源码映射次序比较可选语义见证。
 *
 * ## Constraints and lifecycle
 * - 每个维度双方都零时可跳过；任一非零则要求完全相同，先失败维度决定witnessField和expected/actual。
 * - 仅比较提供的64位摘要，不生成见证、认证真实执行或保证无碰撞。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:900
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:912
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:925
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:946
 * - tests/core/test_ssa_platform_matrix.c:418
 * - tests/core/test_ssa_platform_matrix.c:427
 * - tests/core/test_ssa_platform_matrix.c:433
 * - tests/core/test_ssa_platform_matrix.c:434
 * - tests/core/test_ssa_platform_matrix.c:437
 */

/**
 * @anchor ssa_platform_contract_ZrCommon_SsaPlatform_ValidateArtifact_preload_policy
 *
 * ## Summary
 * 在装载前只用声明值检查产物绑定及受限补丁/JIT策略，避免把构建产物误当运行观察。
 *
 * ## Constraints and lifecycle
 * - 从诊断初始化、声明/身份开始，逐一比对目标、三元组、ABI及三个外层哈希，然后检查需求和目标策略。
 * - 需求子集检查1094先于JIT策略1110；受限目标的JIT需求若不在声明中，先返回REQUIRED_FEATURE_UNSUPPORTED，不能把后面的FORBIDDEN分支承诺为首拒绝。
 * - iOS受限补丁新增导入与机器码JIT各有独立条件；无真实装载、导入、执行或资源取得。
 * - 失败只更新诊断，const输入保留；诊断须独立，按ValidateArtifact约束串行写入。
 *
 * ## References
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1011
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1018
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1046
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1055
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1061
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1094
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1103
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1110
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1123
 * - tests/core/test_ssa_platform_matrix.c:110
 * - tests/core/test_ssa_platform_matrix.c:118
 * - tests/core/test_ssa_platform_matrix.c:283
 */
