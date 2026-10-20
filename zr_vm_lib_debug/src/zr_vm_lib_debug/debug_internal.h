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

/**
 * @struct ZrDebugUnionView
 * @unit UNION-001
 *
 * ## Summary
 * 统一 boxed carrier 与暂停帧 inline union 的展示信息，使预览、子节点展开和受限成员访问共享名称与 payload 描述。
 *
 * ## Fields
 * - `type_name`：固定容量、NUL 终止的展示类型名拷贝；允许截断。
 * - `variant_name`：固定容量、NUL 终止的当前变体名拷贝；允许截断。
 * - `entry_function`：可选的编译元数据上下文，借用函数而不持有引用。
 * - `prototype`：可选 union 原型记录的借用地址。
 * - `members`：可选原型成员数组，与 prototype 的成员数配套。
 * - `variant_member`：可选已匹配变体成员，提供声明参数数量和装饰器索引。
 * - `variant_metadata`：可选变体装饰器对象，由函数常量及 VM 维持存活。
 * - `payload_fields`：可选 payloadFields 数组，不拥有数组或元素。
 * - `payload_count`：有匹配变体时取声明参数数；boxed 降级视图可取自身内部字段条目数。
 * - `tag_size`：inline 视图来自类型布局；boxed 视图仅在可选元数据提供整数时填写。
 *
 * ## Invariants and lifecycle
 * 构造入口先清零。boxed 视图可在元数据指针均为空时仍成功，不能据此证明 canonical 类型身份；inline 视图已经验证 tag 范围但不保证每个 payload 布局可解码。名称是拷贝，其余对象/表指针均只借用，不构成 GC 根；不得跨函数释放或暂停帧恢复继续解引用。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_inline_union_view。
 * - zr_debug_union_payload_value_from_inline。
 * - zr_debug_expand_union_value_handle。
 */

/**
 * @fn TZrBool zr_debug_value_is_union_carrier(SZrState *state, const SZrTypeValue *value)
 * @unit UNION-045
 *
 * ## Summary
 * 检查对象能否提供字符串型内部 variant 字段，为 union 形状分支提供窄判别。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态，查字段名可能分配字符串键。
 * @param[in] value 借用 OBJECT/ARRAY 值；其它标签或空对象不匹配。
 *
 * ## Returns
 * @return TRUE 只说明内部 variant 字段读取到非空字符串对象；FALSE 表示类型/字段不满足。
 *
 * ## Constraints and lifecycle
 * 不要求 type 字段、原型记录或 canonical 事实，不证明对象由编译器生成。可经原型取得字段，不调用脚本 getter但可能分配或 Throw。
 *
 * ## References
 * - zr_debug_union_object_string_field。
 * - zr_debug_value_child_shape。
 * - zr_debug_evaluate_with_policy。
 */

/**
 * @fn TZrSize zr_debug_count_union_carrier_payload_fields(SZrState *state, const SZrTypeValue *value)
 * @unit UNION-046
 *
 * ## Summary
 * 统计 carrier 自身哈希表内采用内部 payload 数字键的条目，为子节点形状和缺编译元数据时的展示提供数量。
 *
 * ## Parameters
 * @param[in] state 借用状态，用于把字符串对象解释成原生文本。
 * @param[in] value 借用 OBJECT/ARRAY 值；空值、无对象或无有效 nodeMap 时计零。
 *
 * ## Returns
 * @return 返回自身哈希条目中前缀和十进制后缀可解析的数量；零不能区分无 payload 与无效输入。
 *
 * ## Constraints and lifecycle
 * 只计自身表，不计原型继承字段；不是最大索引加一，不保证索引连续，也不校验编译来源。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_value_child_shape。
 * - zr_debug_union_parse_payload_index。
 */

/**
 * @fn TZrBool zr_debug_union_value_view(ZrDebugAgent *agent, const SZrTypeValue *value, ZrDebugUnionView *outView)
 * @unit UNION-047
 *
 * ## Summary
 * 为 boxed carrier 构造统一 union 展示视图，先使用运行时名称和自身 payload 数，再在名称匹配时补充编译原型。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及 state，元数据字段名读取可能分配。
 * @param[in] value 借用非空 OBJECT/ARRAY carrier 值。
 * @param[out] outView 非 NULL 的独立视图槽，入口清零；名称为固定容量拷贝，元数据指针均借用。
 *
 * ## Returns
 * @return TRUE 表示取得非空 variant 文本并完成展示视图；即使没有匹配编译原型也可 TRUE。
 *
 * ## Constraints and lifecycle
 * 有匹配变体时 payload_count 改为声明参数数；否则保留自身字段计数。名称可截断，视图不是类型身份认证；对象、函数元数据及暂停上下文必须保持有效。字段读取可 Throw。
 *
 * ## References
 * - zr_debug_union_find_prototype_record_by_name。
 * - zr_debug_union_find_variant_member。
 * - zr_debug_expand_union_value_handle。
 */

/**
 * @fn TZrBool zr_debug_inline_union_view(ZrDebugAgent *agent, const SZrFunction *function, const SZrFunctionFrameSlotLayout *slotLayout, const SZrStackFramePlace *place, ZrDebugUnionView *outView)
 * @unit UNION-048
 *
 * ## Summary
 * 从暂停帧的 inline union 存储读取 tag，并连接函数布局、编译原型和变体元数据形成展示视图。
 *
 * ## Parameters
 * @param[in,out] agent 借用提供 state 的代理，元数据字段读取可能分配。
 * @param[in] function 借用当前有效函数及其帧布局上下文。
 * @param[in] slotLayout 借用该函数中 INLINE_STRUCT 且有 typeLayoutId 的槽布局。
 * @param[in] place 借用已取得的槽地址及 byteSize，至少在本次暂停访问期间有效。
 * @param[out] outView 非 NULL 的输出视图，入口清零；成功时交付拷贝名称和借用元数据。
 *
 * ## Returns
 * @return TRUE 表示 union 类型布局、tag边界、原型及tag对应变体均有效；FALSE 可留下已清零或部分构建的视图。
 *
 * ## Constraints and lifecycle
 * 读取 tag 前验证宽度和 place 边界；不证明所有 payload 字段都可解码。元数据读取仍可能分配/Throw；继续执行或函数释放后不可继续借用该视图。
 *
 * ## References
 * - ZrCore_Function_ResolvePrototypeFrameTypeLayout。
 * - zr_debug_union_find_variant_by_tag。
 * - zr_debug_inline_union_slot_view。
 */

/**
 * @fn TZrBool zr_debug_inline_union_slot_view(ZrDebugAgent *agent, const SZrFunction *function, const SZrCallInfo *callInfo, TZrUInt32 stackSlot, SZrStackFramePlace *outPlace, ZrDebugUnionView *outView)
 * @unit UNION-049
 *
 * ## Summary
 * 将当前调用帧的指定 inline 槽解析为可读原始地址与 union 视图，供展开、预览及 legacy 物化共用。
 *
 * ## Parameters
 * @param[in,out] agent 借用提供 state 的代理，后续视图构造可分配。
 * @param[in] function 借用与 callInfo/stackSlot 相符的函数。
 * @param[in] callInfo 借用仍存活且 functionBase.valuePointer 非空的调用帧。
 * @param[in] stackSlot 函数帧布局中的槽号。
 * @param[out] outPlace 可为 NULL；非空入口清零，只有整体成功后发布槽地址。
 * @param[out] outView 必需视图，非空入口清零；底层视图构造失败可留下部分内容。
 *
 * ## Returns
 * @return TRUE 表示槽为 inline struct、帧位置取得成功并可识别为 union；FALSE 表示任一步不满足。
 *
 * ## Constraints and lifecycle
 * frameBase 为函数基指针之后一个值槽；要求在同一暂停窗口访问，不能跨恢复执行保存 place 地址。普通失败不等同捕获 Throw。
 *
 * ## References
 * - ZrCore_Function_FindFrameSlotLayout。
 * - ZrCore_Function_MakeFrameSlotPlace。
 * - zr_debug_inline_union_view。
 */

/**
 * @fn TZrBool zr_debug_union_payload_display_name(SZrState *state, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-050
 *
 * ## Summary
 * 为 payload 子节点选择装饰器用户名称，缺失时生成 payloadN，统一展示与受限成员查找的名称。
 *
 * ## Parameters
 * @param[in,out] state 可为空；非空时用于借用元数据字段，查询可分配。
 * @param[in] view 可为空；缺少 payload_fields 时使用编号回退。
 * @param[in] payloadIndex 欲展示的零基 payload 序号，本函数不校验 payload_count。
 * @param[out] buffer 非空、至少 bufferSize 字节的写入缓冲区。
 * @param[in] bufferSize 必须大于零，包括结尾 NUL；容量不足会截断。
 *
 * ## Returns
 * @return TRUE 表示有效输出缓冲区已写元数据名称或编号回退；FALSE 只表示缓冲区为空或零容量。
 *
 * ## Constraints and lifecycle
 * 入口在有容量时先清零；名称截断不视为失败。输入元数据及文本只借用；元数据查询仍可能 Throw。
 *
 * ## References
 * - zr_debug_union_array_object_at。
 * - zr_debug_union_safe_get_member_value。
 * - zr_debug_expand_inline_union_handle。
 */

/**
 * @fn TZrBool zr_debug_union_payload_value_from_carrier(ZrDebugAgent *agent, const SZrTypeValue *value, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, SZrTypeValue *outValue)
 * @unit UNION-051
 *
 * ## Summary
 * 从 boxed carrier 直接读取一个内部 payload，缺槽时保留可展示的 null，供展开及受限成员求值。
 *
 * ## Parameters
 * @param[in,out] agent 借用提供 state 的代理，内部字段名和键读取可分配。
 * @param[in] value 借用非空 OBJECT/ARRAY carrier；须与 outValue 独立。
 * @param[in] view 可为空，只参与内部名选择；本函数不重新验证整个视图。
 * @param[in] payloadIndex 调用者验证的 payload 序号，本函数不检查 payload_count。
 * @param[out] outValue 非 NULL 输出，入口重置为 null；成功时为存储值的浅拷贝。
 *
 * ## Returns
 * @return TRUE 表示字段取得或字段缺失按 null 展示；FALSE 表示参数或名称缓冲区规则不满足。
 *
 * ## Constraints and lifecycle
 * NULL 字段也可能来自字符串键创建普通失败，故 TRUE/null 不是强错误区分。浅拷贝不增加所有权或独立 GC 根；源 carrier 必须维持存活。Throw 可非局部退出。
 *
 * ## References
 * - zr_debug_union_object_get_field。
 * - zr_debug_union_payload_internal_name。
 * - zr_debug_union_safe_get_member_value。
 */

/**
 * @fn TZrBool zr_debug_union_payload_value_from_inline(ZrDebugAgent *agent, const SZrFunction *function, const SZrStackFramePlace *place, const ZrDebugUnionView *view, TZrUInt32 payloadIndex, SZrTypeValue *outValue)
 * @unit UNION-052
 *
 * ## Summary
 * 根据当前变体的 payload 元数据读取暂停帧字节，为 inline 展开或 carrier 物化提供一个值。
 *
 * ## Parameters
 * @param[in,out] agent 借用 state，元数据读取和带所有权值的复制可分配。
 * @param[in] function 必须非空的当前函数上下文；当前函数体仅保留该参数，不参与字段解码。
 * @param[in] place 借用有效地址和完整 byteSize 的帧槽位置。
 * @param[in] view 借用与该槽当前变体对应的视图及 payload_fields。
 * @param[in] payloadIndex 必须小于 view->payload_count 的零基序号。
 * @param[out] outValue 非 NULL 独立输出，入口重置为 null，不释放其旧资源。
 *
 * ## Returns
 * @return TRUE 表示字段元数据可解释、offset/size 落在 place 内且底层解码返回成功；FALSE 时输出通常为入口 null。
 *
 * ## Constraints and lifecycle
 * 不验证 place 与 view 的身份一致性，调用者须从同一次槽视图取得二者。地址只在暂停帧有效；布局启发式 TODO 与 Copy/分配 Throw 限制均沿用。
 *
 * ## References
 * - zr_debug_union_payload_field_layout。
 * - zr_debug_union_load_field。
 * - zr_debug_inline_union_slot_view。
 */

/**
 * @fn TZrBool zr_debug_materialize_inline_union_slot(ZrDebugAgent *agent, const SZrFunction *function, const SZrCallInfo *callInfo, TZrUInt32 stackSlot, SZrTypeValue *outValue)
 * @unit UNION-053
 *
 * ## Summary
 * 在 legacy 标识符解析需要值对象时，将当前 inline union 槽物化成包含类型、变体与 payload 的 carrier。
 *
 * ## Parameters
 * @param[in,out] agent 借用当前暂停代理与 state，创建对象和写字段可分配/Throw。
 * @param[in] function 借用与调用帧和槽号相符的函数。
 * @param[in] callInfo 借用仍处于当前暂停窗口的调用帧。
 * @param[in] stackSlot 欲物化的 inline union 槽号。
 * @param[out] outValue 非 NULL 独立输出，入口重置为 null，只有全部字段成功后才发布 carrier。
 *
 * ## Returns
 * @return TRUE 表示 carrier 已完整构建并发布；普通失败返回 FALSE，输出保持 null；Throw 可绕过普通返回。
 *
 * ## Constraints and lifecycle
 * BUG: 允许 legacy fallback 的公开求值在解析失败表达式左侧 inline union 局部变量时可达；对象被显式 Ignore 后本路径及句柄清理都无 Unignore，成功重复物化或后续构造失败均可使 carrier 根滞留至 VM/GC 销毁。先前字段无回滚；本注释仅记录完整静态路径，未运行故障复现。
 *
 * ## References
 * - zr_debug_resolve_identifier_value。
 * - ZrCore_GarbageCollector_IgnoreObject。
 * - ZrCore_GarbageCollector_UnignoreObject。
 * - zr_debug_variable_handles_clear。
 */

/**
 * @fn TZrBool zr_debug_union_safe_get_member_value(ZrDebugAgent *agent, const SZrTypeValue *receiver, const TZrChar *memberName, SZrTypeValue *outValue)
 * @unit UNION-054
 *
 * ## Summary
 * 为 boxed union 暴露受限的 type、variant 和 payload 名称访问，使用直接存储而不执行脚本 getter。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理和 state，名称或字段查询可分配/Throw。
 * @param[in] receiver 借用可构造 union 展示视图的 OBJECT/ARRAY 值，不与输出重叠。
 * @param[in] memberName 借用 NUL 终止的请求名称。
 * @param[out] outValue 非 NULL 输出，入口清为 null；类型/变体为 VM 字符串，payload 为浅拷贝。
 *
 * ## Returns
 * @return TRUE 表示命中 type/variant、有效编号或匹配的展示名并交付结果；FALSE 表示输入/视图/成员不满足。
 *
 * ## Constraints and lifecycle
 * 只读语义不保证零分配；不存在脚本 getter 派发不等于可以绕过正式求值策略。名称截断及 payload 浅借用规则沿用下层。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_union_make_string_value。
 * - zr_debug_safe_get_member_value。
 * - zr_debug_expand_union_value_handle。
 */

/**
 * @fn TZrBool zr_debug_format_union_value_text(ZrDebugAgent *agent, const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-055
 *
 * ## Summary
 * 在普通值预览之前尝试生成 boxed union 的类型与变体文本，供 snapshot 及公开求值结果复用。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理与 state，构造展示视图时可分配。
 * @param[in] value 借用待识别的 carrier 值。
 * @param[out] buffer 非空且可写的输出文本缓冲区，有容量时入口清空。
 * @param[in] bufferSize 必须大于零，包含结尾 NUL。
 *
 * ## Returns
 * @return TRUE 表示成功构成 union 视图并格式化，文本可截断；FALSE 表示缓冲区或视图条件不满足，调用者可采用其它格式器。
 *
 * ## Constraints and lifecycle
 * 不校验 canonical 类型身份，不承诺无分配或捕获 Throw；输出只是停点时的展示文本，不延长底层值存活。
 *
 * ## References
 * - zr_debug_union_value_view。
 * - zr_debug_format_inline_union_text。
 * - zr_debug_fill_value_preview。
 * - zr_debug_evaluate_with_policy。
 */

/**
 * @fn void zr_debug_format_inline_union_text(const ZrDebugUnionView *view, TZrChar *buffer, TZrSize bufferSize)
 * @unit UNION-056
 *
 * ## Summary
 * 将已构成的 union 视图格式化为固定的类型.变体预览，不再读取 VM 字段。
 *
 * ## Parameters
 * @param[in] view 借用视图，可为 NULL；非空时两个固定容量名称应已 NUL 终止。
 * @param[out] buffer 可为空；非空且有容量时写入文本并保证结尾 NUL。
 * @param[in] bufferSize 缓冲区总容量；零表示不写入。
 *
 * ## Returns
 * 无返回值；无效缓冲区直接返回，NULL 视图使有效缓冲区保持空串，正常输出允许截断。
 *
 * ## Constraints and lifecycle
 * 只读取视图内已拷贝名称，无分配或字段访问；不由文本判断原型有效性。
 *
 * ## References
 * - zr_debug_format_union_value_text。
 * - zr_debug_fill_inline_union_preview。
 */

/**
 * @struct ZrDebugThreadEntry
 *
 * ## Summary
 * 把会话内协议线程 ID 映射到借用的 VM state，并记录 Agent 卸载自身 observer 时恢复的旧 hook 掩码；它是 Agent 数组元素，不是 OS 线程句柄。
 *
 * ## Fields
 * - thread_id：登记时分配的会话内标量 ID，零用作查找失败或未指定，不代表 OS 线程号。
 * - previous_debug_hook_signal：首次登记前的 debugHookSignal；只保存掩码，不保存旧 observer 函数和 userData。
 * - state：借用的存活 VM 状态，Agent 不销毁它，也不凭该字段建立独立 GC 根。
 * - name：登记时复制或生成的固定容量零终止显示文本，可按字节截断。
 *
 * ## Invariants and lifecycle
 * Agent 拥有包含条目的 threads 数组；realloc 后旧条目地址失效，AgentStop 释放数组。状态须存活到卸载完成；仅在 state 的 userData 仍属于本 Agent 时恢复旧掩码。表读写由调用方协调，字段不提供同步。
 *
 * ## References
 * zr_debug_agent_register_thread_state 创建条目；zr_debug_agent_find_thread_by_id/by_state 仅借用查找；ZrDebug_AgentStop 检查回调归属后卸载。
 */

/**
 * @struct ZrDebugAgent
 *
 * ## Summary
 * 把调试会话的网络、协议状态、停止/单步状态及只读快照索引集中在一起，使 trace 回调、协议处理和生命周期入口共享一个上下文。
 *
 * ## Fields
 * - state/currentThreadId 是当前请求上下文；state 与 entryFunction 借用 VM 对象，读取其它线程时由 begin/end 临时切换并恢复。
 * - testManifest/hasTestManifest 记录已成功解码的自有原生清单；AgentStop 使用仍存活的 state/global 配对释放。
 * - config 按值复制配置，auth_token 等指针字段不随复制转移所有权；listener/client 拥有网络句柄，hasClient/clientInitialized 标识连接和初始化阶段。
 * - waitForClientPending/startStopPending、pauseRequested/exceptionStopPending、异常订阅和 clientDisconnectContinue 协作决定协议等待/停止；volatile 字段本身不构成完整线程同步保证。
 * - runMode、stepDepth/stepCallInfo、resumeFunction/resumeInstruction、stopStateId/lastStopEvent 保存执行控制和停止代际；指向 VM 的字段仍为借用。
 * - breakpoints 及其各计数、dataBreakpoints 及容量、variableHandles 及容量/nextVariableHandleId 是会话管理的数组或索引；句柄记录不等于取得所有 VM 对象所有权。
 * - threads/threadCount/threadCapacity/nextThreadId 持有协议线程登记；条目 state 借用，新增登记可移动整个数组。
 * - moduleName/endpoint 为会话内部文本副本；客户端拷贝、条目数组和 Agent 自身有各自释放边界。
 *
 * ## Invariants and lifecycle
 * 正常公开会话由 AgentStart 分配并发布，再由 AgentStop 一次销毁；内部测试也可构造最小栈上上下文，但必须自行维护其子资源且不可把栈对象交给 AgentStop。所有注册 state、入口函数和仍使用的鉴权令牌须有效；宿主负责 VM 根及运行/请求/销毁的顺序。先卸载本会话回调，再释放其 userData 所指存储；不恢复旧 observer、不 join，不承诺可并发或重入使用整个结构。
 *
 * ## References
 * ZrDebug_AgentStart/ZrDebug_AgentStop 管理完整会话；zr_debug_agent_trace_observer 进入停止/协议链；zr_debug_agent_begin_thread_access/end_thread_access 配对切换上下文；test_debug_evaluate_result_publishes_canonical_type_and_stop_state 构造并自行释放最小上下文。
 */
