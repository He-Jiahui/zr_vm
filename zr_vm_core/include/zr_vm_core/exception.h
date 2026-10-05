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
/** @note TryRun 直接派发一次；arguments 无延迟持有或释放 */
typedef void (*FZrTryFunction)(struct SZrState *state, TZrPtr arguments);

/** @brief 宿主恐慌处理回调；异常和 Debug 兜底路径在回调返回后终止，原生绑定的异 state 分支转交当前 global handler 后可返回。 */
/** @note exception/Debug 回调正常返回后 abort；native binding 的异 state 或无 context 分支仅在当前 global handler 非自身且非空时转交，随后可普通返回 */
typedef void (*FZrPanicHandlingFunction)(struct SZrState *state);

/** 由 runtime 创建的线程内嵌套恢复点，保存在调用栈上；不能跨线程传递或在 TryRun 返回后使用。 */
/** @note 由 TryRun 栈上构造；不能跨线程或返回后使用 */
struct SZrExceptionLongJump {
/** @note C 分支只在同一活动 setjmp 动态范围内恢复；C++分支不用它 */
    TZrExceptionLongJump jumpBuffer;
/** @note 借用外层自动对象，不拥有或释放其内存 */
    struct SZrExceptionLongJump *previous;
/** @note 入口 FINE；volatile 保留跨 setjmp 的修改；外来 C++异常仅在仍FINE时改INVALID */
    volatile EZrThreadStatus status;
};

typedef struct SZrExceptionLongJump SZrExceptionLongJump;

/** @brief 同步执行回调并捕获 Throw 的线程状态；恢复点在返回前自动撤销。
 * C11 longjmp 路径中，Throw 命中本线程 TryRun 恢复点时，会在发布 mutator inactive
 * 前恢复入口 AOT 根链和 GC 作用域，释放回调新增的递归 mutation lock 层，保留调用者
 * 外层 execution/native/mutation 深度。forced-C++ 展开期间析构函数回入 VM/GC 不在此保证内。 */
/** @pre state 和 tryFunction 非空；arguments 在回调返回前保持有效。 */
/** @note 正常返回时结果为 FINE，不会把回调自行写入的 state->threadStatus 合并为返回值。 */
/** @note arguments 只借用到返回；嵌套恢复 previous 与 nestedNativeCalls；正常返回维持 FINE，不代替回调平衡 scopes 或读取 threadStatus。 */
ZR_CORE_API EZrThreadStatus ZrCore_Exception_TryRun(struct SZrState *state, FZrTryFunction tryFunction, TZrPtr arguments);

/** @brief 将错误状态沿当前恢复点非局部传播；无恢复点时转交主线程或进入 panic 终止。 */
/** @pre state 非空；跨跳转局部资源不能依赖普通返回路径清理。 */
/** @note 非空 state；不保证普通 C 局部资源清理，命中恢复点时即使 FINE 也非局部离开。 */
ZR_CORE_API void ZrCore_Exception_Throw(struct SZrState *state, EZrThreadStatus errorCode);

/** @brief 保留既有 Stop 调用接口；TODO: 当前仅回传 status，level 没有参与退出层级，需核对调用方预期。 */
/** @note state 与 level 当前均不参与处理；TODO: 沿 State_ResetThread 的 level=1 调用确认是否仍需要分层停止语义。 */
ZR_CORE_API EZrThreadStatus ZrCore_Exception_TryStop(struct SZrState *state, TZrMemoryOffset level, EZrThreadStatus status);

/** @brief 把执行失败状态落实到调用栈结果槽，供状态退出与调用方诊断。 */
/** @pre previousTop 指向当前线程可写的栈槽。 */
/** @note previousTop 须属于有效可写栈且其前一槽可作 top；写完 stackTop=previousTop-1，未保证 top 指向结果之后。 */
ZR_CORE_API void ZrCore_Exception_MarkError(struct SZrState *state, EZrThreadStatus errorCode,
                                      TZrStackValuePointer previousTop);

/** @brief 在处理完异常后清除当前线程保存的异常值、状态和存在标记。 */
/** @note 同时清 value/status/has 标志；不改 threadStatus，不执行对象销毁或 handler 清理。 */
ZR_CORE_API void ZrCore_Exception_ClearCurrent(struct SZrState *state);

/** @brief 把任意抛出值规范化成 Error 对象并保存于当前线程，供 catch/AOT/宿主读取。 */
/** @return 成功返回真；若正常返回假则未完成规范化，分配失败也可能通过 Throw 离开。 */
/** @note 已有 Error 保留对象身份但重建诊断字段；其它值包装基础 Error；假返回不保证已改字段回滚。 */
ZR_CORE_API TZrBool ZrCore_Exception_NormalizeThrownValue(struct SZrState *state,
                                                    const SZrTypeValue *payload,
                                                    struct SZrCallInfo *throwCallInfo,
                                                    EZrThreadStatus status);

/** @brief 为原生状态错误创建可供诊断的 Error 对象；已有异常时仅更新状态。 */
/** @note EXECUTION_TERMINATED 不归一化；已有异常只改 currentExceptionStatus；其它状态可查看末尾栈槽并分配。 */
ZR_CORE_API TZrBool ZrCore_Exception_NormalizeStatus(struct SZrState *state,
                                               EZrThreadStatus status);

/** @brief 按已注册的原型名构建运行时错误并保存为当前异常，不负责 Throw。 */
/** @note 名称解析失败返回假；保存 RUNTIME_ERROR 而不自行 Throw，调用方继续 handler 分派或失败回退。 */
ZR_CORE_API TZrBool ZrCore_Exception_RaiseNamedRuntimeError(
        struct SZrState *state,
        const TZrChar *prototypeName,
        const TZrChar *message,
        struct SZrCallInfo *throwCallInfo);

/** @brief 判断当前 Error 的原型链是否匹配 catch 注解；空注解匹配基础 Error。 */
/** @note TODO: 未解析的非空类型名也回退到基础 Error；需核对编译器是否事先拒绝未知 catch 类型。 */
/** @note 只匹配 Error 继承；TODO: 未找到非空类型名时仍回退基础 Error，沿 parser catchClause.typeName 生成与校验核查未知类型准入。 */
ZR_CORE_API TZrBool ZrCore_Exception_CatchMatchesTypeName(struct SZrState *state,
                                                    const SZrTypeValue *errorValue,
                                                    struct SZrString *typeName);

/** @brief 按指令偏移查找最近的源行；无映射时返回零供诊断层显示。 */
/** @note 依赖 executionLocationInfoList 按 instructionOffset 递增；无映射返回零，不使用 debug hook 的无行哨兵。 */
ZR_CORE_API TZrUInt32 ZrCore_Exception_FindSourceLine(struct SZrFunction *function,
                                                TZrMemoryOffset instructionOffset);

/** @brief 将未捕获异常格式化到给定 FILE，空 stream 使用 stderr。 */
/** @note 空 FILE 选择 stderr；格式化普通失败不输出；正常输出后 free 文本，不关闭借用 stream。 */
ZR_CORE_API void ZrCore_Exception_PrintUnhandled(struct SZrState *state,
                                           const SZrTypeValue *errorValue,
                                           FILE *stream);

/** @brief 经运行时日志通道报告未捕获异常。 */
/** @note 按 EXCEPTION/STDERR/DIAGNOSTIC 分类发送；调用后释放宿主文本，路由须同步消费或复制文本。 */
ZR_CORE_API void ZrCore_Exception_LogUnhandled(struct SZrState *state,
                                               const SZrTypeValue *errorValue);


/** @brief 判断状态是否属于错误区间；保留既有拼写以兼容现有调用。 */
/** @note 按 enum 顺序与 RUNTIME_ERROR 下界比较；不读取 state 或 currentException。 */
ZR_FORCE_INLINE TZrBool ZrCore_Exception_IsStausError(EZrThreadStatus status) {
    return status >= ZR_THREAD_STATUS_RUNTIME_ERROR;
}
#endif // ZR_VM_CORE_EXCEPTION_H
