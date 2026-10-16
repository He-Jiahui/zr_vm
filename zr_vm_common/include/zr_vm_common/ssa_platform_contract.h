/*
 * SSA 平台契约由能力声明、产物声明和运行观察三份值对象组成。调用方可先验证
 * 目标及 ABI，再把实际执行证据与声明比对；对象不含宿主指针或运行时句柄，
 * 便于跨进程保存到矩阵报告，旨在区分交叉编译与真实目标运行。
 */

#ifndef ZR_VM_COMMON_SSA_PLATFORM_CONTRACT_H
#define ZR_VM_COMMON_SSA_PLATFORM_CONTRACT_H

#include "zr_vm_common/zr_api_conf.h"
#include "zr_vm_common/zr_common_conf.h"

#include <limits.h>

/* 持久化记录的身份及固定长度字符串边界；读入旧/未知记录须先验 schema。 */
#define ZR_SSA_PLATFORM_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_SSA_PLATFORM_CONTRACT_MAGIC ((TZrUInt32)0x53504d31u)
#define ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY ((TZrSize)96u)
#define ZR_SSA_PLATFORM_COMPILER_CAPACITY ((TZrSize)96u)
#define ZR_SSA_PLATFORM_RUNTIME_CAPACITY ((TZrSize)128u)
/* ABI 字段只表达受支持的宽度与对齐范围，不从当前宿主推断目标 ABI。 */
#define ZR_SSA_PLATFORM_MAX_ALIGNMENT ((TZrUInt32)4096u)
#define ZR_SSA_PLATFORM_INT8_WIDTH_BITS ((TZrUInt32)8u)
#define ZR_SSA_PLATFORM_INT16_WIDTH_BITS ((TZrUInt32)16u)
#define ZR_SSA_PLATFORM_INT32_WIDTH_BITS ((TZrUInt32)32u)
#define ZR_SSA_PLATFORM_INT64_WIDTH_BITS ((TZrUInt32)64u)
#define ZR_SSA_PLATFORM_FLOAT32_WIDTH_BITS ((TZrUInt32)32u)
#define ZR_SSA_PLATFORM_FLOAT64_WIDTH_BITS ((TZrUInt32)64u)

/* 编译期只排除本契约无法表达的 C 类型宽度；目标指针、对齐、返回约定和
 * 回调 ABI 仍由 SZrSsaPlatformAbi 随声明与观察显式校验。 */
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

/** @brief 区分运行平台的策略域；移动端和 WASM 的 JIT 限制依此判断。 */
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

/** @brief 与目标平台配对的架构身份，用于拒绝不可能的目标组合。 */
typedef enum EZrSsaPlatformArchitecture {
    ZR_SSA_PLATFORM_ARCH_UNKNOWN = 0,
    ZR_SSA_PLATFORM_ARCH_X86_64,
    ZR_SSA_PLATFORM_ARCH_AARCH64,
    ZR_SSA_PLATFORM_ARCH_WASM32,
    ZR_SSA_PLATFORM_ARCH_WASM64,
    ZR_SSA_PLATFORM_ARCH_COUNT
} EZrSsaPlatformArchitecture;

/** @brief 观察行采用的执行后端；必须能映射到能力声明中的对应特性。 */
typedef enum EZrSsaPlatformBackend {
    ZR_SSA_PLATFORM_BACKEND_NONE = 0,
    ZR_SSA_PLATFORM_BACKEND_EXECBC,
    ZR_SSA_PLATFORM_BACKEND_AOT_C,
    ZR_SSA_PLATFORM_BACKEND_AOT_LLVM,
    ZR_SSA_PLATFORM_BACKEND_HOST_JIT,
    ZR_SSA_PLATFORM_BACKEND_COUNT
} EZrSsaPlatformBackend;

/** @brief 记录证据来源；交叉编译行不得凭编译成功声称运行验收。 */
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

/** @brief 区分通过、环境不可用、特性不支持和已执行失败。 */
typedef enum EZrSsaPlatformOutcome {
    ZR_SSA_PLATFORM_OUTCOME_UNSET = 0,
    ZR_SSA_PLATFORM_OUTCOME_PASSED,
    ZR_SSA_PLATFORM_OUTCOME_UNAVAILABLE,
    ZR_SSA_PLATFORM_OUTCOME_UNSUPPORTED,
    ZR_SSA_PLATFORM_OUTCOME_FAILED,
    ZR_SSA_PLATFORM_OUTCOME_COUNT
} EZrSsaPlatformOutcome;

/** @brief ABI 值的一部分，跨设备比对时不能采用验证机的默认字节序。 */
typedef enum EZrSsaPlatformEndianness {
    ZR_SSA_PLATFORM_ENDIAN_UNKNOWN = 0,
    ZR_SSA_PLATFORM_ENDIAN_LITTLE,
    ZR_SSA_PLATFORM_ENDIAN_BIG,
    ZR_SSA_PLATFORM_ENDIAN_COUNT
} EZrSsaPlatformEndianness;

/** @brief 聚合返回约定的显式见证；通用 sizeof 探针无法确定目标约定。 */
typedef enum EZrSsaPlatformStructReturnKind {
    ZR_SSA_PLATFORM_STRUCT_RETURN_UNKNOWN = 0,
    ZR_SSA_PLATFORM_STRUCT_RETURN_REGISTER,
    ZR_SSA_PLATFORM_STRUCT_RETURN_MEMORY,
    ZR_SSA_PLATFORM_STRUCT_RETURN_MIXED,
    ZR_SSA_PLATFORM_STRUCT_RETURN_COUNT
} EZrSsaPlatformStructReturnKind;

/** @brief ABI 失败时的首个不匹配字段，供矩阵报告定位原因。 */
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

/** @brief 实际使用的分派形式，可与另一形式共享相同的语义见证。 */
typedef enum EZrSsaPlatformDispatchKind {
    ZR_SSA_PLATFORM_DISPATCH_UNKNOWN = 0,
    ZR_SSA_PLATFORM_DISPATCH_SWITCH,
    ZR_SSA_PLATFORM_DISPATCH_COMPUTED_GOTO,
    ZR_SSA_PLATFORM_DISPATCH_COUNT
} EZrSsaPlatformDispatchKind;

/** @brief 语义哈希不符时的结果、异常或源码映射维度。 */
typedef enum EZrSsaPlatformWitnessField {
    ZR_SSA_PLATFORM_WITNESS_FIELD_NONE = 0,
    ZR_SSA_PLATFORM_WITNESS_FIELD_RESULT,
    ZR_SSA_PLATFORM_WITNESS_FIELD_EXCEPTION,
    ZR_SSA_PLATFORM_WITNESS_FIELD_SOURCE_MAP
} EZrSsaPlatformWitnessField;

/* 能力位同时用于声明和观察的 requiredFeatures；新增位必须更新 KNOWN_MASK
 * 及后端/目标策略，否则未知位应被校验器拒绝。 */
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

/* 分派位描述允许/观察到的实现集合，dispatchKind 表示本次实际选择。 */
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

/** @brief 校验结果的稳定分类；诊断对象补充具体字段和预期/实际值。 */
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

/** @brief 可序列化的目标 ABI 见证；callback 哈希由目标适配器提供。
 * @note 对齐必须是非零的二次幂；空初始化记录必须填充后才能校验通过。
 */
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

/** @brief 目标预期；Check 和 ValidateArtifact 均以其作为声明比较基准。
 * @note targetTriple 须在固定容量内以 NUL 结束；ABI/数值/布局哈希须非零。
 * semanticResultHash、exceptionContractHash、sourceMapHash 双方均为零时
 * 不要求该维度见证，任一方非零则必须相等。
 */
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

/** @brief 后端执行后的证据；compiled、executed、semanticPassed 是不同阶段。
 * @note unsupportedFeatures 必须是 requiredFeatures 的子集；PASSED 行
 * 不能携带 unsupportedFeatures。compiler/deviceOrRuntime 仅供报告，
 * 当前校验器不会用这两个描述字段证明实际执行。
 */
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

/** @brief 构建产物声明；装载前比对身份、ABI、数值/布局契约和特性需求。 */
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

/** @brief 单次校验的诊断；调用方应保留上下文而非只记录布尔结果。
 * @note Check/ValidateArtifact 会重置对象；单独调用较低层校验时应先
 * DiagnosticInit，且只读取当前 status 对应的字段，避免沿用上次失败值。
 */
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

/** @brief 清空诊断并置为 OK，适用于重复使用同一输出对象；可传 NULL。 */
ZR_API void ZrCommon_SsaPlatform_DiagnosticInit(
        SZrSsaPlatformDiagnostic *diagnostic);
/** @brief 将状态转为报告用静态名称；返回值无需释放。 */
ZR_API const TZrChar *ZrCommon_SsaPlatform_StatusName(
        EZrSsaPlatformStatus status);
/** @brief 将运行结果转为报告用静态名称；返回值无需释放。 */
ZR_API const TZrChar *ZrCommon_SsaPlatform_OutcomeName(
        EZrSsaPlatformOutcome outcome);

/** @brief 建立空 ABI 见证，调用方须随后填充目标特有字段；可传 NULL。 */
ZR_API void ZrCommon_SsaPlatform_AbiInit(SZrSsaPlatformAbi *abi);
/** @brief 为当前宿主生成初始 ABI 见证，不能替代交叉目标的专用探针。
 * @note 聚合返回类别为保守占位，callback 哈希由通用字段推得；
 * 此接口没有执行 native callback，不提供真实调用约定或跨目标运行证明。
 */
ZR_API void ZrCommon_SsaPlatform_DetectHostAbi(SZrSsaPlatformAbi *abi);
/** @brief 对 ABI 字段生成稳定哈希，供声明、产物和观察采用同一编码。
 * @note 不验证 ABI 是否有效；NULL 返回零。各契约中的 abiHash 由适配器
 * 提供，校验器比较其一致性而不强制采用此算法重算。
 */
ZR_API TZrUInt64 ZrCommon_SsaPlatform_ComputeAbiHash(
        const SZrSsaPlatformAbi *abi);
/** @brief 检查 ABI 见证是否落在可表达范围；失败字段写入可选 diagnostic。
 * @note 不重置 diagnostic，也不验证该 ABI 是否属于某一目标架构。
 */
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateAbi(
        const SZrSsaPlatformAbi *abi,
        SZrSsaPlatformDiagnostic *diagnostic);
/** @brief 比对两份有效 ABI，诊断返回首个不匹配字段。
 * @note 任一输入无效均返回假；不重置 diagnostic。
 */
ZR_API TZrBool ZrCommon_SsaPlatform_AbiEqual(
        const SZrSsaPlatformAbi *expected,
        const SZrSsaPlatformAbi *actual,
        SZrSsaPlatformDiagnostic *diagnostic);

/** @brief 初始化声明容器；目标、三元组、哈希和特性仍须由适配器补全。 */
ZR_API void ZrCommon_SsaPlatform_CapabilityInit(
        SZrSsaPlatformCapability *capability);
/** @brief 验证目标能力的身份、ABI 和平台禁用特性，再允许后续比对。
 * @note 不重置 diagnostic；哈希只要求非零，不在此重算。
 */
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateCapability(
        const SZrSsaPlatformCapability *capability,
        SZrSsaPlatformDiagnostic *diagnostic);
/** @brief 查询单一特性位；组合掩码、未知位及目标禁用位均返回假。
 * @pre 需要可信能力查询时，先用 ValidateCapability 验证 capability。
 */
ZR_API TZrBool ZrCommon_SsaPlatform_CapabilitySupports(
        const SZrSsaPlatformCapability *capability,
        TZrUInt64 feature);

/** @brief 初始化观察容器；未设置运行结果不能被当作通过。 */
ZR_API void ZrCommon_SsaPlatform_ObservationInit(
        SZrSsaPlatformObservation *observation);
/** @brief 将运行观察与能力声明分层比对，返回状态和可选诊断。
 * @note 与 IsRuntimeAcceptance 共用观察来源策略：CROSS_COMPILE 不能携带
 * executed、semanticPassed 或 PASSED；移动端/WASM 禁止 HOST_JIT、JIT 执行标记
 * 和机器码 JIT 需求位。通过此校验不证明记录来自真实设备。
 */
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_Check(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformObservation *observed,
        SZrSsaPlatformDiagnostic *diagnostic);
/** @brief 仅判断观察对象自身是否足以声称运行通过。
 * @note 此接口不接收能力声明，无法替代 Check 的后端、特性与哈希比较。
 * 共用观察来源策略会拒绝交叉编译冒充运行，以及移动端/WASM 的机器码 JIT 观察。
 * 返回真只表示记录满足本接口条件，不能独立证明实际执行或声明能力绑定。
 */
ZR_API TZrBool ZrCommon_SsaPlatform_IsRuntimeAcceptance(
        const SZrSsaPlatformObservation *observation);

/** @brief 初始化产物声明容器；调用方须补全目标、ABI 和需求。 */
ZR_API void ZrCommon_SsaPlatform_ArtifactContractInit(
        SZrSsaPlatformArtifactContract *artifact);
/** @brief 在加载或部署前将产物契约与目标能力比较，不声称运行已成功。 */
ZR_API EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateArtifact(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformArtifactContract *artifact,
        SZrSsaPlatformDiagnostic *diagnostic);

/** @brief 测试矩阵的布尔入口；需要诊断的调用方应使用 Check。 */
ZR_API TZrBool ZrTests_Ssa_CheckPlatform(
        const SZrSsaPlatformCapability *declared,
        const SZrSsaPlatformObservation *observed);

#ifdef __cplusplus
}
#endif

#undef ZR_SSA_PLATFORM_STATIC_ASSERT

#endif /* ZR_VM_COMMON_SSA_PLATFORM_CONTRACT_H */

/**
 * @enum EZrSsaPlatformTarget
 *
 * ## Summary
 * 区分本模型的目标策略域，为身份组合、JIT限制及WASM规则提供共同枚举。
 *
 * ## Constraints and lifecycle
 * - UNKNOWN=0用于未填记录，COUNT为边界；两个哨兵不通过目标有效性检查。
 * - 桌面Windows/Linux/Darwin、Android、iOS、WASM为本schema列举集合，不枚举所有系统。
 * - 目标名不证明设备执行；iOS仅配AArch64，WASM仅配WASM32/64，其余仅配x86_64/AArch64。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:58
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:256
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:278
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:309
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:49
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:83
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:190
 */

/**
 * @enum EZrSsaPlatformArchitecture
 *
 * ## Summary
 * 给声明、产物与观察的目标架构标识提供可比较的值域。
 *
 * ## Constraints and lifecycle
 * - UNKNOWN/COUNT是未填和范围哨兵；不作为受支持架构。
 * - 仅表达x86_64、AArch64、WASM32、WASM64；非WASM的指针宽度仍是独立ABI字段。
 * - ValidateCapability额外绑定WASM32/64指针位宽；IsRuntimeAcceptance不调用这项声明校验。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:70
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:256
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:278
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:309
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:54
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:83
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:626
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:950
 */

/**
 * @enum EZrSsaPlatformBackend
 *
 * ## Summary
 * 标识观察记录采用的执行后端，使Check能独立核验相应能力位。
 *
 * ## Constraints and lifecycle
 * - NONE与COUNT不通过观察枚举门禁；四个有效值映射到EXECBC/AOT_C/AOT_LLVM/机器码JIT能力位。
 * - 枚举存在不意味着编译、执行或对应后端已实现；调用方还需填阶段证据。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:80
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:283
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:60
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:233
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:719
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:823
 */

/**
 * @enum EZrSsaPlatformRunner
 *
 * ## Summary
 * 描述记录来源，区分编译观察和有资格填写运行阶段的来源。
 *
 * ## Constraints and lifecycle
 * - NONE/COUNT无效；CROSS_COMPILE是可识别来源，但不得携带executed、semanticPassed或PASSED。
 * - HOST、EMULATOR、REAL_DEVICE、BROWSER_RUNTIME、WASM_RUNTIME只是本值契约接受的来源枚举，不认证真实runner，也没有按target进一步配对。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:90
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:284
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:65
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:207
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:723
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 */

/**
 * @enum EZrSsaPlatformOutcome
 *
 * ## Summary
 * 把观察结果分为未填、通过、不可用、不支持与失败，供有序门禁解释。
 *
 * ## Constraints and lifecycle
 * - UNSET和COUNT不通过结果枚举门禁。
 * - UNAVAILABLE/UNSUPPORTED在阶段成功检查前处理；具体首个状态仍受更早身份、策略和能力门禁影响。
 * - PASSED还要求完整阶段、零unsupportedFeatures及相应见证；FAILED不自动绕过其余记录校验。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:102
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:285
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:70
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:850
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:860
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:875
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:884
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:937
 */

/**
 * @enum EZrSsaPlatformEndianness
 *
 * ## Summary
 * 记录目标ABI的显式字节序，而不使用验证机器的默认字节序替代目标。
 *
 * ## Constraints and lifecycle
 * - UNKNOWN和COUNT不被ValidateAbi接受；LITTLE/BIG可表达。
 * - 字段哈希本身使用固定低字节在前编码，和此目标端序字段含义不同。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:112
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:239
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:11
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:35
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:357
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:411
 */

/**
 * @enum EZrSsaPlatformStructReturnKind
 *
 * ## Summary
 * 为聚合返回约定提供显式ABI见证分类。
 *
 * ## Constraints and lifecycle
 * - UNKNOWN和COUNT无效；REGISTER/MEMORY/MIXED为可表达类别。
 * - DetectHostAbi使用MIXED占位，不能据此断言实际聚合返回约定已被调用验证。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:120
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:247
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:371
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:466
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:557
 */

/**
 * @enum EZrSsaPlatformAbiField
 *
 * ## Summary
 * 指示ABI校验或比较失败时应解释的首个细分维度。
 *
 * ## Constraints and lifecycle
 * - NONE为初始化状态；字段枚举不是ABI本体的有效性证明。
 * - ValidateAbi写首个无效维度，AbiEqual写首个有效ABI差异；HASH用于Check外层observedAbiHash比较。
 * - 未参与当前status的旧字段可能保留，不应无条件解释。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:129
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:335
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:403
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:430
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:503
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:563
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:780
 */

/**
 * @enum EZrSsaPlatformDispatchKind
 *
 * ## Summary
 * 给观察指定实际选择的分派形式，区别于可允许的dispatchFlags集合。
 *
 * ## Constraints and lifecycle
 * - UNKNOWN允许表达不依赖这两种分派的观察；SWITCH和COMPUTED_GOTO要求相应observed旗位。
 * - COUNT不被接受；flags仍需已知且在声明许可内，featureFlags中的分派能力位是另一独立需求域。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:146
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:298
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:76
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:736
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:840
 */

/**
 * @enum EZrSsaPlatformWitnessField
 *
 * ## Summary
 * 区分运行语义比较失败的结果、异常或源码映射维度。
 *
 * ## Constraints and lifecycle
 * - NONE为无当前见证差异；RESULT/EXCEPTION/SOURCE_MAP对应Check的比较顺序。
 * - 它只是诊断分类；哈希值并不认证实际运行，零/零维度在Check中可不要求比较。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:154
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:342
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:900
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:912
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:925
 */

/**
 * @enum EZrSsaPlatformStatus
 *
 * ## Summary
 * 为记录格式、身份、ABI、策略、能力、阶段和见证问题提供稳定的返回分类。
 *
 * ## Constraints and lifecycle
 * - OK=0供成功和诊断初始化使用；COUNT是枚举边界而非成功。
 * - StatusName只为列举状态提供名称，其余包括COUNT返回invalid-platform-status。
 * - 首失败状态依入口顺序决定；不可把一个状态等同所有更晚分支已经被执行。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:200
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:330
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:256
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:693
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1002
 */

/**
 * @struct SZrSsaPlatformAbi
 *
 * ## Summary
 * 保存目标ABI的纯值见证，供声明、产物和观察逐字段比较及可选摘要编码。
 *
 * ## Fields and invariants
 * - pointerWidthBits：TZrUInt32，解释及边界见下列约束。
 * - endianness：EZrSsaPlatformEndianness，解释及边界见下列约束。
 * - maxAlignment：TZrUInt32，解释及边界见下列约束。
 * - int8WidthBits：TZrUInt32，解释及边界见下列约束。
 * - int16WidthBits：TZrUInt32，解释及边界见下列约束。
 * - int32WidthBits：TZrUInt32，解释及边界见下列约束。
 * - int64WidthBits：TZrUInt32，解释及边界见下列约束。
 * - float32WidthBits：TZrUInt32，解释及边界见下列约束。
 * - float64WidthBits：TZrUInt32，解释及边界见下列约束。
 * - structReturnKind：EZrSsaPlatformStructReturnKind，解释及边界见下列约束。
 * - nativeCallbackAbiHash：TZrUInt64，解释及边界见下列约束。
 *
 * ## Constraints and lifecycle
 * - pointerWidthBits仅32/64；端序须有效；maxAlignment非零、2的幂且不超过4096。
 * - 整数位宽为8/16/32/64，浮点位宽为32/64；仅比较宽度，不证明浮点格式或数值行为。
 * - structReturnKind须有效、nativeCallbackAbiHash非零；通用探针产生的占位不是真实回调调用证据。
 * - 不含指针或堆所有权；原生struct布局与填充不是已定义的跨ABI线格式，持久化应按字段编码。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:237
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:340
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:348
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:384
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:395
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:486
 */

/**
 * @struct SZrSsaPlatformCapability
 *
 * ## Summary
 * 为目标接受观察与产物提供声明基准，而不是自动检测出的真实设备能力。
 *
 * ## Fields and invariants
 * - magic：TZrUInt32，解释及边界见下列约束。
 * - schemaVersion：TZrUInt32，解释及边界见下列约束。
 * - target：EZrSsaPlatformTarget，解释及边界见下列约束。
 * - architecture：EZrSsaPlatformArchitecture，解释及边界见下列约束。
 * - targetTriple：TZrChar[ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY]，解释及边界见下列约束。
 * - abi：SZrSsaPlatformAbi，解释及边界见下列约束。
 * - abiHash：TZrUInt64，解释及边界见下列约束。
 * - numericContractHash：TZrUInt64，解释及边界见下列约束。
 * - layoutContractHash：TZrUInt64，解释及边界见下列约束。
 * - semanticResultHash：TZrUInt64，解释及边界见下列约束。
 * - exceptionContractHash：TZrUInt64，解释及边界见下列约束。
 * - sourceMapHash：TZrUInt64，解释及边界见下列约束。
 * - featureFlags：TZrUInt64，解释及边界见下列约束。
 * - dispatchFlags：TZrUInt32，解释及边界见下列约束。
 *
 * ## Constraints and lifecycle
 * - magic/schema标识解释版本；target/architecture配对和targetTriple非空、96字节界内NUL结束均要验证。
 * - abi是内嵌值；abiHash/numericContractHash/layoutContractHash须非零但不被重算。
 * - semanticResultHash/exceptionContractHash/sourceMapHash在Check中各自零/零可选，任一非零则须相等。
 * - featureFlags和dispatchFlags均拒未知位；两域独立。初始化featureFlags为0，dispatchFlags为两种分派许可。
 * - WASM架构绑定指针位宽，受限目标禁止JIT，WASM禁止PMU；这些值检查不认证真实机器或实际权限。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:256
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:577
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:589
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:626
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:642
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:650
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:900
 */

/**
 * @struct SZrSsaPlatformObservation
 *
 * ## Summary
 * 记录编译、执行与语义阶段及对应目标见证，使矩阵能分别报告不可用、不支持和通过。
 *
 * ## Fields and invariants
 * - magic：TZrUInt32，解释及边界见下列约束。
 * - schemaVersion：TZrUInt32，解释及边界见下列约束。
 * - target：EZrSsaPlatformTarget，解释及边界见下列约束。
 * - architecture：EZrSsaPlatformArchitecture，解释及边界见下列约束。
 * - backend：EZrSsaPlatformBackend，解释及边界见下列约束。
 * - runner：EZrSsaPlatformRunner，解释及边界见下列约束。
 * - outcome：EZrSsaPlatformOutcome，解释及边界见下列约束。
 * - targetTriple：TZrChar[ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY]，解释及边界见下列约束。
 * - compiler：TZrChar[ZR_SSA_PLATFORM_COMPILER_CAPACITY]，解释及边界见下列约束。
 * - deviceOrRuntime：TZrChar[ZR_SSA_PLATFORM_RUNTIME_CAPACITY]，解释及边界见下列约束。
 * - observedAbi：SZrSsaPlatformAbi，解释及边界见下列约束。
 * - observedAbiHash：TZrUInt64，解释及边界见下列约束。
 * - observedNumericContractHash：TZrUInt64，解释及边界见下列约束。
 * - observedLayoutContractHash：TZrUInt64，解释及边界见下列约束。
 * - semanticResultHash：TZrUInt64，解释及边界见下列约束。
 * - exceptionContractHash：TZrUInt64，解释及边界见下列约束。
 * - sourceMapHash：TZrUInt64，解释及边界见下列约束。
 * - requiredFeatures：TZrUInt64，解释及边界见下列约束。
 * - unsupportedFeatures：TZrUInt64，解释及边界见下列约束。
 * - dispatchKind：EZrSsaPlatformDispatchKind，解释及边界见下列约束。
 * - dispatchFlags：TZrUInt32，解释及边界见下列约束。
 * - sourceId：TZrUInt32，解释及边界见下列约束。
 * - instructionId：TZrUInt32，解释及边界见下列约束。
 * - compiled：TZrBool，解释及边界见下列约束。
 * - executed：TZrBool，解释及边界见下列约束。
 * - semanticPassed：TZrBool，解释及边界见下列约束。
 * - machineCodeJitExecuted：TZrBool，解释及边界见下列约束。
 *
 * ## Constraints and lifecycle
 * - magic/schema、target/architecture、后端/来源/结果/分派及targetTriple由入口按其有限规则检查。
 * - observedAbi及三种外层哈希在Check中与声明绑定；IsRuntimeAcceptance不做这份声明比较。
 * - unsupportedFeatures须是requiredFeatures子集，PASSED不可携带不支持位；compiled/executed/semanticPassed互为独立记录阶段。
 * - compiler/deviceOrRuntime是内嵌报告数组，当前校验器不验证其NUL或读取它们来证明执行；targetTriple才做有界非空结束检查。
 * - sourceId/instructionId是复制到诊断的标量上下文，不在此解析实际源码或指令对象；machineCodeJitExecuted也只是记录位。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:278
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:682
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:693
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:702
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:950
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:984
 */

/**
 * @struct SZrSsaPlatformArtifactContract
 *
 * ## Summary
 * 保存装载或部署前要与目标声明匹配的产物需求和ABI值。
 *
 * ## Fields and invariants
 * - magic：TZrUInt32，解释及边界见下列约束。
 * - schemaVersion：TZrUInt32，解释及边界见下列约束。
 * - target：EZrSsaPlatformTarget，解释及边界见下列约束。
 * - architecture：EZrSsaPlatformArchitecture，解释及边界见下列约束。
 * - targetTriple：TZrChar[ZR_SSA_PLATFORM_TARGET_TRIPLE_CAPACITY]，解释及边界见下列约束。
 * - abi：SZrSsaPlatformAbi，解释及边界见下列约束。
 * - abiHash：TZrUInt64，解释及边界见下列约束。
 * - numericContractHash：TZrUInt64，解释及边界见下列约束。
 * - layoutContractHash：TZrUInt64，解释及边界见下列约束。
 * - requiredFeatures：TZrUInt64，解释及边界见下列约束。
 * - newNativeImportCount：TZrUInt32，解释及边界见下列约束。
 * - machineCodeJit：TZrBool，解释及边界见下列约束。
 * - restrictedPatch：TZrBool，解释及边界见下列约束。
 *
 * ## Constraints and lifecycle
 * - magic/schema、目标/架构、界内结束的targetTriple、内嵌ABI及三个外层哈希须通过ValidateArtifact与声明的有序比较。
 * - requiredFeatures仅允许已知且已声明的位；machineCodeJit标记另受目标与声明JIT许可检查。
 * - iOS restrictedPatch且newNativeImportCount非零时拒绝；这只是元数据策略，不进行真实导入、装载或执行。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:309
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:992
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1002
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1055
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1094
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1103
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1123
 */

/**
 * @struct SZrSsaPlatformDiagnostic
 *
 * ## Summary
 * 携带单次校验的状态、首个差异和来源坐标，避免只有布尔失败无法归因。
 *
 * ## Fields and invariants
 * - status：EZrSsaPlatformStatus，解释及边界见下列约束。
 * - target：EZrSsaPlatformTarget，解释及边界见下列约束。
 * - architecture：EZrSsaPlatformArchitecture，解释及边界见下列约束。
 * - backend：EZrSsaPlatformBackend，解释及边界见下列约束。
 * - runner：EZrSsaPlatformRunner，解释及边界见下列约束。
 * - abiField：EZrSsaPlatformAbiField，解释及边界见下列约束。
 * - requiredFeatures：TZrUInt64，解释及边界见下列约束。
 * - unsupportedFeatures：TZrUInt64，解释及边界见下列约束。
 * - expected：TZrUInt64，解释及边界见下列约束。
 * - actual：TZrUInt64，解释及边界见下列约束。
 * - sourceId：TZrUInt32，解释及边界见下列约束。
 * - instructionId：TZrUInt32，解释及边界见下列约束。
 * - witnessField：EZrSsaPlatformWitnessField，解释及边界见下列约束。
 *
 * ## Constraints and lifecycle
 * - Check/ValidateArtifact先清空整份记录；ValidateAbi/AbiEqual/ValidateCapability及内部助手仅写部分字段，调用方宜先DiagnosticInit。
 * - 只解释当前status对应字段；成功可能仍保留低层调用前的abiField/expected/actual等。
 * - 全部成员是标量或枚举，不保留输入指针；来源字段在完整校验前复制，值可能尚不可信。
 * - 作为输出时须与被读声明/观察/产物/ABI及其内嵌数组不重叠，并由调用方串行或独占写入。
 * - 对象由调用方提供值存储；模块不取得所有权，不分配、释放或延长任何外部对象生命周期。
 * - 允许多线程读取稳定只读值；写入/初始化/诊断必须由调用方同步，本模块没有锁或原子发布协议。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:329
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:113
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:121
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:137
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:248
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:395
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:702
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1011
 */

/**
 * @fn void ZrCommon_SsaPlatform_DiagnosticInit( SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 为下一次独立或组合校验建立无旧字段残留的诊断输出。
 *
 * ## Parameters
 * @param[out] diagnostic 可空；非空须能独占写入整个诊断记录。
 *
 * ## Returns
 * void：NULL无操作；否则全记录清零并置status为OK。
 *
 * ## Constraints and lifecycle
 * - 非空记录须可独占写入整个对象；对象只含内嵌值和定长数组，不拥有堆资源。重复初始化覆盖旧值，不使对象地址失效；旧字段借用的内容发生改变。NULL 为无操作。
 * - 清空不表示某份输入记录已验证通过；这是输出对象初始化。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:350
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:248
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:251
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:702
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1011
 * - tests/core/test_ssa_platform_matrix.c:474
 */

/**
 * @fn const TZrChar *ZrCommon_SsaPlatform_StatusName( EZrSsaPlatformStatus status)
 *
 * ## Summary
 * 把状态分类转换为报告可用的只读静态名称。
 *
 * ## Parameters
 * @param[in] status 状态枚举；可传未知或COUNT值。
 *
 * ## Returns
 * @return 返回对应静态字符串；未列举值返回invalid-platform-status。
 *
 * ## Constraints and lifecycle
 * - 返回借用在该模块静态存储有效期间可读；调用方不修改或释放。无分配和诊断输出。
 * - 当前仓内未检索到直接调用，公开仓外消费者未知；不据此断言死代码。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:353
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:256
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:319
 */

/**
 * @fn const TZrChar *ZrCommon_SsaPlatform_OutcomeName( EZrSsaPlatformOutcome outcome)
 *
 * ## Summary
 * 为观察结果提供稳定的报告名称，而不执行结果验收。
 *
 * ## Parameters
 * @param[in] outcome 待显示的结果枚举，包括未知值。
 *
 * ## Returns
 * @return PASSED/UNAVAILABLE/UNSUPPORTED/FAILED各有静态名称；其余值包括UNSET和COUNT返回unset。
 *
 * ## Constraints and lifecycle
 * - 只读静态借用，无需释放，不把显示名称当成通过判据。
 * - 仓内未见实际调用，公开外部消费未知。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:356
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:324
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:335
 */

/**
 * @fn void ZrCommon_SsaPlatform_AbiInit(SZrSsaPlatformAbi *abi)
 *
 * ## Summary
 * 建立尚未填充目标见证的空ABI记录，以便调用方显式填写目标属性。
 *
 * ## Parameters
 * @param[out] abi 可空的ABI输出。
 *
 * ## Returns
 * void：NULL无操作；非空时全清零，端序和聚合返回类别置UNKNOWN。
 *
 * ## Constraints and lifecycle
 * - 非空记录须可独占写入整个对象；对象只含内嵌值和定长数组，不拥有堆资源。重复初始化覆盖旧值，不使对象地址失效；旧字段借用的内容发生改变。NULL 为无操作。
 * - 空记录不能通过ValidateAbi；没有读取宿主或执行回调。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:360
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:340
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:355
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:585
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:689
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:998
 */

/**
 * @fn void ZrCommon_SsaPlatform_DetectHostAbi(SZrSsaPlatformAbi *abi)
 *
 * ## Summary
 * 给当前编译宿主提供初始ABI字段，供宿主测试与目标专用见证的起点使用。
 *
 * ## Parameters
 * @param[out] abi 可空且可独占写入的ABI输出。
 *
 * ## Returns
 * void：NULL无操作；非空覆盖为当前宿主宽度/端序/对齐及MIXED占位和非零回调占位摘要。
 *
 * ## Constraints and lifecycle
 * - 非空记录须可独占写入整个对象；对象只含内嵌值和定长数组，不拥有堆资源。重复初始化覆盖旧值，不使对象地址失效；旧字段借用的内容发生改变。NULL 为无操作。
 * - _MSC_VER分支取ZR_ALIGN_SIZE，另一分支取alignof(max_align_t)；这不是跨目标探测。
 * - 聚合返回类别MIXED为保守占位，不证明寄存器/内存实际返回规则。
 * - TODO: zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:374 nativeCallbackAbiHash仅由通用字段与常量构造；tests/core/test_ssa_platform_matrix.c:446、tests/core/test_ssa_platform_matrix.c:449 与tests/core/test_ssa_platform_matrix.c:453 只调用探针并检查哈希变化，未执行真实native callback。目标适配器仍需真实调用见证。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:365
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:348
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:360
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:363
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:371
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:374
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:377
 * - tests/core/test_ssa_platform_matrix.c:446
 * - tests/core/test_ssa_platform_matrix.c:447
 * - tests/core/test_ssa_platform_matrix.c:449
 * - tests/core/test_ssa_platform_matrix.c:453
 */

/**
 * @fn TZrUInt64 ZrCommon_SsaPlatform_ComputeAbiHash( const SZrSsaPlatformAbi *abi)
 *
 * ## Summary
 * 为声明、产物与观察提供同一字段顺序的ABI摘要计算工具。
 *
 * ## Parameters
 * @param[in] abi 可空；非空时须是完整、稳定可读的ABI记录，不要求先验证。
 *
 * ## Returns
 * @return NULL返回零；否则返回通用字段摘要再混入nativeCallbackAbiHash的64位值；可能为零或碰撞。
 *
 * ## Constraints and lifecycle
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 * - 不重算其它哈希、不验证目标、三元组或ABI合法性；ValidateCapability只要求调用方提供的abiHash/数值/布局哈希非零，不强制用本算法重算。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:370
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:384
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:388
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:391
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:660
 * - tests/core/test_ssa_platform_matrix.c:449
 * - tests/core/test_ssa_platform_matrix.c:453
 */

/**
 * @fn EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateAbi( const SZrSsaPlatformAbi *abi, SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 检查ABI值对象是否落在本模型能表达的宽度、端序、对齐及回调见证范围。
 *
 * ## Parameters
 * @param[in] abi 可空；空值返回INVALID_ARGUMENT，非空须稳定可读。
 * @param[in,out] diagnostic 可空的部分诊断输出；须与abi不重叠。
 *
 * ## Returns
 * @return NULL返回INVALID_ARGUMENT；依次拒绝指针位宽、端序、对齐、整数宽度、浮点宽度、聚合返回类别和零回调哈希为ABI_INVALID；通过返回OK。
 *
 * ## Constraints and lifecycle
 * - diagnostic 可空；非空输出须与所有被读输入记录及其内嵌数组不重叠。低层函数不清空整份诊断，仅更新当前分支字段和status，调用前可先 DiagnosticInit，返回后只解释本次status对应字段。
 * - 指针仅32/64；对齐非零、为2的幂且<=4096；宽度按固定常量比对；不证明IEEE格式、目标架构匹配或真实调用约定。
 * - 成功仅更新status为OK，可能保留上次的细分字段。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:375
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:395
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:403
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:420
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:430
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:453
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:466
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:475
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:482
 * - tests/core/test_ssa_platform_matrix.c:447
 * - tests/core/test_ssa_platform_matrix.c:465
 */

/**
 * @fn TZrBool ZrCommon_SsaPlatform_AbiEqual( const SZrSsaPlatformAbi *expected, const SZrSsaPlatformAbi *actual, SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 在两份ABI都有效后按固定字段顺序定位首个不匹配，使上层能报告稳定归因。
 *
 * ## Parameters
 * @param[in] expected 可空的预期ABI；空值被拒绝。
 * @param[in] actual 可空的实际ABI；允许与expected指向同一只读记录。
 * @param[in,out] diagnostic 可空、与两份输入均不重叠的诊断。
 *
 * ## Returns
 * @return 空值或任一ValidateAbi失败返回假；字段不同写首个差异并返回假；所有字段相同返回真。
 *
 * ## Constraints and lifecycle
 * - diagnostic 可空；非空输出须与所有被读输入记录及其内嵌数组不重叠。低层函数不清空整份诊断，仅更新当前分支字段和status，调用前可先 DiagnosticInit，返回后只解释本次status对应字段。
 * - 先验证expected，再短路验证actual；无效输入不会进入相等比较。
 * - 比较含nativeCallbackAbiHash，不比较外层abiHash；不保存输入指针。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:381
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:486
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:490
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:495
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:501
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:563
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:569
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:573
 * - tests/core/test_ssa_platform_matrix.c:110
 * - tests/core/test_ssa_platform_matrix.c:322
 */

/**
 * @fn void ZrCommon_SsaPlatform_CapabilityInit( SZrSsaPlatformCapability *capability)
 *
 * ## Summary
 * 建立能力声明的版本身份及默认分派许可，要求适配器随后补齐目标与契约字段。
 *
 * ## Parameters
 * @param[out] capability 可空的声明输出。
 *
 * ## Returns
 * void：NULL无操作；否则清零后填写magic/schema并许可两种dispatchFlags，嵌套ABI为空。
 *
 * ## Constraints and lifecycle
 * - 非空记录须可独占写入整个对象；对象只含内嵌值和定长数组，不拥有堆资源。重复初始化覆盖旧值，不使对象地址失效；旧字段借用的内容发生改变。NULL 为无操作。
 * - featureFlags初值仍为零；两分派许可不等于实际后端、执行或computed-goto能力已被证明。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:387
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:577
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:580
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:583
 * - tests/core/test_ssa_platform_matrix.c:15
 * - tests/core/test_ssa_platform_matrix.c:475
 * - tests/core/test_ssa_platform_matrix.c:479
 * - tests/core/test_ssa_platform_matrix.c:480
 */

/**
 * @fn EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateCapability( const SZrSsaPlatformCapability *capability, SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 在声明可用于观察或产物比对前检查版本、身份、掩码、ABI与本模块目标策略。
 *
 * ## Parameters
 * @param[in] capability 可空、稳定可读的声明；空值返回INVALID_ARGUMENT。
 * @param[in,out] diagnostic 可空、与声明不重叠的部分诊断。
 *
 * ## Returns
 * @return 返回首个身份/掩码/ABI错误；WASM位宽不符返回ABI_INVALID；受限JIT返回MACHINE_CODE_JIT_FORBIDDEN；WASM PMU返回REQUIRED_FEATURE_UNSUPPORTED；三种必需哈希为零返回ABI_INVALID；通过返回OK。
 *
 * ## Constraints and lifecycle
 * - diagnostic 可空；非空输出须与所有被读输入记录及其内嵌数组不重叠。低层函数不清空整份诊断，仅更新当前分支字段和status，调用前可先 DiagnosticInit，返回后只解释本次status对应字段。
 * - 仅WASM32/64显式绑定指针位宽，不能外推所有target/architecture/ABI的真实一致性。
 * - 三个哈希只要求非零，不重算；语义/异常/sourceMap哈希可为零，不在这里认证真实能力。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:392
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:589
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:599
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:605
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:613
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:621
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:626
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:642
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:650
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:660
 * - tests/core/test_ssa_platform_matrix.c:89
 * - tests/core/test_ssa_platform_matrix.c:296
 * - tests/core/test_ssa_platform_matrix.c:302
 */

/**
 * @fn TZrBool ZrCommon_SsaPlatform_CapabilitySupports( const SZrSsaPlatformCapability *capability, TZrUInt64 feature)
 *
 * ## Summary
 * 在调用方已验证声明的前提下查询一个已知、目标允许且已声明的能力位。
 *
 * ## Parameters
 * @param[in] capability 可空能力记录；可信查询应先ValidateCapability。
 * @param[in] feature 恰好一个已知能力位。
 *
 * ## Returns
 * @return 空声明、零/多比特/未知或目标禁止位返回假；否则按featureFlags是否含该位返回布尔值。
 *
 * ## Constraints and lifecycle
 * - 本函数不验证magic、schema、三元组、ABI或其它位，不替代ValidateCapability；当前仓内无直接调用或测试断言。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:398
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:669
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:673
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:676
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:679
 */

/**
 * @fn void ZrCommon_SsaPlatform_ObservationInit( SZrSsaPlatformObservation *observation)
 *
 * ## Summary
 * 建立尚未记录运行阶段的观察容器，避免未填记录自然成为通过结果。
 *
 * ## Parameters
 * @param[out] observation 可空的观察输出。
 *
 * ## Returns
 * void：NULL无操作；非空清零后设置magic/schema、UNSET outcome和空ABI。
 *
 * ## Constraints and lifecycle
 * - 非空记录须可独占写入整个对象；对象只含内嵌值和定长数组，不拥有堆资源。重复初始化覆盖旧值，不使对象地址失效；旧字段借用的内容发生改变。NULL 为无操作。
 * - 不采集编译器、设备、运行、语义或JIT证据；所有阶段字段保持假。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:403
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:682
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:685
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:688
 * - tests/core/test_ssa_platform_matrix.c:47
 * - tests/core/test_ssa_platform_matrix.c:476
 * - tests/core/test_ssa_platform_matrix.c:481
 * - tests/core/test_ssa_platform_matrix.c:482
 */

/**
 * @fn EZrSsaPlatformStatus ZrCommon_SsaPlatform_Check( const SZrSsaPlatformCapability *declared, const SZrSsaPlatformObservation *observed, SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 把运行观察与一份能力声明按首失败顺序比对，输出完整状态而不把人工记录认证为真实执行。
 *
 * ## Parameters
 * @param[in] declared 可空声明；非空须稳定可读。
 * @param[in] observed 可空观察；非空须稳定可读。
 * @param[out] diagnostic 可空；非空在读取输入前清零，须与两份输入及其内嵌数组完全不重叠。
 *
 * ## Returns
 * @return 通过全部门禁返回OK；否则返回首个状态。诊断status对应返回值，细分字段只在相应分支有意义。
 *
 * ## Constraints and lifecycle
 * - 先清空诊断并复制尚未验证的观察上下文，再拒空和验证声明；身份、枚举、分派选择、target/arch/triple、ABI和三种哈希依次检查。
 * - 随后检查需求掩码/不支持子集、共享来源策略、后端能力、需求集合与分派许可，再处理UNAVAILABLE/UNSUPPORTED、PASSED携带不支持位、编译/执行/语义阶段和三种可选见证。
 * - 共享策略只在此前门禁通过后触发；CROSS_COMPILE不能携带executed/semanticPassed/PASSED；Android/iOS/WASM的三种JIT触发被拒绝。
 * - 诊断不可别名任何被读输入。函数不检查任意输入区间或认证地址，调用方仍须提供各自有效C对象及可读extent；没有分配、执行、加载、认证来源或保留指针。
 * - 输入在读取期间稳定存活；允许共享只读输入，诊断输出须独占写入，本模块不提供锁或原子发布。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:410
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:693
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:702
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:709
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:713
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:764
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:769
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:778
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:815
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:822
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:850
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:875
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:884
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:900
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:946
 * - tests/core/test_ssa_platform_matrix.c:91
 * - tests/core/test_ssa_platform_matrix.c:96
 * - tests/core/test_ssa_platform_matrix.c:200
 * - tests/core/test_ssa_platform_matrix.c:209
 * - tests/core/test_ssa_platform_matrix.c:260
 * - tests/core/test_ssa_platform_matrix.c:365
 * - tests/core/test_ssa_platform_matrix.c:382
 * - tests/core/test_ssa_platform_matrix.c:400
 * - tests/core/test_ssa_platform_matrix.c:427
 * - tests/core/test_ssa_platform_matrix.c:434
 */

/**
 * @fn TZrBool ZrCommon_SsaPlatform_IsRuntimeAcceptance( const SZrSsaPlatformObservation *observation)
 *
 * ## Summary
 * 只判断单份观察是否满足本接口内部的运行通过条件，不代替与声明绑定的完整Check。
 *
 * ## Parameters
 * @param[in] observation 可空、稳定可读的观察；空值返回假。
 *
 * ## Returns
 * @return 任一记录/掩码/选择/ABI门禁失败返回假；之后仅当compiled、executed、semanticPassed、PASSED、不支持位为零及共享来源策略均满足才返回真。
 *
 * ## Constraints and lifecycle
 * - &&按顺序短路，来源助手可能不被执行；返回假不意味着特定后续分支已被覆盖。
 * - 无declared参数，不比较声明后端能力、ABI/数值/布局或可选见证哈希；也不调用ValidateCapability的WASM架构—指针宽度额外绑定。
 * - 返回真只是记录自洽的有限谓词，不是设备/后端运行证据。
 * - 不分配、不释放、不保存指针或调用设备/回调；输入在本次读取期间须保持存活且不被并发修改。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:419
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:950
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:952
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:966
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:980
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:984
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:988
 * - tests/core/test_ssa_platform_matrix.c:183
 * - tests/core/test_ssa_platform_matrix.c:206
 * - tests/core/test_ssa_platform_matrix.c:211
 * - tests/core/test_ssa_platform_matrix.c:234
 * - tests/core/test_ssa_platform_matrix.c:266
 * - tests/core/test_ssa_platform_matrix.c:352
 * - tests/core/test_ssa_platform_matrix.c:386
 * - tests/core/test_ssa_platform_matrix.c:478
 * - tests/core/test_ssa_platform_matrix.c:494
 * - tests/core/test_ssa_platform_matrix.c:496
 * - tests/core/test_ssa_platform_matrix.c:500
 */

/**
 * @fn void ZrCommon_SsaPlatform_ArtifactContractInit( SZrSsaPlatformArtifactContract *artifact)
 *
 * ## Summary
 * 建立加载前产物声明的版本身份，并留给发布者补齐目标、ABI和需求。
 *
 * ## Parameters
 * @param[out] artifact 可空的产物输出。
 *
 * ## Returns
 * void：NULL无操作；非空全清零，设置magic/schema并建立空ABI。
 *
 * ## Constraints and lifecycle
 * - 非空记录须可独占写入整个对象；对象只含内嵌值和定长数组，不拥有堆资源。重复初始化覆盖旧值，不使对象地址失效；旧字段借用的内容发生改变。NULL 为无操作。
 * - 初始化不表示产物已编译、装载、执行或具备原生导入权限。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:423
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:992
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:995
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:998
 * - tests/core/test_ssa_platform_matrix.c:70
 * - tests/core/test_ssa_platform_matrix.c:477
 */

/**
 * @fn EZrSsaPlatformStatus ZrCommon_SsaPlatform_ValidateArtifact( const SZrSsaPlatformCapability *declared, const SZrSsaPlatformArtifactContract *artifact, SZrSsaPlatformDiagnostic *diagnostic)
 *
 * ## Summary
 * 在部署或装载前把产物的身份、ABI、契约哈希和需求与目标声明比较。
 *
 * ## Parameters
 * @param[in] declared 可空、稳定可读的能力声明。
 * @param[in] artifact 可空、稳定可读的产物声明。
 * @param[out] diagnostic 可空；非空先清空，须与两份输入及其内嵌数组不重叠。
 *
 * ## Returns
 * @return 返回首个声明/身份/target/architecture/triple/ABI/哈希/需求或平台策略错误；全部通过返回OK。
 *
 * ## Constraints and lifecycle
 * - 先清空并复制原始产物上下文，再拒空、验证声明和产物身份；逐一比对ABI本体及三个外层哈希。
 * - 需求须为已知声明子集；requiredFeatures中的JIT位若未被声明支持，先在需求子集检查返回REQUIRED_FEATURE_UNSUPPORTED，不保证到达后面的MACHINE_CODE_JIT_FORBIDDEN分支。
 * - iOS restrictedPatch且新增native导入数非零被拒；JIT需求位和machineCodeJit标记分别受目标/声明许可检查。
 * - 不消费Observation、不执行代码、不分配或保留产物/声明指针；不能当作成功运行、实际导入或装载的证明。
 * - 输入在读取期间稳定存活；允许共享只读输入，诊断输出须独占写入，本模块不提供锁或原子发布。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:426
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1002
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1011
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1018
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1022
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1046
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1055
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1061
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1070
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1080
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1089
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1094
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1103
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1110
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1117
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1123
 * - tests/core/test_ssa_platform_matrix.c:110
 * - tests/core/test_ssa_platform_matrix.c:118
 * - tests/core/test_ssa_platform_matrix.c:283
 */

/**
 * @fn TZrBool ZrTests_Ssa_CheckPlatform( const SZrSsaPlatformCapability *declared, const SZrSsaPlatformObservation *observed)
 *
 * ## Summary
 * 为矩阵调用方提供仅保留Check是否OK的布尔包装。
 *
 * ## Parameters
 * @param[in] declared 可空声明，按Check前提解释。
 * @param[in] observed 可空观察，按Check前提解释。
 *
 * ## Returns
 * @return 仅Check返回OK时为真；其余状态均为假。
 *
 * ## Constraints and lifecycle
 * - 使用局部栈诊断并在返回时丢弃；需要错误字段的调用方应直接Check。
 * - 没有独立的设备运行或额外验收门禁，不分配或保存指针。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:432
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1137
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1140
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1142
 * - tests/core/test_ssa_platform_matrix.c:93
 * - tests/core/test_ssa_platform_matrix.c:99
 * - tests/core/test_ssa_platform_matrix.c:145
 * - tests/core/test_ssa_platform_matrix.c:205
 * - tests/core/test_ssa_platform_matrix.c:233
 * - tests/core/test_ssa_platform_matrix.c:265
 * - tests/core/test_ssa_platform_matrix.c:385
 */

/**
 * @def ZR_SSA_PLATFORM_STATIC_ASSERT
 *
 * ## Summary
 * 在头文件自身编译时拒绝本值契约无法表达的基本类型宽度，并在结束时撤销辅助宏。
 *
 * ## Parameters
 * @param[in] CONDITION 编译期可判定条件；头内八次使用分别检查字节、整数、浮点及指针宽度。
 * @param[in] MESSAGE 编译器要求的静态断言诊断文本，不作为运行期字符串保存。
 *
 * ## Returns
 * 没有运行期返回值；条件不成立会导致当前翻译单元编译失败。
 *
 * ## Constraints and lifecycle
 * - __cplusplus分支展开static_assert，C分支展开_Static_assert；须由适当C++11/C11编译环境支持。
 * - 八次使用分别检查8位字节、四种无符号整数宽度、两种浮点宽度和32/64位指针；只验证编译目标当前基本类型。
 * - 只验证位宽，不能证明IEEE格式、目标ABI、返回约定或真实native callback。
 * - 宏在zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:440被undef，不是头包含结束后供外部调用的公共函数或保留宏。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:33
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:32
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:35
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:36
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:39
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:41
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:43
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:45
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:47
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:49
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:51
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:53
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:440
 */

/**
 * @def ZR_SSA_PLATFORM_DISPATCH_FLAG_KNOWN_MASK
 *
 * ## Summary
 * 限定dispatchFlags的可解释许可位域，使声明和观察拒绝未定义分派位。
 *
 * ## Constraints and lifecycle
 * - 当前只有SWITCH和COMPUTED_GOTO；不同于64位featureFlags域和dispatchKind枚举。
 * - 观察选择具体kind时还需自身对应位，Check另验观察集合是声明许可子集。无原子发布或实际分派动作。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:180
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:613
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:736
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:840
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:972
 */

/**
 * @def ZR_SSA_PLATFORM_FEATURE_KNOWN_MASK
 *
 * ## Summary
 * 界定当前schema可解释的13个64位能力位，使未知声明/需求能在集合比较前被拒绝。
 *
 * ## Constraints and lifecycle
 * - 由各已知单比特OR成；不得把掩码包含当作能力实际可用、后端实现或运行证明。
 * - ValidateCapability检查声明，Check检查需求及不支持子集，IsRuntimeAcceptance检查两个集合，ValidateArtifact检查需求。
 * - 新增能力位要同步本掩码和各入口目标/后端策略；宏自身只给值，不实施策略。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/ssa_platform_contract.h:184
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:605
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:675
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:805
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:966
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:969
 * - zr_vm_common/src/zr_vm_common/ssa_platform_contract.c:1089
 */
