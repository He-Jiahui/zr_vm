#include "zr_vm_parser/compile_optimization_profile.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 与仓库稳定哈希参数保持一致；改变它们会改变规范化策略的缓存身份。 */
#define ZR_COMPILE_PROFILE_HASH_OFFSET UINT64_C(1469598103934665603)
#define ZR_COMPILE_PROFILE_HASH_PRIME UINT64_C(1099511628211)

static void compile_profile_diagnostic_clear(
        SZrCompileOptimizationDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        (void)memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

static TZrBool compile_profile_diagnostic_set(
        SZrCompileOptimizationDiagnostic *diagnostic,
        EZrCompileOptimizationDiagnosticCode code,
        TZrUInt32 field,
        TZrUInt64 expected,
        TZrUInt64 actual) {
    /* 诊断输出可选；统一以失败值结束，供各校验分支直接早退。 */
    if (diagnostic != ZR_NULL) {
        diagnostic->code = code;
        diagnostic->field = field;
        diagnostic->expected = expected;
        diagnostic->actual = actual;
    }
    return ZR_FALSE;
}

static TZrBool compile_profile_bool_valid(TZrBool value) {
    return (TZrBool)(value == ZR_FALSE || value == ZR_TRUE);
}

static TZrBool compile_profile_build_mode_valid(EZrCompileBuildMode value) {
    return (TZrBool)(value >= ZR_COMPILE_BUILD_MODE_DEV &&
                     value < ZR_COMPILE_BUILD_MODE_COUNT);
}

static TZrBool compile_profile_numeric_valid(
        EZrCompileNumericPermission value) {
    return (TZrBool)(value >= ZR_COMPILE_NUMERIC_STRICT &&
                     value < ZR_COMPILE_NUMERIC_PERMISSION_COUNT);
}

static TZrBool compile_profile_target_valid(EZrCompileTarget value) {
    return (TZrBool)(value >= ZR_COMPILE_TARGET_HOST &&
                     value < ZR_COMPILE_TARGET_COUNT);
}

static TZrBool compile_profile_backend_valid(EZrCompileBackend value) {
    return (TZrBool)(value >= ZR_COMPILE_BACKEND_INTERPRETER &&
                     value < ZR_COMPILE_BACKEND_COUNT);
}

static TZrBool compile_profile_preset_valid(EZrCompileOptimizationPreset value) {
    return (TZrBool)(value >= ZR_COMPILE_PRESET_DEFAULT &&
                     value < ZR_COMPILE_PRESET_COUNT);
}

/**
 * @brief 初始化为开发模式、严格数值策略和解释器后端的默认配置。
 * @param profile 接收默认配置；为空时不执行任何操作。
 * @note 先清零整份结构，再写入当前 schema 和基础标量优化通道。
 */
void ZrParser_CompileOptimizationProfile_Init(
        SZrCompileOptimizationProfile *profile) {
    if (profile == ZR_NULL) {
        return;
    }
    (void)memset(profile, 0, sizeof(*profile));
    profile->schemaVersion = ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION;
    profile->preset = ZR_COMPILE_PRESET_DEFAULT;
    profile->buildMode = ZR_COMPILE_BUILD_MODE_DEV;
    profile->numericPermission = ZR_COMPILE_NUMERIC_STRICT;
    profile->target = ZR_COMPILE_TARGET_HOST;
    profile->backend = ZR_COMPILE_BACKEND_INTERPRETER;
    profile->passMask = ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR;
}

/* 每个预设先重置共享维度；schema、预设来源和目标/ABI 哈希由调用方保留。 */
static void compile_profile_set_common(
        SZrCompileOptimizationProfile *profile,
        EZrCompileBuildMode buildMode,
        EZrCompileTarget target,
        EZrCompileBackend backend,
        TZrUInt32 passMask) {
    profile->buildMode = buildMode;
    profile->numericPermission = ZR_COMPILE_NUMERIC_STRICT;
    profile->target = target;
    profile->backend = backend;
    profile->frameBudgetBytes = 0U;
    profile->passMask = passMask;
    profile->frameSafe = ZR_FALSE;
    profile->requestFastMath = ZR_FALSE;
    profile->enableLto = ZR_FALSE;
    profile->enablePgo = ZR_FALSE;
}

/**
 * @brief 将一个预设展开为可继续编辑的独立配置维度。
 * @param profile 待更新配置；为空时失败。
 * @param preset 需要展开的预设值。
 * @param diagnostic 可选诊断输出；失败时记录参数错误。
 * @return 配置成功更新时返回 ZR_TRUE，否则返回 ZR_FALSE。
 * @note 仅把零 schema 升级为当前版本；非零版本由 Normalize 校验。
 */
TZrBool ZrParser_CompileOptimizationProfile_ApplyPreset(
        SZrCompileOptimizationProfile *profile,
        EZrCompileOptimizationPreset preset,
        SZrCompileOptimizationDiagnostic *diagnostic) {
    compile_profile_diagnostic_clear(diagnostic);
    if (profile == ZR_NULL || !compile_profile_preset_valid(preset)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_INVALID_ARGUMENT,
                0U,
                ZR_COMPILE_PRESET_DEFAULT,
                (TZrUInt64)preset);
    }
    if (profile->schemaVersion == 0U) {
        profile->schemaVersion = ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION;
    }
    profile->preset = preset;
    switch (preset) {
        case ZR_COMPILE_PRESET_DEFAULT:
        case ZR_COMPILE_PRESET_DEV:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_DEV,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_INTERPRETER,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR);
            break;
        case ZR_COMPILE_PRESET_INTERACTIVE:
        case ZR_COMPILE_PRESET_HOST_JIT:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_INTERACTIVE,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_HOST_JIT,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR);
            break;
        case ZR_COMPILE_PRESET_RELEASE:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_AOT_C,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR |
                            ZR_COMPILE_PASS_VECTOR);
            break;
        case ZR_COMPILE_PRESET_RELEASE_LTO:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_AOT_LLVM,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR |
                            ZR_COMPILE_PASS_VECTOR | ZR_COMPILE_PASS_LTO);
            profile->enableLto = ZR_TRUE;
            break;
        case ZR_COMPILE_PRESET_RELEASE_PGO:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_AOT_LLVM,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR |
                            ZR_COMPILE_PASS_VECTOR | ZR_COMPILE_PASS_PGO);
            profile->enablePgo = ZR_TRUE;
            break;
        case ZR_COMPILE_PRESET_RELEASE_FAST_MATH:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_AOT_C,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR |
                            ZR_COMPILE_PASS_VECTOR);
            profile->numericPermission = ZR_COMPILE_NUMERIC_FAST_MATH;
            profile->requestFastMath = ZR_TRUE;
            break;
        case ZR_COMPILE_PRESET_FRAME_SAFE:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_HOST,
                    ZR_COMPILE_BACKEND_AOT_C,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR);
            profile->frameSafe = ZR_TRUE;
            profile->frameBudgetBytes = 65536U;
            break;
        case ZR_COMPILE_PRESET_WASM:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_WASM,
                    ZR_COMPILE_BACKEND_AOT_C,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR |
                            ZR_COMPILE_PASS_VECTOR);
            break;
        case ZR_COMPILE_PRESET_ANDROID_AOT:
        case ZR_COMPILE_PRESET_IOS_AOT:
            compile_profile_set_common(
                    profile,
                    ZR_COMPILE_BUILD_MODE_RELEASE,
                    ZR_COMPILE_TARGET_MOBILE,
                    ZR_COMPILE_BACKEND_AOT_C,
                    ZR_COMPILE_PASS_BASIC | ZR_COMPILE_PASS_SCALAR |
                            ZR_COMPILE_PASS_VECTOR);
            break;
        default:
            return compile_profile_diagnostic_set(
                    diagnostic,
                    ZR_COMPILE_PROFILE_DIAGNOSTIC_INVALID_ARGUMENT,
                    0U,
                    ZR_COMPILE_PRESET_DEFAULT,
                    (TZrUInt64)preset);
    }
    return ZR_TRUE;
}

/* 按低位字节优先追加固定宽度整数，避免依赖主机整数的内存表示。 */
static void compile_profile_hash_u32(TZrUInt64 *hash, TZrUInt32 value) {
    TZrUInt32 index;
    for (index = 0U; index < 4U; ++index) {
        *hash ^= (TZrUInt64)((value >> (index * 8U)) & 0xffU);
        *hash *= ZR_COMPILE_PROFILE_HASH_PRIME;
    }
}

static void compile_profile_hash_u64(TZrUInt64 *hash, TZrUInt64 value) {
    TZrUInt32 index;
    for (index = 0U; index < 8U; ++index) {
        *hash ^= (TZrUInt64)((value >> (index * 8U)) & 0xffU);
        *hash *= ZR_COMPILE_PROFILE_HASH_PRIME;
    }
}

/**
 * @brief 按固定字段顺序计算有效策略的稳定哈希。
 * @param effective 有效策略；为空时返回零。
 * @return 不含预设来源和既有 profileHash 的策略标识。
 * @note 字段逐一序列化，不依赖结构体填充、进程地址或主机字节序。
 */
TZrUInt64 ZrParser_CompileOptimizationProfile_Hash(
        const SZrCompileEffectivePolicy *effective) {
    TZrUInt64 hash = ZR_COMPILE_PROFILE_HASH_OFFSET;
    if (effective == ZR_NULL) {
        return 0U;
    }
    compile_profile_hash_u32(&hash, effective->schemaVersion);
    /* 预设只记录来源，不是有效语义维度；相同展开结果应得到相同哈希。 */
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->buildMode);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->numericPermission);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->target);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->backend);
    compile_profile_hash_u32(&hash, effective->frameBudgetBytes);
    compile_profile_hash_u32(&hash, effective->passMask);
    compile_profile_hash_u64(&hash, effective->targetHash);
    compile_profile_hash_u64(&hash, effective->hostAbiHash);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->frameSafe);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->fastMathEnabled);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->ltoEnabled);
    compile_profile_hash_u32(&hash, (TZrUInt32)effective->pgoEnabled);
    return hash;
}

/**
 * @brief 校验配置并生成供上层读取的有效策略。
 * @param profile 待校验配置；为空时报告无效参数。
 * @param effective 必需的输出对象；成功时写入规范化维度和 profileHash。
 * @param diagnostic 可选诊断输出；失败时记录按校验顺序遇到的首个问题。
 * @return 校验通过并生成有效策略时返回 ZR_TRUE。
 * @note 参数检查通过后先清零 effective；之后的字段校验失败会留下全零输出。
 */
/* TODO: 当前源码唯一可见的跨模块 profile 使用点是在缓存构键前校验 profileHash 并纳入键值
 * (zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c:91;
 *  zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c:170;
 *  zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c:189;
 *  zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c:209)，
 * 尚未找到按 backend/passMask/buildMode 驱动生产编译或 pass 选择的调用。沿 parser
 * compile entry 与 pass manager 核实维度覆盖优先级，并验证策略变化的缓存失效路径。 */
TZrBool ZrParser_CompileOptimizationProfile_Normalize(
        const SZrCompileOptimizationProfile *profile,
        SZrCompileEffectivePolicy *effective,
        SZrCompileOptimizationDiagnostic *diagnostic) {
    compile_profile_diagnostic_clear(diagnostic);
    if (profile == ZR_NULL || effective == ZR_NULL) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_INVALID_ARGUMENT,
                0U,
                1U,
                0U);
    }
    (void)memset(effective, 0, sizeof(*effective));
    /* TODO: field 目前以 0..15 裸值写入；公开结构只暴露整数域
     * (zr_vm_parser/include/zr_vm_parser/compile_optimization_profile.h:125)，
     * 当前测试只检查 code (tests/parser/test_ssa_build_profiles.c:35 和
     * tests/parser/test_ssa_build_profiles.c:42)。确认 field 数值是否属于外部稳定契约，
     * 再决定公开字段枚举或明确为内部细节并补映射验证。 */
    /* 下列条件按此顺序短路，诊断只报告第一个不满足的约束。 */
    if (profile->schemaVersion !=
            ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_SCHEMA,
                1U,
                ZR_COMPILE_OPTIMIZATION_PROFILE_SCHEMA_VERSION,
                profile->schemaVersion);
    }
    if (!compile_profile_preset_valid(profile->preset)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_INVALID_ARGUMENT,
                2U,
                ZR_COMPILE_PRESET_DEFAULT,
                (TZrUInt64)profile->preset);
    }
    if (!compile_profile_build_mode_valid(profile->buildMode)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_BUILD_MODE,
                3U,
                ZR_COMPILE_BUILD_MODE_DEV,
                (TZrUInt64)profile->buildMode);
    }
    if (!compile_profile_numeric_valid(profile->numericPermission)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_NUMERIC_PERMISSION,
                4U,
                ZR_COMPILE_NUMERIC_STRICT,
                (TZrUInt64)profile->numericPermission);
    }
    if (!compile_profile_target_valid(profile->target)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_TARGET,
                5U,
                ZR_COMPILE_TARGET_HOST,
                (TZrUInt64)profile->target);
    }
    if (!compile_profile_backend_valid(profile->backend)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_BACKEND,
                6U,
                ZR_COMPILE_BACKEND_INTERPRETER,
                (TZrUInt64)profile->backend);
    }
    if (!compile_profile_bool_valid(profile->frameSafe) ||
        !compile_profile_bool_valid(profile->requestFastMath) ||
        !compile_profile_bool_valid(profile->enableLto) ||
        !compile_profile_bool_valid(profile->enablePgo)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_INVALID_ARGUMENT,
                7U,
                0U,
                2U);
    }
    if (profile->target != ZR_COMPILE_TARGET_HOST &&
        profile->backend == ZR_COMPILE_BACKEND_HOST_JIT) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_TARGET_BACKEND,
                8U,
                ZR_COMPILE_TARGET_HOST,
                profile->target);
    }
    if (profile->requestFastMath &&
        profile->numericPermission != ZR_COMPILE_NUMERIC_FAST_MATH) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_NUMERIC_PERMISSION,
                9U,
                ZR_COMPILE_NUMERIC_FAST_MATH,
                profile->numericPermission);
    }
    if (profile->frameSafe && profile->frameBudgetBytes == 0U) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_FRAME_BUDGET,
                10U,
                1U,
                0U);
    }
    if (profile->enableLto &&
        profile->backend != ZR_COMPILE_BACKEND_AOT_LLVM) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_LTO_BACKEND,
                11U,
                ZR_COMPILE_BACKEND_AOT_LLVM,
                profile->backend);
    }
    if (profile->enablePgo &&
        profile->backend != ZR_COMPILE_BACKEND_AOT_LLVM) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_PGO_BACKEND,
                12U,
                ZR_COMPILE_BACKEND_AOT_LLVM,
                profile->backend);
    }
    if (profile->enableLto && profile->enablePgo) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_LTO_PGO_CONFLICT,
                13U,
                0U,
                1U);
    }
    if (profile->passMask == 0U ||
        (profile->passMask & ~ZR_COMPILE_PASS_KNOWN_MASK) != 0U) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_PASS_PIPELINE,
                14U,
                ZR_COMPILE_PASS_KNOWN_MASK,
                profile->passMask);
    }
    if ((profile->enableLto != ZR_FALSE) !=
                ((profile->passMask & ZR_COMPILE_PASS_LTO) != 0U) ||
        (profile->enablePgo != ZR_FALSE) !=
                ((profile->passMask & ZR_COMPILE_PASS_PGO) != 0U)) {
        return compile_profile_diagnostic_set(
                diagnostic,
                ZR_COMPILE_PROFILE_DIAGNOSTIC_PASS_PIPELINE,
                15U,
                profile->enableLto ? ZR_COMPILE_PASS_LTO :
                (profile->enablePgo ? ZR_COMPILE_PASS_PGO : 0U),
                profile->passMask);
    }

    effective->schemaVersion = profile->schemaVersion;
    effective->preset = profile->preset;
    effective->buildMode = profile->buildMode;
    effective->numericPermission = profile->numericPermission;
    effective->target = profile->target;
    effective->backend = profile->backend;
    effective->frameBudgetBytes = profile->frameBudgetBytes;
    effective->passMask = profile->passMask;
    effective->targetHash = profile->targetHash;
    effective->hostAbiHash = profile->hostAbiHash;
    effective->frameSafe = profile->frameSafe;
    effective->fastMathEnabled = (TZrBool)(
            profile->requestFastMath &&
            profile->numericPermission == ZR_COMPILE_NUMERIC_FAST_MATH);
    effective->ltoEnabled = profile->enableLto;
    effective->pgoEnabled = profile->enablePgo;
    effective->profileHash = ZrParser_CompileOptimizationProfile_Hash(effective);
    return ZR_TRUE;
}

static const TZrChar *compile_profile_build_name(EZrCompileBuildMode mode) {
    switch (mode) {
        case ZR_COMPILE_BUILD_MODE_DEV: return "dev";
        case ZR_COMPILE_BUILD_MODE_INTERACTIVE: return "interactive";
        case ZR_COMPILE_BUILD_MODE_RELEASE: return "release";
        default: return "invalid";
    }
}

static const TZrChar *compile_profile_numeric_name(
        EZrCompileNumericPermission permission) {
    return permission == ZR_COMPILE_NUMERIC_FAST_MATH ? "fast-math" :
           permission == ZR_COMPILE_NUMERIC_STRICT ? "strict" : "invalid";
}

static const TZrChar *compile_profile_target_name(EZrCompileTarget target) {
    switch (target) {
        case ZR_COMPILE_TARGET_HOST: return "host";
        case ZR_COMPILE_TARGET_MOBILE: return "mobile";
        case ZR_COMPILE_TARGET_WASM: return "wasm";
        default: return "invalid";
    }
}

static const TZrChar *compile_profile_backend_name(EZrCompileBackend backend) {
    switch (backend) {
        case ZR_COMPILE_BACKEND_INTERPRETER: return "interpreter";
        case ZR_COMPILE_BACKEND_HOST_JIT: return "host-jit";
        case ZR_COMPILE_BACKEND_AOT_C: return "aot-c";
        case ZR_COMPILE_BACKEND_AOT_LLVM: return "aot-llvm";
        default: return "invalid";
    }
}

/**
 * @brief 将有效策略格式化为便于检查的单行描述。
 * @param effective 待显示策略；为空时失败。
 * @param buffer 接收以零结尾的描述文本。
 * @param bufferSize buffer 的可写字节数，包含结尾零字节的空间。
 * @return 参数有效且完整文本可写入时返回 ZR_TRUE；截断或参数无效时返回 ZR_FALSE。
 */
/* TODO: 文档定义此函数为 CLI dry-run 展示接口（docs/cli-and-tooling/optimization-build-profiles.md:18, docs/cli-and-tooling/optimization-build-profiles.md:19），
 * 目前只找到测试调用（tests/parser/test_ssa_build_profiles.c:20）；沿 CLI 命令入口核实接线，
 * 并在入口测试中覆盖字段展示、参数错误和缓冲区不足的处理。 */
TZrBool ZrParser_CompileOptimizationProfile_Describe(
        const SZrCompileEffectivePolicy *effective,
        TZrChar *buffer,
        TZrSize bufferSize) {
    int written;
    if (effective == ZR_NULL || buffer == ZR_NULL || bufferSize == 0U) {
        return ZR_FALSE;
    }
    written = snprintf(
            buffer,
            bufferSize,
            "build=%s numeric=%s target=%s backend=%s passes=0x%08x "
            "frame-safe=%u frame-budget=%u lto=%u pgo=%u hash=%016llx",
            compile_profile_build_name(effective->buildMode),
            compile_profile_numeric_name(effective->numericPermission),
            compile_profile_target_name(effective->target),
            compile_profile_backend_name(effective->backend),
            (unsigned)effective->passMask,
            (unsigned)effective->frameSafe,
            (unsigned)effective->frameBudgetBytes,
            (unsigned)effective->ltoEnabled,
            (unsigned)effective->pgoEnabled,
            (unsigned long long)effective->profileHash);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}
