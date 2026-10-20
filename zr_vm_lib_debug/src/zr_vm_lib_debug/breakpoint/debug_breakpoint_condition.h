#ifndef ZR_VM_DEBUG_BREAKPOINT_CONDITION_H
#define ZR_VM_DEBUG_BREAKPOINT_CONDITION_H

#include "debug_internal.h"

/**
 * @brief 在当前 trace 位置的首帧求值断点条件，给 hook 的命中判断返回布尔结果。
 *
 * 条件为空视为命中；求值只允许无副作用表达式，失败由调用方决定如何
 * 报告并跳过该断点。当前内部约定使用主帧 ID 1。
 */
ZR_DEBUG_API TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent,
                                                            const TZrChar *condition,
                                                            TZrBool *outSatisfied,
                                                            TZrChar *errorBuffer,
                                                            TZrSize errorBufferSize);

#endif // ZR_VM_DEBUG_BREAKPOINT_CONDITION_H

/**
 * @fn TZrBool zr_debug_breakpoint_condition_evaluate(ZrDebugAgent *agent, const TZrChar *condition, TZrBool *outSatisfied, TZrChar *errorBuffer, TZrSize errorBufferSize)
 * ## Summary
 * 为断点命中判定提供正式、零 capability 的条件入口；调用方须分别处理执行成功与条件真值，而不把假条件当作错误。
 *
 * ## Parameters
 * @param[in,out] agent 借用非 NULL agent；引用求值所需当前 state/首帧事实由调用者保持，正式准备可能分配临时 VM/编译器数据。
 * @param[in] condition 借用 NUL 结尾文本；NULL/空串在有效请求中表示无条件。
 * @param[out] outSatisfied 非 NULL 输出；入口先写假，成功时写空条件或表达式最终真值。
 * @param[out] errorBuffer 可为 NULL；仅非 NULL 且容量大于零时交付以 NUL 终止、可能截断的诊断。
 * @param[in] errorBufferSize 诊断缓冲可写字节数；零表示无可写空间。
 *
 * ## Returns
 * @return 真是求值成功而非必定命中；假为请求/正式求值失败，有效 outSatisfied 保持假。上层失败时报告诊断并跳过断点。
 *
 * ## Constraints and lifecycle
 * 固定首帧编号1，不授 getter/调用/分配等 capability，不退回 legacy parser；这不等于解析准备零分配。接口不取得 agent、文本或缓冲的所有权，不添加线程同步或本地异常恢复；非局部 Throw 可越过普通返回和清理，不承诺事务回滚。纯 literal 算式可不依赖暂停帧元数据。
 *
 * ## References
 * - zr_debug_breakpoint_condition_evaluate — zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_condition.c:34 对应实现。
 * - zr_debug_breakpoint_condition_satisfied — zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1341 真实命中调用方。
 * - test_debug_breakpoint_condition_requires_pure_formal_evaluation — tests/debug/test_debug_breakpoint_condition_cases.h:14 现有测试入口。
 */
