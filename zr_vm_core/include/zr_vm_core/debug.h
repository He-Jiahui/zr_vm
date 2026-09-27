//
// Created by HeJiahui on 2025/6/19.
//

#ifndef ZR_VM_CORE_DEBUG_H
#define ZR_VM_CORE_DEBUG_H

#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/conf.h"
struct SZrState;
struct SZrCallInfo;
struct SZrTypeValue;
struct SZrObjectPrototype;
struct SZrObject;
struct SZrFunction;
struct SZrFunctionTypedTypeRef;
struct SZrClosure;
struct SZrMetadataRuntime;

/** @brief 区分调度器身份来自源码事实还是已装载的产物行。 */
typedef enum EZrDebugAsyncContractOrigin {
    ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_NONE = 0,
    ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_SOURCE_FACT,
    ZR_DEBUG_ASYNC_CONTRACT_ORIGIN_ARTIFACT_ROW
} EZrDebugAsyncContractOrigin;

/** @brief 调试视图中的附着/隔离任务终态。 */
typedef enum EZrDebugAsyncTerminalState {
    ZR_DEBUG_ASYNC_TERMINAL_NONE = 0,
    ZR_DEBUG_ASYNC_TERMINAL_ATTACHED_COMPLETED,
    ZR_DEBUG_ASYNC_TERMINAL_ATTACHED_FAULTED,
    ZR_DEBUG_ASYNC_TERMINAL_ISOLATED_COMPLETED,
    ZR_DEBUG_ASYNC_TERMINAL_ISOLATED_FAULTED
} EZrDebugAsyncTerminalState;

/** @brief 将任务失败归因到策略、传输或作业执行阶段。 */
typedef enum EZrDebugAsyncFaultProvenance {
    ZR_DEBUG_ASYNC_FAULT_NONE = 0,
    ZR_DEBUG_ASYNC_FAULT_POLICY_REJECTED,
    ZR_DEBUG_ASYNC_FAULT_TRANSPORT_PREPARE_FAILED,
    ZR_DEBUG_ASYNC_FAULT_TRANSPORT_DECODE_FAILED,
    ZR_DEBUG_ASYNC_FAULT_TRANSPORT_COMMIT_FAILED,
    ZR_DEBUG_ASYNC_FAULT_CANCELLED,
    ZR_DEBUG_ASYNC_FAULT_SHUTDOWN,
    ZR_DEBUG_ASYNC_FAULT_JOB_THROWN,
    ZR_DEBUG_ASYNC_FAULT_MAX
} EZrDebugAsyncFaultProvenance;

/*
 * Canonical scheduler identity for a debug projection. Source location is
 * deliberately absent: the active frame owns presentation-only source data.
 */
typedef struct SZrDebugAsyncSchedulerContract {
    EZrDebugAsyncContractOrigin origin;
    TZrMetadataToken schedulerTypeToken;
    TZrMetadataToken taskTypeToken;
    TZrMetadataToken jobTypeToken;
    TZrMetadataToken scheduleMemberToken;
    TZrMetadataToken scheduleSignatureToken;
    TZrUInt64 scheduleSignatureHash;
    TZrUInt32 schedulerAbiVersion;
    TZrUInt32 schedulerPolicyMask;
    TZrUInt32 attachedRequirementFlags;
    TZrUInt32 isolatedRequirementFlags;
    TZrUInt64 transportContractHash;
    TZrUInt64 schedulerContractHash;
} SZrDebugAsyncSchedulerContract;

/** @brief 任务运行状态在调试层的终态投影；仅保存标量快照。 */
typedef struct SZrDebugAsyncTerminalEvent {
    EZrDebugAsyncTerminalState terminalState;
    TZrUInt32 taskFrameStatus;
    EZrDebugAsyncFaultProvenance faultProvenance;
    TZrBool isFaulted;
} SZrDebugAsyncTerminalEvent;
/** @brief VM 向调试钩子报告的调用、返回、行与计数事件。 */
enum EZrDebugHookEvent {
    ZR_DEBUG_HOOK_EVENT_CALL,
    ZR_DEBUG_HOOK_EVENT_RETURN,
    ZR_DEBUG_HOOK_EVENT_LINE,
    ZR_DEBUG_HOOK_EVENT_COUNT,
    ZR_DEBUG_HOOK_EVENT_MAX
};

typedef enum EZrDebugHookEvent EZrDebugHookEvent;

/** @brief 与钩子事件位序对应的订阅掩码。 */
enum EZrDebugHookMask {
    ZR_DEBUG_HOOK_MASK_CALL = 1 << ZR_DEBUG_HOOK_EVENT_CALL,
    ZR_DEBUG_HOOK_MASK_RETURN = 1 << ZR_DEBUG_HOOK_EVENT_RETURN,
    ZR_DEBUG_HOOK_MASK_LINE = 1 << ZR_DEBUG_HOOK_EVENT_LINE,
    ZR_DEBUG_HOOK_MASK_COUNT = 1 << ZR_DEBUG_HOOK_EVENT_COUNT,
    ZR_DEBUG_HOOK_MASK_MAX = 1 << ZR_DEBUG_HOOK_EVENT_MAX
};

typedef enum EZrDebugHookMask EZrDebugHookMask;

/** @brief 按位请求帧元数据；PUSH_FUNCTION 还会在 VM 栈压入函数值。 */
enum EZrDebugInfoType {
    ZR_DEBUG_INFO_SOURCE_FILE = 1,
    ZR_DEBUG_INFO_LINE_NUMBER = 2,
    ZR_DEBUG_INFO_CLOSURE = 4,
    ZR_DEBUG_INFO_TAIL_CALL = 8,
    ZR_DEBUG_INFO_FUNCTION_NAME = 16,
    ZR_DEBUG_INFO_RETURN_VALUE = 32,
    /* TODO: 当前 GetInfo 无行表输出字段或填充分支；核对该位是否为遗留占位，
     * 以及调用方应否使用 GetActiveLines。 */
    ZR_DEBUG_INFO_LINE_TABLE = 64,
    ZR_DEBUG_INFO_PUSH_FUNCTION = 128,
    ZR_DEBUG_INFO_MAX = 256
};

typedef enum EZrDebugInfoType EZrDebugInfoType;

/** @brief 调试名称所属的可见作用域。 */
enum EZrDebugScope {
    ZR_DEBUG_SCOPE_GLOBAL,
    ZR_DEBUG_SCOPE_LOCAL,
    ZR_DEBUG_SCOPE_CLOSURE,
    ZR_DEBUG_SCOPE_FUNCTION,
    ZR_DEBUG_SCOPE_MAX
};

typedef enum EZrDebugScope EZrDebugScope;

/** @brief 名称在运行时帧中的语义类别。 */
enum EZrDebugNameWhat {
    ZR_DEBUG_NAMEWHAT_UNKNOWN = 0,
    ZR_DEBUG_NAMEWHAT_GLOBAL,
    ZR_DEBUG_NAMEWHAT_LOCAL,
    ZR_DEBUG_NAMEWHAT_FIELD,
    ZR_DEBUG_NAMEWHAT_METHOD,
    ZR_DEBUG_NAMEWHAT_UPVALUE
};

typedef enum EZrDebugNameWhat EZrDebugNameWhat;

/** @brief 借用当前调用帧和函数；帧弹出后不得继续使用。 */
struct ZR_STRUCT_ALIGN SZrDebugActivation {
    struct SZrCallInfo *callInfo;
    struct SZrFunction *function;
};

typedef struct SZrDebugActivation SZrDebugActivation;

/** @brief 暂停帧求值失败时区分参数错误、过期帧和元数据缺失。 */
typedef enum EZrDebugEvaluationContextStatus {
    ZR_DEBUG_EVALUATION_CONTEXT_STATUS_OK = 0,
    ZR_DEBUG_EVALUATION_CONTEXT_STATUS_INVALID_ARGUMENT,
    ZR_DEBUG_EVALUATION_CONTEXT_STATUS_STALE_FRAME,
    ZR_DEBUG_EVALUATION_CONTEXT_STATUS_METADATA_UNAVAILABLE,
    ZR_DEBUG_EVALUATION_CONTEXT_STATUS_NO_MORE_BINDINGS,
    ZR_DEBUG_EVALUATION_CONTEXT_STATUS_NO_RECEIVER
} EZrDebugEvaluationContextStatus;

/** @brief 将活跃局部变量的栈槽与规范符号、类型和源码位置关联。 */
typedef struct SZrDebugFrameBinding {
    TZrUInt32 stackSlot;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 placeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
    TZrUInt32 scopeDepth;
    TZrUInt32 roleFlags;
} SZrDebugFrameBinding;

/** @brief 暂停帧快照；generation 与指令偏移必须在后续解析时保持一致。 */
typedef struct SZrDebugEvaluationContext {
    SZrDebugActivation activation;
    TZrUInt64 frameGeneration;
    TZrUInt32 instructionOffset;
    TZrUInt32 activeBindingCount;
    TZrBool hasGenericContext;
    TZrBool hasGenericMethodContext;
    TZrUInt16 reserved0;
} SZrDebugEvaluationContext;

/** @brief 调试求值允许显式解析的运行时根类别。 */
typedef enum EZrDebugRuntimeRootKind {
    ZR_DEBUG_RUNTIME_ROOT_ZR = 1
} EZrDebugRuntimeRootKind;

/** @brief 运行时根的帧代数令牌；解析前必须重新验证。 */
typedef struct SZrDebugRuntimeRootBinding {
    EZrDebugRuntimeRootKind kind;
    TZrUInt64 token;
} SZrDebugRuntimeRootBinding;

/*
 * A borrowed closure capture selected from the exact paused VM closure.
 * `type` remains valid only while the same evaluation context is current.
 */
typedef struct SZrDebugClosureCaptureBinding {
    TZrUInt32 captureIndex;
    const struct SZrFunctionTypedTypeRef *type;
    TZrUInt32 symbolId;
    TZrUInt32 typeId;
    TZrUInt32 declarationStartLine;
    TZrUInt32 declarationStartColumn;
    TZrUInt32 declarationEndLine;
    TZrUInt32 declarationEndColumn;
    TZrUInt64 token;
} SZrDebugClosureCaptureBinding;

/** @brief 区分类泛型上下文与方法泛型上下文。 */
typedef enum EZrDebugGenericContextKind {
    ZR_DEBUG_GENERIC_CONTEXT_TYPE = 0,
    ZR_DEBUG_GENERIC_CONTEXT_METHOD
} EZrDebugGenericContextKind;

/*
 * Borrowed reflection type object resolved for the currently paused frame.
 * The object remains valid only while the evaluation context remains current.
 */
typedef struct SZrDebugGenericArgument {
    EZrDebugGenericContextKind contextKind;
    TZrMetadataToken ownerToken;
    TZrUInt32 parameterIndex;
    struct SZrObject *typeObject;
} SZrDebugGenericArgument;

/** @brief 钩子和栈回溯共用的借用帧信息；字符串随所属函数存活。 */
struct ZR_STRUCT_ALIGN SZrDebugInfo {
    EZrDebugHookEvent event;

    TZrNativeString name;
    EZrDebugScope scope;

    TZrBool isNative;

    TZrNativeString source;

    TZrSize sourceLength;

    TZrSize currentLine;
    TZrSize definedLineStart;
    TZrSize definedLineEnd;
    TZrSize closureValuesCount;
    TZrSize parametersCount;

    TZrBool hasVariableParameters;
    TZrBool isTailCall;

    TZrUInt32 transferStart;
    TZrUInt32 transferCount;
    struct SZrCallInfo *callInfo;
    EZrDebugNameWhat nameWhat;
};

typedef struct SZrDebugInfo SZrDebugInfo;

/** @brief 钩子在 VM 解锁期间执行，返回前不得保留临时 debugInfo。 */
typedef void (*FZrDebugHook)(struct SZrState *state, SZrDebugInfo *debugInfo);

/** @brief 每条 VM 指令上的外部观察者，可返回调试 trap 信号。 */
typedef TZrDebugSignal (*FZrDebugTraceObserver)(struct SZrState *state,
                                                struct SZrFunction *function,
                                                const TZrInstruction *programCounter,
                                                TZrUInt32 instructionOffset,
                                                TZrUInt32 sourceLine,
                                                TZrPtr userData);


/** @brief 查询栈顶帧；type 为零时包含 PUSH_FUNCTION 并在 VM 栈压入函数值。
 * @note 查询可能扩栈；调用方应保存偏移并在调用后重新取得栈指针。 */
ZR_CORE_API TZrBool ZrCore_DebugInfo_Get(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo);

/** @brief 按可调试帧层级取得借用的活动帧。 */
ZR_CORE_API TZrBool ZrCore_Debug_GetStack(struct SZrState *state,
                                          TZrUInt32 level,
                                          SZrDebugActivation *outActivation);

/** @brief 固定暂停帧的代数、指令偏移与活跃绑定计数。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_GetEvaluationContext(
        struct SZrState *state,
        TZrUInt32 level,
        SZrDebugEvaluationContext *outContext);

/** @brief 按活动序号读取规范局部变量身份；会拒绝过期帧。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetBinding(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        TZrUInt32 activeBindingIndex,
        SZrDebugFrameBinding *outBinding);

/** @brief 解析当前接收者；返回值为借用快照，内联结构可产生对象副本。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetReceiver(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        SZrDebugFrameBinding *outBinding,
        struct SZrTypeValue *outValue);

/** @brief 取得运行时根令牌，供同一暂停帧内再次解析。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetRuntimeRoot(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        EZrDebugRuntimeRootKind kind,
        SZrDebugRuntimeRootBinding *outBinding);

/** @brief 校验令牌及暂停帧后复制运行时根的借用值。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_ResolveRuntimeRoot(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        const SZrDebugRuntimeRootBinding *binding,
        struct SZrTypeValue *outValue);

/** @brief 从实际 VM 闭包取得捕获项身份和帧代数令牌。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetClosureCapture(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        TZrUInt32 captureIndex,
        SZrDebugClosureCaptureBinding *outBinding);

/** @brief 复核捕获项身份后读取借用值，防止旧令牌误指新帧。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_ResolveClosureCapture(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        const SZrDebugClosureCaptureBinding *binding,
        struct SZrTypeValue *outValue);

/** @brief 在当前帧的泛型上下文中解析反射类型对象。 */
ZR_CORE_API EZrDebugEvaluationContextStatus ZrCore_Debug_EvaluationContext_GetGenericArgument(
        struct SZrState *state,
        const SZrDebugEvaluationContext *context,
        struct SZrMetadataRuntime *runtime,
        EZrDebugGenericContextKind contextKind,
        TZrMetadataToken ownerToken,
        TZrUInt32 parameterIndex,
        SZrDebugGenericArgument *outArgument);

/** @brief 按掩码读取借用帧信息；PUSH_FUNCTION 额外占用一个 VM 栈槽。
 * @note PUSH_FUNCTION 可能扩栈；调用方应保存栈偏移，并在调用后重新取指针。 */
ZR_CORE_API TZrBool ZrCore_Debug_GetInfo(struct SZrState *state,
                                         const SZrDebugActivation *activation,
                                         EZrDebugInfoType type,
                                         SZrDebugInfo *outInfo);

/** @brief 设置钩子并同步当前 VM 帧的逐指令 trap 状态。 */
ZR_CORE_API void ZrCore_Debug_SetHook(struct SZrState *state,
                                      FZrDebugHook hook,
                                      TZrUInt32 mask,
                                      TZrUInt32 count);

/** @brief 读取当前钩子回调。 */
ZR_CORE_API FZrDebugHook ZrCore_Debug_GetHook(struct SZrState *state);

/** @brief 读取当前事件订阅掩码。 */
ZR_CORE_API TZrUInt32 ZrCore_Debug_GetHookMask(struct SZrState *state);

/** @brief 读取计数钩子的基础间隔。 */
ZR_CORE_API TZrUInt32 ZrCore_Debug_GetHookCount(struct SZrState *state);

/** @brief 返回去重后的源码行总数，并按容量写入排序后的行号。 */
ZR_CORE_API TZrSize ZrCore_Debug_GetActiveLines(const struct SZrFunction *function,
                                                TZrUInt32 *outLines,
                                                TZrSize lineCapacity);

/** @brief 将函数指令及源码行映射打印到调用方提供的流。 */
ZR_CORE_API void ZrCore_Debug_DisassembleFunction(struct SZrState *state,
                                                  const struct SZrFunction *function,
                                                  FILE *output);

/** @brief 将当前堆对象和 GC 统计快照输出到调用方提供的流。 */
ZR_CORE_API void ZrCore_Debug_HeapSummary(struct SZrState *state, FILE *output);

/** @brief 按一基序号读取活跃局部变量；普通值借用，内联结构会物化对象。 */
ZR_CORE_API TZrNativeString ZrCore_Debug_GetLocal(struct SZrState *state,
                                                  const SZrDebugActivation *activation,
                                                  TZrInt32 localIndex,
                                                  struct SZrTypeValue *outValue);

/** @brief 更新活跃非内联局部变量，成功时返回借用的变量名。 */
ZR_CORE_API TZrNativeString ZrCore_Debug_SetLocal(struct SZrState *state,
                                                  const SZrDebugActivation *activation,
                                                  TZrInt32 localIndex,
                                                  const struct SZrTypeValue *value);

/** @brief 按一基序号读取闭包捕获项名及借用值。 */
ZR_CORE_API TZrNativeString ZrCore_Debug_GetUpvalue(struct SZrState *state,
                                                    struct SZrClosure *closure,
                                                    TZrInt32 upvalueIndex,
                                                    struct SZrTypeValue *outValue);

/** @brief 更新闭包捕获值，成功时返回借用的捕获项名。 */
ZR_CORE_API TZrNativeString ZrCore_Debug_SetUpvalue(struct SZrState *state,
                                                    struct SZrClosure *closure,
                                                    TZrInt32 upvalueIndex,
                                                    const struct SZrTypeValue *value);

/** @brief 返回捕获槽的借用身份地址；闭包释放后失效。 */
ZR_CORE_API TZrPtr ZrCore_Debug_GetUpvalueId(struct SZrState *state,
                                             struct SZrClosure *closure,
                                             TZrInt32 upvalueIndex);

/** @brief 注册逐指令观察者及借用 userData，生命周期由调用方管理。 */
ZR_CORE_API void ZrCore_Debug_SetTraceObserver(struct SZrState *state,
                                               FZrDebugTraceObserver observer,
                                               TZrPtr userData);

/** @brief 在有限缓冲区构造零终止回溯，返回实际写入的字节数。 */
ZR_CORE_API TZrSize ZrCore_Debug_Traceback(struct SZrState *state,
                                           TZrNativeString prefixMessage,
                                           TZrUInt32 level,
                                           TZrUInt32 maxFrames,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);

/** @brief 将非可调用值转为 VM 运行时异常；不返回。 */
ZR_CORE_API ZR_NO_RETURN void ZrCore_Debug_CallError(struct SZrState *state, struct SZrTypeValue *value);

/** @brief 在 VM 指令边界分发行/计数钩子和外部观察者。 */
ZR_CORE_API TZrDebugSignal ZrCore_Debug_TraceExecution(struct SZrState *state, const TZrInstruction *programCounter);

/** @brief 规范化错误对象并进入 VM 异常链；不返回。 */
ZR_CORE_API ZR_NO_RETURN void ZrCore_Debug_RunError(struct SZrState *state, TZrNativeString format, ...);

/** @brief 错误处理期间再次失败时调用 panic 处理器并中止。 */
ZR_CORE_API ZR_NO_RETURN void ZrCore_Debug_ErrorWhenHandlingError(struct SZrState *state);

/** @brief 暂时解锁 VM 调用外部钩子，返回后恢复帧栈状态。 */
ZR_CORE_API void ZrCore_Debug_Hook(struct SZrState *state, EZrDebugHookEvent event, TZrUInt32 line, TZrUInt32 transferStart,
                             TZrUInt32 transferCount);

/** @brief 在返回时提供结果传输区给钩子，并恢复调用者指令位置。 */
ZR_CORE_API void ZrCore_Debug_HookReturn(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount);

/** @brief 将 prototype 的类型、继承链、字段及 Meta 方法写入调用方流。 */
ZR_CORE_API void ZrCore_Debug_PrintPrototype(struct SZrState *state, struct SZrObjectPrototype *prototype, FILE *output);

/** @brief 将对象 prototype 与有上限的字段值写入调用方流。 */
ZR_CORE_API void ZrCore_Debug_PrintObject(struct SZrState *state, struct SZrObject *object, FILE *output);

/** @brief 从函数 prototypeData 解码并输出类似 zri 的诊断文本。 */
ZR_CORE_API void ZrCore_Debug_PrintPrototypesFromData(struct SZrState *state, struct SZrFunction *entryFunction,
                                                FILE *output);
/** @brief 经规范投影把产物类型 ID 解析为调试可见类型。 */
ZR_CORE_API EZrArtifactStatus ZrCore_Debug_ResolveArtifactType(
        const SZrCanonicalConsumerProjection *projection,
        TZrUInt32 canonicalTypeId,
        SZrCanonicalTypeProjection *outType,
        SZrArtifactDiagnostic *diagnostic);

/** @brief 从函数源码事实投影带来源标记的调度器身份。 */
ZR_CORE_API TZrBool ZrCore_Debug_ProjectSchedulerSourceContract(
        const struct SZrFunction *function,
        TZrUInt32 schedulerTypeId,
        SZrDebugAsyncSchedulerContract *outContract);
/** @brief 从产物行投影调度器身份；失败时清零输出。 */
ZR_CORE_API TZrBool ZrCore_Debug_ProjectSchedulerArtifactContract(
        const SZrArtifactSchedulerContractRow *row,
        SZrDebugAsyncSchedulerContract *outContract);
/** @brief 比较源码/产物调度器的共同身份字段。 */
ZR_CORE_API TZrBool ZrCore_Debug_AsyncSchedulerContractsEqual(
        const SZrDebugAsyncSchedulerContract *left,
        const SZrDebugAsyncSchedulerContract *right);
/** @brief 将任务帧终态与失败来源投影到调试事件。 */
ZR_CORE_API TZrBool ZrCore_Debug_ProjectTaskFrameTerminal(
        TZrUInt32 taskFrameStatus,
        TZrBool isolatedTransport,
        EZrDebugAsyncFaultProvenance faultProvenance,
        SZrDebugAsyncTerminalEvent *outEvent);

#endif // ZR_VM_CORE_DEBUG_H
