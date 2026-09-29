#include "zr_vm_core/gc_compact.h"

#include <limits.h>
#include <string.h>

/**
 * @brief 将当前拒绝原因写入可选输出，供各入口返回可消费的失败分类。
 * @note diagnostic 可为空；非空时须是可写且不与输入输出别名的记录。
 */
static void gc_compact_diag(SZrGcCompactDiagnostic *diagnostic,
                            EZrGcCompactDiagnosticCode code,
                            TZrUInt32 index,
                            TZrUInt64 expected,
                            TZrUInt64 actual) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->code = code;
    diagnostic->index = index;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
}

/**
 * @brief 汇总跨区域计数时饱和而非回绕，避免大堆统计被误判为小值。
 * @note 饱和值表示上限，不再保留精确总量。
 */
static TZrUInt64 gc_compact_sat_add(TZrUInt64 left, TZrUInt64 right) {
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

/**
 * @brief 以整数上取整比较单个旧区的可回收比例，避免浮点边界差异。
 * @note 空区不入选；零阈值也要求存在可回收字节，防止规划零收益区。
 */
static TZrBool gc_compact_threshold_met(TZrUInt64 reclaimable,
                                         TZrUInt64 used,
                                         TZrUInt32 thresholdPercent) {
    TZrUInt64 whole;
    TZrUInt64 remainder;
    TZrUInt64 required;
    TZrUInt64 extra;

    if (used == 0u) {
        return ZR_FALSE;
    }
    if (thresholdPercent == 0u) {
        return reclaimable != 0u;
    }
    whole = used / 100u;
    remainder = used % 100u;
    required = whole * (TZrUInt64)thresholdPercent;
    extra = (remainder * (TZrUInt64)thresholdPercent + 99u) / 100u;
    if (required > UINT64_MAX - extra) {
        return ZR_TRUE;
    }
    required += extra;
    return reclaimable >= required;
}

/**
 * @brief 清除复用诊断输出中的旧状态，供多个拒绝入口共用。
 * @note diagnostic 可为空。
 */
void ZrCore_GcCompact_DiagnosticClear(
        SZrGcCompactDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
    }
}

/**
 * @brief 把诊断码转换为宿主可展示的稳定文本。
 * @return 静态字符串；未知扩展码统一回退为 "unknown"。
 */
const TZrChar *ZrCore_GcCompact_DiagnosticName(
        EZrGcCompactDiagnosticCode code) {
    switch (code) {
        case ZR_GC_COMPACT_DIAGNOSTIC_NONE:
            return "none";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT:
            return "invalid-argument";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_MAGIC:
            return "invalid-magic";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_SCHEMA:
            return "invalid-schema";
        case ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION:
            return "invalid-region";
        case ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW:
            return "overflow";
        case ZR_GC_COMPACT_DIAGNOSTIC_BUDGET:
            return "budget";
        default:
            return "unknown";
    }
}

/**
 * @brief 初始化带当前契约头的空计划，作为输出和失败时的安全基线。
 * @note plan 可为空；初始化结果不代表计划已准入或对象已搬迁。
 */
void ZrCore_GcCompact_Init(SZrGcCompactPlan *plan) {
    if (plan == ZR_NULL) {
        return;
    }
    memset(plan, 0, sizeof(*plan));
    plan->magic = ZR_GC_COMPACT_CONTRACT_MAGIC;
    plan->schemaVersion = ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION;
    plan->mode = ZR_GC_COMPACT_MODE_NON_MOVING;
}

/**
 * @brief 校验可交换计划记录的头部和本函数覆盖的基础计数关系。
 * @return plan 为空时拒绝；diagnostic 可为空。本校验不重读区域事实、不执行搬迁。
 */
TZrBool ZrCore_GcCompact_Validate(
        const SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic) {
    ZrCore_GcCompact_DiagnosticClear(diagnostic);
    if (plan == ZR_NULL) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
                        0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (plan->magic != ZR_GC_COMPACT_CONTRACT_MAGIC) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_MAGIC,
                        1u, ZR_GC_COMPACT_CONTRACT_MAGIC, plan->magic);
        return ZR_FALSE;
    }
    if (plan->schemaVersion != ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_SCHEMA,
                        2u, ZR_GC_COMPACT_CONTRACT_SCHEMA_VERSION,
                        plan->schemaVersion);
        return ZR_FALSE;
    }
    /* BUG: 有符号 enum 实现上，伪造的负 mode 不满足 >= COUNT，可能被误接受。 */
    /* BUG: 单独检查两类计数不超过 regionCount，未验证互斥分区的计数和。 */
    if (plan->mode >= ZR_GC_COMPACT_MODE_COUNT ||
        plan->candidateRegionCount > plan->regionCount ||
        plan->pinnedRegionCount > plan->regionCount ||
        plan->plannedBytes > plan->movableBytes ||
        (plan->deferred && plan->eligible == ZR_FALSE &&
         plan->candidateRegionCount == 0u)) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION,
                        3u, plan->regionCount, plan->candidateRegionCount);
        return ZR_FALSE;
    }
    if (plan->mode == ZR_GC_COMPACT_MODE_NON_MOVING &&
        plan->plannedBytes != 0u) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_BUDGET,
                        4u, 0u, plan->plannedBytes);
        return ZR_FALSE;
    }
    /* TODO: 确认 Validate 是否还应拒绝无法由 Plan 生成的 eligible/mode 组合。 */
    return ZR_TRUE;
}

/**
 * @brief 从稳定的只读区域快照生成纯标量压缩准入计划。
 * @pre request 和 regions 非空时，regions 至少覆盖 regionCount 个描述符且在调用期间保持稳定。
 * @return request/plan 为空、regions 缺失或请求数据无效时返回 false；有效计划返回 true。
 * @note 阈值筛选旧区；预算按输入顺序整区接纳 liveBytes，零预算表示不设上限。
 *       该函数不搬迁对象、不改写登记表，也不重写对象引用。
 */
TZrBool ZrCore_GcCompact_Plan(
        const SZrGcCompactRequest *request,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic) {
    SZrGcCompactPlan candidate;
    TZrSize index;
    TZrUInt64 plannedRegionBytes = 0u;
    TZrUInt32 plannedRegionCount = 0u;
    TZrBool budgetLimited = ZR_FALSE;

    ZrCore_GcCompact_DiagnosticClear(diagnostic);
    if (plan != ZR_NULL) {
        ZrCore_GcCompact_Init(plan);
    }
    /* 先验证借用输入的边界；拒绝时公开输出保持初始化基线而非半份候选。 */
    if (request == ZR_NULL || plan == ZR_NULL ||
        (request->regionCount != 0u && request->regions == ZR_NULL) ||
        request->fragmentationThresholdPercent > 100u) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
                        0u, 1u, request != ZR_NULL
                                       ? request->fragmentationThresholdPercent
                                       : 0u);
        return ZR_FALSE;
    }
    if (request->regionCount > (TZrSize)UINT32_MAX) {
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW,
                        0u, UINT32_MAX, request->regionCount);
        return ZR_FALSE;
    }
    /* 只有完整通过后才将局部累计值发布到调用方计划对象。 */
    ZrCore_GcCompact_Init(&candidate);
    candidate.regionCount = (TZrUInt32)request->regionCount;
    candidate.mode = request->allowMoving
                             ? ZR_GC_COMPACT_MODE_SELECTIVE_MOVING
                             : ZR_GC_COMPACT_MODE_NON_MOVING;

    /* 先验证快照事实，再按可移动候选、固定排除区和其他代际分组统计。 */
    for (index = 0u; index < request->regionCount; ++index) {
        const SZrGarbageCollectRegionDescriptor *region =
                &request->regions[index];
        TZrUInt64 reclaimable;

        if (region->kind <= ZR_GARBAGE_COLLECT_REGION_KIND_INVALID ||
            region->kind >= ZR_GARBAGE_COLLECT_REGION_KIND_MAX ||
            region->usedBytes > region->capacityBytes ||
            region->liveBytes > region->usedBytes) {
            gc_compact_diag(diagnostic,
                            ZR_GC_COMPACT_DIAGNOSTIC_INVALID_REGION,
                            (TZrUInt32)index, region->capacityBytes,
                            region->usedBytes);
            return ZR_FALSE;
        }
        if (region->kind == ZR_GARBAGE_COLLECT_REGION_KIND_PINNED ||
            region->kind == ZR_GARBAGE_COLLECT_REGION_KIND_LARGE ||
            region->kind == ZR_GARBAGE_COLLECT_REGION_KIND_PERMANENT) {
            ++candidate.pinnedRegionCount;
            candidate.pinnedBytes = gc_compact_sat_add(
                    candidate.pinnedBytes, region->usedBytes);
            continue;
        }
        if (region->kind != ZR_GARBAGE_COLLECT_REGION_KIND_OLD) {
            continue;
        }
        reclaimable = region->usedBytes - region->liveBytes;
        if (!gc_compact_threshold_met(
                    reclaimable, region->usedBytes,
                    request->fragmentationThresholdPercent)) {
            continue;
        }
        ++candidate.candidateRegionCount;
        candidate.candidateBytes = gc_compact_sat_add(
                candidate.candidateBytes, reclaimable);
        candidate.movableBytes = gc_compact_sat_add(
                candidate.movableBytes, region->liveBytes);
        candidate.estimatedWorkUnits = gc_compact_sat_add(
                candidate.estimatedWorkUnits, region->liveBytes);

        /* 候选统计涵盖所有过阈值旧区；许可与预算只决定整区是否进入计划量。 */
        if (!request->allowMoving) {
            continue;
        }
        if (request->budgetBytes != 0u &&
            region->liveBytes > request->budgetBytes -
                                (plannedRegionBytes <= request->budgetBytes
                                         ? plannedRegionBytes
                                         : request->budgetBytes)) {
            budgetLimited = ZR_TRUE;
            continue;
        }
        plannedRegionBytes = gc_compact_sat_add(
                plannedRegionBytes, region->liveBytes);
        ++plannedRegionCount;
    }

    if (!request->allowMoving) {
        candidate.deferred = candidate.candidateRegionCount != 0u;
    } else {
        candidate.plannedBytes = plannedRegionBytes;
        candidate.eligible = plannedRegionCount != 0u;
        candidate.deferred = budgetLimited ||
                             plannedRegionCount < candidate.candidateRegionCount;
        if (request->budgetBytes != 0u &&
            candidate.plannedBytes > request->budgetBytes) {
            gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_OVERFLOW,
                            0u, request->budgetBytes,
                            candidate.plannedBytes);
            return ZR_FALSE;
        }
    }
    if (candidate.eligible) {
        candidate.estimatedWorkUnits = candidate.plannedBytes;
    }
    *plan = candidate;
    return ZR_TRUE;
}

/**
 * @brief 从 global 当前登记表装配只读请求，再委托纯规划器。
 * @pre global/collector 非空时，其 regions 指针、长度及数组在读取期间须保持稳定。
 * @return global 或 collector 为空时返回 false，plan 保持原值；否则返回规划器结果。
 * TODO: 仓内无调用点能证明安全点或快照保护，需明确登记表并发修改时的同步契约。
 * TODO: 此包装器在 global 无效时不会像 Plan 一样初始化 plan，需明确失败输出是否须清空。
 */
TZrBool ZrCore_GarbageCollector_PlanCompaction(
        struct SZrGlobalState *global,
        TZrUInt64 budgetBytes,
        TZrUInt32 fragmentationThresholdPercent,
        TZrBool allowMoving,
        SZrGcCompactPlan *plan,
        SZrGcCompactDiagnostic *diagnostic) {
    SZrGcCompactRequest request;

    if (global == ZR_NULL || global->garbageCollector == ZR_NULL) {
        ZrCore_GcCompact_DiagnosticClear(diagnostic);
        gc_compact_diag(diagnostic, ZR_GC_COMPACT_DIAGNOSTIC_INVALID_ARGUMENT,
                        0u, 1u, 0u);
        return ZR_FALSE;
    }
    request.regions = global->garbageCollector->regions;
    request.regionCount = global->garbageCollector->regionCount;
    request.budgetBytes = budgetBytes;
    request.fragmentationThresholdPercent = fragmentationThresholdPercent;
    request.allowMoving = allowMoving;
    return ZrCore_GcCompact_Plan(&request, plan, diagnostic);
}
