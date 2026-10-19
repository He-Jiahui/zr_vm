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

/**
 * @enum EZrDebugStopReason
 *
 * ## Summary
 * 表达本次停止事件在协议中的原因。
 *
 * ## Value model
 * - NONE 表示没有本次停止；ENTRY、BREAKPOINT、PAUSE、STEP、EXCEPTION、DATA_BREAKPOINT 对应 trace 分支。
 * - TERMINATED 保留结束分类值，结束协议通知由独立入口负责。
 *
 * ## Invariants and lifecycle
 * trace 按异常、暂停、启动、断点、数据断点、单步的分支顺序选择一个原因。TERMINATED 枚举不替代独立 terminated 通知；单个原因不证明其它条件未成立。
 *
 * ## References
 * - \ref EZrDebugStopReason：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:12）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @enum EZrDebugBreakpointKind
 *
 * ## Summary
 * 区分共享存储中的行断点与函数入口断点。
 *
 * ## Value model
 * - LINE=1 与 FUNCTION=2 用于数组分组和解析规则；未知非零 kind 不会在 enum 声明处自动拒绝。
 *
 * ## Invariants and lifecycle
 * 两个设置入口按 kind 替换集合并保留其它 kind；spec.kind 非零会覆盖入口 fallback。调用方应使用这两个已定义值，枚举本身不验证输入。
 *
 * ## References
 * - \ref EZrDebugBreakpointKind：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:35）。
 * - \ref zr_debug_copy_breakpoint_from_spec：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1735）。
 */
/**
 * @enum EZrDebugExceptionFilter
 *
 * ## Summary
 * 标识一次异常停点为 caught 或 uncaught。
 *
 * ## Value model
 * - NONE=0、CAUGHT=1、UNCAUGHT=2 是互斥的事件分类；两个订阅布尔值独立保存。
 *
 * ## Invariants and lifecycle
 * 这是事件分类值，不是可组合位掩码。订阅保存在 Agent 的两个布尔字段，NONE 用于非异常停点；不持有异常对象或控制 Throw 恢复。
 *
 * ## References
 * - \ref EZrDebugExceptionFilter：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:41）。
 * - \ref zr_debug_agent_fill_stop_event：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:210）。
 */
/**
 * @struct ZrDebugBreakpointSpec
 *
 * ## Summary
 * 承载宿主或 JSON 请求同步提交的断点规格。
 *
 * ## Fields
 * - kind/line 是本次类别和期望行；module_name/source_file/function_name 是可选定位约束。
 * - condition/hit_condition/log_message 为可选过滤及输出文本；所有字符串仅借用到同步复制结束。
 *
 * ## Invariants and lifecycle
 * 文本是调用期间借用的 NUL 字符串，设置函数会复制到固定容量记录；count>0 时 specs 必须有足量有效元素。line 和 kind 由调用方提供，不转移字符串所有权，也不保证在提交时已解析成功。
 *
 * ## References
 * - \ref ZrDebugBreakpointSpec：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:62）。
 * - \ref zr_debug_copy_breakpoint_from_spec：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1735）。
 */
/**
 * @struct ZrDebugStopEvent
 *
 * ## Summary
 * 保存最近一次停点可按值读取的协议快照。
 *
 * ## Fields
 * - reason/exception_filter 是选择后的停止分类；thread_id、line、instruction_index 和 state_id 标识该次停点。
 * - module_name/source_file/function_name、data_id/data_description、exception_stack 为内嵌文本快照，不含被转移的 VM 对象。
 *
 * ## Invariants and lifecycle
 * 文本复制到结构内固定数组并可能截断；state_id 与线程/源码/指令坐标属于该停点。Agent 下一停点会覆盖内部快照，恢复时清除变量句柄；本结构不拥有 VM、函数或异常对象。
 *
 * ## References
 * - \ref ZrDebugStopEvent：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:74）。
 * - \ref zr_debug_agent_fill_stop_event：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:210）。
 */
/**
 * @fn ZR_DEBUG_API TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count)
 *
 * ## Summary
 * 以新规格整体替换行断点类别并保留其它类别。
 *
 * ## Parameters
 * @param[in,out] agent 借用且由会话拥有的 Agent。
 * @param[in] specs count 个有效规格；字符串在调用期间稳定。
 * @param[in] count 规格数；0 用于清空此类别。
 *
 * ## Returns
 * @return NULL Agent 或替代数组普通分配失败返回假；正常提交返回真。
 *
 * ## Constraints and lifecycle
 * 同步复制文本并尽力解析，不要求提交时全部 resolved。保留的是其它 kind，不是其它 source 的行断点；调用方应提供 LINE 或零 kind。只保证普通分配失败前旧集合仍可用，不证明线程安全或全事务。
 *
 * ## References
 * - \ref ZrDebug_SetBreakpoints：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:242）。
 * - \ref zr_debug_agent_process_set_breakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:553）。
 */
/**
 * @fn ZR_DEBUG_API TZrBool ZrDebug_SetFunctionBreakpoints(ZrDebugAgent *agent,
                                                    const ZrDebugBreakpointSpec *specs,
                                                    TZrSize count)
 *
 * ## Summary
 * 以新规格整体替换函数入口断点类别并保留行断点。
 *
 * ## Parameters
 * @param[in,out] agent 借用且由会话拥有的 Agent。
 * @param[in] specs count 个有效规格，字符串在同步调用期间稳定。
 * @param[in] count 规格数；0 清空此类别。
 *
 * ## Returns
 * @return NULL Agent 或替代数组普通分配失败返回假；正常提交返回真。
 *
 * ## Constraints and lifecycle
 * 使用 FUNCTION fallback，但非零 spec.kind 仍由实现采用。文本复制可能截断、位置可延迟解析；不拥有函数、不提供并发原子或非局部回滚保证。
 *
 * ## References
 * - \ref ZrDebug_SetFunctionBreakpoints：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:244）。
 * - \ref zr_debug_agent_process_set_function_breakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:598）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_SetExceptionBreakpoints(ZrDebugAgent *agent, TZrBool caught, TZrBool uncaught)
 *
 * ## Summary
 * 独立设置 caught 与 uncaught 两种异常停点订阅。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent；NULL 不处理。
 * @param[in] caught 是否在有当前异常的 CATCH 观察点停下。
 * @param[in] uncaught 是否响应未捕获异常通知或待处理标志。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 输入规范化为布尔值；仅改变订阅，不立即抛出、清理或暂停异常，也不把枚举当位掩码。
 *
 * ## References
 * - \ref ZrDebug_SetExceptionBreakpoints：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:248）。
 * - \ref zr_debug_agent_process_set_exception_breakpoints：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:681）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_Continue(ZrDebugAgent *agent)
 *
 * ## Summary
 * 清除当前变量句柄、撤销暂停请求并切回连续运行意图。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent；NULL 不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 通常由暂停协议请求调用；函数本身不验证已暂停、不等待 VM 完成，也不保留旧句柄有效性。不等同于线程加入或原子状态提交。
 *
 * ## References
 * - \ref ZrDebug_Continue：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:251）。
 * - \ref zr_debug_agent_process_continue：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:420）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_Pause(ZrDebugAgent *agent)
 *
 * ## Summary
 * 提交由下一 trace 观察点消费的暂停请求。
 *
 * ## Parameters
 * @param[in,out] agent 借用且回调仍可访问的 Agent；NULL 不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 只经 volatile/store 辅助函数置请求，不立即挂起 VM、不等待确认；需继续产生 trace 点。外部线程同步仍是既有 TODO，不据网络测试静态入口宣称线程安全。
 *
 * ## References
 * - \ref ZrDebug_Pause：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:253）。
 * - \ref zr_debug_agent_process_pause：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:426）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_StepInto(ZrDebugAgent *agent)
 *
 * ## Summary
 * 请求继续到下一可观察位置，允许进入调用函数。
 *
 * ## Parameters
 * @param[in,out] agent 含活动 VM 状态的借用 Agent；缺失时不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 委托单步初始化，清旧变量句柄并保存起点；实际停止由 trace 的位置推进判断决定，不同步等待且不保证每条机器指令都观察。
 *
 * ## References
 * - \ref ZrDebug_StepInto：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:255）。
 * - \ref zr_debug_agent_process_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:431）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_StepOver(ZrDebugAgent *agent)
 *
 * ## Summary
 * 请求跳过当前逻辑调用并在起点帧后续位置停下。
 *
 * ## Parameters
 * @param[in,out] agent 含活动 VM 状态的借用 Agent；缺失时不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 委托单步初始化；更深帧与同深度尾调用按帧/函数身份暂缓普通断点，独立异常和暂停仍有优先级。不承诺忽略所有事件或同步完成。
 *
 * ## References
 * - \ref ZrDebug_StepOver：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:257）。
 * - \ref zr_debug_agent_process_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:431）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_StepOut(ZrDebugAgent *agent)
 *
 * ## Summary
 * 请求继续到逻辑调用深度小于单步起点。
 *
 * ## Parameters
 * @param[in,out] agent 含活动 VM 状态的借用 Agent；缺失时不处理。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 委托单步初始化；异常未处理时需到 CATCH 可观察点，实际停止依赖 trace 和调用链。清旧句柄，不拥有原帧或同步等待返回。
 *
 * ## References
 * - \ref ZrDebug_StepOut：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:259）。
 * - \ref zr_debug_agent_process_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/protocol/debug_protocol.c:431）。
 */
/**
 * @fn ZR_DEBUG_API void ZrDebug_NotifyException(ZrDebugAgent *agent)
 *
 * ## Summary
 * 在宿主获知执行失败时按 uncaught 订阅建立异常停点。
 *
 * ## Parameters
 * @param[in,out] agent 借用 Agent 及其活动状态。
 *
 * ## Returns
 * 无返回值；缺失 Agent/状态、未订阅或无客户端且不等待时直接返回。
 *
 * ## Constraints and lifecycle
 * 优先读保存异常帧，再回退当前调用帧，构造快照并进入暂停循环。该通知不执行 Throw、不消费或释放异常；可能阻塞等待协议。CLI 应在函数/状态释放之前调用，不承诺分配/非局部失败清理。
 *
 * ## References
 * - \ref ZrDebug_NotifyException：本单元定义/声明（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:335）。
 * - \ref ZrDebug_NotifyException：配套调用或事件构造入口（zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1086）。
 */

/**
 * @struct ZrDebugAgent
 *
 * ## Summary
 * 公开 API 使用的不透明调试会话身份，连接宿主保留的 VM/入口函数与调试客户端；内部布局和分配大小不是公开 ABI。
 *
 * ## Value model
 * 正常调用方只通过 AgentStart 得到指针，以协议/快照 API 使用，再交给 AgentStop 销毁；不能按公开头猜测布局、复制结构或用 ZrDebug_Free 代替销毁。
 *
 * ## Invariants and lifecycle
 * 会话拥有监听器、客户端连接及内部原生表，但不接管 VM 状态、入口函数和配置令牌。成功指针在 AgentStop 后立即失效；Stop 不会清零宿主变量，也不等待执行线程。宿主须先结束并发执行/访问并维持所有借用对象有效，直到卸载回调完成。
 *
 * ## References
 * ZrDebug_AgentStart 发布会话；ZrDebug_AgentStop 配对释放；ZrDebugAgentConfig 定义启动选项，ZrDebugTestEntrySnapshot 是另行释放的复制结果。
 */

/**
 * @struct ZrDebugAgentConfig
 *
 * ## Summary
 * 在启动时选择 loopback 监听地址、入口停点与客户端等待/鉴权方式；Agent 复制配置值，再由 trace/协议链消费待停标志和令牌。
 *
 * ## Fields
 * - address：启动时读取的零终止地址；null 或空串使用 127.0.0.1:0，端口零由系统分配，监听器只允许当前 loopback 实现认可的地址。
 * - suspend_on_start：设置入口待停请求；没有客户端且不要求等待时，trace observer 可清除此请求继续执行，不等于无条件立即暂停。
 * - wait_for_client：要求入口等待/保留待停行为，后续 trace 与异常通知也读取该策略；启动本身不执行目标或创建线程。
 * - auth_token：借用鉴权文本，初始化请求由 TokenMatches 比较；不是启动时深复制的秘密存储。
 * - stop_on_uncaught_exception：初始化未捕获异常停点订阅，之后可由异常断点接口修改会话订阅。
 *
 * ## Invariants and lifecycle
 * 配置结构可在启动返回后由调用方回收；其字符串指针未深复制。address 必须至少在启动解析期间有效；auth_token 须在会话可能鉴权期间保持可读且按宿主约定管理，通常保持到 AgentStop。传 null 配置等价于零值选项；这些布尔选项不提供线程同步或网络可达性保证。
 *
 * ## References
 * ZrDebug_AgentStart 建立 effectiveConfig；zr_debug_agent_trace_observer 处理入口待停，zr_debug_agent_process_initialize 使用 auth_token；CLI 在 Stop 后清除自己的令牌缓冲。
 */

/**
 * @struct ZrDebugTestEntrySnapshot
 *
 * ## Summary
 * 调试测试发现使用的平面元数据快照，描述入口清单中的一个测试声明；它不包含完整参数数据，也不代表该测试已经执行或通过。
 *
 * ## Fields
 * - function_symbol_id/function_type_id/callable_child_index 从解码声明复制，用于关联入口测试函数身份。
 * - case_count/is_async 是声明的用例数量和异步标记；不能据此推导本次运行数量、结果或调度完成。
 * - module_id/qualified_name/skip_reason 是固定容量自有字符数组，缺失源文本转为空串，过长文本按字节截断并终止。
 *
 * ## Invariants and lifecycle
 * ReadTestManifest 成功时返回独立数组，所有公开字段为值或内嵌数组，可在 AgentStop 后继续读取。调用方用 ZrDebug_Free 一次释放数组，不逐字段释放，不把字段文本当作无截断的完整清单标识。
 *
 * ## References
 * ZrDebug_ReadTestManifest 构造投影；ZrParser_TestManifest_Decode 提供内部源记录；test_debug_agent_projects_typed_test_manifest 核对复制出的 ID、数量和名称。
 */

/**
 * @fn TZrBool ZrDebug_AgentStart(SZrState *state, SZrFunction *entryFunction, const TZrChar *moduleName, const ZrDebugAgentConfig *config, ZrDebugAgent **outAgent, TZrChar *errorBuffer, TZrSize errorBufferSize)
 *
 * ## Summary
 * 在入口函数执行前创建调试会话，解码入口 TestManifest、打开 loopback 监听器并登记主 VM 状态；只建立调试设施，不执行入口函数，也不创建执行线程。
 *
 * ## Parameters
 * @param[in,out] state 必需的存活 VM 状态；成功后其 trace observer/hook 由会话使用，宿主保留状态和 global。
 * @param[in] entryFunction 必需的入口函数；元数据在启动时读取，函数指针仍借用到 AgentStop，宿主负责其有效性和 GC 根。
 * @param[in] moduleName 可选显示名称；null 视为空串，启动时复制到 Agent 固定数组，可截断。
 * @param[in] config 可选配置；null 使用零值配置与 127.0.0.1:0。结构按值复制，其中 auth_token 仍借用。
 * @param[out] outAgent 必需的会话输出；进入时清零，成功后移交新 Agent，必须以 AgentStop 释放。
 * @param[out] errorBuffer 可选普通失败诊断缓冲；可为 null，与容量一起交给有界文本复制，不保证成功时清空。
 * @param[in] errorBufferSize 诊断缓冲字节容量；零表示不写，非零且缓冲非空时须提供相应可写空间。
 *
 * ## Returns
 * @return 成功返回 true 并发布 *outAgent；必需参数缺失、C 分配失败、Manifest 不合法或监听器/线程登记失败时普通返回 false。已取得的会话资源在这些失败分支经部分初始化安全的 AgentStop 清理，输出保持 null。
 *
 * ## Constraints and lifecycle
 * state、entryFunction、注册的其它 state 和鉴权令牌在会话使用期间保持有效；配置不替宿主管理 VM/函数根。启动覆写 state observer，不保存先前 observer，只保存其 hook 掩码。调用方须协调 observer 归属，停止执行/请求访问后再 Stop；本接口没有并发或重入保护。原生分配失败走 bool/NULL 路径，不借此承诺任意宿主分配器回调的非局部异常清理。
 *
 * ## References
 * ZrCli_Runtime_RunPreparedProjectCapture 在执行前启动并在执行后停止；ZrParser_TestManifest_Decode 解码副本，ZrNetwork_ListenerOpenLoopback 建立监听，zr_debug_agent_register_thread_state 安装主状态回调，ZrDebug_AgentStop 配对释放。
 */

/**
 * @fn void ZrDebug_AgentStop(ZrDebugAgent *agent)
 *
 * ## Summary
 * 销毁启动成功或启动途中构造的 Agent，先解除仍归属于它的 VM 回调，再关闭网络和释放会话拥有的数组、句柄表及解码清单，防止释放后回调继续使用会话。
 *
 * ## Parameters
 * @param[in,out] agent 由 AgentStart 分配的会话；null 为无操作。非空调用后该指针及其内部借用视图失效，调用方变量不会被自动清零。
 *
 * ## Returns
 * 无返回值；卸载满足归属条件的 observer、恢复对应旧 hook 掩码并释放会话。不会销毁借用 VM 状态、入口函数或鉴权令牌。
 *
 * ## Constraints and lifecycle
 * 调用前须确保 VM 执行、协议请求和临时线程访问已结束，且所有登记 state/global 仍存活；本函数不 join、不等待回调、不提供并发销毁保护。仅当 state->debugTraceUserData 仍等于本 Agent 才卸载并恢复掩码；不恢复注册前的 observer，也不触碰已改由其它 owner 持有的回调。非空会话只能销毁一次。
 *
 * ## References
 * ZrDebug_AgentStart 的普通失败分支调用本函数；CLI 在入口执行结束后调用，test_debug_threads_enumerates_main_thread_and_routes_snapshots 先 join 执行线程再 Stop；zr_debug_agent_close_client、ZrParser_TestManifest_Free 释放所属资源。
 */

/**
 * @fn TZrBool ZrDebug_ReadTestManifest(ZrDebugAgent *agent, ZrDebugTestEntrySnapshot **outEntries, TZrSize *outCount)
 *
 * ## Summary
 * 把启动时已解码的入口测试清单投影为调试客户端可读取的平面快照，供测试发现列出标识、用例数量和显示文本；不执行测试或复制完整参数描述。
 *
 * ## Parameters
 * @param[in] agent 存活会话；读取其解码清单但不改变它。
 * @param[out] outEntries 必需数组输出；进入时先清零，成功非空数组由调用方用 ZrDebug_Free 释放。
 * @param[out] outCount 必需元素数输出；进入时先置零，成功后与数组长度对应。
 *
 * ## Returns
 * @return 无清单或条目数为零时 true 并交付 null/0；复制成功时 true 并交付独立 calloc 数组；空必需输入或分配失败时 false，能够写入的输出保持空值。
 *
 * ## Constraints and lifecycle
 * 快照的标量和固定文本数组均复制，成功结果不借用 Agent 内部字符串，可在 AgentStop 后继续持有至 ZrDebug_Free。文本按字节截断到固定容量；case_count/is_async 是元数据，不表示实际执行结果。读取须与会话销毁协调，无内部同步。
 *
 * ## References
 * ZrDebug_AgentStart 负责解码并持有源清单；test_debug_agent_projects_typed_test_manifest 读取并释放快照；ZrDebugTestEntrySnapshot 定义投影字段，ZrDebug_Free 释放结果。
 */

/**
 * @fn TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 把监听器实际绑定端点复制给 CLI 握手输出或测试 TCP 连接代码，避免客户端继续使用配置中的临时端口零；不建立连接。
 *
 * ## Parameters
 * @param[in] agent 已完成启动且仍存活的会话。
 * @param[out] buffer 调用方提供的文本缓冲；非空且容量大于零时才可复制。
 * @param[in] bufferSize 可写字节容量，包含终止零空间。
 *
 * ## Returns
 * @return 空 Agent/缓冲、容量零或会话端点为空时 false，缓冲不改；其它情况 true 并复制一个零终止串。true 仅表示进行了有界复制，不保证端点未截断，容量为一时也可能只得到空串。
 *
 * ## Constraints and lifecycle
 * 不分配、不转移网络资源。完整端点要求调用方提供足够容量并在使用前保持会话/监听器有效；截断按字节发生，不承诺 UTF-8 字符边界。与 Stop 并发读取不受保护。
 *
 * ## References
 * zr_cli_runtime_emit_debug_endpoint 发布握手信息；test_debug_threads 和 test_debug_agent_protocol 的 connect helper 取得该地址；ZrNetwork_FormatEndpoint 在 AgentStart 中生成实际地址。
 */
