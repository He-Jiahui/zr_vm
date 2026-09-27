//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_EXCEPTION_H
#define ZR_VM_CORE_EXCEPTION_H

#include <stdio.h>

#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
struct SZrState;
struct SZrCallInfo;
struct SZrFunction;
struct SZrString;

/** @brief TryRun 同步执行的受保护函数；arguments 仅在调用期间借用。 */
typedef void (*FZrTryFunction)(struct SZrState *state, TZrPtr arguments);

/** @brief 宿主恐慌处理回调；异常和 Debug 兜底路径在回调返回后终止，原生绑定转交前任 handler 后可返回。 */
typedef void (*FZrPanicHandlingFunction)(struct SZrState *state);

/** 线程内的嵌套恢复点，保存在调用栈上；不能跨线程传递或在 TryRun 返回后使用。 */
struct SZrExceptionLongJump {
    TZrExceptionLongJump jumpBuffer;
    struct SZrExceptionLongJump *previous;
    volatile EZrThreadStatus status;
};

typedef struct SZrExceptionLongJump SZrExceptionLongJump;

/** @brief 同步执行回调并捕获 Throw 的线程状态；恢复点在返回前自动撤销。 */
/** @pre state 和 tryFunction 非空；arguments 在回调返回前保持有效。 */
/** @note 正常返回时结果为 FINE，不会把回调自行写入的 state->threadStatus 合并为返回值。 */
ZR_CORE_API EZrThreadStatus ZrCore_Exception_TryRun(struct SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments);

/** @brief 将错误状态沿当前恢复点非局部传播；无恢复点时转交主线程或进入 panic 终止。 */
/** @pre state 非空；跨跳转局部资源不能依赖普通返回路径清理。 */
ZR_CORE_API void ZrCore_Exception_Throw(struct SZrState *state, EZrThreadStatus errorCode);

/** @brief 保留既有 Stop 调用接口；TODO: 当前仅回传 status，level 没有参与退出层级，需核对调用方预期。 */
ZR_CORE_API EZrThreadStatus ZrCore_Exception_TryStop(struct SZrState *state, TZrMemoryOffset level, EZrThreadStatus status);

/** @brief 把执行失败状态落实到调用栈结果槽，供状态退出与调用方诊断。 */
/** @pre previousTop 指向当前线程可写的栈槽。 */
ZR_CORE_API void ZrCore_Exception_MarkError(struct SZrState *state, EZrThreadStatus errorCode,
                                      TZrStackValuePointer previousTop);

/** @brief 在处理完异常后清除当前线程保存的异常值、状态和存在标记。 */
ZR_CORE_API void ZrCore_Exception_ClearCurrent(struct SZrState *state);

/** @brief 把任意抛出值规范化成 Error 对象并保存于当前线程，供 catch/AOT/宿主读取。 */
/** @return 成功返回真；若正常返回假则未完成规范化，分配失败也可能通过 Throw 离开。 */
ZR_CORE_API TZrBool ZrCore_Exception_NormalizeThrownValue(struct SZrState *state,
                                                    const SZrTypeValue *payload,
                                                    struct SZrCallInfo *throwCallInfo,
                                                    EZrThreadStatus status);

/** @brief 为原生状态错误创建可供诊断的 Error 对象；已有异常时仅更新状态。 */
ZR_CORE_API TZrBool ZrCore_Exception_NormalizeStatus(struct SZrState *state,
                                               EZrThreadStatus status);

/** @brief 按已注册的原型名构建运行时错误并保存为当前异常，不负责 Throw。 */
ZR_CORE_API TZrBool ZrCore_Exception_RaiseNamedRuntimeError(
        struct SZrState *state,
        const TZrChar *prototypeName,
        const TZrChar *message,
        struct SZrCallInfo *throwCallInfo);

/** @brief 判断当前 Error 的原型链是否匹配 catch 注解；空注解匹配基础 Error。 */
/** @note TODO: 未解析的非空类型名也回退到基础 Error；需核对编译器是否事先拒绝未知 catch 类型。 */
ZR_CORE_API TZrBool ZrCore_Exception_CatchMatchesTypeName(struct SZrState *state,
                                                    const SZrTypeValue *errorValue,
                                                    struct SZrString *typeName);

/** @brief 按指令偏移查找最近的源行；无映射时返回零供诊断层显示。 */
ZR_CORE_API TZrUInt32 ZrCore_Exception_FindSourceLine(struct SZrFunction *function,
                                                TZrMemoryOffset instructionOffset);

/** @brief 将未捕获异常格式化到给定 FILE，空 stream 使用 stderr。 */
ZR_CORE_API void ZrCore_Exception_PrintUnhandled(struct SZrState *state,
                                           const SZrTypeValue *errorValue,
                                           FILE *stream);

/** @brief 经运行时日志通道报告未捕获异常。 */
ZR_CORE_API void ZrCore_Exception_LogUnhandled(struct SZrState *state,
                                               const SZrTypeValue *errorValue);


/** @brief 判断状态是否属于错误区间；保留既有拼写以兼容现有调用。 */
ZR_FORCE_INLINE TZrBool ZrCore_Exception_IsStausError(EZrThreadStatus status) {
    return status >= ZR_THREAD_STATUS_RUNTIME_ERROR;
}
#endif // ZR_VM_CORE_EXCEPTION_H
