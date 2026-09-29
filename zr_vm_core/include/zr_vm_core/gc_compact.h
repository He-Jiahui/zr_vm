#ifndef ZR_VM_CORE_GC_COMPACT_H
#define ZR_VM_CORE_GC_COMPACT_H

/* 压缩规划只决定旧区准入和字节预算，不搬迁对象、不发布转发地址。
 * 它按可回收比例挑选整区，跳过固定、大型和永久区；计划只含标量统计。
 * GC cycle 的 major 回收另有旧区整理路径，重排逻辑区登记并保持对象地址。
 * 两条路径尚未连接；计划值不能视为已执行压缩，也不保存对象或转发地址。 */

#include "zr_vm_core/gc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 对外计划记录的布局/语义版本；不兼容变更须提升此值。 */
#define ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_COMPACT_CONTRACT_MAGIC ((TZrUInt32)0x31434347u) /* GCC1 标识 */

/** @brief 模式表示允许搬迁与否；本模块仍只产出计划，不执行搬迁。 */
typedef enum EZrGcCompactMode {
    ZR_GC_COMPACT_MODE_NON_MOVING = 0,
    ZR_GC_COMPACT_MODE_SELECTIVE_MOVING,
    ZR_GC_COMPACT_MODE_COUNT
} EZrGcCompactMode;

/** @brief 诊断分类供调用方分支处理；Name 对未知数值返回稳定的 "unknown"。 */
typedef enum EZrGcCompactDiagnosticCode {
    ZR_GC_COMPACT_DIAGNOSTIC_NONE = 0,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION,
    ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW,
    ZR_GC_COMPACT_DIAGNOSTIC_BUDGET,
    ZR_GC_COMPACT_DIAGNOSTIC_COUNT
} EZrGcCompactDiagnosticCode;

/** @brief 失败原因及对应输入序号；expected/actual 由具体拒绝分支填写。 */
typedef struct SZrGcCompactDiagnostic {
    EZrGcCompactDiagnosticCode code;
    TZrUInt32 index;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcCompactDiagnostic;

/** @brief 只读区段快照请求；budgetBytes 为零表示不限制计划字节数。 */
typedef struct SZrGcCompactRequest {
    const SZrGarbageCollectRegionDescriptor *regions;
    TZrSize regionCount;
    TZrUInt64 budgetBytes;
    TZrUInt32 fragmentationThresholdPercent;
    TZrBool allowMoving;
} SZrGcCompactRequest;

/** @brief 准入结果统计候选、排除和预算选中的整区；plannedBytes 不是已搬迁量。 */
typedef struct SZrGcCompactPlan {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrGcCompactMode mode;
    TZrBool eligible;
    TZrBool deferred;
    TZrUInt32 regionCount;
    TZrUInt32 candidateRegionCount;
    TZrUInt32 pinnedRegionCount;
    TZrUInt64 candidateBytes;
    TZrUInt64 movableBytes;
    TZrUInt64 plannedBytes;
    TZrUInt64 pinnedBytes;
    TZrUInt64 estimatedWorkUnits;
} SZrGcCompactPlan;

/** @brief 将可选诊断记录清零；diagnostic 为空时不操作。 */
ZR_CORE_API void ZrCore_GcCompact_DiagnosticClear(
        SZrGcCompactDiagnostic *diagnostic);
/** @brief 返回诊断码的稳定小写名称；未识别的数值返回 "unknown"。 */
ZR_CORE_API const TZrChar *ZrCore_GcCompact_DiagnosticName(
        EZrGcCompactDiagnosticCode code);
/** @brief 初始化为空的 non-moving 计划；plan 为空时不操作。 */
ZR_CORE_API void ZrCore_GcCompact_Init(SZrGcCompactPlan *plan);
/** @brief 校验计划标识、模式、区段计数和预算字段，并清空可选诊断。
 * @pre plan 与 diagnostic（若提供）不重叠。 */
ZR_CORE_API TZrBool ZrCore_GcCompact_Validate(
        const SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic);
/** @brief 校验请求并按旧区段碎片率和整区 live 字节数生成纯标量计划。
 * @pre request、plan、diagnostic（若提供）不重叠；region 数组在调用期间只读。
 * @note allowMoving 为假时不选择搬迁区；budgetBytes 为零表示无预算上限。 */
ZR_CORE_API TZrBool ZrCore_GcCompact_Plan(
        const SZrGcCompactRequest *request,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic);
/** @brief 从 global 的当前区段登记表构造请求；本函数不执行 GC 或对象迁移。
 * @pre 登记表在调用期间须保持稳定。TODO: 仓内尚无生产调用，需确认安全点/快照要求。 */
ZR_CORE_API TZrBool ZrCore_GarbageCollector_PlanCompaction(
        struct SZrGlobalState *global,
        TZrUInt64 budgetBytes,
        TZrUInt32 fragmentationThresholdPercent,
        TZrBool allowMoving,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_COMPACT_H */
