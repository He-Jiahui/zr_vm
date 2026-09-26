#ifndef ZR_VM_DEBUG_COVERAGE_H
#define ZR_VM_DEBUG_COVERAGE_H

#include "zr_vm_lib_debug/conf.h"
#include "zr_vm_core/debug.h"

#define ZR_DEBUG_COVERAGE_NAME_CAPACITY ZR_DEBUG_NAME_CAPACITY
#define ZR_DEBUG_COVERAGE_SOURCE_CAPACITY ZR_DEBUG_TEXT_CAPACITY

/**
 * @brief Coverage 的一条函数/源行聚合结果。
 *
 * `function` 是借用的 VM 函数身份，必须由 VM 保持存活；字符串字段是
 * 覆盖率对象持有的快照，调用方不能把行指针带过 Destroy。
 */
typedef struct ZrDebugCoverageLine {
    const struct SZrFunction *function;
    TZrChar name[ZR_DEBUG_COVERAGE_NAME_CAPACITY];
    TZrChar source[ZR_DEBUG_COVERAGE_SOURCE_CAPACITY];
    TZrUInt32 line;
    TZrBool executable;
    TZrBool executed;
} ZrDebugCoverageLine;

/**
 * @brief 挂在一个 VM state 上的覆盖率会话。
 *
 * 一个 state 同时只能有一个活动 Coverage；Start 会接管 line hook 并在
 * Stop 时恢复原 hook/mask/count。对象本身由调用方分配，Init 只初始化，
 * Destroy 才释放内部数组。
 */
typedef struct ZrDebugCoverage {
    struct SZrState *state;
    FZrDebugHook previous_hook;
    TZrUInt32 previous_mask;
    TZrUInt32 previous_count;
    TZrBool active;
    ZrDebugCoverageLine *lines;
    TZrSize line_count;
    TZrSize line_capacity;
    struct ZrDebugCoverage *next_active;
} ZrDebugCoverage;

/** @brief 仅初始化未初始化的调用方对象；复用已有对象须先 Destroy。 */
ZR_DEBUG_API void ZrDebug_Coverage_Init(ZrDebugCoverage *coverage);
/** @brief 保留已分配容量，仅清除已收集的行结果；活动会话应先 Stop。 */
ZR_DEBUG_API void ZrDebug_Coverage_Reset(ZrDebugCoverage *coverage);
/** @brief 注册单个函数的可执行源行，供后续 hook 把未执行行也导出。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunction(ZrDebugCoverage *coverage, const struct SZrFunction *function);
/** @brief 递归注册入口函数及其嵌套函数树；失败时结果可能已部分注册。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_RegisterFunctionTree(ZrDebugCoverage *coverage,
                                                           const struct SZrFunction *function);
/** @brief 在 state 上安装覆盖率 hook；同一 state 不能重复 Start。 */
ZR_DEBUG_API TZrBool ZrDebug_Coverage_Start(ZrDebugCoverage *coverage, struct SZrState *state);
/** @brief 停止采集并恢复 Start 前保存的 VM hook 配置。 */
ZR_DEBUG_API void ZrDebug_Coverage_Stop(ZrDebugCoverage *coverage);
/** @brief 停止活动会话并释放内部行数组，之后对象可重新 Init。 */
ZR_DEBUG_API void ZrDebug_Coverage_Destroy(ZrDebugCoverage *coverage);
/** @brief 返回当前快照中的行数；空指针返回 0。 */
ZR_DEBUG_API TZrSize ZrDebug_Coverage_GetLineCount(const ZrDebugCoverage *coverage);
/** @brief 借用行结果；后续注册/采集扩容、Reset 或 Destroy 会使指针失效。 */
ZR_DEBUG_API const ZrDebugCoverageLine *ZrDebug_Coverage_GetLine(const ZrDebugCoverage *coverage, TZrSize index);

#endif
