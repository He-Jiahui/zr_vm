#ifndef ZR_VM_CORE_EXECUTION_BUDGET_H
#define ZR_VM_CORE_EXECUTION_BUDGET_H

#include "zr_vm_core/conf.h"

/* 预算只借用执行线程及全局状态；宿主负责保持它们活到本次调用结束。 */
struct SZrState;
struct SZrGlobalState;
struct SZrCallInfo;

/** @brief 由宿主持有的一次性取消信号，须活过所有引用它的有界调用。 */
typedef struct SZrExecutionCancelToken SZrExecutionCancelToken;

/** @brief 有界调用的首个终止原因；一旦写入，在宿主撤销预算前保持不变。 */
typedef enum EZrExecutionTermination {
    ZR_EXECUTION_TERMINATION_NONE = 0,
    ZR_EXECUTION_TERMINATION_INSTRUCTION_LIMIT = 1,
    ZR_EXECUTION_TERMINATION_DEADLINE = 2,
    ZR_EXECUTION_TERMINATION_CANCELLED = 3,
    ZR_EXECUTION_TERMINATION_HEAP_LIMIT = 4,
    ZR_EXECUTION_TERMINATION_NATIVE_CALL_LIMIT = 5,
    ZR_EXECUTION_TERMINATION_GC_TIME_LIMIT = 6
} EZrExecutionTermination;

/**
 * @brief 一次项目导出调用的可选限制与累计用量。
 * @note 宿主零初始化后填写 has* 和上限，临时绑定到 state；内存峰值属于
 *       global 分配器的本次统计窗口，native/GC 消耗在协作式边界结算。
 */
typedef struct SZrExecutionBudget {
    TZrUInt64 maxInstructions;
    TZrUInt64 deadlineMicros;
    const SZrExecutionCancelToken *cancelToken;
    TZrBool hasInstructionLimit;
    TZrBool hasDeadline;
    TZrUInt64 executedInstructions;
    EZrExecutionTermination termination;
    TZrUInt64 maxHeapBytes;
    TZrUInt64 maxNativeCalls;
    TZrUInt64 maxGcMicros;
    TZrBool hasHeapLimit;
    TZrBool hasNativeCallLimit;
    TZrBool hasGcTimeLimit;
    TZrUInt64 peakHeapBytes;
    TZrUInt64 nativeCalls;
    TZrUInt64 gcMicros;
    TZrUInt64 gcStartedMicros;
    TZrUInt32 gcDepth;
    struct SZrCallInfo *countedNativeFrame;
} SZrExecutionBudget;

/** @brief 创建一次性取消信号；失败返回空指针，由宿主释放。 */
ZR_CORE_API SZrExecutionCancelToken *ZrCore_ExecutionCancelToken_New(void);
/** @brief 原子置位取消信号；空指针无操作，可与有界调用并发。 */
ZR_CORE_API void ZrCore_ExecutionCancelToken_Cancel(SZrExecutionCancelToken *token);
/** @brief 原子读取取消状态；空指针视为未取消。 */
ZR_CORE_API TZrBool ZrCore_ExecutionCancelToken_IsCancelled(const SZrExecutionCancelToken *token);
/** @brief 释放宿主持有的信号；须等所有有界调用及并发访问结束。 */
ZR_CORE_API void ZrCore_ExecutionCancelToken_Free(SZrExecutionCancelToken *token);

/** @brief 读取不受墙钟调整影响的单调微秒；时钟失败返回最大值，使期限检查失败关闭。 */
ZR_CORE_API TZrUInt64 ZrCore_ExecutionBudget_NowMicros(void);

/**
 * @brief 在解释器字节码分派前收费，或在 native/GC 边界只检查其他限制。
 * @return 预算缺失或仍可执行时为真；终止时设置线程状态并返回假，
 *         让宿主按正常 C 返回路径退出外部回调栈。
 */
ZR_CORE_API TZrBool ZrCore_ExecutionBudget_Poll(struct SZrState *state, TZrBool consumeInstruction);
/** @brief 从当前 VM 帧清理到外层 native 帧；终止不会交给 guest catch/finally 处理。 */
ZR_CORE_API void ZrCore_ExecutionBudget_UnwindVmFrames(struct SZrState *state);
/** @brief native 入口检查并计数；binding 再次进入同一已计数帧时只收费一次。 */
ZR_CORE_API TZrBool ZrCore_ExecutionBudget_NativeEnter(struct SZrState *state, TZrBool throughBinding);
/** @brief 记录 GC 嵌套深度，最外层开始计时。 */
ZR_CORE_API void ZrCore_ExecutionBudget_GcBegin(struct SZrState *state);
/** @brief 最外层 GC 结束时累计耗时并检查预算。 */
ZR_CORE_API void ZrCore_ExecutionBudget_GcEnd(struct SZrState *state);

/**
 * @brief 全局分配器外层，在上游成功操作后按请求大小更新存量和峰值。
 * @note 不含绕过此分配器的内存；超限在下一次协作式边界才终止，分配本身仍可成功。
 */
ZR_CORE_API TZrPtr ZrCore_ExecutionBudget_Allocate(TZrPtr userData, TZrPtr pointer,
        TZrSize originalSize, TZrSize newSize, TZrInt64 flag);
/** @brief 将全局峰值窗口复位到当前存量，返回本次有界调用的起点。 */
ZR_CORE_API TZrUInt64 ZrCore_ExecutionBudget_BeginMemory(struct SZrGlobalState *global);
/** @brief 读取全局分配器当前统计窗口的峰值，供 Poll 比较堆上限。 */
ZR_CORE_API TZrUInt64 ZrCore_ExecutionBudget_MemoryPeak(const struct SZrGlobalState *global);

#endif
