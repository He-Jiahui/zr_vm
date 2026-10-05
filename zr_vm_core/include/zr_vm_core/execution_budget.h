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
/** @brief hasInstructionLimit 开启时的分派入场上限；等于已执行数时拒绝下一条，native 本身不收指令。 */
    TZrUInt64 maxInstructions;
/** @brief hasDeadline 开启时与单调微秒比较的绝对期限；不是相对超时或抢占计时器。 */
    TZrUInt64 deadlineMicros;
/** @brief 借用宿主一次性取消信号；预算及并发读取结束前不得释放，空指针视为未取消。 */
    const SZrExecutionCancelToken *cancelToken;
/** @brief 区分未设置指令限额与限额为零；零限额仍允许不消费指令的 native 边界。 */
    TZrBool hasInstructionLimit;
/** @brief 启用绝对期限门禁；未开启时 deadlineMicros 不参与 Poll。 */
    TZrBool hasDeadline;
/** @brief 按 dispatch 入场累计，未放行的下一条不计；饱和后不回绕，供宿主 usage。 */
    TZrUInt64 executedInstructions;
/** @brief 首个终止原因锁存；后续 Poll 保持原因并发布线程终止，不转交 guest handler。 */
    EZrExecutionTermination termination;
/** @brief hasHeapLimit 开启时的全局统计窗口峰值上限；已有存量也计入，等于上限仍放行。 */
    TZrUInt64 maxHeapBytes;
/** @brief hasNativeCallLimit 开启时的 native 入口限额；同一注册帧经 binding 不重复收费。 */
    TZrUInt64 maxNativeCalls;
/** @brief hasGcTimeLimit 开启时的已完成最外层 GC 累计上限；等于上限仍放行。 */
    TZrUInt64 maxGcMicros;
/** @brief 区分无堆限制与零峰值上限；本预算不阻止 allocator 当次成功申请。 */
    TZrBool hasHeapLimit;
/** @brief 启用 native 入场限额；零值在首次入口前拒绝，不执行该 native 回调。 */
    TZrBool hasNativeCallLimit;
/** @brief 启用 GC 累计时间限制；在协作边界检查已结算耗时，不中断 collector。 */
    TZrBool hasGcTimeLimit;
/** @brief 本调用观察的 global 请求字节峰值，包含调用前存量；不是 RSS 或单次调用净增量。 */
    TZrUInt64 peakHeapBytes;
/** @brief 计入已放行 native 入口的累计数；同帧 binding 去重，饱和不回绕。 */
    TZrUInt64 nativeCalls;
/** @brief 已完成最外层 GC 的累计微秒，嵌套区间不重复收取，累计饱和。 */
    TZrUInt64 gcMicros;
/** @brief 外层 GcBegin 的单调时间起点；仅在 gcDepth 从零进入时更新。 */
    TZrUInt64 gcStartedMicros;
/** @brief 须配对的 GC 嵌套计数；只有回到零才结算时间，宿主结束调用前补齐未闭合层。 */
    TZrUInt32 gcDepth;
/** @brief 借用刚由函数入口收费的 callInfo 身份，供紧接的 binding 去重；返回native后由caller清空。 */
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
