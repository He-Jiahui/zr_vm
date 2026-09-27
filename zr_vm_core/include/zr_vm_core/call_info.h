//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_CALL_INFO_H
#define ZR_VM_CORE_CALL_INFO_H
#include "zr_vm_core/stack.h"
struct SZrState;
struct SZrFunction;

/* TODO: ALLOW_HOOK 仅在测试设置，YIELD_CALL 无使用，HOOK_YIELD/DECONSTRUCTOR_CALL/CLOSE_CALL
 * 仅被返回快路径断言读取；对照 ABI 与跨语言消费者核查这些位是否仍需保留。 */
/** @brief 调用帧的执行状态位；native/VM 分类、尾调用和调试钩子共享此位集。 */
enum EZrCallStatus {
    ZR_CALL_STATUS_NONE = 0,
    ZR_CALL_STATUS_ALLOW_HOOK = 1 << 0,
    ZR_CALL_STATUS_NATIVE_CALL = 1 << 1,
    ZR_CALL_STATUS_CREATE_FRAME = 1 << 2,
    ZR_CALL_STATUS_DEBUG_HOOK = 1 << 3,
    ZR_CALL_STATUS_YIELD_CALL = 1 << 4,
    ZR_CALL_STATUS_TAIL_CALL = 1 << 5,
    ZR_CALL_STATUS_HOOK_YIELD = 1 << 6,
    ZR_CALL_STATUS_DECONSTRUCTOR_CALL = 1 << 7,
    ZR_CALL_STATUS_CALL_INFO_TRANSFER = 1 << 8,
    ZR_CALL_STATUS_CLOSE_CALL = 1 << 9,
};

typedef enum EZrCallStatus EZrCallStatus;

/** @brief 预留的 native 续体回调签名，参数为上一次线程状态及保存的调用参数。 */
typedef EZrThreadStatus (*FZrContinuationNativeFunction)(struct SZrState *state, EZrThreadStatus previousStatus,
                                                         TZrNativePtr arguments);


/** @brief VM 帧的执行位置、调试陷阱和可变参数计数；只在非 native 帧使用。 */
struct ZR_STRUCT_ALIGN SZrCallInfoContext {
    const TZrInstruction *programCounter;
    volatile TZrDebugSignal trap;
    TZrSize variableArgumentCount;
};

typedef struct SZrCallInfoContext SZrCallInfoContext;

/* TODO: continuationFunction 仅在入口初始化时清空，其他 native 续体字段仅在测试中赋值；
 * 核查旧续体协议或外部 ABI 消费者后决定保留、接线或移除。 */
/** @brief native 帧的续体元数据，与 VM 的 programCounter 等字段共用联合体存储。 */
struct ZR_STRUCT_ALIGN SZrCallInfoNativeContext {
    FZrContinuationNativeFunction continuationFunction;
    TZrMemoryOffset previousErrorFunction;
    TZrNativePtr continuationArguments;
};

typedef struct SZrCallInfoNativeContext SZrCallInfoNativeContext;

/** @brief 按 NATIVE_CALL 状态选择 VM 或 native 帧视图；两种视图不可同时解释。 */
union TZrCallInfoContext {
    SZrCallInfoContext context;
    SZrCallInfoNativeContext nativeContext;
};

typedef union TZrCallInfoContext TZrCallInfoContext;

/* TODO: functionIndex、yieldValueCount 未见生产读写，returnValueCount 仅被重置；对照生成代码和 ABI 消费者
 * 核查其是否为预留字段，避免把重叠的联合体视图当作并存状态。 */
/** @brief 复用的 yield/返回/调试转移字段；当前生产读写以 transferStart/transferCount 为主。 */
union TZrCallInfoYieldContext {
    TZrSize functionIndex;
    TZrUInt64 yieldValueCount;
    TZrUInt64 returnValueCount;

    struct {
        TZrUInt32 transferStart;
        TZrUInt32 transferCount;
    };
};

typedef union TZrCallInfoYieldContext TZrCallInfoYieldContext;

/** @brief 每个线程的调用帧记录；baseCallInfo 内嵌于 state，Extend 新增记录由线程清理时释放。
 *  活动调用链沿 previous 回溯，next 可保留已退出帧供后续调用复用；GC 只扫描活动链。
 *  functionBase/functionTop、返回槽和实参源帧指向可移动栈，扩容时借助对应 reusableOffset 重定位。
 *  metadataFunction 与泛型上下文供调试、GC 和内联帧布局查询；调试帧代数区分复用后的激活。
 *  native 快路径也可用栈上临时帧接入活动链，其存储不归 Extend 的缓存链所有。 */
struct ZR_STRUCT_ALIGN SZrCallInfo {
    // base is always point to the function closure which is current called
    TZrStackPointer functionBase;
    TZrStackPointer functionTop;

    EZrCallStatus callStatus;
    struct SZrFunction *metadataFunction;
    SZrTypeValue interpreterGenericContext;
    SZrTypeValue interpreterGenericMethodContext;

    struct SZrCallInfo *previous;
    struct SZrCallInfo *next;

    TZrCallInfoContext context;
    TZrCallInfoYieldContext yieldContext;

    TZrSize expectedReturnCount;
    /** 返回值写入的栈槽；ZR_NULL 时 ZrFunctionMoveReturns 使用 functionBase.valuePointer（兼容旧语义） */
    TZrStackValuePointer returnDestination;
    TZrMemoryOffset returnDestinationReusableOffset;
    TZrBool hasReturnDestination;
    /* count + 1 in legacy ABI padding; zero preserves the metadata-scan fallback. */
    TZrUInt8 frameStorageSlotCountPlusOne[3];

    TZrStackPointer argumentSourceFrameBase;
    TZrMemoryOffset argumentSourceFrameBaseReusableOffset;
    TZrUInt32 argumentSourceStartSlot;
    TZrBool hasArgumentSourceFrame;
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
 *  @pre callInfo 可写且尚未进入活动链，functionIndex.valuePointer 指向有效可写的空入口栈槽；
 *  functionTop 属于同一线程的栈，previous 为调用链前驱或 NULL。
 *  @note 不分配帧；入口栈槽只重置字段，不释放此前持有的外部资源。 */
ZR_CORE_API void ZrCore_CallInfo_EntryNativeInit(struct SZrState *state, SZrCallInfo *callInfo, TZrStackPointer functionIndex,
                                           TZrStackPointer functionTop, SZrCallInfo *previous);

/** @brief 在线程当前帧后追加可复用的帧记录，供后续 native/VM 调用初始化。
 *  @pre state 与当前 callInfoList 有效，当前记录为缓存链尾且 next 为 NULL。
 *  @return 新帧；分配失败时内存层可能抛错，若返回 NULL 则原帧链保持不变。
 *  @note 新帧由 state 的调用帧链持有，随线程清理释放；调用方不得单独释放。 */
ZR_CORE_API SZrCallInfo *ZrCore_CallInfo_Extend(struct SZrState *state);
#endif // ZR_VM_CORE_CALL_INFO_H
