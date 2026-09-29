//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CORE_STATE_H
#define ZR_VM_CORE_STATE_H
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/call_binding.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/conf.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/stack.h"

/** @file state.h 每个 VM 线程的状态布局与生命周期接口；GlobalState 持有状态对象并共享全局资源。 */
struct SZrGlobalState;
struct SZrFunction;
struct SZrAotGcRootMap;
struct SZrGcDomain;
struct SZrExecutionBudget;

/** 异常处理器所处的 try/catch/finally 阶段；同一 handler 随控制流更新 phase。 */
typedef enum EZrVmExceptionHandlerPhase {
    ZR_VM_EXCEPTION_HANDLER_PHASE_TRY = 0, /**< 保护区域内执行。 */
    ZR_VM_EXCEPTION_HANDLER_PHASE_CATCH, /**< 已跳转到匹配的 catch。 */
    ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY /**< 正在执行 finally 清理。 */
} EZrVmExceptionHandlerPhase;

/** finally 执行期间暂挂的异常、return、break 或 continue 控制请求。 */
typedef enum EZrVmPendingControlKind {
    ZR_VM_PENDING_CONTROL_NONE = 0, /**< 当前没有待恢复控制。 */
    ZR_VM_PENDING_CONTROL_EXCEPTION, /**< 延后传播异常。 */
    ZR_VM_PENDING_CONTROL_RETURN, /**< 延后写回返回值并退出调用帧。 */
    ZR_VM_PENDING_CONTROL_BREAK, /**< 延后跳转到 break 目标。 */
    ZR_VM_PENDING_CONTROL_CONTINUE /**< 延后跳转到 continue 目标。 */
} EZrVmPendingControlKind;

/** 保存一次尚待 finally/resume 的控制转移；GC 扫描按 hasValue 判断 value 是否有效。 */
typedef struct SZrVmPendingControl {
    EZrVmPendingControlKind kind; /**< 请求类别；NONE 表示记录已清空。 */
    SZrCallInfo *callInfo; /**< 控制恢复时关联的调用帧，可为空表示当前帧。 */
    TZrMemoryOffset targetInstructionOffset; /**< break/continue 或 finally 后的目标指令偏移。 */
    TZrUInt32 valueSlot; /**< RETURN 写回目标帧的槽索引。 */
    SZrTypeValue value; /**< 暂存返回值；hasValue 为真时必须由 GC 追踪。 */
    TZrBool hasValue; /**< value 是否仍有效；清理后与 NONE 一致。 */
} SZrVmPendingControl;

/** 处理器栈中的单个保护帧；保存关闭边界以及 finally 暂停的控制/异常状态。 */
typedef struct SZrVmExceptionHandlerState {
    SZrCallInfo *callInfo; /**< 所属调用帧，决定异常表查找范围。 */
    TZrUInt32 handlerIndex; /**< 函数异常表中的 handler 索引。 */
    EZrVmExceptionHandlerPhase phase; /**< 当前 TRY/CATCH/FINALLY 阶段。 */
    TZrMemoryOffset toBeClosedBoundaryOffset; /**< 进入保护区时待关闭值链的栈偏移。 */
    SZrVmPendingControl suspendedControl; /**< finally 执行期间暂存的控制请求。 */
    SZrTypeValue suspendedException; /**< finally 执行期间暂存的异常值。 */
    EZrThreadStatus suspendedExceptionStatus; /**< 暂存异常对应的线程状态码。 */
    TZrBool hasSuspendedException; /**< suspendedException 是否需要恢复。 */
    TZrBool restoreSuspendedControl; /**< 出栈时是否恢复控制请求及异常。 */
} SZrVmExceptionHandlerState;

/** AOT 根帧由调用方压入 state 链；描述表、登记节点与根存储须保持有效直到 Pop。 */
/* The node, rootMap, and rootMap->roots descriptors stay host-stable until Pop.
 * A node is linked at most once while active; frameBase may point to values in
 * the movable VM stack and is rebased when that allocation moves. */
typedef struct SZrAotGcRootFrame {
    const struct SZrAotGcRootMap *rootMap; /**< GC 用于解释根槽位置的布局。 */
    TZrStackValuePointer frameBase; /**< 根偏移的寻址基址，可指 C 局部地址或 VM 栈帧。 */
    struct SZrAotGcRootFrame *previous; /**< state 根帧链中的前一帧。 */
} SZrAotGcRootFrame;

/** 单个 VM 线程的可变状态；栈、调用帧和异常控制均按线程隔离，global 资源共享。 */
struct ZR_STRUCT_ALIGN SZrState {
    SZrRawObject super; /**< 必须置首的 GC 对象头；分配后由所属域登记。 */
    struct SZrGlobalState *global; /**< 所属全局状态，也是共享 allocator/registry 的访问入口。 */
    struct SZrGcDomain *gcDomain; /**< AttachState 成功后非空；销毁时先解除登记。 */

    EZrThreadStatus threadStatus; /**< 当前线程运行状态。 */
    struct SZrExecutionBudget *executionBudget; /**< 可选的当前执行预算上下文；绑定层设置/清除此借用指针。 */
    TZrMemoryOffset previousProgramCounter; /**< 调试/恢复使用的上一条指令偏移。 */

    /** 活动帧由 callInfoList 指向；baseCallInfo 内嵌，扩展帧由 State_Free 回收。 */
    SZrCallInfo *callInfoList; /**< 当前活动调用帧。 */
    TZrUInt32 callInfoListLength; /**< 已分配并缓存的扩展帧数量，不是活动深度。 */
    SZrCallInfo baseCallInfo; /**< 线程入口帧；其 next 链串接需回收的扩展帧。 */

    /** 三个边界必须属于同一分配；Grow 可搬迁缓冲区并使旧裸地址失效。 */
    TZrStackPointer stackTop; /**< 当前逻辑栈顶。 */
    TZrStackPointer stackTail; /**< 逻辑容量尾界，不含 allocator extra slots。 */
    TZrStackPointer stackBase; /**< 栈分配起点，扩容后用于偏移重定位。 */

    TZrStackPointer toBeClosedValueList; /**< 待关闭值链头；与栈槽中的反向偏移配对。 */
    SZrAotGcRootFrame *aotGcRootFrameStack; /**< 当前 AOT 根帧链顶。 */
    TZrUInt32 aotGcRootFrameDepth; /**< Push/Pop 必须成对维护的链深度。 */
    /** 栈闭包链成员状态；自身指针是未加入全局链的哨兵。 */
    struct SZrState *threadWithStackClosures; /**< 全局链中的前驱 state，或 state 自身。 */
    SZrClosureValue *stackClosureValueList; /**< 本线程持有的开放栈闭包链。 */

    /** 异常与挂起控制状态；其中的 value 必须被 GC 标记和转发路径覆盖。 */
    TZrUInt32 nestedNativeCalls; /**< 当前嵌套原生调用数；TryRun 恢复进入时深度。 */
    TZrUInt32 nestedNativeCallYieldFlag; /**< 可 yield 的嵌套 native 调用数。 */
    SZrExceptionLongJump *exceptionRecoverPoint; /**< 当前线程栈上的恢复点，仅在 TryRun 动态范围有效。 */
    TZrMemoryOffset exceptionHandlingFunctionOffset; /**< TODO: 当前仓内仅初始化；核对仓外/生成代码消费者后决定用途。 */
    SZrTypeValue currentException; /**< 当前异常对象；与 hasCurrentException 成对设置/清除。 */
    EZrThreadStatus currentExceptionStatus; /**< currentException 对应的错误状态。 */
    SZrCallBindingDiagnostic lastCallBindingError; /**< 最近一次调用绑定诊断，供错误报告读取。 */
    TZrBool hasCurrentException; /**< currentException 是否持有待处理异常。 */
    SZrVmExceptionHandlerState *exceptionHandlerStack; /**< 动态扩展的 finally/handler 栈。 */
    TZrUInt32 exceptionHandlerStackLength; /**< 有效元素数，始终不大于 capacity。 */
    TZrUInt32 exceptionHandlerStackCapacity; /**< raw allocator 申请的元素容量。 */
    SZrVmPendingControl pendingControl; /**< 当前待恢复请求；由执行控制和 GC 协同维护。 */

    /** 调试 hook、AOT 观察策略及运行时检查均由该 state 单独保存。 */
    TZrBool allowDebugHook; /**< 防止调试回调在递归调用时重复进入。 */
    TZrUInt32 baseDebugHookCount; /**< 调试 hook 的配置间隔。 */
    TZrUInt32 debugHookCount; /**< 到下一次计数型 hook 的剩余次数。 */
    volatile FZrDebugHook debugHook; /**< 当前调试回调。 */
    volatile TZrDebugSignal debugHookSignal; /**< 待处理的调试事件位。 */
    volatile FZrDebugTraceObserver debugTraceObserver; /**< 可选 trace 观察回调。 */
    TZrPtr debugTraceUserData; /**< 传回 trace 回调的宿主数据；其寿命由宿主覆盖回调使用期。 */
    struct SZrFunction *debugLastFunction; /**< 最近调试位置所属函数。 */
    TZrUInt32 debugLastLine; /**< 最近调试位置行号。 */
    TZrUInt32 aotObservationMask; /**< AOT 运行时逐指令观察掩码。 */
    TZrBool hasAotObservationPolicyOverride; /**< 是否覆盖函数自身的 AOT 观察策略。 */
    TZrBool aotPublishAllInstructions; /**< 覆盖策略是否要求发布全部指令。 */
    
    /** 运行时检查开关按 state 配置；ResetThread 不会自动恢复初始化默认值。 */
    TZrBool enableRuntimeBoundsCheck; /**< 启用运行时边界检查。 */
    TZrBool enableRuntimeTypeCheck; /**< 启用运行时类型检查。 */
    TZrBool enableRuntimeRangeCheck; /**< 启用运行时范围检查。 */
    /** Native provider 默认为 Runtime 阶段；非 Runtime 宿主显式切换。 */
    TZrUInt8 nativeProviderPhase;
    TZrUInt64 debugFrameGenerationNext; /**< 新调用帧的非零 generation 计数。 */
};

typedef struct SZrState SZrState;

/** @brief 分配并初始化 state；GlobalState 持有返回对象。
 * @param global 必须是仍有效的全局状态。
 * @return 分配失败时返回空指针。
 */
ZR_CORE_API SZrState *ZrCore_State_New(struct SZrGlobalState *global);
/** @brief 初始化调用方已分配且已清零、已构造 RawObject 头的 state；通常由 State_New 调用。
 * @pre state 与 global 均非空，state 尚未初始化。
 */
ZR_CORE_API void ZrCore_State_Init(SZrState *state, struct SZrGlobalState *global);
/** @brief 建立主线程栈并按序初始化字符串表、注册表和元数据。
 * @pre state 是所属 GlobalState 的 mainThreadState；由 GlobalState_New 经 TryRun 调用。
 * @note afterStateInitialized 回调错误通过 state 当前异常路径传播。
 */
ZR_CORE_API void ZrCore_State_MainThreadLaunch(SZrState *state, TZrPtr arguments);

/**
 * @brief 为已附着到调用方 GC 域的次级 state 分配栈并进入 mutator 运行态。
 * @pre state 来自 ZrCore_State_New，global/mainThreadState/gcDomain 有效且尚未 launch。
 * @return 参数或 MutatorEnter 不满足时返回 false；失败时回收已分配的线程栈。
 * @note 不初始化全局注册表或第二主线程；成功后须先 MutatorExit 再 State_Free。
 */
ZR_CORE_API TZrBool ZrCore_State_MutatorLaunch(SZrState *state);
/** @brief 配对结束一次成功的 MutatorLaunch；只退出运行作用域，不释放 state。 */
ZR_CORE_API void ZrCore_State_MutatorExit(SZrState *state);
/** @brief 退出钩子；当前唯一调用方在主线程启动失败后立即执行 GlobalState_Free。
 * TODO: 此钩子目前无操作，是否仍需独立清理职责尚待确认。
 */
ZR_CORE_API void ZrCore_State_Exit(SZrState *state);
/** @brief 解除 GC 域登记并释放调用帧、栈、异常处理器和 state 本体。
 * @pre global 必须是 state->global，且 state 已退出 mutator 执行作用域。
 */
ZR_CORE_API void ZrCore_State_Free(struct SZrGlobalState *global, SZrState *state);
/** @brief 清理异常 handler/pending control 并恢复基础 native 帧。
 * @pre state 已 launch，且仍处于调用方拥有的执行上下文中。
 * @return 清理期间的最终线程状态；handler 或 pending 清理错误可覆盖传入状态。
 * @note ResetThread 会清空 currentException；主线程有恢复点时，worker 异常转发在清理后执行。
 */
ZR_CORE_API TZrInt32 ZrCore_State_ResetThread(SZrState *state, EZrThreadStatus status);
/** @brief 从 entry 加载源文件。
 * @return 加载失败返回 runtime error；当前成功分支仍未转换或执行源码，仓内无调用者。
 */
ZR_CORE_API EZrThreadStatus ZrCore_State_DoRun(SZrState *state, TZrNativeString entry);
/** @brief 返回逻辑栈容量（槽数）；state 必须已分配栈，结果不含 extra reserve。
 * @pre stackBase 与 stackTail 属于同一有效分配。
 */
ZR_FORCE_INLINE TZrSize ZrCore_State_StackGetSize(SZrState *state) {
    return state->stackTail.valuePointer - state->stackBase.valuePointer;
}

/** @brief 判断 state 是否加入全局栈闭包线程链；state 自身指针表示未入链。
 * @pre state 非空。
 */
ZR_FORCE_INLINE TZrBool ZrCore_State_IsInClosureValueThreadList(SZrState *state) {
    // 自身指针是未入链哨兵；其他值指向全局闭包线程链中的前驱。
    return state->threadWithStackClosures != state;
}


#endif // ZR_VM_CORE_STATE_H
