/** @file
 * 供宿主直接驱动 poll 的公开 continuation ABI：Init、Start、按需 Resume，最后 Free。
 * 仓内 API 驱动来自 task-frame 与 debug 测试，生产 debug 投影器另消费状态枚举；
 * guest task/job runtime 未见调用本接口。
 * TODO: 接入生产 lowering/调度器或仓外宿主时，核对串行驱动、异常退出和 VM 销毁前的释放约定；
 * 当前测试入口不能证明这些外部调用契约已经落实。
 */
#ifndef ZR_VM_CORE_TASK_FRAME_RUNTIME_H
#define ZR_VM_CORE_TASK_FRAME_RUNTIME_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/value.h"

/**
 * @brief 为分次运行并可挂起的任务提供由 poll 驱动的 continuation 存储。
 *
 * poll callback 可以同步完成而不分配 frame。若要
 * 挂起，应记录有效状态及仍存活的 slot，再返回 SUSPEND；
 * runtime 会保留 frame，直到任务完成、故障或清理。
 * 本 API 不负责调度任务或等待依赖；由调用方驱动
 * Start 和 Resume。
 * layout 和 callback user data 均为借用数据，生命周期必须覆盖整个 task。pool
 * 必须比所有使用它的 task 活得更久；只有这些 task
 * 归还 frame 后才能释放 pool。
 */
struct SZrState;
struct SZrCoreTaskFrame;
struct SZrDebugAsyncTerminalEvent;

/** @brief 单个 task-frame task 的生命周期状态；Await 可复制值或转移拥有所有权的 result，Free 会清理剩余值。 */
typedef enum EZrCoreTaskFrameStatus {
    ZR_CORE_TASK_FRAME_STATUS_IDLE = 0, /**< 已初始化或清理完成，可以调用 Start。 */
    ZR_CORE_TASK_FRAME_STATUS_RUNNING,  /**< 已开始一次推进；结果 root 失败时也可能保留此状态，见失败路径 TODO。 */
    ZR_CORE_TASK_FRAME_STATUS_SUSPENDED,/**< 已保留 frame，可以恢复执行。 */
    ZR_CORE_TASK_FRAME_STATUS_COMPLETED,/**< poll 已完成，可通过 Await 读取结果。 */
    ZR_CORE_TASK_FRAME_STATUS_FAULTED   /**< poll 或显式故障已终止任务。 */
} EZrCoreTaskFrameStatus;

/** @brief 一次 poll 调用返回的结果。 */
typedef enum EZrCoreTaskFramePollOutcome {
    ZR_CORE_TASK_FRAME_POLL_COMPLETE = 0, /**< 将最终值写入 outResult。 */
    ZR_CORE_TASK_FRAME_POLL_SUSPEND,      /**< 必须先由 Suspend 创建 frame。 */
    ZR_CORE_TASK_FRAME_POLL_FAULT         /**< 若 callback 尚未设置故障，runtime 会将 task 置为故障状态。 */
} EZrCoreTaskFramePollOutcome;

/** @brief 查询 task 的终态值或错误时返回的结果。 */
typedef enum EZrCoreTaskFrameAwaitStatus {
    ZR_CORE_TASK_FRAME_AWAIT_READY = 0, /**< 结果已复制或所有权已转移。 */
    ZR_CORE_TASK_FRAME_AWAIT_PENDING,   /**< task 正在运行或已挂起。 */
    ZR_CORE_TASK_FRAME_AWAIT_FAULTED,   /**< task 已故障，或 task / result 无效。 */
    ZR_CORE_TASK_FRAME_AWAIT_RESULT_CONSUMED /**< 拥有所有权的结果已转移过。 */
} EZrCoreTaskFrameAwaitStatus;

/**
 * @brief 已初始化 frame slot 的可选清理回调。
 * @note runtime 释放 slot 值及 root 前会调用此回调。value
 *       指针仅在回调执行期间有效。
 * TODO: 现有测试 drop 仅计数/比较地址；核查外部 drop 的 GC、异常和同 slot 重入约束，
 *       cleanup 尚未在调用前撤销 initialized，也未在此处重新 Resolve 移动后的 slot 地址。
 */
typedef void (*FZrCoreTaskFrameDrop)(struct SZrState *state,
                                     SZrTypeValue *value,
                                     TZrPtr userData);

struct SZrCoreTaskFrameTask;

/**
 * @brief 每个 task 的清理回调；释放已保留 slot 前最多调用一次。
 * @note 回调执行期间 task 及已初始化的 slot 仍可访问；
 *       回调返回后不得继续持有这些指针。
 * @note finallyRan 在派发前置位，避免再次进入 finally；这不保证同一 task 的其他清理可以重入。
 * TODO: 现有 finally 只 Reset 后 LoadSlot；外部回调抛异常或改变 task/layout 的收尾需在接入点核查。
 */
typedef void (*FZrCoreTaskFrameFinally)(struct SZrState *state,
                                        struct SZrCoreTaskFrameTask *task,
                                        TZrPtr userData);

/** @brief 描述 continuation layout 中单个 slot 的 GC / drop 策略。 */
typedef struct SZrCoreTaskFrameSlotLayout {
    TZrBool isGcRoot;                /**< frame 存活期间保持 GC 对象可达。 */
    TZrBool requiresDrop;            /**< 已初始化的 slot 是否要求调用用户 drop 回调。 */
    FZrCoreTaskFrameDrop drop;       /**< TODO: requiresDrop 为真且 drop 为空时当前会跳过用户清理；接入点需确认是否应拒绝此配置。 */
    TZrPtr dropUserData;             /**< 借用数据，直到 task frame 清理完成。 */
} SZrCoreTaskFrameSlotLayout;

/**
 * @brief 描述 task continuation 状态及 spill slot 的借用布局。
 * @pre slotCount 非零时，slotLayouts 必须包含 slotCount 个元素。poll 若要
 *      挂起，传给 Suspend 的 stateId 必须小于 stateCount。
 * @note layout、slotLayouts 数组、回调及 user data 必须持续有效，
 *       从 Start 一直到终态清理结束；仅同步运行的 task 可以令 stateCount 为零。
 */
typedef struct SZrCoreTaskFrameLayout {
    TZrUInt32 stateCount;                               /**< 有效 continuation state ID 的数量。 */
    TZrUInt32 slotCount;                                /**< 下方描述的 spill slot 数量。 */
    const SZrCoreTaskFrameSlotLayout *slotLayouts;      /**< 借用的数组；slotCount 为零时可为空。 */
    FZrCoreTaskFrameFinally finally;                    /**< 可选的终态 / 清理回调，最多调用一次。 */
    TZrPtr finallyUserData;                             /**< finally 使用的借用上下文。 */
} SZrCoreTaskFrameLayout;

/**
 * @brief 管理 continuation frame 的复用池。
 * @pre 在 Start 前初始化；pool 必须持续有效，直到所有相关
 *      task 均已释放；所有活动 frame 归还后才能调用 Pool_Free。
 * @note 计数器分别表示已分配、已借出和已缓存的 frame 数量。
 *       多个线程并发修改同一 pool 时，调用方必须提供同步。
 */
typedef struct SZrCoreTaskFramePool {
    struct SZrCoreTaskFrame *freeFrames; /**< 私有空闲链表头；不要直接修改。 */
    TZrUInt32 frameAllocationCount;      /**< 自 Init 以来分配的 frame 总数。 */
    TZrUInt32 activeFrameCount;          /**< 当前由 task 借出的 frame 数量。 */
    TZrUInt32 pooledFrameCount;          /**< 可供复用的 frame 数量。 */
} SZrCoreTaskFramePool;

typedef struct SZrCoreTaskFrameTask SZrCoreTaskFrameTask;

/**
 * @brief 同步推进一次 task continuation 的 poll 回调。
 * @pre 调用期间 state、task 和 outResult 必须有效；返回 SUSPEND 前应调用 Suspend 和 slot
 *      访问函数；返回 COMPLETE 时必须初始化 outResult。
 * @note runtime 会在 Start 和每次 Resume 时调用此回调。userData 为借用数据。
 *       outResult 在每次派发前已 ResetAsNull；COMPLETE 的可转移 owner 由 header 接管。
 * TODO: SUSPEND/FAULT 当前不释放回调临时 outResult；接入回调须核查非 COMPLETE 路径
 *       是否约定保持该输出为空，并核实异常退出后的 task/pool 收尾。
 */
typedef EZrCoreTaskFramePollOutcome (*FZrCoreTaskFramePoll)(
        struct SZrState *state,
        SZrCoreTaskFrameTask *task,
        TZrPtr userData,
        SZrTypeValue *outResult);

/**
 * @brief 可分配在栈上的 task header；从 Init 到 Free 期间由本 runtime 管理。
 * @note 不要按值复制正在使用的 task：其中包含所有权值及 GC
 *       root handle。pool、layout 和 callback user data 均为借用数据。
 *       同一 task 的 poll、resume 和 cleanup 必须由调用方串行执行。
 */
struct SZrCoreTaskFrameTask {
    EZrCoreTaskFrameStatus status; /**< 由 Status 返回的生命周期状态。 */
    struct SZrCoreTaskFrame *frame; /**< 私有 continuation frame；首次 Suspend 前为空。 */
    SZrCoreTaskFramePool *pool; /**< 借用的 pool，用于取得或归还 frame 存储。 */
    const SZrCoreTaskFrameLayout *layout; /**< 此 task 借用的状态 / slot 策略。 */
    FZrCoreTaskFramePoll poll; /**< 由 Start 和 Resume 调用的回调。 */
    TZrPtr userData; /**< poll 回调使用的借用上下文。 */
    SZrTypeValue result; /**< header 持有的值槽；普通 Await 可重复复制，拥有所有权的值只转移一次。 */
    SZrTypeValue error; /**< runtime 持有终态 error 直到 Free；Await 只会复制给调用方。 */
    /* TODO: GC 更新域根表不等于更新 result/error 裸值槽；核查实际移动后的 Await、再 Fault 和 Free，
     * 当前完成 GC 用例只检查非空/域戳，未证明这些 header 地址已经刷新。 */
    SZrGcRootHandle resultRoot; /**< header 保活句柄；普通结果到 Free 才撤销，拥有所有权的结果在 Await 转移后撤销。 */
    SZrGcRootHandle errorRoot; /**< fault 错误的保活句柄；Await 只复制错误，句柄仍由 task 保留到 Free 或替换。 */
    TZrUInt32 debugAsyncFaultProvenance; /**< 用于 debug 投影的故障来源标记。 */
    TZrBool resultConsumed; /**< Await 转移拥有所有权的 result 后置位，避免重复转移。 */
    TZrBool finallyRan; /**< 防止清理时重复调用 finally。 */
};

/** @brief 在 Start 前初始化空的 frame 复用池；pool 为空时不执行操作。 */
ZR_CORE_API void ZrCore_TaskFramePool_Init(SZrCoreTaskFramePool *pool);
/**
 * @brief 所有相关 task 释放后，销毁缓存的 frame 并重置 pool。
 * @pre 清理缓存 slot 或释放 GC root 期间，state 必须保持有效。
 * @note 仅销毁空闲链表；活动 frame 属于 task，Pool_Free 不替代 Task_Free。
 */
ZR_CORE_API void ZrCore_TaskFramePool_Free(struct SZrState *state,
                                            SZrCoreTaskFramePool *pool);

/** @brief 初始化新存储或已 Free 的 task header，以供首次 Start；task 为空时不执行操作。 */
ZR_CORE_API void ZrCore_TaskFrameTask_Init(SZrCoreTaskFrameTask *task);
/**
 * @brief 释放 task 的值、root 和 frame，再将 header 重置为 IDLE。
 * @pre task 必须已初始化；state 和 task 使用的 pool 必须有效，释放 pool 前先 Free 所有 task。
 * @note 活动 task 会先运行 finally，再清理存活的 slot。
 */
ZR_CORE_API void ZrCore_TaskFrameTask_Free(struct SZrState *state,
                                            SZrCoreTaskFrameTask *task);
/**
 * @brief 绑定 layout 并同步执行第一次 poll。
 * @pre state 和 pool 必须有效，task 必须已初始化且处于 IDLE；layout 和 poll 不能为空，
 *      非零 slotCount 必须提供 slotLayouts；layout 数据及 callback 上下文须持续有效到 Free。
 * @return 输入无效或 poll / result rooting 内部操作失败时返回 false；
 *         true 表示 poll 已处理，包括已记录为故障的 FAULT 结果。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Start(struct SZrState *state,
                                                SZrCoreTaskFrameTask *task,
                                                SZrCoreTaskFramePool *pool,
                                                const SZrCoreTaskFrameLayout *layout,
                                                FZrCoreTaskFramePoll poll,
                                                TZrPtr userData);
/**
 * @brief 重新运行 SUSPENDED task 的 poll；只有已保留 frame 的 task 才能恢复。
 * @pre state 和 task 必须有效；task 必须处于 SUSPENDED 且保留有 frame；同一 task 操作须串行。
 * @return 无效状态时返回 false；poll 已处理时返回 true，包括 task 已转入故障状态。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Resume(struct SZrState *state,
                                                 SZrCoreTaskFrameTask *task);
/** @brief 读取 task 生命周期状态；task 为空时返回 IDLE。 */
ZR_CORE_API EZrCoreTaskFrameStatus ZrCore_TaskFrameTask_Status(
        const SZrCoreTaskFrameTask *task);
/**
 * @brief 将 task 终态和故障来源投影为 debug event。
 * @return outEvent 为空或 task 没有可投影的终态时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_ProjectDebugTerminal(
        const SZrCoreTaskFrameTask *task,
        TZrBool isolatedTransport,
        struct SZrDebugAsyncTerminalEvent *outEvent);
/**
 * @brief 读取 continuation 状态；未保留 frame 时返回零。
 * @note 零也可能是有效的 continuation state ID；可用 Status 判断是否有 frame。
 */
ZR_CORE_API TZrUInt32 ZrCore_TaskFrameTask_State(
        const SZrCoreTaskFrameTask *task);
/**
 * @brief 在有效的 state ID 上保留 continuation frame。
 * @pre 应在运行中的 poll callback 返回 POLL_SUSPEND 前调用；stateId
 *      必须小于 layout->stateCount。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Suspend(struct SZrState *state,
                                                  SZrCoreTaskFrameTask *task,
                                                  TZrUInt32 stateId);
/**
 * @brief 保存或替换 layout slot；替换前会清理已初始化的旧值。
 * @pre 必须在 frame 已保留时调用（通常来自 poll/finally），并传入有效的
 *      state、slot index 和已初始化的源 value。
 * @return 根注册失败时通过正常 drop/value/root 清理回滚刚复制的 slot，返回 false；旧值已被覆盖清理。
 * TODO: 通用 Value_Copy 可复制的 ownership 种类与 direct unique 的内部镜像约束不同；
 *       接入生成 spill 时核查源是否可复制，不能由本 API 的 bool 假定任意 owner 可复制。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_StoreSlot(struct SZrState *state,
                                                    SZrCoreTaskFrameTask *task,
                                                    TZrUInt32 slotIndex,
                                                    const SZrTypeValue *value);
/**
 * @brief 在 frame 和 GC root 均有效时，复制已初始化的 slot 值。
 * @pre state、已初始化的 outValue 目标及已初始化的有效范围内 slot 均必须有效。
 * @note 先 Resolve slot root 更新地址，再按普通 Value_Copy 语义覆盖输出；输出跨 GC 使用需另持根。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_LoadSlot(struct SZrState *state,
                                                   SZrCoreTaskFrameTask *task,
                                                   TZrUInt32 slotIndex,
                                                   SZrTypeValue *outValue);
/**
 * @brief 将 task 置为故障，运行 finally、释放 frame，并保留可选的 error。
 * @pre state 和 task 必须有效；若提供 error，则其值在本次调用期间必须可读。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_Fault(struct SZrState *state,
                                                SZrCoreTaskFrameTask *task,
                                                const SZrTypeValue *error);
/**
 * @brief 将 task 置为故障，并附加经过验证的 debug async-fault 来源。
 * @pre state 和 task 必须有效；faultProvenance 必须有效；若提供 error，其值在调用期间必须可读。
 * @return 输入无效或可选 error 无法建立 root 时返回 false。
 * @note finally/frame 清理发生在 error root 注册之前；失败不保证已经发布 FAULTED，调用方仍须 Free。
 */
ZR_CORE_API TZrBool ZrCore_TaskFrameTask_FaultWithDebugProvenance(
        struct SZrState *state,
        SZrCoreTaskFrameTask *task,
        const SZrTypeValue *error,
        TZrUInt32 faultProvenance);
/**
 * @brief 查询 pending / faulted / completed 状态，并复制或转移终态值。
 * @pre 复制或转移 result / error 时必须传入有效的 state；completed task 要求
 *      outResult 已初始化。若 task 已故障且 outError 非空，该目标必须已初始化；
 *      仅查询故障状态时可将 outError 置空。
 * @note 拥有所有权的 result 只转移一次；普通值可通过多次 Await 复制。
 *       普通 GC result 的复制输出不新增独立句柄；error 输出遵循 Value_Copy 的所有权语义。
 *       task 保活不替代输出跨 task Free/GC 使用所需的根。
 * TODO: header 结果/错误读取未在此 Resolve root；核查实际移动后的地址更新入口与更强的内容断言。
 */
ZR_CORE_API EZrCoreTaskFrameAwaitStatus ZrCore_TaskFrameTask_Await(
        struct SZrState *state,
        SZrCoreTaskFrameTask *task,
        SZrTypeValue *outResult,
        SZrTypeValue *outError);

#endif /* ZR_VM_CORE_TASK_FRAME_RUNTIME_H */
