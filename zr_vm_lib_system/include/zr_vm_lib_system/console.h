//
// zr.system.console native callbacks.
//

#ifndef ZR_VM_LIB_SYSTEM_CONSOLE_H
#define ZR_VM_LIB_SYSTEM_CONSOLE_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 将第一个 VM 参数转换为字符串，经标准输出日志通道写出，不附加换行。
 *
 * ## Parameters
 * @param[in] context 有效且 state 有效的原生调用上下文；注册描述符要求一个实参。dispatcher 建立正常注册入口的 context；
 *                    对象与参数在 callback 期间借用，值转字符串可能调用脚本协议。
 * @param[out] result 可写的原生调用结果槽；分派器先置为 null，成功写出后仍为 VM null，不转移槽的所有权。
 *
 * ## Returns
 * @return ZR_TRUE 表示 callback 完成并提交了一次日志调用；不表示操作系统或自定义 sink 写入成功。
 *         无效 context/result、绕过注册分派的缺失参数或转换未给出字符串时返回 ZR_FALSE；VM `toString` 协议或其字符串分配
 *         可能抛出 VM 异常。正常注册调用的实参数量不符会先由 dispatcher 抛出元数异常，不会以 callback 的 ZR_FALSE 表示。
 *
 * ## Constraints and lifecycle
 * 值转换可执行用户定义的 `toString` 并重入 VM；异常发生时不会继续写日志。转换结果按显式字节长度校验 UTF-8，
 * 但日志 native sink 按 NUL 结尾文本消费，合法字符串中的嵌入 NUL 会截断输出。默认 stdout sink 写出后还会同步通知宿主
 * observer；observer 收到的消息仅在回调期间借用，需要保留时必须自行复制。回调不承诺持久化或检查任一 sink 的错误。
 *
 * ## References
 * `system_console_print` 执行转换、校验与 `ZrCore_Log_Write`；注册元数见 `ZrSystem_ConsoleRegistry_GetModule`。
 */
TZrBool ZrSystem_Console_Print(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将第一个 VM 参数转换为字符串，经标准输出日志通道写出并追加一个换行字节。
 *
 * ## Parameters
 * @param[in] context 有效且 state 有效的原生调用上下文；注册描述符要求一个实参。dispatcher 建立正常注册入口的 context；
 *                    对象与参数在 callback 期间借用，值转字符串可能调用脚本协议。
 * @param[out] result 可写的原生调用结果槽；分派器先置为 null，成功写出后仍为 VM null。
 *
 * ## Returns
 * @return ZR_TRUE 表示 callback 完成并提交了一次日志调用，不代表底层 sink 写入成功；防御性输入无效或转换未返回字符串时
 *         为 ZR_FALSE。正常注册调用的元数错误由 dispatcher 在 callback 前抛出，不是 ZR_FALSE；用户 `toString` 或字符串
 *         分配路径可能抛 VM 异常。
 *
 * ## Constraints and lifecycle
 * 转换可能执行用户代码并重入 VM。文本按显式长度验证 UTF-8，但格式化日志 sink 使用 NUL 结尾字符串，嵌入 NUL 会截断
 * 可见文本；换行追加在截断前缀之后。默认 stdout sink 写出后还会同步通知宿主 observer；observer 消息仅在回调期借用，
 * 需保留时自行复制，sink 错误不反映在结果槽中。
 *
 * ## References
 * `system_console_print` 以 `Log_Printf` 写入 stdout channel；模块 descriptor 将其注册为 `printLine`。
 */
TZrBool ZrSystem_Console_PrintLine(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将第一个 VM 参数转换为字符串，经标准错误诊断通道写出，不附加换行。
 *
 * ## Parameters
 * @param[in] context 有效且 state 有效的原生调用上下文；注册描述符要求一个实参。dispatcher 建立正常注册入口的 context；
 *                    对象与参数在 callback 期间借用，值转字符串可能调用脚本协议。
 * @param[out] result 可写的原生调用结果槽；分派器先置为 null，成功写出后仍为 VM null。
 *
 * ## Returns
 * @return ZR_TRUE 表示 callback 完成并提交了一次日志调用，不表示底层 sink 写入成功；无效输入或无字符串转换结果时
 *         返回 ZR_FALSE。正常注册调用的元数错误由 dispatcher 在 callback 前抛出，不是 ZR_FALSE；脚本协议或字符串分配失败
 *         也可能抛 VM 异常。
 *
 * ## Constraints and lifecycle
 * 转换可执行用户代码。UTF-8 按显式字节长度检查，日志 sink 却按 NUL 结尾字符串写出，因此嵌入 NUL 后的文本会被截断。
 * 默认 stderr sink 写出后还会同步通知宿主 observer，channel/kind 一并交付；observer 消息仅在同步调用期借用，需保留时自行复制。
 *
 * ## References
 * `system_console_print` 将此 callback 映射到 stderr channel 并调用 `ZrCore_Log_Write`；descriptor 名为 `printError`。
 */
TZrBool ZrSystem_Console_PrintError(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 将第一个 VM 参数转换为字符串，经标准错误诊断通道写出并追加一个换行字节。
 *
 * ## Parameters
 * @param[in] context 有效且 state 有效的原生调用上下文；注册描述符要求一个实参。dispatcher 建立正常注册入口的 context；
 *                    对象与参数在 callback 期间借用，值转字符串可能调用脚本协议。
 * @param[out] result 可写的原生调用结果槽；分派器先置为 null，成功写出后仍为 VM null。
 *
 * ## Returns
 * @return ZR_TRUE 表示 callback 完成并提交了一次日志调用，不表示底层 sink 写入成功；无效输入或无字符串转换结果时
 *         返回 ZR_FALSE。正常注册调用的元数错误由 dispatcher 在 callback 前抛出，不是 ZR_FALSE；脚本协议或字符串分配失败
 *         也可能抛 VM 异常。
 *
 * ## Constraints and lifecycle
 * 转换可执行用户代码。UTF-8 按显式字节长度检查，格式化日志却按 NUL 结尾字符串读取，嵌入 NUL 会截断文本，随后在
 * 可见前缀后追加换行。默认 stderr sink 写出后还会同步通知宿主 observer，channel/kind 一并交付；observer 消息仅在同步调用期
 * 借用，需保留时自行复制。
 *
 * ## References
 * `system_console_print` 将此 callback 映射到 stderr channel 并调用 `ZrCore_Log_Printf`；descriptor 名为 `printErrorLine`。
 */
TZrBool ZrSystem_Console_PrintErrorLine(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 从标准输入消费一个 UTF-8 码点并创建 VM 字符串；首字节处 EOF 返回 null。
 *
 * ## Parameters
 * @param[in] context 有效且 state 有效的原生调用上下文；注册描述符不接受实参，dispatcher 建立正常注册入口的 context；
 *                    上下文在 callback 期间借用。
 * @param[out] result 可写的原生调用结果槽；成功时接收 VM 管理的字符串或 null，调用者不释放字符串对象。
 *
 * ## Returns
 * @return ZR_TRUE 表示读到并交付码点，或在首字节 EOF 时正常交付 null；无效 context/result 返回 ZR_FALSE。非法、截断的
 *         UTF-8 序列由 `ZrCore_Debug_RunError` 抛 VM 异常；字符串硬分配失败也可能抛异常，短串驻留表扩容的 raw OOM
 *         则可软失败为 null 并仍返回 ZR_TRUE，见实现 BUG。注册调用的额外实参由 dispatcher 在 callback 前抛元数异常。
 *
 * ## Constraints and lifecycle
 * 调用先刷新默认 stdout/stderr sinks，不刷新自定义 log observer；随后对 `stdin` 执行可能无限等待的 `fgetc`。读取会
 * 消费原始字节且无回滚；UTF-8 首字节 helper 只给出候选长度，后续解码器才判定完整合法性。EOF 与流错误均表现为 EOF，
 * 首字节处返回 null；中途 EOF/错误导致已消费前缀后抛异常。EOF 与 ferror 当前共用此路径，具体 I/O 错误策略待定。
 * 多个 VM state 可并发读共享 stdin；本 callback 不锁住一个完整
 * 码点，跨线程读取可按字节交错，调用方如需码点原子性须自行串行化。
 *
 * ## References
 * `system_console_utf8_expected_length` 只估计长度，`ZrCore_Utf8_DecodeCodePoint` 负责严格解码；注册元数由模块 descriptor 固定为零。
 */
TZrBool ZrSystem_Console_Read(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 从标准输入读取到 LF 或 EOF，验证 UTF-8 并交付一条 VM 字符串；行首 EOF 返回 null。
 *
 * ## Parameters
 * @param[in] context 有效且 state 有效的原生调用上下文；注册描述符不接受实参，dispatcher 建立正常注册入口的 context；
 *                    上下文在 callback 期间借用。
 * @param[out] result 可写的原生调用结果槽；成功时接收 VM 管理的字符串或 null，调用者不释放字符串对象。
 *
 * ## Returns
 * @return ZR_TRUE 表示一行或行首 EOF 已交付；无效 context/result 返回 ZR_FALSE。UTF-8 错误、realloc 失败或底层 VM 字符串
 *         的硬分配失败以 VM 异常报告；短字符串驻留表扩容的特定 soft-OOM 路径可返回成功 null，见实现 BUG。注册调用的
 *         额外实参由 dispatcher 在 callback 前抛元数异常。
 *
 * ## Constraints and lifecycle
 * 调用只刷新默认 stdout/stderr sinks，然后可能无限等待共享 `stdin`。没有整行互斥，多个 VM state 的逐字节读取可能交错；
 * 调用方如需行原子性须自行串行化。LF 被移除，任意 EOF/LF 结束行时末尾一个 CR 都会移除；因此孤立 CR 在 EOF 前也被裁掉。
 * 空行返回空字符串，未读到任何字节的 EOF 返回 null；流错误按 EOF 处理，部分行可能照常返回。TODO: 明确并测试 ferror
 * 与末尾孤立 CR 的预期行策略。realloc 行缓冲在正常路径
 * 复制到 VM 字符串后释放；但已分配行缓冲时的 String_Create 硬分配异常会非局部跳转并跳过 free，具体 BUG 见实现。无输入长度上限。
 *
 * ## References
 * `ZrCore_Utf8_IsValid` 对显式行长校验；`ZrCore_String_Create` 创建 VM 所有的结果；注册元数由模块 descriptor 固定为零。
 */
TZrBool ZrSystem_Console_ReadLine(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_CONSOLE_H
