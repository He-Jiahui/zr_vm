#ifndef ZR_VM_PARSER_BACKEND_AOT_IR_COVERAGE_H
#define ZR_VM_PARSER_BACKEND_AOT_IR_COVERAGE_H

#include "backend_aot_ir_adapter.h"

/** @brief 每个语义位点的互斥覆盖类别。 */
typedef enum EZrBackendAotIrCoverageClass {
    ZR_BACKEND_AOT_IR_COVERAGE_NATIVE = 0,
    ZR_BACKEND_AOT_IR_COVERAGE_RUNTIME_HELPER,
    ZR_BACKEND_AOT_IR_COVERAGE_INTERPRETER_FALLBACK,
    ZR_BACKEND_AOT_IR_COVERAGE_UNSUPPORTED,
    ZR_BACKEND_AOT_IR_COVERAGE_CLASS_COUNT
} EZrBackendAotIrCoverageClass;

/** @brief 语义位点分母及各目标处理类别的计数。 */
typedef struct SZrBackendAotIrCoverage {
    TZrUInt64 semanticSiteCount;
    TZrUInt64 nativeSiteCount;
    TZrUInt64 runtimeHelperSiteCount;
    TZrUInt64 interpreterFallbackSiteCount;
    TZrUInt64 unsupportedSiteCount;
} SZrBackendAotIrCoverage;

/** @brief 覆盖校验的状态、类别与期望/实际计数。 */
typedef struct SZrBackendAotIrCoverageDiagnostic {
    EZrBackendAotIrStatus status;
    EZrBackendAotIrCoverageClass coverageClass;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrBackendAotIrCoverageDiagnostic;

/** @brief 将覆盖计数清零，开始新的统计。 */
ZR_PARSER_API void backend_aot_ir_coverage_init(
        SZrBackendAotIrCoverage *coverage);

/** @brief 设置语义位点分母并清空所有分类计数。 */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_set_semantic_sites(
        SZrBackendAotIrCoverage *coverage,
        TZrUInt64 semanticSiteCount,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

/** @brief 为指定类别累加位点，拒绝 64 位溢出。 */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_add(
        SZrBackendAotIrCoverage *coverage,
        EZrBackendAotIrCoverageClass coverageClass,
        TZrUInt64 count,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

/** @brief 验证分母非零且互斥分类之和等于分母。 */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_validate(
        const SZrBackendAotIrCoverage *coverage,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

/** @brief 从静态 lowering 事实构造覆盖分母；每条 AOTIR 指令各占一个语义位点。
 *  @note 桥接、回退和不支持操作仍在分母内，融合或内联不能缩小分母。 */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_from_facts(
        const SZrBackendAotIrFacts *facts,
        SZrBackendAotIrCoverage *outCoverage,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

/** @brief 输出各处理类别的千分比；零分母不可用，且保持输出参数不变。 */
ZR_PARSER_API TZrBool backend_aot_ir_coverage_ratio_per_mille(
        const SZrBackendAotIrCoverage *coverage,
        TZrUInt32 *outNative,
        TZrUInt32 *outRuntimeHelper,
        TZrUInt32 *outInterpreterFallback,
        SZrBackendAotIrCoverageDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_BACKEND_AOT_IR_COVERAGE_H */
