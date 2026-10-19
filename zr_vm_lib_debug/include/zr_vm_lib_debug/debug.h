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
