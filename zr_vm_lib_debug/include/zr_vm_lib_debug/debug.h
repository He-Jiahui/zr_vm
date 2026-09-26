#ifndef ZR_VM_DEBUG_DEBUG_H
#define ZR_VM_DEBUG_DEBUG_H

#include "zr_vm_lib_debug/conf.h"

struct SZrFunction;

/** @brief VM state、协议监听器、断点与停点快照的宿主级调试会话。 */
typedef struct ZrDebugAgent ZrDebugAgent;

/** @brief 协议停止事件的原因分类；结束通知由独立 terminated 事件发送。 */
typedef enum EZrDebugStopReason {
    ZR_DEBUG_STOP_REASON_NONE = 0,
    ZR_DEBUG_STOP_REASON_ENTRY = 1,
    ZR_DEBUG_STOP_REASON_BREAKPOINT = 2,
    ZR_DEBUG_STOP_REASON_PAUSE = 3,
    ZR_DEBUG_STOP_REASON_STEP = 4,
    ZR_DEBUG_STOP_REASON_EXCEPTION = 5,
    ZR_DEBUG_STOP_REASON_TERMINATED = 6,
    ZR_DEBUG_STOP_REASON_DATA_BREAKPOINT = 7
} EZrDebugStopReason;

/** @brief 帧变量视图类别，用于 ReadScopes 与 ReadVariables 的请求关联。 */
typedef enum EZrDebugScopeKind {
    ZR_DEBUG_SCOPE_KIND_ARGUMENTS = 1,
    ZR_DEBUG_SCOPE_KIND_LOCALS = 2,
    ZR_DEBUG_SCOPE_KIND_CLOSURES = 3,
    ZR_DEBUG_SCOPE_KIND_GLOBALS = 4,
    ZR_DEBUG_SCOPE_KIND_PROTOTYPE = 5,
    ZR_DEBUG_SCOPE_KIND_STATICS = 6,
    ZR_DEBUG_SCOPE_KIND_EXCEPTION = 7
} EZrDebugScopeKind;

/** @brief 行断点与函数入口断点共享的注册规格。 */
typedef enum EZrDebugBreakpointKind {
    ZR_DEBUG_BREAKPOINT_KIND_LINE = 1,
    ZR_DEBUG_BREAKPOINT_KIND_FUNCTION = 2
} EZrDebugBreakpointKind;

/** @brief 对捕获/未捕获异常停止事件的订阅条件。 */
typedef enum EZrDebugExceptionFilter {
    ZR_DEBUG_EXCEPTION_FILTER_NONE = 0,
    ZR_DEBUG_EXCEPTION_FILTER_CAUGHT = 1,
    ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT = 2
} EZrDebugExceptionFilter;

/**
 * @brief 启动调试监听器及初始暂停行为的配置。
 *
 * 空地址使用 loopback 临时端口；Agent 按值复制配置，其中鉴权令牌仍是
 * 借用指针，宿主须使其在 AgentStop 前保持有效。
 */
typedef struct ZrDebugAgentConfig {
    const TZrChar *address;
    TZrBool suspend_on_start;
    TZrBool wait_for_client;
    const TZrChar *auth_token;
    TZrBool stop_on_uncaught_exception;
} ZrDebugAgentConfig;

/** @brief 宿主提交的断点规格；Agent 会复制字符串字段到固定容量记录。 */
typedef struct ZrDebugBreakpointSpec {
    EZrDebugBreakpointKind kind;
    const TZrChar *module_name;
    const TZrChar *source_file;
    TZrUInt32 line;
    const TZrChar *function_name;
    const TZrChar *condition;
    const TZrChar *hit_condition;
    const TZrChar *log_message;
} ZrDebugBreakpointSpec;

/** @brief 一次停点的按值事件快照，字符串可能受固定容量截断。 */
typedef struct ZrDebugStopEvent {
    EZrDebugStopReason reason;
    EZrDebugExceptionFilter exception_filter;
    TZrUInt32 thread_id;
    TZrUInt32 line;
    TZrUInt32 instruction_index;
    TZrUInt64 state_id;
    TZrChar module_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar data_id[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar data_description[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar exception_stack[ZR_DEBUG_EXCEPTION_STACK_CAPACITY];
} ZrDebugStopEvent;

/** @brief 停点堆栈帧快照，含源码、receiver、异步及测试契约元数据。 */
typedef struct ZrDebugFrameSnapshot {
    TZrUInt32 thread_id;
    TZrUInt32 frame_id;
    TZrUInt32 line;
    TZrUInt32 instruction_index;
    TZrUInt32 frame_depth;
    TZrUInt32 receiver_variables_reference;
    TZrUInt32 argument_count;
    TZrInt32 return_slot;
    TZrBool is_exception_frame;
    TZrBool has_async_contract;
    TZrUInt32 async_contract_origin;
    TZrMetadataToken async_scheduler_type_token;
    TZrMetadataToken async_task_type_token;
    TZrMetadataToken async_job_type_token;
    TZrMetadataToken async_schedule_member_token;
    TZrMetadataToken async_schedule_signature_token;
    TZrUInt64 async_schedule_signature_hash;
    TZrUInt32 async_scheduler_abi_version;
    TZrUInt32 async_scheduler_policy_mask;
    TZrUInt32 async_attached_requirement_flags;
    TZrUInt32 async_isolated_requirement_flags;
    TZrUInt64 async_transport_contract_hash;
    TZrUInt64 async_scheduler_contract_hash;
    TZrBool has_test_contract;
    TZrBool test_is_async;
    TZrUInt32 test_function_symbol_id;
    TZrUInt32 test_function_type_id;
    TZrUInt32 test_case_count;
    TZrChar test_qualified_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar module_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar call_kind[ZR_DEBUG_NAME_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar receiver_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar receiver_type_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar receiver_value_text[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugFrameSnapshot;

/** @brief 从入口函数 TestManifest 复制给客户端的测试项。 */
typedef struct ZrDebugTestEntrySnapshot {
    TZrUInt32 function_symbol_id;
    TZrUInt32 function_type_id;
    TZrUInt32 callable_child_index;
    TZrUInt32 case_count;
    TZrBool is_async;
    TZrChar module_id[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar qualified_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar skip_reason[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugTestEntrySnapshot;

/** @brief 帧内可展开作用域；scope_id 由帧 ID 与作用域种类编码。 */
typedef struct ZrDebugScopeSnapshot {
    TZrUInt32 thread_id;
    TZrUInt32 scope_id;
    TZrUInt32 frame_id;
    EZrDebugScopeKind kind;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
} ZrDebugScopeSnapshot;

/**
 * @brief 变量树中的一个展示节点。
 *
 * `variables_reference` 只在当前停点有效；继续执行或产生下一停点后
 * 不能再次提交旧句柄。名称、值和语义摘要为受容量限制的快照文本。
 */
typedef struct ZrDebugValuePreview {
    TZrUInt32 variables_reference;
    TZrSize named_variables;
    TZrSize indexed_variables;
    EZrDebugScopeKind scope_kind;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar type_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar value_text[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar semantic_summary[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar reference_summary[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugValuePreview;

/** @brief 暂停帧求值的展示结果与可展开句柄；句柄仅限当前停点。 */
typedef struct ZrDebugEvaluateResult {
    TZrUInt64 state_id;
    TZrUInt32 canonical_type_id;
    TZrBool has_canonical_type;
    TZrUInt32 variables_reference;
    TZrSize named_variables;
    TZrSize indexed_variables;
    TZrChar type_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar value_text[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar semantic_summary[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar reference_summary[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugEvaluateResult;

/** @brief 求值失败所属阶段，供协议层构造可解释诊断。 */
typedef enum EZrDebugEvaluateFailureKind {
    ZR_DEBUG_EVALUATE_FAILURE_NONE = 0,
    ZR_DEBUG_EVALUATE_FAILURE_REQUEST,
    ZR_DEBUG_EVALUATE_FAILURE_PARSER,
    ZR_DEBUG_EVALUATE_FAILURE_SEMANTIC,
    ZR_DEBUG_EVALUATE_FAILURE_CAPABILITY,
    ZR_DEBUG_EVALUATE_FAILURE_CANONICAL_FACTS,
    ZR_DEBUG_EVALUATE_FAILURE_FORMAL_EXECUTION,
    ZR_DEBUG_EVALUATE_FAILURE_LEGACY_COMPATIBILITY
} EZrDebugEvaluateFailureKind;

/** @brief 求值失败的稳定代码、范围、原因和修复提示快照。 */
typedef struct ZrDebugEvaluateFailure {
    EZrDebugEvaluateFailureKind kind;
    TZrUInt64 state_id;
    TZrUInt32 descriptor_id;
    TZrSize range_start_offset;
    TZrSize range_end_offset;
    TZrChar code[ZR_DEBUG_NAME_CAPACITY];
    TZrChar message[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar cause[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar suggestion[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugEvaluateFailure;

/** @brief 暂停求值可涉及的作用类别，供能力白名单检查。 */
typedef enum EZrDebugEvaluationEffect {
    ZR_DEBUG_EVALUATION_EFFECT_NONE = 0u,
    ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER = 1u << 0u,
    ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION = 1u << 1u,
    ZR_DEBUG_EVALUATION_EFFECT_CALL = 1u << 2u,
    ZR_DEBUG_EVALUATION_EFFECT_NATIVE_CALL = 1u << 3u,
    ZR_DEBUG_EVALUATION_EFFECT_MUTATION = 1u << 4u,
    ZR_DEBUG_EVALUATION_EFFECT_OWNER_MUTATION = 1u << 5u
} EZrDebugEvaluationEffect;

/** @brief 表达式经语义分析得到的作用位集及规范事实可用性。 */
typedef struct ZrDebugEvaluationEffectPolicy {
    TZrUInt32 effectFlags;
    TZrBool isPure;
    TZrBool hasCanonicalFacts;
} ZrDebugEvaluationEffectPolicy;

/**
 * @brief 按配置启动调试监听器，并为入口函数所在 VM state 建立 Agent。
 *
 * @pre `state`、`entryFunction`、`outAgent` 非空且前两者存活到 AgentStop。
 * @return 成功时 `*outAgent` 由调用方持有；失败时清零并释放已建资源。
 */
ZR_DEBUG_API TZrBool ZrDebug_AgentStart(SZrState *state, struct SZrFunction *entryFunction, const TZrChar *moduleName,
                                        const ZrDebugAgentConfig *config, ZrDebugAgent **outAgent, TZrChar *errorBuffer,
                                        TZrSize errorBufferSize);

/** @brief 解除 VM 回调、关闭客户端/监听器并释放 Agent；可传空指针。 */
ZR_DEBUG_API void ZrDebug_AgentStop(ZrDebugAgent *agent);

/** @brief 将实际监听端点复制到缓冲区，供 zrdbg 客户端或 DAP 桥连接。 */
ZR_DEBUG_API TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize);

/** @brief 替换行断点集合；函数断点由独立入口设置，规格会复制进 Agent。 */
ZR_DEBUG_API TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count);
/** @brief 替换函数入口断点集合，不清除当前行断点。 */
ZR_DEBUG_API TZrBool ZrDebug_SetFunctionBreakpoints(ZrDebugAgent *agent,
                                                    const ZrDebugBreakpointSpec *specs,
                                                    TZrSize count);
/** @brief 设置捕获/未捕获异常的停点订阅。 */
ZR_DEBUG_API void ZrDebug_SetExceptionBreakpoints(ZrDebugAgent *agent, TZrBool caught, TZrBool uncaught);

/** @brief 从暂停状态继续执行并使旧变量句柄失效。 */
ZR_DEBUG_API void ZrDebug_Continue(ZrDebugAgent *agent);
/** @brief 请求正在执行的 VM 在下个 trace 观察点暂停。 */
ZR_DEBUG_API void ZrDebug_Pause(ZrDebugAgent *agent);
/** @brief 继续到下一条可观察语句，进入被调用函数。 */
ZR_DEBUG_API void ZrDebug_StepInto(ZrDebugAgent *agent);
/** @brief 继续到当前帧的下一条可观察语句。 */
ZR_DEBUG_API void ZrDebug_StepOver(ZrDebugAgent *agent);
/** @brief 继续运行直到离开当前帧。 */
ZR_DEBUG_API void ZrDebug_StepOut(ZrDebugAgent *agent);

/**
 * @brief 读取当前运行栈或异常对象保存的栈快照，供协议层响应 stackTrace。
 * @pre 调用方须确保 VM 栈稳定（通常在停点）；函数本身不验证暂停状态。
 * @return 成功时数组由调用方用 ZrDebug_Free 释放；空栈可返回空数组。
 */
ZR_DEBUG_API TZrBool ZrDebug_ReadStack(ZrDebugAgent *agent, ZrDebugFrameSnapshot **outFrames, TZrSize *outCount);

/** @brief 复制入口 TestManifest；成功数组由调用方用 ZrDebug_Free 释放。 */
ZR_DEBUG_API TZrBool ZrDebug_ReadTestManifest(ZrDebugAgent *agent,
                                              ZrDebugTestEntrySnapshot **outEntries,
                                              TZrSize *outCount);

/**
 * @brief 为当前帧列出可展开作用域。
 * @pre 使用本停点的有效 frameId；成功数组由调用方用 ZrDebug_Free 释放。
 */
ZR_DEBUG_API TZrBool ZrDebug_ReadScopes(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugScopeSnapshot **outScopes,
                                        TZrSize *outCount);

/**
 * @brief 展开作用域或对支持分页的变量句柄取窗口，并返回 named/indexed 总数。
 * @pre 作用域 ID 应取自当前 ReadScopes 结果；动态变量句柄仅在创建它的停点有效。
 * 结果数组由调用方用 ZrDebug_Free 释放。
 */
ZR_DEBUG_API TZrBool ZrDebug_ReadVariables(ZrDebugAgent *agent,
                                           TZrUInt32 scopeId,
                                           TZrSize start,
                                           TZrSize count,
                                           ZrDebugValuePreview **outValues,
                                           TZrSize *outCount,
                                           TZrSize *outNamedVariables,
                                           TZrSize *outIndexedVariables);
/** @brief 在暂停帧求值表达式，使用默认能力策略与兼容求值路径；结果按值接收。 */
ZR_DEBUG_API TZrBool ZrDebug_Evaluate(ZrDebugAgent *agent,
                                      TZrUInt32 frameId,
                                      const TZrChar *expression,
                                      ZrDebugEvaluateResult *outResult,
                                      TZrChar *errorBuffer,
                                      TZrSize errorBufferSize);
/** @brief 在暂停帧求值表达式，并限制允许的 getter/调用/写入等作用位。 */
ZR_DEBUG_API TZrBool ZrDebug_EvaluateWithCapabilities(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        TZrUInt32 allowedEffectFlags,
        ZrDebugEvaluateResult *outResult,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
/** @brief 返回结构化失败阶段和诊断，供协议 evaluate 响应使用。 */
ZR_DEBUG_API TZrBool ZrDebug_EvaluateWithCapabilitiesDetailed(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        TZrUInt32 allowedEffectFlags,
        ZrDebugEvaluateResult *outResult,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
/** @brief 先分类表达式作用，供客户端在求值前提示能力要求。 */
ZR_DEBUG_API TZrBool ZrDebug_ClassifyEvaluationEffect(ZrDebugAgent *agent,
                                                       TZrUInt32 frameId,
                                                       const TZrChar *expression,
                                                       ZrDebugEvaluationEffectPolicy *outPolicy,
                                                       TZrChar *errorBuffer,
                                                       TZrSize errorBufferSize);
/** @brief 检查分类结果是否满足调用方给出的作用白名单。 */
ZR_DEBUG_API TZrBool ZrDebug_EvaluationEffectPolicy_Allows(
        const ZrDebugEvaluationEffectPolicy *policy,
        TZrUInt32 allowedEffectFlags);

/** @brief 释放 ReadStack/Scopes/Variables/TestManifest 返回的数组；可传空。 */
ZR_DEBUG_API void ZrDebug_Free(void *pointer);

/** @brief 通知 Agent VM 有未捕获异常，并按 uncaught 订阅策略决定是否停住。 */
ZR_DEBUG_API void ZrDebug_NotifyException(ZrDebugAgent *agent);

/** @brief 通知执行结束；已初始化的客户端最多接收一次 terminated 事件。 */
ZR_DEBUG_API void ZrDebug_NotifyTerminated(ZrDebugAgent *agent, TZrBool success);

#endif
