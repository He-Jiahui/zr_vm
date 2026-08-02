#ifndef ZR_VM_DEBUG_DEBUG_H
#define ZR_VM_DEBUG_DEBUG_H

#include "zr_vm_lib_debug/conf.h"

struct SZrFunction;

typedef struct ZrDebugAgent ZrDebugAgent;

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

typedef enum EZrDebugScopeKind {
    ZR_DEBUG_SCOPE_KIND_ARGUMENTS = 1,
    ZR_DEBUG_SCOPE_KIND_LOCALS = 2,
    ZR_DEBUG_SCOPE_KIND_CLOSURES = 3,
    ZR_DEBUG_SCOPE_KIND_GLOBALS = 4,
    ZR_DEBUG_SCOPE_KIND_PROTOTYPE = 5,
    ZR_DEBUG_SCOPE_KIND_STATICS = 6,
    ZR_DEBUG_SCOPE_KIND_EXCEPTION = 7
} EZrDebugScopeKind;

typedef enum EZrDebugBreakpointKind {
    ZR_DEBUG_BREAKPOINT_KIND_LINE = 1,
    ZR_DEBUG_BREAKPOINT_KIND_FUNCTION = 2
} EZrDebugBreakpointKind;

typedef enum EZrDebugExceptionFilter {
    ZR_DEBUG_EXCEPTION_FILTER_NONE = 0,
    ZR_DEBUG_EXCEPTION_FILTER_CAUGHT = 1,
    ZR_DEBUG_EXCEPTION_FILTER_UNCAUGHT = 2
} EZrDebugExceptionFilter;

typedef struct ZrDebugAgentConfig {
    const TZrChar *address;
    TZrBool suspend_on_start;
    TZrBool wait_for_client;
    const TZrChar *auth_token;
    TZrBool stop_on_uncaught_exception;
} ZrDebugAgentConfig;

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
    TZrChar module_name[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar call_kind[ZR_DEBUG_NAME_CAPACITY];
    TZrChar function_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar source_file[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar receiver_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar receiver_type_name[ZR_DEBUG_NAME_CAPACITY];
    TZrChar receiver_value_text[ZR_DEBUG_TEXT_CAPACITY];
} ZrDebugFrameSnapshot;

typedef struct ZrDebugScopeSnapshot {
    TZrUInt32 thread_id;
    TZrUInt32 scope_id;
    TZrUInt32 frame_id;
    EZrDebugScopeKind kind;
    TZrChar name[ZR_DEBUG_NAME_CAPACITY];
} ZrDebugScopeSnapshot;

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

typedef enum EZrDebugEvaluationEffect {
    ZR_DEBUG_EVALUATION_EFFECT_NONE = 0u,
    ZR_DEBUG_EVALUATION_EFFECT_PROPERTY_GETTER = 1u << 0u,
    ZR_DEBUG_EVALUATION_EFFECT_ALLOCATION = 1u << 1u,
    ZR_DEBUG_EVALUATION_EFFECT_CALL = 1u << 2u,
    ZR_DEBUG_EVALUATION_EFFECT_NATIVE_CALL = 1u << 3u,
    ZR_DEBUG_EVALUATION_EFFECT_MUTATION = 1u << 4u,
    ZR_DEBUG_EVALUATION_EFFECT_OWNER_MUTATION = 1u << 5u
} EZrDebugEvaluationEffect;

typedef struct ZrDebugEvaluationEffectPolicy {
    TZrUInt32 effectFlags;
    TZrBool isPure;
    TZrBool hasCanonicalFacts;
} ZrDebugEvaluationEffectPolicy;

ZR_DEBUG_API TZrBool ZrDebug_AgentStart(SZrState *state, struct SZrFunction *entryFunction, const TZrChar *moduleName,
                                        const ZrDebugAgentConfig *config, ZrDebugAgent **outAgent, TZrChar *errorBuffer,
                                        TZrSize errorBufferSize);

ZR_DEBUG_API void ZrDebug_AgentStop(ZrDebugAgent *agent);

ZR_DEBUG_API TZrBool ZrDebug_AgentGetEndpoint(ZrDebugAgent *agent, TZrChar *buffer, TZrSize bufferSize);

ZR_DEBUG_API TZrBool ZrDebug_SetBreakpoints(ZrDebugAgent *agent, const ZrDebugBreakpointSpec *specs, TZrSize count);
ZR_DEBUG_API TZrBool ZrDebug_SetFunctionBreakpoints(ZrDebugAgent *agent,
                                                    const ZrDebugBreakpointSpec *specs,
                                                    TZrSize count);
ZR_DEBUG_API void ZrDebug_SetExceptionBreakpoints(ZrDebugAgent *agent, TZrBool caught, TZrBool uncaught);

ZR_DEBUG_API void ZrDebug_Continue(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_Pause(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_StepInto(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_StepOver(ZrDebugAgent *agent);
ZR_DEBUG_API void ZrDebug_StepOut(ZrDebugAgent *agent);

ZR_DEBUG_API TZrBool ZrDebug_ReadStack(ZrDebugAgent *agent, ZrDebugFrameSnapshot **outFrames, TZrSize *outCount);

ZR_DEBUG_API TZrBool ZrDebug_ReadScopes(ZrDebugAgent *agent, TZrUInt32 frameId, ZrDebugScopeSnapshot **outScopes,
                                        TZrSize *outCount);

ZR_DEBUG_API TZrBool ZrDebug_ReadVariables(ZrDebugAgent *agent,
                                           TZrUInt32 scopeId,
                                           TZrSize start,
                                           TZrSize count,
                                           ZrDebugValuePreview **outValues,
                                           TZrSize *outCount,
                                           TZrSize *outNamedVariables,
                                           TZrSize *outIndexedVariables);
ZR_DEBUG_API TZrBool ZrDebug_Evaluate(ZrDebugAgent *agent,
                                      TZrUInt32 frameId,
                                      const TZrChar *expression,
                                      ZrDebugEvaluateResult *outResult,
                                      TZrChar *errorBuffer,
                                      TZrSize errorBufferSize);
ZR_DEBUG_API TZrBool ZrDebug_EvaluateWithCapabilities(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        TZrUInt32 allowedEffectFlags,
        ZrDebugEvaluateResult *outResult,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
ZR_DEBUG_API TZrBool ZrDebug_EvaluateWithCapabilitiesDetailed(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const TZrChar *expression,
        TZrUInt32 allowedEffectFlags,
        ZrDebugEvaluateResult *outResult,
        ZrDebugEvaluateFailure *outFailure,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);
ZR_DEBUG_API TZrBool ZrDebug_ClassifyEvaluationEffect(ZrDebugAgent *agent,
                                                       TZrUInt32 frameId,
                                                       const TZrChar *expression,
                                                       ZrDebugEvaluationEffectPolicy *outPolicy,
                                                       TZrChar *errorBuffer,
                                                       TZrSize errorBufferSize);
ZR_DEBUG_API TZrBool ZrDebug_EvaluationEffectPolicy_Allows(
        const ZrDebugEvaluationEffectPolicy *policy,
        TZrUInt32 allowedEffectFlags);

ZR_DEBUG_API void ZrDebug_Free(void *pointer);

ZR_DEBUG_API void ZrDebug_NotifyException(ZrDebugAgent *agent);

ZR_DEBUG_API void ZrDebug_NotifyTerminated(ZrDebugAgent *agent, TZrBool success);

#endif
