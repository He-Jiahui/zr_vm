#ifndef ZR_TESTS_SSA_HOTPATCH_FAULT_CASES_H
#define ZR_TESTS_SSA_HOTPATCH_FAULT_CASES_H

#include "zr_vm_core/hotpatch_generation.h"

/* TODO: 故障注入词汇尚无实际包含者；rollback/restricted 测试目前只测正常和少量
 * 拒绝路径，需从测试入口接入失败点矩阵后才能声称覆盖准备、发布和撤销故障。 */
typedef enum EZrSsaHotPatchFaultPoint {
    ZR_SSA_HOTPATCH_FAULT_SIGNATURE = 0,
    ZR_SSA_HOTPATCH_FAULT_CONTENT,
    ZR_SSA_HOTPATCH_FAULT_PREPARE,
    ZR_SSA_HOTPATCH_FAULT_PUBLISH,
    ZR_SSA_HOTPATCH_FAULT_RETIRE,
    ZR_SSA_HOTPATCH_FAULT_CANCEL,
    ZR_SSA_HOTPATCH_FAULT_OOM,
    ZR_SSA_HOTPATCH_FAULT_STALE,
    ZR_SSA_HOTPATCH_FAULT_POINT_COUNT
} EZrSsaHotPatchFaultPoint;

typedef struct SZrSsaHotPatchFaultCase {
    EZrSsaHotPatchFaultPoint point;
    TZrBool preservesActiveGeneration;
    TZrBool requiresRetry;
} SZrSsaHotPatchFaultCase;

/* 短别名供计划中的 rollback 夹具使用；目前仍无消费者，不能视作已运行测试。 */
typedef EZrSsaHotPatchFaultPoint EZrHotPatchFaultPoint;
typedef SZrSsaHotPatchFaultCase SZrHotPatchFaultCase;
#define ZR_HOT_PATCH_FAULT_SIGNATURE ZR_SSA_HOTPATCH_FAULT_SIGNATURE
#define ZR_HOT_PATCH_FAULT_CONTENT ZR_SSA_HOTPATCH_FAULT_CONTENT
#define ZR_HOT_PATCH_FAULT_PREPARE ZR_SSA_HOTPATCH_FAULT_PREPARE
#define ZR_HOT_PATCH_FAULT_PUBLISH ZR_SSA_HOTPATCH_FAULT_PUBLISH
#define ZR_HOT_PATCH_FAULT_RETIRE ZR_SSA_HOTPATCH_FAULT_RETIRE
#define ZR_HOT_PATCH_FAULT_CANCEL ZR_SSA_HOTPATCH_FAULT_CANCEL
#define ZR_HOT_PATCH_FAULT_OOM ZR_SSA_HOTPATCH_FAULT_OOM
#define ZR_HOT_PATCH_FAULT_STALE ZR_SSA_HOTPATCH_FAULT_STALE

/* 供未来参数化测试输出失败点名称；无调用时不会改变运行时 ABI。 */
static inline const TZrChar *ZrTests_SsaHotPatchFaultPointName(
        EZrSsaHotPatchFaultPoint point) {
    switch (point) {
        case ZR_SSA_HOTPATCH_FAULT_SIGNATURE: return "signature";
        case ZR_SSA_HOTPATCH_FAULT_CONTENT: return "content";
        case ZR_SSA_HOTPATCH_FAULT_PREPARE: return "prepare";
        case ZR_SSA_HOTPATCH_FAULT_PUBLISH: return "publish";
        case ZR_SSA_HOTPATCH_FAULT_RETIRE: return "retire";
        case ZR_SSA_HOTPATCH_FAULT_CANCEL: return "cancel";
        case ZR_SSA_HOTPATCH_FAULT_OOM: return "oom";
        case ZR_SSA_HOTPATCH_FAULT_STALE: return "stale";
        default: return "unknown";
    }
}

#endif
