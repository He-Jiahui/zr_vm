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
