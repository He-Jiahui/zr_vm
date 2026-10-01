#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"

#define TAIL_DISPATCH_RUNTIME_ITERATIONS 8192u

static SZrState *g_state;

typedef struct TailDispatchRuntimeTrace {
    const SZrFunction *tailFunction;
    const SZrFunction *dynamicCallFunction;
    const SZrFunction *catchFunction;
    const SZrFunction *destructorFunction;
    const SZrCallInfo *firstTailCallInfo;
    TZrSize firstFrameBaseOffset;
    TZrUInt32 tailCallCount;
    TZrUInt32 dynamicCallCount;
    TZrUInt32 catchEntryCount;
    TZrSize ownershipRootCountAtCatch;
    TZrUInt32 callInfoDepthAtCatch;
    TZrBool exceptionPresentAtCatch;
    TZrUInt32 destructorEntryCount;
    TZrUInt32 destructorNameEntryCount;
    TZrUInt32 observerInstructionCount;
    TZrUInt32 observedFunctionEntryCount;
    TZrUInt32 maximumCallInfoDepth;
    TZrSize maximumStackSlotCount;
    TZrBool tailFrameStayedStable;
} TailDispatchRuntimeTrace;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrFunction *compile_source(const TZrChar *source, const TZrChar *sourceName) {
    SZrString *name;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceName);
    name = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)sourceName);
    TEST_ASSERT_NOT_NULL(name);
    return ZrParser_Source_Compile(g_state, source, strlen(source), name);
}

static const SZrFunction *find_child_function_by_name(const SZrFunction *function,
                                                       const TZrChar *name,
                                                       TZrUInt32 depth) {
    if (function == ZR_NULL || name == ZR_NULL || depth > 64u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < function->childFunctionLength; ++index) {
        const SZrFunction *child = &function->childFunctionList[index];
        const TZrChar *childName = child->functionName != ZR_NULL
                                           ? ZrCore_String_GetNativeString(child->functionName)
                                           : ZR_NULL;
        const SZrFunction *nested;

        if (childName != ZR_NULL && strcmp(childName, name) == 0) {
            return child;
        }
        nested = find_child_function_by_name(child, name, depth + 1u);
        if (nested != ZR_NULL) {
            return nested;
        }
    }
    return ZR_NULL;
}

static const SZrFunction *find_meta_function(const SZrFunction *function, EZrMetaType metaType) {
    const TZrByte *data;
    TZrUInt32 prototypeCount;
    TZrSize offset = sizeof(prototypeCount);

    if (function == ZR_NULL || function->prototypeData == ZR_NULL ||
        function->prototypeDataLength < sizeof(prototypeCount) || function->constantValueList == ZR_NULL) {
        return ZR_NULL;
    }

    data = function->prototypeData;
    memcpy(&prototypeCount, data, sizeof(prototypeCount));
    for (TZrUInt32 prototypeIndex = 0u; prototypeIndex < prototypeCount; ++prototypeIndex) {
        SZrCompiledPrototypeInfo prototype;
        TZrSize inheritBytes;
        TZrSize decoratorBytes;
        TZrSize memberBytes;

        if (offset > function->prototypeDataLength ||
            function->prototypeDataLength - offset < sizeof(prototype)) {
            return ZR_NULL;
        }
        memcpy(&prototype, data + offset, sizeof(prototype));
        offset += sizeof(prototype);
        inheritBytes = (TZrSize)prototype.inheritsCount * sizeof(TZrUInt32);
        decoratorBytes = (TZrSize)prototype.decoratorsCount * sizeof(TZrUInt32);
        memberBytes = (TZrSize)prototype.membersCount * sizeof(SZrCompiledMemberInfo);
        if (inheritBytes > function->prototypeDataLength - offset) {
            return ZR_NULL;
        }
        offset += inheritBytes;
        if (decoratorBytes > function->prototypeDataLength - offset) {
            return ZR_NULL;
        }
        offset += decoratorBytes;
        if (memberBytes > function->prototypeDataLength - offset) {
            return ZR_NULL;
        }

        for (TZrUInt32 memberIndex = 0u; memberIndex < prototype.membersCount; ++memberIndex) {
            SZrCompiledMemberInfo member;
            const SZrTypeValue *constant;
            memcpy(&member, data + offset + (TZrSize)memberIndex * sizeof(member), sizeof(member));
            if (member.isMetaMethod == 0u || member.metaType != (TZrUInt32)metaType ||
                member.functionConstantIndex >= function->constantValueLength) {
                continue;
            }
            constant = &function->constantValueList[member.functionConstantIndex];
            if (constant->type == ZR_VALUE_TYPE_FUNCTION && constant->value.object != ZR_NULL &&
                constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                return ZR_CAST(const SZrFunction *, constant->value.object);
            }
        }
        offset += memberBytes;
    }
    return ZR_NULL;
}

static TZrBool is_dynamic_tail_opcode(EZrInstructionCode opcode) {
    return (TZrBool)(opcode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
                     opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_CACHED) ||
                     opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_TAIL_CALL_NO_ARGS));
}

static TZrBool is_dynamic_call_opcode(EZrInstructionCode opcode) {
    return (TZrBool)(opcode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||
                     opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_CACHED) ||
                     opcode == ZR_INSTRUCTION_ENUM(SUPER_DYN_CALL_NO_ARGS));
}

static TZrUInt32 count_dynamic_tail_opcodes(const SZrFunction *function) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
        EZrInstructionCode opcode =
                (EZrInstructionCode)function->instructionsList[index].instruction.operationCode;
        if (is_dynamic_tail_opcode(opcode)) {
            ++count;
        }
    }
    return count;
}

static TZrUInt32 count_dynamic_call_opcodes(const SZrFunction *function) {
    TZrUInt32 count = 0u;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; ++index) {
        EZrInstructionCode opcode =
                (EZrInstructionCode)function->instructionsList[index].instruction.operationCode;
        if (is_dynamic_call_opcode(opcode)) {
            ++count;
        }
    }
    return count;
}

static TZrUInt32 call_info_depth(const SZrCallInfo *callInfo) {
    TZrUInt32 depth = 0u;
    while (callInfo != ZR_NULL && depth < 1024u) {
        ++depth;
        callInfo = callInfo->previous;
    }
    return depth;
}

static TZrSize stack_pointer_offset(SZrState *state, TZrStackValuePointer pointer) {
    if (state == ZR_NULL || pointer == ZR_NULL || state->stackBase.valuePointer == ZR_NULL ||
        pointer < state->stackBase.valuePointer) {
        return 0u;
    }
    return (TZrSize)(pointer - state->stackBase.valuePointer);
}

static TZrDebugSignal observe_tail_dispatch(struct SZrState *state,
                                            struct SZrFunction *function,
                                            const TZrInstruction *programCounter,
                                            TZrUInt32 instructionOffset,
                                            TZrUInt32 sourceLine,
                                            TZrPtr userData) {
    TailDispatchRuntimeTrace *trace = (TailDispatchRuntimeTrace *)userData;
    TZrUInt32 depth;
    TZrSize stackSlots;

    ZR_UNUSED_PARAMETER(programCounter);
    ZR_UNUSED_PARAMETER(sourceLine);
    if (state == ZR_NULL || trace == ZR_NULL) {
        return ZR_DEBUG_SIGNAL_NONE;
    }

    ++trace->observerInstructionCount;
    if (instructionOffset == 0u) {
        ++trace->observedFunctionEntryCount;
        if (trace->destructorFunction != ZR_NULL &&
            trace->destructorFunction->functionName != ZR_NULL && function != ZR_NULL &&
            function->functionName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(trace->destructorFunction->functionName),
                   ZrCore_String_GetNativeString(function->functionName)) == 0) {
            ++trace->destructorNameEntryCount;
        }
    }

    depth = call_info_depth(state->callInfoList);
    if (depth > trace->maximumCallInfoDepth) {
        trace->maximumCallInfoDepth = depth;
    }
    stackSlots = stack_pointer_offset(state, state->stackTop.valuePointer);
    if (stackSlots > trace->maximumStackSlotCount) {
        trace->maximumStackSlotCount = stackSlots;
    }

    if (trace->tailFunction == function && instructionOffset < function->instructionsLength &&
        is_dynamic_tail_opcode((EZrInstructionCode)function->instructionsList[instructionOffset]
                                       .instruction.operationCode)) {
        const SZrCallInfo *callInfo = state->callInfoList;
        TZrSize frameBaseOffset = callInfo != ZR_NULL
                                          ? stack_pointer_offset(state, callInfo->functionBase.valuePointer)
                                          : 0u;
        if (trace->tailCallCount == 0u) {
            trace->firstTailCallInfo = callInfo;
            trace->firstFrameBaseOffset = frameBaseOffset;
            trace->tailFrameStayedStable = ZR_TRUE;
        } else if (callInfo != trace->firstTailCallInfo || frameBaseOffset != trace->firstFrameBaseOffset) {
            trace->tailFrameStayedStable = ZR_FALSE;
        }
        ++trace->tailCallCount;
    }

    if (trace->dynamicCallFunction == function && instructionOffset < function->instructionsLength &&
        is_dynamic_call_opcode((EZrInstructionCode)function->instructionsList[instructionOffset]
                                       .instruction.operationCode)) {
        ++trace->dynamicCallCount;
    }

    if (trace->catchFunction == function && instructionOffset < function->instructionsLength &&
        (EZrInstructionCode)function->instructionsList[instructionOffset].instruction.operationCode ==
                ZR_INSTRUCTION_ENUM(CATCH)) {
        ++trace->catchEntryCount;
        trace->ownershipRootCountAtCatch =
                ZrCore_GcDomain_GetOwnershipRootCount(state);
        trace->callInfoDepthAtCatch = call_info_depth(state->callInfoList);
        trace->exceptionPresentAtCatch = state->hasCurrentException;
    }

    if (trace->destructorFunction == function && instructionOffset == 0u) {
        ++trace->destructorEntryCount;
    }
    return ZR_DEBUG_SIGNAL_NONE;
}

static const TZrChar *error_object_string_field(SZrState *state,
                                                const SZrTypeValue *errorValue,
                                                const TZrChar *fieldName) {
    SZrObject *errorObject;
    SZrString *fieldNameString;
    SZrTypeValue key;
    const SZrTypeValue *fieldValue;

    if (state == ZR_NULL || errorValue == ZR_NULL || errorValue->type != ZR_VALUE_TYPE_OBJECT ||
        errorValue->value.object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    errorObject = ZR_CAST_OBJECT(state, errorValue->value.object);
    if (errorObject == ZR_NULL) {
        return ZR_NULL;
    }
    fieldNameString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    fieldValue = ZrCore_Object_GetValue(state, errorObject, &key);
    if (fieldValue == ZR_NULL || fieldValue->type != ZR_VALUE_TYPE_STRING || fieldValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, fieldValue->value.object));
}

static void test_eligible_dynamic_tail_call_reuses_active_runtime_frame(void) {
    static const TZrChar *source =
            "fn tailDispatch(target: object, remaining: int, value: int): object {\n"
            "  if (remaining == 0) { return value; }\n"
            "  return target(target, remaining - 1, value + 1);\n"
            "}\n"
            "return <int> tailDispatch(tailDispatch, 8192, 0);\n";
    SZrFunction *function;
    const SZrFunction *tailFunction;
    TailDispatchRuntimeTrace trace;
    TZrInt64 result = 0;
    TZrBool completed;

    function = compile_source(source, "ssa_tail_dispatch_runtime_positive.zr");
    TEST_ASSERT_NOT_NULL(function);
    tailFunction = find_child_function_by_name(function, "tailDispatch", 0u);
    TEST_ASSERT_NOT_NULL(tailFunction);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_dynamic_tail_opcodes(tailFunction));

    memset(&trace, 0, sizeof(trace));
    trace.tailFunction = tailFunction;
    ZrCore_Debug_SetTraceObserver(g_state, observe_tail_dispatch, &trace);
    completed = ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result);
    ZrCore_Debug_SetTraceObserver(g_state, ZR_NULL, ZR_NULL);
    TEST_ASSERT_TRUE(completed);

    TEST_ASSERT_EQUAL_INT64((TZrInt64)TAIL_DISPATCH_RUNTIME_ITERATIONS, result);
    TEST_ASSERT_EQUAL_UINT32(TAIL_DISPATCH_RUNTIME_ITERATIONS, trace.tailCallCount);
    TEST_ASSERT_TRUE(trace.tailFrameStayedStable);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(4u, trace.maximumCallInfoDepth);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(64u, trace.maximumStackSlotCount);

    TEST_ASSERT_EQUAL_PTR(g_state->stackBase.valuePointer + 2, g_state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_INT64(8192,
            ZrCore_Stack_GetValue(g_state->stackBase.valuePointer + 1)->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(g_state, function);
}

static void test_noncallable_dynamic_tail_failure_restores_error_and_owned_frame(void) {
    static const TZrChar *source =
            "resource class Tracker {\n"
            "  pub @constructor() {}\n"
            "  pub @destructor() {}\n"
            "}\n"
            "fn relay(target: object): object {\n"
            "  return target();\n"
            "}\n"
            "var owner: Unique<Tracker> = own Tracker();\n"
            "var result: object = relay(1);\n"
            "if (owner == null) { return null; }\n"
            "return result;\n";
    SZrFunction *function;
    const SZrFunction *relayFunction;
    const SZrFunction *destructorFunction;
    TailDispatchRuntimeTrace trace;
    SZrTypeValue result;
    TZrSize ownershipRootCount;
    TZrBool completed;
    const TZrChar *message;
    const TZrChar *stack;
    TZrSize ownershipRootCountAfter;

    function = compile_source(source, "ssa_tail_dispatch_runtime_failure.zr");
    TEST_ASSERT_NOT_NULL(function);
    relayFunction = find_child_function_by_name(function, "relay", 0u);
    destructorFunction = find_meta_function(function, ZR_META_DESTRUCTOR);
    TEST_ASSERT_NOT_NULL(relayFunction);
    TEST_ASSERT_NOT_NULL(destructorFunction);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_dynamic_tail_opcodes(relayFunction));

    memset(&trace, 0, sizeof(trace));
    trace.tailFunction = relayFunction;
    trace.dynamicCallFunction = relayFunction;
    trace.destructorFunction = destructorFunction;
    ownershipRootCount = ZrCore_GcDomain_GetOwnershipRootCount(g_state);
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_Debug_SetTraceObserver(g_state, observe_tail_dispatch, &trace);
    completed = ZrTests_Runtime_Function_ExecuteCaptureFailure(g_state, function, &result);
    ZrCore_Debug_SetTraceObserver(g_state, ZR_NULL, ZR_NULL);
    ownershipRootCountAfter = ZrCore_GcDomain_GetOwnershipRootCount(g_state);
    if (trace.destructorEntryCount != 1u || ownershipRootCountAfter != ownershipRootCount) {
        const TZrChar *destructorName = destructorFunction->functionName != ZR_NULL
                                                ? ZrCore_String_GetNativeString(destructorFunction->functionName)
                                                : "<unnamed>";
        fprintf(stderr,
                "tail cleanup probe: tailCalls=%u traceInstructions=%u functionEntries=%u "
                "destructorIdentityEntries=%u destructorNameEntries=%u destructorInstructions=%u "
                "destructor=%p name=%s roots=%llu->%llu exception=%u status=%u callInfoDepth=%u stackSlots=%llu\n",
                (unsigned)trace.tailCallCount,
                (unsigned)trace.observerInstructionCount,
                (unsigned)trace.observedFunctionEntryCount,
                (unsigned)trace.destructorEntryCount,
                (unsigned)trace.destructorNameEntryCount,
                (unsigned)destructorFunction->instructionsLength,
                (void *)destructorFunction,
                destructorName,
                (unsigned long long)ownershipRootCount,
                (unsigned long long)ownershipRootCountAfter,
                (unsigned)g_state->hasCurrentException,
                (unsigned)g_state->currentExceptionStatus,
                (unsigned)call_info_depth(g_state->callInfoList),
                (unsigned long long)stack_pointer_offset(g_state, g_state->stackTop.valuePointer));
    }

    TEST_ASSERT_FALSE(completed);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.tailCallCount);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)ownershipRootCount, (TZrUInt64)ownershipRootCountAfter);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.destructorEntryCount);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, g_state->currentExceptionStatus);
    message = error_object_string_field(g_state, &g_state->currentException, "message");
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "Attempted to call non-callable value"));
    stack = error_object_string_field(g_state, &g_state->currentException, "stack");
    TEST_ASSERT_NOT_NULL(stack);
    TEST_ASSERT_NOT_NULL(strstr(stack, "relay"));
    TEST_ASSERT_NOT_NULL(strstr(stack, "ssa_tail_dispatch_runtime_failure.zr"));
    TEST_ASSERT_EQUAL_PTR(&g_state->baseCallInfo, g_state->callInfoList);
    TEST_ASSERT_NULL(g_state->baseCallInfo.metadataFunction);
    TEST_ASSERT_EQUAL_PTR(g_state->stackBase.valuePointer + 1, g_state->stackTop.valuePointer);

    ZrCore_Function_Free(g_state, function);
}

static void test_noncallable_dynamic_call_failure_restores_error_and_owned_frame(void) {
    static const TZrChar *source =
            "resource class Tracker {\n"
            "  pub @constructor() {}\n"
            "  pub @destructor() {}\n"
            "}\n"
            "fn relayOrdinary(target: object): object {\n"
            "  var value: object = target();\n"
            "  return value;\n"
            "}\n"
            "var owner: Unique<Tracker> = own Tracker();\n"
            "var result: object = relayOrdinary(1);\n"
            "if (owner == null) { return null; }\n"
            "return result;\n";
    SZrFunction *function;
    const SZrFunction *relayFunction;
    const SZrFunction *destructorFunction;
    TailDispatchRuntimeTrace trace;
    SZrTypeValue result;
    TZrSize ownershipRootCount;
    TZrBool completed;
    const TZrChar *message;
    const TZrChar *stack;
    TZrSize ownershipRootCountAfter;

    function = compile_source(source, "ssa_dynamic_call_runtime_failure.zr");
    TEST_ASSERT_NOT_NULL(function);
    relayFunction = find_child_function_by_name(function, "relayOrdinary", 0u);
    destructorFunction = find_meta_function(function, ZR_META_DESTRUCTOR);
    TEST_ASSERT_NOT_NULL(relayFunction);
    TEST_ASSERT_NOT_NULL(destructorFunction);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_dynamic_call_opcodes(relayFunction));
    TEST_ASSERT_EQUAL_UINT32(0u, count_dynamic_tail_opcodes(relayFunction));

    memset(&trace, 0, sizeof(trace));
    trace.tailFunction = relayFunction;
    trace.dynamicCallFunction = relayFunction;
    trace.destructorFunction = destructorFunction;
    ownershipRootCount = ZrCore_GcDomain_GetOwnershipRootCount(g_state);
    ZrCore_Value_ResetAsNull(&result);
    ZrCore_Debug_SetTraceObserver(g_state, observe_tail_dispatch, &trace);
    completed = ZrTests_Runtime_Function_ExecuteCaptureFailure(g_state, function, &result);
    ZrCore_Debug_SetTraceObserver(g_state, ZR_NULL, ZR_NULL);
    ownershipRootCountAfter = ZrCore_GcDomain_GetOwnershipRootCount(g_state);

    TEST_ASSERT_FALSE(completed);
    TEST_ASSERT_EQUAL_UINT32(0u, trace.tailCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.dynamicCallCount);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)ownershipRootCount, (TZrUInt64)ownershipRootCountAfter);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.destructorEntryCount);
    TEST_ASSERT_TRUE(g_state->hasCurrentException);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_RUNTIME_ERROR, g_state->currentExceptionStatus);
    message = error_object_string_field(g_state, &g_state->currentException, "message");
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_NOT_NULL(strstr(message, "Attempted to call non-callable value"));
    stack = error_object_string_field(g_state, &g_state->currentException, "stack");
    TEST_ASSERT_NOT_NULL(stack);
    TEST_ASSERT_NOT_NULL(strstr(stack, "relayOrdinary"));
    TEST_ASSERT_NOT_NULL(strstr(stack, "ssa_dynamic_call_runtime_failure.zr"));
    TEST_ASSERT_EQUAL_PTR(&g_state->baseCallInfo, g_state->callInfoList);
    TEST_ASSERT_NULL(g_state->baseCallInfo.metadataFunction);
    TEST_ASSERT_EQUAL_PTR(g_state->stackBase.valuePointer + 1, g_state->stackTop.valuePointer);

    ZrCore_Function_Free(g_state, function);
}

static void test_noncallable_dynamic_call_failure_is_caught_and_owner_closes(void) {
    static const TZrChar *source =
            "resource class Tracker {\n"
            "  pub static var caught: int = 0;\n"
            "  pub @constructor() {}\n"
            "  pub @destructor() {}\n"
            "}\n"
            "fn relayCaught(target: object): object {\n"
            "  try {\n"
            "    var value: object = target();\n"
            "    return value;\n"
            "  } catch (error) {\n"
            "    if (error.message != null && error.stack != null) {\n"
            "      Tracker.caught = Tracker.caught + 1;\n"
            "    } else {\n"
            "      Tracker.caught = -1;\n"
            "    }\n"
            "    return null;\n"
            "  }\n"
            "}\n"
            "var owner: Unique<Tracker> = own Tracker();\n"
            "var result: object = relayCaught(1);\n"
            "if (owner == null || result != null) { return -1; }\n"
            "return Tracker.caught;\n";
    SZrFunction *function;
    const SZrFunction *relayFunction;
    const SZrFunction *destructorFunction;
    TailDispatchRuntimeTrace trace;
    TZrInt64 result = 0;
    TZrSize ownershipRootCount;
    TZrBool completed;
    TZrSize ownershipRootCountAfter;

    function = compile_source(source, "ssa_dynamic_call_runtime_caught.zr");
    TEST_ASSERT_NOT_NULL(function);
    relayFunction = find_child_function_by_name(function, "relayCaught", 0u);
    destructorFunction = find_meta_function(function, ZR_META_DESTRUCTOR);
    TEST_ASSERT_NOT_NULL(relayFunction);
    TEST_ASSERT_NOT_NULL(destructorFunction);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, count_dynamic_call_opcodes(relayFunction));
    TEST_ASSERT_EQUAL_UINT32(0u, count_dynamic_tail_opcodes(relayFunction));

    memset(&trace, 0, sizeof(trace));
    trace.tailFunction = relayFunction;
    trace.dynamicCallFunction = relayFunction;
    trace.catchFunction = relayFunction;
    trace.destructorFunction = destructorFunction;
    ownershipRootCount = ZrCore_GcDomain_GetOwnershipRootCount(g_state);
    ZrCore_Debug_SetTraceObserver(g_state, observe_tail_dispatch, &trace);
    completed = ZrTests_Runtime_Function_ExecuteExpectInt64(g_state, function, &result);
    ZrCore_Debug_SetTraceObserver(g_state, ZR_NULL, ZR_NULL);
    ownershipRootCountAfter = ZrCore_GcDomain_GetOwnershipRootCount(g_state);

    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL_INT64(1, result);
    TEST_ASSERT_EQUAL_UINT32(0u, trace.tailCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.dynamicCallCount);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.catchEntryCount);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)(ownershipRootCount + 1u),
                             (TZrUInt64)trace.ownershipRootCountAtCatch);
    TEST_ASSERT_TRUE(trace.exceptionPresentAtCatch);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(3u, trace.callInfoDepthAtCatch);
    TEST_ASSERT_EQUAL_UINT32(1u, trace.destructorEntryCount);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)ownershipRootCount, (TZrUInt64)ownershipRootCountAfter);
    TEST_ASSERT_FALSE(g_state->hasCurrentException);
    TEST_ASSERT_EQUAL_PTR(&g_state->baseCallInfo, g_state->callInfoList);
    TEST_ASSERT_NULL(g_state->baseCallInfo.metadataFunction);
    TEST_ASSERT_EQUAL_PTR(g_state->stackBase.valuePointer + 2, g_state->stackTop.valuePointer);
    TEST_ASSERT_EQUAL_INT64(1,
            ZrCore_Stack_GetValue(g_state->stackBase.valuePointer + 1)->value.nativeObject.nativeInt64);

    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eligible_dynamic_tail_call_reuses_active_runtime_frame);
    RUN_TEST(test_noncallable_dynamic_tail_failure_restores_error_and_owned_frame);
    RUN_TEST(test_noncallable_dynamic_call_failure_restores_error_and_owned_frame);
    RUN_TEST(test_noncallable_dynamic_call_failure_is_caught_and_owner_closes);
    return UNITY_END();
}
