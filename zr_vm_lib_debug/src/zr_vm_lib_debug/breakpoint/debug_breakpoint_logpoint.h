#ifndef ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H
#define ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H

#include "debug_internal.h"

/**
 * @brief 将断点 logMessage 的文本和 `{expression}` 拼成单条输出事件。
 *
 * 插值沿用条件断点的只读表达式能力；每个插值失败会转成错误文本，
 * 格式化本身仍可成功。输出缓冲区由调用方提供且可能截断。
 */
ZR_DEBUG_API TZrBool zr_debug_breakpoint_logpoint_format(ZrDebugAgent *agent,
                                                         const TZrChar *logMessage,
                                                         TZrChar *outText,
                                                         TZrSize outTextSize);

#endif // ZR_VM_DEBUG_BREAKPOINT_LOGPOINT_H

/**
 * @fn TZrBool zr_debug_breakpoint_logpoint_format(ZrDebugAgent *agent, const TZrChar *logMessage, TZrChar *outText, TZrSize outTextSize)
 * ## Summary
 * 为命中路径生成 console 事件所需的 logpoint 文本，保留普通插值失败后的继续处理；事件发送和断点是否停止由外层决定。
 *
 * ## Parameters
 * @param[in,out] agent 借用非 NULL agent；插值借用其当前 state/首帧事实，可能产生临时准备分配。
 * @param[in] logMessage 借用非 NULL、NUL 结尾模板，调用期间稳定且不得与 outText 重叠。
 * @param[out] outText 非 NULL、正容量缓冲；有效时先清空，再接收可能截断且 NUL 终止的文本。
 * @param[in] outTextSize outText 的可写字节容量，必须大于零。
 *
 * ## Returns
 * @return 假为本层参数无效；真只表示格式化流程普通完成，包含插值错误或截断时仍可为真，不证明事件已发送或末尾换行完整。
 *
 * ## Constraints and lifecycle
 * 模板只配对首个 { 与后续首个 }，不识别嵌套/转义；未闭合后缀作为字面文本。临时片段/表达式及总输出都受字节容量限制；满缓冲也不跳过后续求值。插值 frameId=1、零 capability、禁用兼容；普通失败变成 <error:...> 并继续，非局部异常不受此保证。本层不保留、释放调用方缓冲或 agent，不新增并发、重入或整体回滚保证。
 *
 * ## References
 * - zr_debug_breakpoint_logpoint_format — zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_logpoint.c:23 对应实现及截断规则。
 * - zr_debug_breakpoint_emit_logpoint — zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1358 真正的事件发送调用方。
 * - test_debug_breakpoint_logpoint_requires_pure_formal_evaluation — tests/debug/test_debug_breakpoint_logpoint_cases.h:12 现有测试入口。
 */
