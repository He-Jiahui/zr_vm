//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_CALL_INFO_H
#define ZR_VM_CORE_CALL_INFO_H
#include "zr_vm_core/stack.h"
/** @brief 所属线程状态；这里只声明指针类型，完整状态及同步责任由 state 接口定义。 */
struct SZrState;
/** @brief 帧关联的函数元数据；此处前置声明不拥有函数存储。 */
struct SZrFunction;

/* TODO: ALLOW_HOOK 当前仅测试赋值，YIELD_CALL 未见当前首方消费；HOOK_YIELD/DECONSTRUCTOR_CALL/CLOSE_CALL
 * 仅见返回快路径断言及整体状态比较的保守排除，未见生产置位；外部 ABI 与旧续体协议尚待核查。 */
/** @brief 调用帧的执行状态位；native/VM 分类、尾调用和调试钩子共享此位集。 */
enum EZrCallStatus {
    /** @brief VM 初始化时的无附加状态；不能单凭零值认定帧已经激活。 */
    ZR_CALL_STATUS_NONE = 0,
    /** @brief 预留允许钩子位；当前测试会设置，生产钩子许可不由此位单独决定。 */
    ZR_CALL_STATUS_ALLOW_HOOK = 1 << 0,
    /** @brief native 视图判别位；VM 解释、栈恢复与异常回溯先用它选择边界。 */
    ZR_CALL_STATUS_NATIVE_CALL = 1 << 1,
    /** @brief 标记由外层调用启动的 VM 执行边界，返回时结束本次执行循环。 */
    ZR_CALL_STATUS_CREATE_FRAME = 1 << 2,
    /** @brief 钩子执行期间的重入状态；钩子结束恢复先前状态。 */
    ZR_CALL_STATUS_DEBUG_HOOK = 1 << 3,
    /** @brief 预留 yield 调用位；当前首方未见置位或按此位处理的入口。 */
    ZR_CALL_STATUS_YIELD_CALL = 1 << 4,
    /** @brief 标记尾调用复用与调试报告；失败路径会撤销临时标志。 */
    ZR_CALL_STATUS_TAIL_CALL = 1 << 5,
    /** @brief 旧钩子挂起状态位；当前返回快路径排除，未见生产置位。 */
    ZR_CALL_STATUS_HOOK_YIELD = 1 << 6,
    /** @brief 旧析构调用状态位；当前返回快路径排除，未见生产置位。 */
    ZR_CALL_STATUS_DECONSTRUCTOR_CALL = 1 << 7,
    /** @brief 钩子传输范围有效的标志，范围存放于 yieldContext 的重叠视图。 */
    ZR_CALL_STATUS_CALL_INFO_TRANSFER = 1 << 8,
    /** @brief 旧关闭调用状态位；当前返回快路径排除，未见生产置位。 */
    ZR_CALL_STATUS_CLOSE_CALL = 1 << 9,
};

typedef enum EZrCallStatus EZrCallStatus;

/** @brief 预留的 native 续体回调签名，参数为上一次线程状态及保存的调用参数。 */
typedef EZrThreadStatus (*FZrContinuationNativeFunction)(struct SZrState *state, EZrThreadStatus previousStatus,
                                                         TZrNativePtr arguments);


/** @brief VM 帧的执行位置、调试陷阱和可变参数计数；只在非 native 帧使用。 */
struct ZR_STRUCT_ALIGN SZrCallInfoContext {
    /** @brief VM 执行位置，供调试和异常定位；仅 VM 视图有效。 */
    const TZrInstruction *programCounter;
    /** @brief VM 调试陷阱信号；移动栈后重新触发检查，volatile 不提供线程同步。 */
    volatile TZrDebugSignal trap;
    /** @brief 变参窗口计数，用于返回钩子还原调用基址。 */
    TZrSize variableArgumentCount;
};

typedef struct SZrCallInfoContext SZrCallInfoContext;

/* TODO: 入口初始化置空 continuationFunction；复用 native 帧时整个 context 联合体会清零。
 * 关闭边界会检查 continuationFunction 是否为空，但未见当前首方调用或非零赋值；其余续体字段未见生产显式读写，外部 ABI 待核查。 */
/** @brief native 帧的续体元数据，与 VM 的 programCounter 等字段共用联合体存储。 */
struct ZR_STRUCT_ALIGN SZrCallInfoNativeContext {
    /** @brief 预留续体入口；关闭边界只接受空值，当前未见非零赋值或调用。 */
    FZrContinuationNativeFunction continuationFunction;
    /** @brief 旧续体错误处理位置；native 复用会清零，当前生产未见显式消费。 */
    TZrMemoryOffset previousErrorFunction;
    /** @brief 旧续体参数载荷；native 复用会清零，不据此推定载荷所有权。 */
    TZrNativePtr continuationArguments;
};

typedef struct SZrCallInfoNativeContext SZrCallInfoNativeContext;

/** @brief 按 NATIVE_CALL 状态选择 VM 或 native 帧视图；两种视图不可同时解释。 */
union TZrCallInfoContext {
    /** @brief 非 native 帧的 VM 视图；读取前先核对 NATIVE_CALL 判别位。 */
    SZrCallInfoContext context;
    /** @brief native 帧视图，与 VM 执行字段共用存储。 */
    SZrCallInfoNativeContext nativeContext;
};

typedef union TZrCallInfoContext TZrCallInfoContext;

/* TODO: functionIndex、yieldValueCount 未见生产读写，returnValueCount 在生产路径仅重置（测试验证复用）；对照生成代码和 ABI 消费者
 * 核查其是否为预留字段，避免把重叠的联合体视图当作并存状态。 */
/** @brief 复用的 yield/返回/调试转移字段；当前生产读写以 transferStart/transferCount 为主。 */
union TZrCallInfoYieldContext {
    /** @brief 预留 yield 函数位置；与其余视图重叠，当前首方未见显式读写。 */
    TZrSize functionIndex;
    /** @brief 预留 yield 数量；不能与 transfer 字段同时保存独立值。 */
    TZrUInt64 yieldValueCount;
    /** @brief 当前 VM 初始化仅将此视图清零；非零返回协议仍待外部 ABI 核查。 */
    TZrUInt64 returnValueCount;

    /** @brief 钩子传输范围的匿名视图；两字段共享同一联合体载荷。 */
    struct {
        /** @brief 钩子报告的传输起点；与数量共同解释，不是额外独立 yield 状态。 */
        TZrUInt32 transferStart;
        /** @brief 钩子报告的传输数量；使用与 transferStart 相同的传输约定。 */
        TZrUInt32 transferCount;
    };
};

typedef union TZrCallInfoYieldContext TZrCallInfoYieldContext;

/** @brief 每个线程的调用帧记录；baseCallInfo 内嵌于 state，Extend 新增记录由线程清理时释放。
 *  活动调用链沿 previous 回溯，next 可保留已退出帧供后续调用复用；GC 只扫描活动链。
 *  functionBase/functionTop、返回槽和实参源帧指向可移动栈，扩容时借助对应 reusableOffset 重定位。
 *  metadataFunction 与泛型上下文供调试、GC 和内联帧布局查询；调试帧代数区分复用后的激活。
 *  帧指针与栈指针均为所属线程执行期借用；回调、栈扩容或弹帧后应重新取活动帧和栈锚点。
 *  此记录不是异步快照，也不提供跨线程同步；异步冻结必须另行物化值和根。
 *  native 快路径也可用栈上临时帧接入活动链，其存储不归 Extend 的缓存链所有。 */
struct ZR_STRUCT_ALIGN SZrCallInfo {
    /** @brief 调用窗口的函数槽；入口边界将该槽值置空，变参返回钩子可临时调整基址。 */
    TZrStackPointer functionBase;
    /** @brief 帧存储上界；VM 根扫描与回退时使用，不等同于当前 stackTop。 */
    TZrStackPointer functionTop;

    /** @brief 帧视图和执行边界位集；读取 context 前必须遵守 native/VM 判别。 */
    EZrCallStatus callStatus;
    /** @brief 活动帧元数据缓存；供布局与 GC 查询，不能凭非空将 native 帧当作 VM。 */
    struct SZrFunction *metadataFunction;
    /** @brief 解释器类型泛型上下文值；活动链 GC 标记和迁移更新，退出帧不提供长期根。 */
    SZrTypeValue interpreterGenericContext;
    /** @brief 解释器方法泛型上下文值；复用时清空，活动链负责 GC 可达性。 */
    SZrTypeValue interpreterGenericMethodContext;

    /** @brief 活动调用链前驱；GC、异常与调试沿此链回溯，借用者不得跨弹帧持有。 */
    struct SZrCallInfo *previous;
    /** @brief 可复用后继缓存；Extend 创建的记录随 state 清理，临时 native 帧需恢复原缓存链接。 */
    struct SZrCallInfo *next;

    /** @brief 按 callStatus 解释的 VM/native 联合体；复用必须重新初始化选定视图。 */
    TZrCallInfoContext context;
    /** @brief 重叠的旧 yield/返回与调试传输视图；不可把多个成员当作独立状态。 */
    TZrCallInfoYieldContext yieldContext;

    /** @brief 调用方要求的返回数量，控制复制、补空及单结果快路径资格。 */
    TZrSize expectedReturnCount;
    /** @brief 显式返回目标；仅 hasReturnDestination 为真时使用，否则回退到 functionBase。 */
    TZrStackValuePointer returnDestination;
    /** @brief 栈重定位期间保存的返回槽偏移；不是脱离所属栈的永久地址。 */
    TZrMemoryOffset returnDestinationReusableOffset;
    /** @brief 选择显式返回槽；假时采用 functionBase，不能仅按指针空值推断。 */
    TZrBool hasReturnDestination;
    /** @brief 既有 ABI 空隙中的 24 位槽数加一缓存；零要求查询元数据，不是零槽数。 */
    TZrUInt8 frameStorageSlotCountPlusOne[3];

    /** @brief 借用调用者的原始实参帧，供内联接收者回写；不是异步冻结的独立副本。 */
    TZrStackPointer argumentSourceFrameBase;
    /** @brief 保存实参源帧的栈相对偏移，仅在有效标志为真时参与重定位。 */
    TZrMemoryOffset argumentSourceFrameBaseReusableOffset;
    /** @brief 原实参在源帧内的起点槽号，供接收者回写定位；与源帧一起有效。 */
    TZrUInt32 argumentSourceStartSlot;
    /** @brief 是否借用原实参帧；假时消费者回退到活动前驱帧。 */
    TZrBool hasArgumentSourceFrame;
    /** @brief 调试激活代数；校验时还要确认帧仍在活动链，入口清零不代表有效激活。 */
    TZrUInt64 debugFrameGeneration;
};

typedef struct SZrCallInfo SZrCallInfo;

/** @brief 按 NATIVE_CALL 状态位判定当前帧是否可作为 VM 帧解释。
 *  @pre CALL_INFO 非空，且该帧的 callStatus 已初始化。 */
#define ZR_CALL_INFO_IS_VM(CALL_INFO) ((TZrBool) (!((CALL_INFO)->callStatus & ZR_CALL_STATUS_NATIVE_CALL)))

/** @brief 解码压缩在既有对齐空隙中的帧存储槽数加一值。
 *  @return 零表示无缓存，应回退到函数元数据查询；非零值减一才是槽数。 */
ZR_FORCE_INLINE TZrUInt32 ZrCore_CallInfo_GetFrameStorageSlotCountPlusOne(
        const SZrCallInfo *callInfo) {
    if (callInfo == ZR_NULL) {
        return 0u;
    }

    return (TZrUInt32)callInfo->frameStorageSlotCountPlusOne[0] |
           ((TZrUInt32)callInfo->frameStorageSlotCountPlusOne[1] << 8u) |
           ((TZrUInt32)callInfo->frameStorageSlotCountPlusOne[2] << 16u);
}

/** @brief 读取 native 帧标志；状态位之外的调试/转移标志不影响结果。
 *  @pre callInfo 非空且 callStatus 已初始化。 */
ZR_FORCE_INLINE TZrBool ZrCore_CallInfo_IsNative(SZrCallInfo *callInfo) {
    return (TZrBool) ((callInfo->callStatus & ZR_CALL_STATUS_NATIVE_CALL) > 0);
}

/** @brief 初始化线程的入口 native 帧并清空入口函数栈槽。
 *  @pre callInfo 可写，旧帧资源已由调用方处理；functionIndex.valuePointer 为有效可写入口槽，
 *  functionTop 为同一线程的有效栈界，previous 为活动前驱或 NULL；复用边界时先保存需保留的 next。
 *  @note 不分配帧，也不发布为当前帧；整体清零丢弃旧字段，重置入口槽不负责旧资源释放。 */
ZR_CORE_API void ZrCore_CallInfo_EntryNativeInit(struct SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                                           TZrStackPointer functionTop, SZrCallInfo *previous);

/** @brief 在线程当前帧后追加可复用的帧记录，供后续 native/VM 调用初始化。
 *  @pre state 与当前 callInfoList 有效，当前记录为缓存链尾且 next 为 NULL。
 *  @return 尚未激活的新帧；内存层可能先 GC 或抛错，返回 NULL 时本函数尚未发布 next 或增加链长。
 *  @note 新帧由 state 的调用帧链持有，随线程清理释放；调用方不得单独释放。 */
ZR_CORE_API SZrCallInfo *ZrCore_CallInfo_Extend(struct SZrState *state);
#endif // ZR_VM_CORE_CALL_INFO_H
