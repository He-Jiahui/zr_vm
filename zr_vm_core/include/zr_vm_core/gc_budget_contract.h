#ifndef ZR_VM_CORE_GC_BUDGET_CONTRACT_H
#define ZR_VM_CORE_GC_BUDGET_CONTRACT_H

/**
 * @brief 分片 GC 的无指针预算契约，供宿主调度器和 major 状态机记录阶段边界。
 * 此处只验证单次工作量并生成标量见证；实际回收、暂停和对象所有权由调用方负责。
 * 输出可用于游标与遥测持久化，但不代表回收器已经执行或承诺硬实时暂停上限。
 */

#include "zr_vm_core/conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 预算与账本共享的布局版本；持久化记录应先核对 magic 和版本。 */
#define ZR_GC_BUDGET_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_GC_BUDGET_CONTRACT_MAGIC ((TZrUInt32)0x31474243u) /* CBG1 */

/** 允许压缩需另给字节上限；压力报告影响 pressure 及无其他暂停原因时的 pauseReason，不改变接纳或游标。 */
#define ZR_GC_BUDGET_FLAG_ALLOW_COMPACT ((TZrUInt32)1u << 0u)
#define ZR_GC_BUDGET_FLAG_REPORT_PRESSURE ((TZrUInt32)1u << 1u)
/** 验证器拒绝此集合之外的标志位。 */
#define ZR_GC_BUDGET_FLAG_KNOWN_MASK \
    (ZR_GC_BUDGET_FLAG_ALLOW_COMPACT | ZR_GC_BUDGET_FLAG_REPORT_PRESSURE)

/** 诊断分类供宿主展示；具体失败字段由 diagnostic.field 标识。 */
typedef enum EZrGcBudgetDiagnosticCode {
    ZR_GC_BUDGET_DIAGNOSTIC_NONE = 0,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_ARGUMENT,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_MAGIC,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_SCHEMA,
    ZR_GC_BUDGET_DIAGNOSTIC_UNKNOWN_FLAGS,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_PHASE,
    ZR_GC_BUDGET_DIAGNOSTIC_OVERFLOW,
    ZR_GC_BUDGET_DIAGNOSTIC_INVALID_BUDGET,
    ZR_GC_BUDGET_DIAGNOSTIC_COUNT
} EZrGcBudgetDiagnosticCode;

/** 验证失败的标量诊断；field 是内部字段编号，不是源文件位置。 */
typedef struct SZrGcBudgetDiagnostic {
    EZrGcBudgetDiagnosticCode code;
    TZrUInt32 field;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrGcBudgetDiagnostic;

/** 可评估的工作阶段仅为 INITIAL_SNAPSHOT 至 COMPACT，终态不接收工作量。 */
typedef enum EZrGcBudgetPhase {
    ZR_GC_BUDGET_PHASE_IDLE = 0,
    ZR_GC_BUDGET_PHASE_INITIAL_SNAPSHOT,
    ZR_GC_BUDGET_PHASE_MARK_CONCURRENT,
    ZR_GC_BUDGET_PHASE_REMARK,
    ZR_GC_BUDGET_PHASE_SWEEP,
    ZR_GC_BUDGET_PHASE_COMPACT,
    ZR_GC_BUDGET_PHASE_COMPLETE,
    ZR_GC_BUDGET_PHASE_COUNT
} EZrGcBudgetPhase;

/** 暂停归因随结果返回，部分值供其他调度路径记录。 */
typedef enum EZrGcBudgetPauseReason {
    ZR_GC_BUDGET_PAUSE_NONE = 0,
    ZR_GC_BUDGET_PAUSE_BUDGET,
    ZR_GC_BUDGET_PAUSE_SAFEPOINT,
    ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION,
    ZR_GC_BUDGET_PAUSE_PRESSURE,
    ZR_GC_BUDGET_PAUSE_OOM,
    ZR_GC_BUDGET_PAUSE_CANCELLED,
    ZR_GC_BUDGET_PAUSE_COMPLETE,
    ZR_GC_BUDGET_PAUSE_COUNT
} EZrGcBudgetPauseReason;

/** OVER_BUDGET 表示工作已被接纳但原子暂停超限；输出游标包含已完成工作。 */
typedef enum EZrGcBudgetStepStatus {
    ZR_GC_BUDGET_STEP_ACCEPTED = 0,
    ZR_GC_BUDGET_STEP_REJECTED,
    ZR_GC_BUDGET_STEP_DEFERRED,
    ZR_GC_BUDGET_STEP_OVER_BUDGET
} EZrGcBudgetStepStatus;

/**
 * @brief 调度器提交的单次预算配置；结构体不持有堆资源。
 * 四项工作上限至少一项非零，各维度的零值表示该维不单独限流。
 * 开启压缩时 compactBudgetBytes 必须非零；压力阈值和原子暂停上限可选。
 */
typedef struct SZrGcBudget {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    TZrUInt32 flags;
    TZrUInt32 reserved;
    TZrUInt64 maxElapsedUs;
    TZrUInt64 maxWorkUnits;
    TZrUInt64 maxBytes;
    TZrUInt64 maxObjects;
    TZrUInt64 compactBudgetBytes;
    TZrUInt64 pressureThresholdBytes;
    TZrUInt64 maxAtomicPauseUs;
} SZrGcBudget;

/**
 * @brief 一次预算评估的标量见证，供 major 状态机、账本与宿主遥测消费。
 * EvaluateStep 单步输出中，ACCEPTED 和 OVER_BUDGET 的 nextCursor/workDone
 * 包含已接纳工作，REJECTED 与 DEFERRED 保留输入游标。
 * GetBudgetStats 复用此类型承载累计值；调用失败时不得假定输出已初始化。
 */
typedef struct SZrGcBudgetStepResult {
    EZrGcBudgetStepStatus status;
    EZrGcBudgetPhase phase;
    EZrGcBudgetPauseReason pauseReason;
    TZrUInt64 nextCursor;
    TZrUInt64 workDone;
    TZrUInt64 elapsedUs;
    TZrUInt64 bytesDone;
    TZrUInt64 objectsDone;
    TZrInt64 debtBytes;
    TZrUInt64 maxAtomicPauseUs;
    TZrUInt64 overBudgetCount;
    TZrUInt64 compactDeferredCount;
    TZrBool pressure;
    TZrBool consistentBoundary;
} SZrGcBudgetStepResult;

/**
 * @brief major 调度器可保存的累计账本，不拥有回收器状态或对象引用。
 * 只接纳 consistentBoundary 的单步结果；累计计数饱和，游标以已接纳工作推进。
 */
typedef struct SZrGcBudgetLedger {
    TZrUInt32 magic;
    TZrUInt32 schemaVersion;
    EZrGcBudgetStepStatus lastStatus;
    EZrGcBudgetPhase lastPhase;
    EZrGcBudgetPauseReason lastPauseReason;
    TZrUInt64 cursor;
    TZrUInt64 workDone;
    TZrUInt64 elapsedUs;
    TZrUInt64 bytesDone;
    TZrUInt64 objectsDone;
    TZrInt64 debtBytes;
    TZrUInt64 overBudgetCount;
    TZrUInt64 compactDeferredCount;
    TZrBool pressure;
    TZrBool consistentBoundary;
} SZrGcBudgetLedger;

/** @brief 清空可选诊断输出，供下一次验证或评估复用。 */
ZR_CORE_API void ZrCore_GcBudget_DiagnosticClear(
        SZrGcBudgetDiagnostic *diagnostic);
/** @brief 返回诊断枚举的展示名称；控制流判断应使用枚举值。 */
ZR_CORE_API const TZrChar *ZrCore_GcBudget_DiagnosticName(
        EZrGcBudgetDiagnosticCode code);
/** @brief 初始化预算头与默认压力报告标志；调用方仍需设置至少一项工作上限。 */
ZR_CORE_API void ZrCore_GcBudget_Init(SZrGcBudget *budget);
/**
 * @brief 安装或评估前验证预算布局、标志与限额组合。
 * @return 有效时为真；失败时可选 diagnostic 给出首个拒绝原因。
 */
ZR_CORE_API TZrBool ZrCore_GcBudget_Validate(
        const SZrGcBudget *budget,
        SZrGcBudgetDiagnostic *diagnostic);
/**
 * @brief 根据调用方已测得的单片工作量生成可持久化的阶段见证。
 * @pre budget 已按配置语义准备；phase 为活动阶段且 workUnits 非零。
 * @return 参数与计数合法时为真，是否接纳由 result.status 决定。
 * @note 失败时仅 diagnostic 可靠；预算预检失败可使 result 保持原内容。
 * 此函数不执行 GC，也不测量 elapsedUs 或 atomicPauseUs。
 */
ZR_CORE_API TZrBool ZrCore_GcBudget_EvaluateStep(
        const SZrGcBudget *budget,
        EZrGcBudgetPhase phase,
        TZrUInt64 cursor,
        TZrUInt64 workUnits,
        TZrUInt64 elapsedUs,
        TZrUInt64 bytes,
        TZrUInt64 objects,
        TZrInt64 debtBytes,
        TZrUInt64 atomicPauseUs,
        SZrGcBudgetStepResult *result,
        SZrGcBudgetDiagnostic *diagnostic);
/** @brief 返回步骤状态的展示名称；调度决策应使用枚举值。 */
ZR_CORE_API const TZrChar *ZrCore_GcBudget_StatusName(
        EZrGcBudgetStepStatus status);
/** @brief 返回暂停原因的展示名称；调度决策应使用枚举值。 */
ZR_CORE_API const TZrChar *ZrCore_GcBudget_PauseReasonName(
        EZrGcBudgetPauseReason reason);
/** @brief 初始化可复用的累计账本及一致性边界。 */
ZR_CORE_API void ZrCore_GcBudget_LedgerInit(SZrGcBudgetLedger *ledger);
/** @brief 验证账本头与状态范围；失败时可选 diagnostic 给出原因。 */
ZR_CORE_API TZrBool ZrCore_GcBudget_LedgerValidate(
        const SZrGcBudgetLedger *ledger,
        SZrGcBudgetDiagnostic *diagnostic);
/**
 * @brief 将一致边界上的步骤见证并入账本，供 major 周期累计与游标恢复。
 * @pre result 来自同一账本游标上的评估，且调用方已确认实际工作边界。
 * @return 非法状态或游标关系被拒绝时为假，账本保持原值。
 * TODO: 当前账本接受相同已推进游标的重复见证并再次累计工作量；
 * 需明确调用方禁止重放，或由账本识别并拒绝重复提交。
 */
ZR_CORE_API TZrBool ZrCore_GcBudget_LedgerAccumulate(
        SZrGcBudgetLedger *ledger,
        const SZrGcBudgetStepResult *result,
        SZrGcBudgetDiagnostic *diagnostic);

#ifdef __cplusplus
}
#endif

#endif /* ZR_VM_CORE_GC_BUDGET_CONTRACT_H */
