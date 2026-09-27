#ifndef ZR_VM_TESTS_RUNTIME_SUPPORT_H
#define ZR_VM_TESTS_RUNTIME_SUPPORT_H

#include <stdio.h>
#include "zr_test_log_macros.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

/** @brief VM 测试默认分配器，遵循 global allocator 的分配、重分配和释放约定。 */
TZrPtr ZrTests_Runtime_Allocator_Default(TZrPtr userData,
                                         TZrPtr pointer,
                                         TZrSize originalSize,
                                         TZrSize newSize,
                                         TZrInt64 flag);

/** @brief fatal panic 后供 Unity crash guard 截断当前测试的进程级回调。 */
typedef void (*FZrTestsRuntimeFatalCrashHook)(SZrState *state);

/** @brief 创建并初始化独立 VM；成功返回的 state 由 State_Destroy 释放。 */
SZrState *ZrTests_Runtime_State_Create(FZrPanicHandlingFunction panicHandler);

/** @brief 移除 panic 注册和 crash scope 后释放 state 所属 global。 */
void ZrTests_Runtime_State_Destroy(SZrState *state);

/** @brief 设置进程级 fatal hook；调用方负责控制安装时序。 */
void ZrTests_Runtime_SetFatalCrashHook(FZrTestsRuntimeFatalCrashHook hook);

/** @brief 执行函数并保留失败异常供测试检查，不打印未处理异常。 */
TZrBool ZrTests_Runtime_Function_ExecuteCaptureFailure(SZrState *state, SZrFunction *function, SZrTypeValue *result);

/** @brief 执行函数并尝试分发未处理异常；成功时填写 result。 */
TZrBool ZrTests_Runtime_Function_Execute(SZrState *state, SZrFunction *function, SZrTypeValue *result);

/** @brief 执行并检查整数结果，仅在成功且类型匹配时写 result。 */
TZrBool ZrTests_Runtime_Function_ExecuteExpectInt64(SZrState *state, SZrFunction *function, TZrInt64 *result);

/** @brief 在当前线程登记活动 VM，供崩溃恢复时输出上下文。 */
void ZrTests_Runtime_CrashScope_Begin(SZrState *state);

/** @brief 移除当前线程中最后一个匹配的活动 VM 登记。 */
void ZrTests_Runtime_CrashScope_End(SZrState *state);

/** @brief 返回当前线程最内层登记的 VM，或最近 panic 的 VM。 */
SZrState *ZrTests_Runtime_CrashState_Current(void);

/** @brief 在已恢复的 Unity 测试边界清空当前线程的 crash 上下文。 */
void ZrTests_Runtime_ClearCrashState(void);

/** @brief 输出活动 VM 的异常栈；无活动 VM 时返回 false。 */
TZrBool ZrTests_Runtime_ReportCrashState(FILE *stream, TZrBool *printedExceptionStack);

#endif
