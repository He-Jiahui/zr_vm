/**
 * @file process.h
 * @brief 声明 `zr.system.process` 导出的宿主进程回调。
 * @details 回调由 process_registry.h 暴露的模块描述符注册；参数上下文与结果槽遵循原生分派器的
 *          借用生命周期，阻塞和进程终止副作用在各接口契约中说明。
 * @see process_registry.h; zr_vm_library/include/zr_vm_library/native_binding.h
 */

#ifndef ZR_VM_LIB_SYSTEM_PROCESS_H
#define ZR_VM_LIB_SYSTEM_PROCESS_H

#include "zr_vm_lib_system/conf.h"

/**
 * ## Summary
 * 作为 `zr.system.process.sleepMilliseconds` 的一元原生回调，按平台等待后将 VM 结果写为 null；
 * Windows/POSIX 在调用线程等待，Emscripten 则在配置异步转换后以同步 C 续行语义让出事件循环。
 *
 * ## Parameters
 * @param[in] context 原生分派器提供的借用调用上下文；注册描述符要求一个显式参数，回调用
 *                    ZrLib_CallContext_ReadInt 读取该值。上下文仅在本次回调期间有效，不得留存。
 * @param[out] result 分派器提供的借用结果槽；等待调用返回后写入 VM null，不转移或保存其所有权。
 *
 * ## Returns
 * @return context/result 为空时返回 ZR_FALSE。注册调用由分派器先检查恰好一个实参，数量不符时
 *         在进入回调前抛 VM 元数异常；进入回调后，ReadInt 对非数值抛 VM 类型异常。直接 C 调用
 *         绕过分派器时，缺参由 ReadInt 抛元数异常。可读的有符号、无符号或浮点数值等待后返回 ZR_TRUE；
 *         POSIX 信号中断或平台宽度转换可能改变实际时长，成功不保证完整等待。
 *
 * ## Constraints and lifecycle
 * 正常注册调用以一个可由 `ZrLib_CallContext_ReadInt` 接受的数值参数进入；直接 C 调用者仍需提供
 * 非空 context/result。负值在各平台分支按零处理。Windows/POSIX 等待阻塞当前 OS 线程；描述符未设置
 * blocking-detached 标志，分派器在回调期间保持 GC-aware native 域，因此长等待可能延后共享全局状态的
 * stop-the-world 暂停。TODO: 核对 Windows/POSIX 长等待是否应让出 GC 域。Emscripten 需以 Asyncify
 * 或 JSPI 配置才能提供伪同步 sleep；它会让出 event loop，挂起期间可处理事件并重入编译代码。TODO:
 * 核对实际 Emscripten 构建配置、重入安全，以及悬挂恢复前借用的 context/result 是否仍有效。context/result
 * 由本次分派借用，回调不释放、不跨回调保存；实现只另存整数标量且不分配 VM 对象。
 *
 * ## References
 * `ZrSystem_ProcessRegistry_GetModule` 将回调注册到 `zr.system.process`；实际调用经
 * `native_binding_auto_check_arity` 先验证数量，`native_binding_invoke_callback_inline` 派发后由
 * `ZrLib_CallContext_ReadInt` 读取数值，`ZrLib_Value_SetNull` 写入结果。
 */
TZrBool ZrSystem_Process_SleepMilliseconds(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 作为 `zr.system.process.exit` 的一元原生回调，读取脚本退出码并调用 C 运行库终止整个宿主进程；
 * 该能力由进程模块导出，因此副作用超出当前 VM 和当前调用线程。
 *
 * ## Parameters
 * @param[in] context 原生分派器提供的借用调用上下文；注册描述符要求一个显式数值参数，回调用
 *                    ZrLib_CallContext_ReadInt 读取并转换为 TZrInt64。
 * @param[in] result 分派器提供的结果槽指针；为匹配回调签名而保留，此函数不读取或写入该槽。
 *
 * ## Returns
 * @return context 为空时返回 ZR_FALSE。注册调用的参数数量不符由分派器在进入回调前抛元数异常；
 *         回调内非数值、以及绕过分派器的直接 C 调用缺参，由 ReadInt 抛 VM 参数异常。
 *         可读数值交给 C 运行库 `exit`，整个宿主进程终止且本函数不返回。
 *
 * ## Constraints and lifecycle
 * 正常注册调用要求一个可由 `ZrLib_CallContext_ReadInt` 读取的数值参数和非空 context；result 指针
 * 不解引用。context/result 仅由分派器在本次回调期间借用，回调不保存或释放它们。仅应向可信脚本
 * 开放此进程级副作用。TODO: 确认超过 C `int` 表示范围的 `TZrInt64` 退出码契约；当前实现直接
 * 窄化且未作范围检查。
 *
 * ## References
 * `ZrSystem_ProcessRegistry_GetModule` 注册此回调；执行路径经
 * `native_binding_auto_check_arity` 先验证数量；`native_binding_invoke_callback_inline` 派发后由
 * `ZrLib_CallContext_ReadInt` 读取参数，成功路径进入标准库 `exit`。
 */
TZrBool ZrSystem_Process_Exit(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_SYSTEM_PROCESS_H
