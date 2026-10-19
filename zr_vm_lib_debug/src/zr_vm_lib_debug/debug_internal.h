#ifndef ZR_VM_DEBUG_DEBUG_INTERNAL_H
#define ZR_VM_DEBUG_DEBUG_INTERNAL_H

#include "zr_vm_lib_debug/debug.h"
#include "zr_vm_parser/test_contract.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "cJSON/cJSON.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

typedef struct SZrCompilerState SZrCompilerState;
typedef struct SZrAstNode SZrAstNode;

#define ZR_DEBUG_WAIT_INFINITE ((TZrUInt32) 0xFFFFFFFFu)
#define ZR_DEBUG_PROTOCOL_NAME "zrdbg/1"
#define ZR_DEBUG_MAIN_THREAD_ID ((TZrUInt32)1u)
/* BUG: scope ID 用 frameId * 10 + kind 编码；第 100 帧起与本动态句柄区间
 * 重叠。ReadScopes 仍发出该 ID，ReadVariables 在无动态句柄时直接拒绝，
 * 合法深递归帧无法展开变量。证据：debug.c:zr_debug_scope_id、
 * debug_snapshot.c:ReadScopes/ReadVariables、core/debug.c:GetStack。 */
#define ZR_DEBUG_VARIABLE_HANDLE_BASE ((TZrUInt32)1000u)
#define ZR_DEBUG_INSTANCE_SYNTHETIC_FIELD_COUNT ((TZrSize)1u)
#define ZR_DEBUG_CANONICAL_TYPE_ID_INVALID ((TZrUInt32)0u)

/* Pause/exception 请求由外部控制路径提交，执行线程在 trace 观察点读取。 */
/* TODO: volatile bool 配合全栅栏不等同于 C 原子对象；核对外部线程与 VM
 * 的真实同步前提，依据 tests/debug/test_debug_agent.c 的跨线程 Pause 路径
 * 决定是否应使用原子 load/store 或现有线程锁。 */
static ZR_FORCE_INLINE void zr_debug_memory_barrier(void) {
#if defined(_WIN32)
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

static ZR_FORCE_INLINE void zr_debug_bool_store(volatile TZrBool *slot, TZrBool value) {
    if (slot == ZR_NULL) {
        return;
    }

    *slot = value;
    zr_debug_memory_barrier();
}

static ZR_FORCE_INLINE TZrBool zr_debug_bool_load(const volatile TZrBool *slot) {
    if (slot == ZR_NULL) {
        return ZR_FALSE;
    }

    zr_debug_memory_barrier();
    return *slot;
}

/* 执行线程根据该模式、起始帧和 PC 判断下一停点；协议请求只选择意图。 */
typedef enum EZrDebugRunMode {
    ZR_DEBUG_RUN_MODE_RUNNING = 0,
    ZR_DEBUG_RUN_MODE_PAUSED = 1,
    ZR_DEBUG_RUN_MODE_STEP_INTO = 2,
    ZR_DEBUG_RUN_MODE_STEP_OVER = 3,
    ZR_DEBUG_RUN_MODE_STEP_OUT = 4
} EZrDebugRunMode;

/* prototype 的不同元数据视图共享句柄通道，避免调用语言层 getter 来展开。 */
typedef enum EZrDebugPrototypeViewKind {
    ZR_DEBUG_PROTOTYPE_VIEW_NONE = 0,
    ZR_DEBUG_PROTOTYPE_VIEW_METADATA = 1,
    ZR_DEBUG_PROTOTYPE_VIEW_MEMBERS = 2,
    ZR_DEBUG_PROTOTYPE_VIEW_METHODS = 3,
    ZR_DEBUG_PROTOTYPE_VIEW_PROPERTIES = 4,
    ZR_DEBUG_PROTOTYPE_VIEW_STATICS = 5,
    ZR_DEBUG_PROTOTYPE_VIEW_PROTOCOLS = 6,
    ZR_DEBUG_PROTOTYPE_VIEW_MANAGED_FIELDS = 7
} EZrDebugPrototypeViewKind;

/* Agent 内持有的断点快照；resolved_function/PC 是函数树解析后的可执行位置。 */
typedef struct ZrDebugBreakpoint {
    EZrDebugBreakpointKind kind;
    TZrChar module_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar condition[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar hit_condition[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar log_message[ZR_DEBUG_TEXT_CAPACITY];
    TZrUInt32 line;
    TZrUInt32 hit_count;
    SZrFunction *resolved_function;
    TZrUInt32 resolved_instruction_index;
    TZrBool resolved;
} ZrDebugBreakpoint;

/* 客户端变量引用按 kind 展开；句柄只能在创建它的 stopStateId 中使用。 */
typedef enum EZrDebugVariableHandleKind {
    ZR_DEBUG_VARIABLE_HANDLE_KIND_NONE = 0,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_VALUE = 1,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_GLOBAL_STATE = 2,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE = 3,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_PROTOTYPE_VIEW = 4,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_EXCEPTION = 5,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_INDEX_WINDOW = 6,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_INLINE_UNION = 7,
    ZR_DEBUG_VARIABLE_HANDLE_KIND_STRING_CHUNKS = 8
} EZrDebugVariableHandleKind;

/* 把 boxed carrier 与 inline union 的元数据映射到同一种展示视图。
 * 指针均借用 VM/编译元数据，仅在当前停点与函数生命周期内访问。 */
typedef struct ZrDebugUnionView {
    TZrChar type_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar variant_name[ZR_DEBUG_NAME_CAPACITY];
    const SZrFunction *entry_function;
    const struct SZrCompiledPrototypeInfo *prototype;
    const struct SZrCompiledMemberInfo *members;
    const struct SZrCompiledMemberInfo *variant_member;
    SZrObject *variant_metadata;
    SZrObject *payload_fields;
    TZrUInt32 payload_count;
    TZrUInt32 tag_size;
} ZrDebugUnionView;

/* 保存停点内可展开值或窗口；继续运行即清空，不能向下一 stopStateId 转移。 */
typedef struct ZrDebugVariableHandle {
    TZrUInt32 handle_id;
    TZrUInt64 state_id;
    EZrDebugVariableHandleKind kind;
    EZrDebugPrototypeViewKind prototype_view_kind;
    TZrSize window_start;
    TZrSize window_count;
    TZrUInt32 frame_id;
    TZrUInt32 stack_slot;
    SZrTypeValue value;
    SZrObjectPrototype *prototype;
} ZrDebugVariableHandle;

/* 数据断点当前只跟踪局部槽和闭包 upvalue，尚非通用对象字段 watch。 */
typedef enum EZrDebugDataBreakpointKind {
    ZR_DEBUG_DATA_BREAKPOINT_KIND_NONE = 0,
    ZR_DEBUG_DATA_BREAKPOINT_KIND_LOCAL = 1,
    ZR_DEBUG_DATA_BREAKPOINT_KIND_UPVALUE = 2
} EZrDebugDataBreakpointKind;

/* 用上次快照与 trace 时读取的值比较；frame/slot 身份来自注册时的当前停点。 */
typedef struct ZrDebugDataBreakpoint {
    EZrDebugDataBreakpointKind kind;
    EZrDebugScopeKind scope_kind;
    TZrUInt32 thread_id;
    TZrUInt32 frame_id;
    TZrUInt32 stack_slot;
    TZrInt32 value_index;
    TZrBool verified;
    TZrBool has_last_value;
    SZrTypeValue last_value;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar data_id[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar description[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugDataBreakpoint;

/* 协议 thread ID 映射到 VM state，并保留卸载 Agent 时要恢复的 hook signal。 */
typedef struct ZrDebugThreadEntry {
    TZrUInt32 thread_id;
    TZrUInt32 previous_debug_hook_signal;
    SZrState *state;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
} ZrDebugThreadEntry;

/* Agent 拥有网络连接、断点数组、变量句柄和解码后的 TestManifest；
 * VM state、入口函数及配置 auth_token 为借用，必须活到 AgentStop。
 * 读取其它线程时只临时切换 state/currentThreadId，结束访问必须恢复。 */
struct ZrDebugAgent {
    SZrState *state;
    SZrFunction *entryFunction;
    SZrParserTestManifest testManifest;
    TZrBool hasTestManifest;
    ZrDebugAgentConfig config;
    SZrNetworkListener listener;
    SZrNetworkStream client;
    TZrBool hasClient;
    TZrBool clientInitialized;
    TZrBool waitForClientPending;
    TZrBool startStopPending;
    volatile TZrBool pauseRequested;
    volatile TZrBool exceptionStopPending;
    TZrBool exceptionBreakOnCaught;
    TZrBool exceptionBreakOnUncaught;
    TZrBool terminatedNotified;
    TZrBool clientDisconnectContinue;
    EZrDebugRunMode runMode;
    TZrUInt32 stepDepth;
    SZrCallInfo *stepCallInfo;
    SZrFunction *resumeFunction;
    TZrUInt32 resumeInstruction;
    TZrUInt64 stopStateId;
    ZrDebugStopEvent lastStopEvent;
    ZrDebugBreakpoint *breakpoints;
    TZrSize breakpointCount;
    TZrSize lineBreakpointCount;
    TZrSize functionBreakpointCount;
    ZrDebugDataBreakpoint *dataBreakpoints;
    TZrSize dataBreakpointCount;
    TZrSize dataBreakpointCapacity;
    ZrDebugVariableHandle *variableHandles;
    TZrSize variableHandleCount;
    TZrSize variableHandleCapacity;
    TZrUInt32 nextVariableHandleId;
    ZrDebugThreadEntry *threads;
    TZrSize threadCount;
    TZrSize threadCapacity;
    TZrUInt32 nextThreadId;
    TZrUInt32 currentThreadId;
    TZrChar moduleName[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar endpoint[ZR_NETWORK_ENDPOINT_TEXT_CAPACITY];
};

ZR_DEBUG_API void zr_debug_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text);
const TZrChar *zr_debug_string_native(SZrString *value);
const TZrChar *zr_debug_function_name(SZrFunction *function);
const TZrChar *zr_debug_function_source(SZrFunction *function);
TZrUInt32 zr_debug_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function);
TZrUInt32 zr_debug_frame_depth(SZrState *state);
const TZrChar *zr_debug_stop_reason_name(EZrDebugStopReason reason);
const TZrChar *zr_debug_exception_filter_name(EZrDebugExceptionFilter filter);
const TZrChar *zr_debug_scope_kind_name(EZrDebugScopeKind kind);
const TZrChar *zr_debug_value_type_name(EZrValueType type);
const TZrChar *zr_debug_prototype_type_name(EZrObjectPrototypeType type);
TZrUInt32 zr_debug_scope_id(TZrUInt32 frameId, EZrDebugScopeKind kind);
TZrBool zr_debug_exception_read_frame(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugFrameSnapshot *outFrame);
TZrSize zr_debug_exception_frame_count(ZrDebugAgent *agent);
SZrCallInfo *zr_debug_find_call_info_by_frame_id(ZrDebugAgent *agent, TZrUInt32 frameId, SZrFunction **outFunction);
const SZrTypeValue *zr_debug_frame_value_slot(SZrState *state,
                                              const SZrFunction *function,
                                              const SZrCallInfo *callInfo,
                                              TZrUInt32 stackSlot);
const SZrTypeValue *zr_debug_closure_capture_value(ZrDebugAgent *agent,
                                                   const SZrFunction *function,
                                                   const SZrCallInfo *callInfo,
                                                   const TZrChar *name);
SZrObjectPrototype *zr_debug_resolve_value_prototype(SZrState *state, const SZrTypeValue *value);
TZrSize zr_debug_count_visible_object_entries(const SZrHashSet *set);
void zr_debug_value_child_shape(SZrState *state,
                                const SZrTypeValue *value,
                                TZrSize *outNamedVariables,
                                TZrSize *outIndexedVariables);
void zr_debug_value_semantic_summary(SZrState *state,
                                     const SZrTypeValue *value,
                                     TZrSize namedVariables,
                                     TZrSize indexedVariables,
                                     TZrChar *buffer,
                                     TZrSize bufferSize);
TZrBool zr_debug_value_is_union_carrier(SZrState *state, const SZrTypeValue *value);
TZrSize zr_debug_count_union_carrier_payload_fields(SZrState *state, const SZrTypeValue *value);
TZrBool zr_debug_union_value_view(ZrDebugAgent *agent,
                                  const SZrTypeValue *value,
                                  ZrDebugUnionView *outView);
TZrBool zr_debug_inline_union_view(ZrDebugAgent *agent,
                                   const SZrFunction *function,
                                   const SZrFunctionFrameSlotLayout *slotLayout,
                                   const SZrStackFramePlace *place,
                                   ZrDebugUnionView *outView);
TZrBool zr_debug_inline_union_slot_view(ZrDebugAgent *agent,
                                        const SZrFunction *function,
                                        const SZrCallInfo *callInfo,
                                        TZrUInt32 stackSlot,
                                        SZrStackFramePlace *outPlace,
                                        ZrDebugUnionView *outView);
TZrBool zr_debug_union_payload_display_name(SZrState *state,
                                            const ZrDebugUnionView *view,
                                            TZrUInt32 payloadIndex,
                                            TZrChar *buffer,
                                            TZrSize bufferSize);
TZrBool zr_debug_union_payload_value_from_carrier(ZrDebugAgent *agent,
                                                  const SZrTypeValue *value,
                                                  const ZrDebugUnionView *view,
                                                  TZrUInt32 payloadIndex,
                                                  SZrTypeValue *outValue);
TZrBool zr_debug_union_payload_value_from_inline(ZrDebugAgent *agent,
                                                 const SZrFunction *function,
                                                 const SZrStackFramePlace *place,
                                                 const ZrDebugUnionView *view,
                                                 TZrUInt32 payloadIndex,
                                                 SZrTypeValue *outValue);
TZrBool zr_debug_materialize_inline_union_slot(ZrDebugAgent *agent,
                                               const SZrFunction *function,
                                               const SZrCallInfo *callInfo,
                                               TZrUInt32 stackSlot,
                                               SZrTypeValue *outValue);
TZrBool zr_debug_union_safe_get_member_value(ZrDebugAgent *agent,
                                             const SZrTypeValue *receiver,
                                             const TZrChar *memberName,
                                             SZrTypeValue *outValue);
TZrBool zr_debug_format_union_value_text(ZrDebugAgent *agent,
                                         const SZrTypeValue *value,
                                         TZrChar *buffer,
                                         TZrSize bufferSize);
void zr_debug_format_inline_union_text(const ZrDebugUnionView *view,
                                       TZrChar *buffer,
                                       TZrSize bufferSize);
ZR_DEBUG_API void zr_debug_append_expression_semantic_facts(ZrDebugAgent *agent,
                                                            TZrUInt32 frameId,
                                                            const TZrChar *expression,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize);
void zr_debug_append_expression_reference_summary(ZrDebugAgent *agent,
                                                  TZrUInt32 frameId,
                                                  const TZrChar *expression,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize);
ZR_DEBUG_API TZrBool zr_debug_semantic_register_bindings(ZrDebugAgent *agent,
                                                           TZrUInt32 frameId,
                                                           SZrCompilerState *compilerState);
TZrBool zr_debug_semantic_register_summary_bindings(ZrDebugAgent *agent,
                                                    TZrUInt32 frameId,
                                                    SZrCompilerState *compilerState,
                                                    const SZrAstNode *expression);
TZrBool zr_debug_formal_evaluate_expression(ZrDebugAgent *agent,
                                            TZrUInt32 frameId,
                                            const TZrChar *expression,
                                            TZrUInt32 allowedEffectFlags,
                                            SZrTypeValue *outValue,
                                            TZrUInt32 *outCanonicalTypeId,
                                            ZrDebugEvaluateFailure *outFailure,
                                            TZrChar *errorBuffer,
                                            TZrSize errorBufferSize,
                                            TZrBool *outHandled,
                                            TZrBool *outParserFailure);
void zr_debug_reference_summary_from_scope(EZrDebugScopeKind scopeKind,
                                           const TZrChar *name,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);
TZrBool zr_debug_identifier_reference_summary(ZrDebugAgent *agent,
                                              TZrUInt32 frameId,
                                              const TZrChar *name,
                                              TZrChar *buffer,
                                              TZrSize bufferSize);
ZR_DEBUG_API void zr_debug_format_value_text_safe(SZrState *state,
                                                  const SZrTypeValue *value,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize);
TZrBool zr_debug_resolve_identifier_value(ZrDebugAgent *agent,
                                          TZrUInt32 frameId,
                                          const TZrChar *name,
                                          SZrTypeValue *outValue,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize);
TZrBool zr_debug_safe_get_member_value(ZrDebugAgent *agent,
                                       const SZrTypeValue *receiver,
                                       const TZrChar *memberName,
                                       SZrTypeValue *outValue,
                                       TZrChar *errorBuffer,
                                       TZrSize errorBufferSize);
TZrBool zr_debug_try_resolve_indexed_storage(SZrState *state,
                                             const SZrTypeValue *value,
                                             SZrObject **outStorage,
                                             const TZrChar **outSyntheticName,
                                             TZrSize *outLength);
TZrBool zr_debug_safe_get_index_value(ZrDebugAgent *agent,
                                      const SZrTypeValue *receiver,
                                      const SZrTypeValue *key,
                                      SZrTypeValue *outValue,
                                      TZrChar *errorBuffer,
                                      TZrSize errorBufferSize);

void zr_debug_agent_close_client(ZrDebugAgent *agent);
void zr_debug_agent_fill_stop_event(ZrDebugAgent *agent,
                                    EZrDebugStopReason reason,
                                    EZrDebugExceptionFilter exceptionFilter,
                                    SZrFunction *function,
                                    TZrUInt32 instructionIndex,
                                    TZrUInt32 sourceLine);
void zr_debug_agent_emit_breakpoint_resolved(ZrDebugAgent *agent, const ZrDebugBreakpoint *breakpoint);
void zr_debug_agent_poll_messages(ZrDebugAgent *agent, TZrUInt32 timeoutMs, TZrBool *outDisconnected);
void zr_debug_agent_pause_loop(ZrDebugAgent *agent);
void zr_debug_agent_emit_output(ZrDebugAgent *agent, const TZrChar *category, const TZrChar *outputText);

TZrUInt32 zr_debug_call_info_line(ZrDebugAgent *agent, SZrCallInfo *callInfo, SZrFunction *function,
                                  TZrBool isTopFrame);
void zr_debug_variable_handles_clear(ZrDebugAgent *agent);
ZrDebugThreadEntry *zr_debug_agent_find_thread_by_id(ZrDebugAgent *agent, TZrUInt32 threadId);
ZrDebugThreadEntry *zr_debug_agent_find_thread_by_state(ZrDebugAgent *agent, SZrState *state);
TZrUInt32 zr_debug_agent_thread_id_for_state(ZrDebugAgent *agent, SZrState *state);
TZrUInt32 zr_debug_agent_effective_thread_id(ZrDebugAgent *agent, TZrUInt32 requestedThreadId);
TZrBool zr_debug_agent_register_thread_state(ZrDebugAgent *agent,
                                             SZrState *state,
                                             const TZrChar *name,
                                             TZrUInt32 *outThreadId);
SZrState *zr_debug_agent_begin_thread_access(ZrDebugAgent *agent,
                                             TZrUInt32 requestedThreadId,
                                             TZrUInt32 *outResolvedThreadId,
                                             SZrState **outPreviousState,
                                             TZrUInt32 *outPreviousThreadId);
void zr_debug_agent_end_thread_access(ZrDebugAgent *agent, SZrState *previousState, TZrUInt32 previousThreadId);
TZrBool zr_debug_agent_data_breakpoint_info(ZrDebugAgent *agent,
                                            TZrUInt32 requestedThreadId,
                                            TZrUInt32 variablesReference,
                                            const TZrChar *name,
                                            TZrChar *outDataId,
                                            TZrSize outDataIdSize,
                                            TZrChar *outDescription,
                                            TZrSize outDescriptionSize);
void zr_debug_agent_clear_data_breakpoints(ZrDebugAgent *agent);
TZrBool zr_debug_agent_add_data_breakpoint(ZrDebugAgent *agent,
                                           const TZrChar *dataId,
                                           const TZrChar *accessType,
                                           TZrChar *outDescription,
                                           TZrSize outDescriptionSize);
ZrDebugDataBreakpoint *zr_debug_agent_check_data_breakpoints(ZrDebugAgent *agent);
TZrBool zr_debug_evaluate_expression(ZrDebugAgent *agent,
                                     TZrUInt32 frameId,
                                     const TZrChar *expression,
                                     SZrTypeValue *outValue,
                                     TZrChar *errorBuffer,
                                     TZrSize errorBufferSize,
                                     TZrChar *referenceBuffer,
                                     TZrSize referenceBufferSize);
TZrBool zr_debug_evaluate_expression_with_capabilities(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        TZrUInt32 allowedEffectFlags,
        TZrBool allowLegacyCompatibility,
        SZrTypeValue *outValue,
        TZrUInt32 *outCanonicalTypeId,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize,
        TZrChar *referenceBuffer,
        TZrSize referenceBufferSize);
void zr_debug_evaluate_failure_clear(ZrDebugEvaluateFailure *failure);
void zr_debug_evaluate_failure_set(ZrDebugEvaluateFailure *failure,
                                   EZrDebugEvaluateFailureKind kind,
                                   TZrUInt64 stateId,
                                   const TZrChar *code,
                                   const TZrChar *message);

#endif

/**
 * @fn static ZR_FORCE_INLINE void zr_debug_memory_barrier(void)
 *
 * ## Summary
 * 发布或读取跨线程布尔请求时调用平台全栅栏。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * Win 使用 MemoryBarrier，其余分支使用 __sync_synchronize；栅栏不把 volatile 对象变成标准 C 原子。保留既有 TODO: 核对外部 Pause 线程和 VM 的同步协议，不以本函数证明线程安全。
 *
 * ## References
 * - \ref zr_debug_memory_barrier：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:44）。
 * - \ref zr_debug_bool_store：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:52）。
 */
/**
 * @fn static ZR_FORCE_INLINE void zr_debug_bool_store(volatile TZrBool *slot, TZrBool value)
 *
 * ## Summary
 * 写入暂停或异常请求槽，再执行全栅栏。
 *
 * ## Parameters
 * @param[in,out] slot 借用的 volatile 布尔槽；NULL 时不访问。
 * @param[in] value 本次要发布的布尔值。
 *
 * ## Returns
 * 无返回值。
 *
 * ## Constraints and lifecycle
 * 只进行普通 volatile 赋值和随后栅栏；不提供互斥、原子读改写或请求排队。既有同步 TODO 与 load/barrier 同族保留。
 *
 * ## References
 * - \ref zr_debug_bool_store：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:52）。
 * - \ref ZrDebug_Pause：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1864）。
 */
/**
 * @fn static ZR_FORCE_INLINE TZrBool zr_debug_bool_load(const volatile TZrBool *slot)
 *
 * ## Summary
 * 在 trace 观察点前置全栅栏并读取控制请求槽。
 *
 * ## Parameters
 * @param[in] slot 借用的 volatile 布尔槽；NULL 返回假。
 *
 * ## Returns
 * @return NULL 返回 ZR_FALSE，否则返回本次普通 volatile 读取值。
 *
 * ## Constraints and lifecycle
 * 不消费或清零槽；消费方另行 store。既有 TODO: 全栅栏和 volatile 不能单独证明 C 线程同步。
 *
 * ## References
 * - \ref zr_debug_bool_load：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:61）。
 * - \ref zr_debug_agent_trace_observer：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1492）。
 */
/**
 * @enum EZrDebugRunMode
 *
 * ## Summary
 * 区分连续运行、暂停循环和三种单步意图。
 *
 * ## Value model
 * - RUNNING/PAUSED 表达运行意图和暂停循环状态。
 * - STEP_INTO/STEP_OVER/STEP_OUT 分别配合位置推进、逻辑帧身份及深度判断。
 *
 * ## Invariants and lifecycle
 * 值保存在 Agent；单步还依赖 stepDepth、stepCallInfo、resumeFunction/Instruction 和 lastStopEvent。模式写入不会自行执行或等待 VM，指针均借用当前会话。
 *
 * ## References
 * - \ref EZrDebugRunMode：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:71）。
 * - \ref zr_debug_begin_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1873）。
 */
/**
 * @struct ZrDebugBreakpoint
 *
 * ## Summary
 * 保存 Agent 拥有的断点规格副本与延迟解析位置。
 *
 * ## Fields
 * - kind 与 line 选择定位规则；模块/源码/函数/条件/命中条件/日志六组固定字符数组拥有复制文本。
 * - hit_count 累计实际位置命中，再用于条件过滤；计数使用 uint32。
 * - resolved、resolved_function、resolved_instruction_index 联合描述借用函数中的位置；函数及其指令不能先于使用结束释放。
 *
 * ## Invariants and lifecycle
 * 固定数组复制文本并可能截断；hit_count 是 uint32 命中计数。resolved_function 及其指令索引借用仍存活的函数元数据，不在此记录中持有 GC 根或释放函数；替换数组会使记录地址失效。
 *
 * ## References
 * - \ref ZrDebugBreakpoint：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:92）。
 * - \ref zr_debug_replace_breakpoints_of_kind：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1784）。
 */
/**
 * @fn TZrUInt32 zr_debug_instruction_offset(SZrCallInfo *callInfo, SZrFunction *function)
 *
 * ## Summary
 * 把当前借用调用帧的 PC 换算为指定函数的指令偏移。
 *
 * ## Parameters
 * @param[in] callInfo 当前有效调用帧，可为 NULL。
 * @param[in] function 与 PC 对应的函数元数据，可为 NULL。
 *
 * ## Returns
 * @return 无帧、函数、指令起点、PC 或 PC 在起点之前时返回 0；否则返回指针差转 uint32。
 *
 * ## Constraints and lifecycle
 * 有效 PC 必须与函数指令数组具有合法的指针比较/相减关系；这里不检查数组上界，0 也可能是合法首指令。结果不延长任何对象生命周期。
 *
 * ## References
 * - \ref zr_debug_instruction_offset：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:232）。
 * - \ref zr_debug_begin_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1873）。
 */
/**
 * @fn TZrUInt32 zr_debug_frame_depth(SZrState *state)
 *
 * ## Summary
 * 统计当前调用链中可取得元数据函数的调试帧。
 *
 * ## Parameters
 * @param[in] state 借用且调用链稳定的 VM 状态；NULL 视为无帧。
 *
 * ## Returns
 * @return 返回可调试帧个数；NULL 返回 0。
 *
 * ## Constraints and lifecycle
 * 遍历 previous 链，忽略不能解析为元数据函数的帧；不检测链环、不持锁、不拥有帧。单步把该逻辑深度与帧身份一并比较。
 *
 * ## References
 * - \ref zr_debug_frame_depth：本单元定义/声明（zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h:233）。
 * - \ref zr_debug_agent_should_stop_for_step：配套调用或事件构造入口（zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1414）。
 */
