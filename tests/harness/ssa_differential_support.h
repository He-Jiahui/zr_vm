#ifndef ZR_VM_TESTS_SSA_DIFFERENTIAL_SUPPORT_H
#define ZR_VM_TESTS_SSA_DIFFERENTIAL_SUPPORT_H

#include "zr_vm_common/zr_common_conf.h"

#define ZR_SSA_MAX_EVENTS 128u

/** @brief SSA 差分轨迹中需要逐项对齐的可观察副作用类别。 */
typedef enum EZrSsaEventKind {
    ZR_SSA_EVENT_INVALID = 0,
    ZR_SSA_EVENT_GET,
    ZR_SSA_EVENT_WRITE,
    ZR_SSA_EVENT_WRITEBACK,
    ZR_SSA_EVENT_DROP,
    ZR_SSA_EVENT_ALLOCATE,
    ZR_SSA_EVENT_SUSPEND,
    ZR_SSA_EVENT_RESUME,
    ZR_SSA_EVENT_THROW,
    ZR_SSA_EVENT_RETURN,
    ZR_SSA_EVENT_CALL,
    ZR_SSA_EVENT_BARRIER,
    ZR_SSA_EVENT_COUNT
} EZrSsaEventKind;

/** @brief 结果位表示所对应的值类型，避免仅比较位模式造成误判。 */
typedef enum EZrSsaResultType {
    ZR_SSA_RESULT_NONE = 0,
    ZR_SSA_RESULT_INTEGER,
    ZR_SSA_RESULT_FLOAT,
    ZR_SSA_RESULT_BOOLEAN,
    ZR_SSA_RESULT_REFERENCE,
    ZR_SSA_RESULT_UNIT
} EZrSsaResultType;

/** @brief 单次副作用的类别、源位置及比较用负载。 */
typedef struct SZrSsaEvent {
    EZrSsaEventKind kind;
    TZrUInt32 sourceId;
    TZrUInt64 valueBits;
    TZrUInt64 auxiliary;
} SZrSsaEvent;

/** @brief 一个 backend 的执行结果和轨迹；eventCount 不得超过 ZR_SSA_MAX_EVENTS。 */
typedef struct SZrSsaObservation {
    TZrUInt32 backend;
    TZrBool completed;
    TZrBool hasException;
    TZrBool fallbackVisible;
    TZrBool nativeCoverageAvailable;
    EZrSsaResultType resultType;
    TZrUInt64 resultBits;
    TZrUInt64 resultAuxiliary;
    TZrUInt32 exceptionType;
    TZrUInt32 exceptionSourceId;
    TZrUInt64 droppedCount;
    TZrUInt64 writebackCount;
    double nativeCoverage;
    TZrUInt32 eventCount;
    SZrSsaEvent events[ZR_SSA_MAX_EVENTS];
} SZrSsaObservation;

/** @brief 比较、执行或覆盖门禁的失败分类。 */
typedef enum EZrSsaDiffReason {
    ZR_SSA_DIFF_NONE = 0,
    ZR_SSA_DIFF_INVALID_ARGUMENT,
    ZR_SSA_DIFF_MALFORMED_OBSERVATION,
    ZR_SSA_DIFF_RESULT_MISMATCH,
    ZR_SSA_DIFF_EXCEPTION_MISMATCH,
    ZR_SSA_DIFF_EFFECT_MISMATCH,
    ZR_SSA_DIFF_EVENT_MISMATCH,
    ZR_SSA_DIFF_BACKEND_UNSUPPORTED,
    ZR_SSA_DIFF_COVERAGE_INCOMPLETE
} EZrSsaDiffReason;

/** @brief 第一个差异的位置及两侧事件，供测试输出定位。 */
typedef struct SZrSsaDiffDiagnostic {
    EZrSsaDiffReason reason;
    TZrUInt32 eventIndex;
    EZrSsaEventKind expectedEvent;
    EZrSsaEventKind actualEvent;
    TZrUInt32 expectedSourceId;
    TZrUInt32 actualSourceId;
} SZrSsaDiffDiagnostic;

struct SZrSsaFixture;
/** @brief fixture runner 填写调用方提供的 observation，不转移其所有权。 */
typedef TZrBool (*FZrSsaFixtureRunner)(const struct SZrSsaFixture *fixture,
                                       TZrUInt32 backend,
                                       SZrSsaObservation *observation);

/** @brief 差分场景及其必须执行的 backend 位集。 */
typedef struct SZrSsaFixture {
    const TZrChar *name;
    TZrUInt32 requiredBackends;
    FZrSsaFixtureRunner runner;
} SZrSsaFixture;

/** @brief 记录已执行和失败的 backend 位集；完成条件只检查 requiredBackends。 */
typedef struct SZrSsaCoverage {
    TZrUInt32 requiredBackends;
    TZrUInt32 executedBackends;
    TZrUInt32 failedBackends;
} SZrSsaCoverage;

/** @brief 重置调用方持有的 observation，供 runner 填写。 */
void ZrTests_Ssa_ObservationInit(SZrSsaObservation *observation);
/** @brief 追加一个有效事件；容量耗尽时返回 false，原计数不变。 */
TZrBool ZrTests_Ssa_ObservationAppendEvent(SZrSsaObservation *observation,
                                            EZrSsaEventKind kind,
                                            TZrUInt32 sourceId,
                                            TZrUInt64 valueBits,
                                            TZrUInt64 auxiliary);
/** @brief 验证观察值的基础范围及事件类别。 */
TZrBool ZrTests_Ssa_ObservationValidate(const SZrSsaObservation *observation);
/** @brief 比较结果、异常、副作用计数及事件顺序，忽略 backend 覆盖元数据。 */
TZrBool ZrTests_Ssa_Compare(const SZrSsaObservation *expected,
                            const SZrSsaObservation *actual,
                            SZrSsaDiffDiagnostic *diagnostic);
/** @brief 运行指定 backend 的 fixture；失败时填写可选 diagnostic。 */
TZrBool ZrTests_Ssa_RunFixture(const SZrSsaFixture *fixture,
                               TZrUInt32 backend,
                               SZrSsaObservation *observation,
                               SZrSsaDiffDiagnostic *diagnostic);
/** @brief 初始化按位记录的 backend 覆盖门禁。 */
void ZrTests_Ssa_CoverageInit(SZrSsaCoverage *coverage, TZrUInt32 requiredBackends);
/** @brief 记录语义结果及实际 backend，fallback 不满足覆盖门禁。 */
void ZrTests_Ssa_CoverageRecord(SZrSsaCoverage *coverage,
                                TZrUInt32 backend,
                                const SZrSsaObservation *observation,
                                TZrBool semanticMatches);
/** @brief 所有必需 backend 均执行且没有门禁失败时返回 true。 */
TZrBool ZrTests_Ssa_CoverageComplete(const SZrSsaCoverage *coverage);
/** @brief 返回稳定的诊断类别名，未知枚举值映射到 UNKNOWN。 */
const TZrChar *ZrTests_Ssa_DiffReasonName(EZrSsaDiffReason reason);

#endif
