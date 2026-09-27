#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_ADAPTER_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_ADAPTER_H

/* 此适配层只传递共享 AOTIR 的标量契约与借用视图；不暴露函数指针、
 * 可执行地址或旧 SZrInstruction。借用视图不得超过调用方 AOTIR 模块的生命周期。 */

#include "zr_vm_parser/aot_ir_lowering.h"

/** @brief 将共享 AOTIR 诊断映射到归档适配层的状态域。 */
typedef enum EZrBackendAotIrStatus {
    ZR_BACKEND_AOT_IR_OK = 0,
    ZR_BACKEND_AOT_IR_INVALID_ARGUMENT,
    ZR_BACKEND_AOT_IR_INVALID_AOTIR,
    ZR_BACKEND_AOT_IR_RELOCATION_PRESENT,
    ZR_BACKEND_AOT_IR_TARGET_MISMATCH,
    ZR_BACKEND_AOT_IR_UNSUPPORTED,
    ZR_BACKEND_AOT_IR_ARTIFACT_UNAVAILABLE,
    ZR_BACKEND_AOT_IR_CAPACITY,
    ZR_BACKEND_AOT_IR_COVERAGE_UNAVAILABLE,
    ZR_BACKEND_AOT_IR_COVERAGE_INVALID,
    ZR_BACKEND_AOT_IR_PROFILE_MISMATCH,
    ZR_BACKEND_AOT_IR_TOOLCHAIN_UNSUPPORTED,
    ZR_BACKEND_AOT_IR_FALLBACK,
    ZR_BACKEND_AOT_IR_STATUS_COUNT
} EZrBackendAotIrStatus;

/** @brief 错误位置及期望/实际值；aotIrStatus 保留共享层原始原因。 */
typedef struct SZrBackendAotIrDiagnostic {
    EZrBackendAotIrStatus status;
    EZrAotIrStatus aotIrStatus;
    TZrUInt32 functionId;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrBackendAotIrDiagnostic;

/** @brief 目标 lowering 的静态计数与哈希；descriptorOnly 始终为真，不代表已生成代码。 */
typedef struct SZrBackendAotIrFacts {
    EZrAotIrEmitterTarget target;
    TZrBool descriptorOnly;
    TZrBool artifactAvailable;
    TZrBool fallbackUsed;
    TZrUInt32 instructionCount;
    TZrUInt32 semanticSiteCount;
    TZrUInt32 nativeLoweredCount;
    TZrUInt32 runtimeBridgeCount;
    TZrUInt32 interpreterFallbackCount;
    TZrUInt32 unsupportedCount;
    TZrUInt64 moduleHash;
    TZrUInt64 sourceHash;
    TZrUInt64 loweringHash;
    TZrUInt64 contractHash;
} SZrBackendAotIrFacts;

/** @brief 返回归档适配状态的稳定文本名称。 */
ZR_PARSER_API const TZrChar *backend_aot_ir_adapter_status_name(
        EZrBackendAotIrStatus status);

/** @brief 先校验共享 AOTIR，再拒绝仍含 relocation 的模块。 */
ZR_PARSER_API EZrBackendAotIrStatus backend_aot_ir_adapter_validate(
        const SZrAotIrModule *module,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 验证单函数显式 ABI 并原样传给未来产物 emitter；UNKNOWN 返回 unsupported。 */
ZR_PARSER_API EZrBackendAotIrStatus backend_aot_ir_adapter_require_executable_abi(
        const SZrAotIrModule *module, TZrUInt32 functionId,
        SZrAotIrCallableAbi *outAbi,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 统计全模块指令数，拒绝 UINT32 容量溢出。 */
ZR_PARSER_API TZrBool backend_aot_ir_adapter_count_instructions(
        const SZrAotIrModule *module,
        TZrUInt32 *outCount,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 将共享 lowering 记录写入调用方缓冲区并校验计数/指针形状。
 *  @pre records 可容纳 capacity 条；本层不分配也不保存借用指针。 */
ZR_PARSER_API TZrBool backend_aot_ir_adapter_collect(
        const SZrAotIrModule *module,
        SZrAotIrLoweringRecord *records,
        TZrUInt32 capacity,
        SZrAotIrLoweringResult *outLowering,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 将 lowering 记录分类成目标描述与覆盖计数。
 *  @pre 手构 lowering 的 records 须在调用期间有效且对应同一 module。
 *  @note 结果只证明记录形状和计数，不证明记录来源或代码产物存在。 */
ZR_PARSER_API TZrBool backend_aot_ir_adapter_facts_from_lowering(
        const SZrAotIrModule *module,
        EZrAotIrEmitterTarget target,
        const SZrAotIrEmitOptions *options,
        const SZrAotIrLoweringResult *lowering,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 调用共享 C/LLVM facade 取得描述和覆盖统计。
 *  @note 不输出源码、LLVM IR 或机器码，artifactAvailable 始终为假。 */
ZR_PARSER_API TZrBool backend_aot_ir_adapter_emit_target(
        const SZrAotIrModule *module,
        EZrAotIrEmitterTarget target,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 为 C 目标请求纯描述投影。 */
ZR_PARSER_API TZrBool backend_aot_ir_c_emit(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief C 目标入口；要求实际产物时以 ARTIFACT_UNAVAILABLE 失败并保留描述事实。 */
ZR_PARSER_API TZrBool backend_aot_ir_c_emit_ex(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        TZrBool requireArtifact,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief 为 LLVM 目标请求纯描述投影。 */
ZR_PARSER_API TZrBool backend_aot_ir_llvm_emit(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/** @brief LLVM 目标入口；要求实际产物时以 ARTIFACT_UNAVAILABLE 失败并保留描述事实。 */
ZR_PARSER_API TZrBool backend_aot_ir_llvm_emit_ex(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        TZrBool requireArtifact,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic);

/* 旧归档记录使用的命名别名，行为与对应目标入口一致。 */
/** @brief C 描述入口的兼容别名。 */
static inline TZrBool backend_aot_ir_emit_c(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    return backend_aot_ir_c_emit(module, options, outFacts, diagnostic);
}

/** @brief LLVM 描述入口的兼容别名。 */
static inline TZrBool backend_aot_ir_emit_llvm(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrBackendAotIrFacts *outFacts,
        SZrBackendAotIrDiagnostic *diagnostic) {
    return backend_aot_ir_llvm_emit(module, options, outFacts, diagnostic);
}

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_ADAPTER_H */
