#include "repl/repl_session.h"

#include <stdlib.h>
#include <string.h>

#include "repl/repl_input_scan.h"
#include "repl/repl_semantic_facts.h"
#include "project/project.h"
#include "runtime/runtime.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

typedef struct ZrCliReplExecuteRequest {
    TZrStackValuePointer callBase;
    TZrStackValuePointer resultBase;
} ZrCliReplExecuteRequest;

static void zr_cli_repl_session_execute_body(SZrState *state, TZrPtr arguments) {
    ZrCliReplExecuteRequest *request = (ZrCliReplExecuteRequest *)arguments;

    if (state == ZR_NULL || request == ZR_NULL || request->callBase == ZR_NULL) {
        return;
    }

    request->resultBase = ZrCore_Function_CallAndRestore(state, request->callBase, 1);
}

static TZrBool zr_cli_repl_session_handle_failure(SZrState *state, EZrThreadStatus status) {
    if (state == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!state->hasCurrentException &&
        !ZrCore_Exception_NormalizeStatus(
                state,
                state->threadStatus != ZR_THREAD_STATUS_FINE ? state->threadStatus : status)) {
        ZrCore_Log_Error(state, "repl execution failed with status %d\n", (int)status);
        ZrCore_State_ResetThread(state, status);
        return ZR_FALSE;
    }

    if (state->hasCurrentException) {
        ZrCore_Exception_LogUnhandled(state, &state->currentException);
        ZrCore_State_ResetThread(state, state->currentExceptionStatus);
        return ZR_FALSE;
    }

    ZrCore_Log_Error(state, "repl execution failed with status %d\n", (int)status);
    ZrCore_State_ResetThread(state, status);
    return ZR_FALSE;
}

static SZrClosure *zr_cli_repl_session_resolve_closure(
        ZrCliReplSession *session,
        const SZrGcRootHandle *root) {
    SZrRawObject *rawClosure = ZR_NULL;

    if (session == ZR_NULL || session->state == ZR_NULL || root == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(session->state, root, &rawClosure) ||
        rawClosure == ZR_NULL || rawClosure->type != ZR_RAW_OBJECT_TYPE_CLOSURE ||
        rawClosure->isNative) {
        return ZR_NULL;
    }

    return (SZrClosure *)rawClosure;
}

static SZrString *zr_cli_repl_session_resolve_source_name(ZrCliReplSession *session) {
    SZrRawObject *rawSourceName = ZR_NULL;

    if (session == ZR_NULL || session->state == ZR_NULL ||
        !ZrCore_GcRootHandle_Resolve(
                session->state, &session->sourceNameRoot, &rawSourceName) ||
        rawSourceName == ZR_NULL || rawSourceName->type != ZR_RAW_OBJECT_TYPE_STRING) {
        return ZR_NULL;
    }

    session->sourceName = (SZrString *)rawSourceName;
    return session->sourceName;
}

static void zr_cli_repl_session_free_result_storage(
        ZrCliReplSession *session) {
    SZrParserSubmissionResult result;

    if (session == ZR_NULL || session->state == ZR_NULL) {
        return;
    }

    memset(&result, 0, sizeof(result));
    result.bindings = session->bindings;
    result.bindingCount = session->bindingCount;
    result.callableSignatures = session->callableSignatures;
    result.callableSignatureCount = session->callableSignatureCount;
    ZrParser_SubmissionResult_Free(session->state, &result);

    session->bindings = ZR_NULL;
    session->bindingCount = 0u;
    session->callableSignatures = ZR_NULL;
    session->callableSignatureCount = 0u;
}

static void zr_cli_repl_session_free_result_containers(
        SZrState *state,
        SZrParserSubmissionResult *result) {
    if (state == ZR_NULL || state->global == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (result->bindings != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                result->bindings,
                sizeof(*result->bindings) * result->bindingCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (result->callableSignatures != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                state->global,
                result->callableSignatures,
                sizeof(*result->callableSignatures) * result->callableSignatureCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    memset(result, 0, sizeof(*result));
}

static TZrBool zr_cli_repl_session_make_empty_closure(ZrCliReplSession *session) {
    SZrClosure *closure;

    if (session == ZR_NULL || session->state == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = ZrCore_Closure_New(session->state, 0u);
    if (closure == ZR_NULL ||
        !ZrCore_GcRootHandle_Create(
                session->state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(closure),
                &session->environmentRoot)) {
        return ZR_FALSE;
    }

    session->activeClosure = closure;
    return ZR_TRUE;
}

static TZrBool zr_cli_repl_session_context(
        const ZrCliReplSession *session,
        SZrParserSubmissionContext *outContext) {
    if (session == ZR_NULL || outContext == ZR_NULL ||
        session->moduleGeneration == 0u || session->environmentGeneration == 0u ||
        session->nextCellGeneration == 0u ||
        (session->bindingCount > 0u && session->bindings == ZR_NULL)) {
        return ZR_FALSE;
    }

    memset(outContext, 0, sizeof(*outContext));
    outContext->bindings = session->bindings;
    outContext->bindingCount = session->bindingCount;
    outContext->callableSignatures = session->callableSignatures;
    outContext->callableSignatureCount = session->callableSignatureCount;
    outContext->moduleGeneration = session->moduleGeneration;
    outContext->environmentGeneration = session->environmentGeneration;
    outContext->cellGeneration = session->nextCellGeneration;
    return ZR_TRUE;
}

static TZrBool zr_cli_repl_session_copy_prior_captures(
        ZrCliReplSession *session,
        SZrClosure *successor) {
    SZrClosure *prior;

    if (session == ZR_NULL || successor == ZR_NULL) {
        return ZR_FALSE;
    }

    prior = zr_cli_repl_session_resolve_closure(session, &session->environmentRoot);
    if (prior == ZR_NULL || prior->closureValueCount != session->bindingCount ||
        successor->closureValueCount < session->bindingCount) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < session->bindingCount; index++) {
        SZrClosureValue *sourceValue = prior->closureValuesExtend[index];
        SZrClosureValue *destinationValue = successor->closureValuesExtend[index];

        if (sourceValue == ZR_NULL || destinationValue == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Value_Copy(
                session->state,
                ZrCore_ClosureValue_GetValue(destinationValue),
                ZrCore_ClosureValue_GetValue(sourceValue));
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_repl_session_execute_rooted(
        ZrCliReplSession *session,
        const SZrGcRootHandle *closureRoot,
        SZrTypeValue *result) {
    ZrCliReplExecuteRequest request;
    SZrClosure *closure;
    SZrFunction *function;
    TZrStackValuePointer callBase;
    SZrTypeValue *closureValue;
    EZrThreadStatus status;

    if (session == ZR_NULL || session->state == ZR_NULL || closureRoot == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    closure = zr_cli_repl_session_resolve_closure(session, closureRoot);
    if (closure == ZR_NULL || closure->function == ZR_NULL) {
        return ZR_FALSE;
    }
    function = closure->function;
    callBase = session->state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndGc(session->state, function->stackSize + 1u, callBase);

    closure = zr_cli_repl_session_resolve_closure(session, closureRoot);
    if (closure == ZR_NULL || closure->function == ZR_NULL) {
        return ZR_FALSE;
    }

    closureValue = ZrCore_Stack_GetValue(callBase);
    ZrCore_Value_InitAsRawObject(
            session->state, closureValue, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    closureValue->type = ZR_VALUE_TYPE_CLOSURE;
    closureValue->isGarbageCollectable = ZR_TRUE;
    closureValue->isNative = ZR_FALSE;
    session->state->stackTop.valuePointer = callBase + 1;

    memset(&request, 0, sizeof(request));
    request.callBase = callBase;
    ZrCore_Value_ResetAsNull(result);
    session->state->threadStatus = ZR_THREAD_STATUS_FINE;
    status = ZrCore_Exception_TryRun(
            session->state, zr_cli_repl_session_execute_body, &request);
    if (status != ZR_THREAD_STATUS_FINE) {
        return zr_cli_repl_session_handle_failure(session->state, status);
    }
    if (session->state->threadStatus != ZR_THREAD_STATUS_FINE || request.resultBase == ZR_NULL) {
        return session->state->threadStatus == ZR_THREAD_STATUS_FINE ? ZR_FALSE :
               zr_cli_repl_session_handle_failure(session->state, session->state->threadStatus);
    }

    ZrCore_Value_Copy(session->state, result, ZrCore_Stack_GetValue(request.resultBase));
    return ZR_TRUE;
}

static TZrBool zr_cli_repl_session_refresh_runtime_facts(
        ZrCliReplSession *session,
        SZrParserSubmissionBinding *bindings,
        TZrSize bindingCount) {
    SZrClosure *closure;

    if (session == ZR_NULL || bindings == ZR_NULL || bindingCount == 0u) {
        return (TZrBool)(session != ZR_NULL && bindingCount == 0u);
    }

    closure = zr_cli_repl_session_resolve_closure(session, &session->environmentRoot);
    if (closure == ZR_NULL || closure->closureValueCount != bindingCount) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < bindingCount; index++) {
        SZrParserSubmissionBinding *binding = &bindings[index];
        SZrClosureValue *capture;
        SZrTypeValue *value;
        TZrInt64 exactInteger;

        if (binding->kind != ZR_PARSER_SUBMISSION_BINDING_KIND_VALUE ||
            binding->captureIndex >= closure->closureValueCount) {
            continue;
        }
        capture = closure->closureValuesExtend[binding->captureIndex];
        if (capture == ZR_NULL) {
            return ZR_FALSE;
        }
        value = ZrCore_ClosureValue_GetValue(capture);
        if (value == ZR_NULL) {
            return ZR_FALSE;
        }

        if (ZR_VALUE_IS_TYPE_INT(binding->inferredType.baseType) &&
            ZR_VALUE_IS_TYPE_INT(value->type)) {
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
                if (value->value.nativeObject.nativeUInt64 > (TZrUInt64)ZR_INT_MAX) {
                    binding->inferredType.hasRangeConstraint = ZR_FALSE;
                    ZrParser_InferredType_ResetRangeSegments(&binding->inferredType);
                    continue;
                }
                exactInteger = (TZrInt64)value->value.nativeObject.nativeUInt64;
            } else {
                exactInteger = value->value.nativeObject.nativeInt64;
            }
            binding->inferredType.minValue = exactInteger;
            binding->inferredType.maxValue = exactInteger;
            binding->inferredType.hasRangeConstraint = ZR_TRUE;
            ZrParser_InferredType_ResetRangeSegments(&binding->inferredType);
        } else if (ZR_VALUE_IS_TYPE_BOOL(binding->inferredType.baseType) &&
                   ZR_VALUE_IS_TYPE_BOOL(value->type)) {
            binding->inferredType.knownBoolValue =
                    (TZrBool)(value->value.nativeObject.nativeBool != 0u);
            binding->inferredType.hasKnownBoolValue = ZR_TRUE;
        }
    }

    return ZR_TRUE;
}

static TZrBool zr_cli_repl_session_publish_result(
        ZrCliReplSession *session,
        SZrParserSubmissionResult *result) {
    TZrSize totalBindingCount;
    TZrSize totalSignatureCount;
    SZrParserSubmissionBinding *newBindings = ZR_NULL;
    SZrParserSubmissionCallableSignature *newSignatures = ZR_NULL;
    TZrUInt64 publishedEnvironmentGeneration;

    if (session == ZR_NULL || session->state == ZR_NULL || session->state->global == ZR_NULL ||
        result == ZR_NULL ||
        result->bindingCount > UINT32_MAX - session->bindingCount ||
        result->callableSignatureCount > UINT32_MAX - session->callableSignatureCount) {
        return ZR_FALSE;
    }

    totalBindingCount = session->bindingCount + result->bindingCount;
    totalSignatureCount = session->callableSignatureCount + result->callableSignatureCount;
    publishedEnvironmentGeneration = session->environmentGeneration + 1u;
    if (publishedEnvironmentGeneration == 0u) {
        return ZR_FALSE;
    }

    if (totalBindingCount > 0u) {
        newBindings = (SZrParserSubmissionBinding *)ZrCore_Memory_RawMallocWithType(
                session->state->global,
                sizeof(*newBindings) * totalBindingCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newBindings == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    if (totalSignatureCount > 0u) {
        newSignatures = (SZrParserSubmissionCallableSignature *)ZrCore_Memory_RawMallocWithType(
                session->state->global,
                sizeof(*newSignatures) * totalSignatureCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (newSignatures == ZR_NULL) {
            if (newBindings != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(
                        session->state->global,
                        newBindings,
                        sizeof(*newBindings) * totalBindingCount,
                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
            return ZR_FALSE;
        }
    }

    if (session->bindingCount > 0u) {
        memcpy(newBindings, session->bindings, sizeof(*newBindings) * session->bindingCount);
    }
    if (result->bindingCount > 0u) {
        memcpy(newBindings + session->bindingCount,
               result->bindings,
               sizeof(*newBindings) * result->bindingCount);
    }
    if (session->callableSignatureCount > 0u) {
        memcpy(newSignatures,
               session->callableSignatures,
               sizeof(*newSignatures) * session->callableSignatureCount);
    }
    if (result->callableSignatureCount > 0u) {
        memcpy(newSignatures + session->callableSignatureCount,
               result->callableSignatures,
               sizeof(*newSignatures) * result->callableSignatureCount);
    }

    for (TZrSize index = 0u; index < totalBindingCount; index++) {
        SZrParserSubmissionBinding *binding = &newBindings[index];

        if (binding->name == ZR_NULL || binding->captureIndex != index) {
            goto cleanup;
        }
        if (index >= session->bindingCount &&
            binding->kind == ZR_PARSER_SUBMISSION_BINDING_KIND_CALLABLE) {
            if (binding->callableSignatureIndex >= result->callableSignatureCount) {
                goto cleanup;
            }
            binding->callableSignatureIndex += (TZrUInt32)session->callableSignatureCount;
        }
        binding->moduleGeneration = session->moduleGeneration;
        binding->environmentGeneration = publishedEnvironmentGeneration;
        binding->cellGeneration = session->nextCellGeneration;
    }
    if (!zr_cli_repl_session_refresh_runtime_facts(
                session, newBindings, totalBindingCount)) {
        goto cleanup;
    }

    if (session->bindings != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                session->state->global,
                session->bindings,
                sizeof(*session->bindings) * session->bindingCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (session->callableSignatures != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                session->state->global,
                session->callableSignatures,
                sizeof(*session->callableSignatures) * session->callableSignatureCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    zr_cli_repl_session_free_result_containers(session->state, result);

    session->bindings = newBindings;
    session->bindingCount = totalBindingCount;
    session->callableSignatures = newSignatures;
    session->callableSignatureCount = totalSignatureCount;
    session->environmentGeneration = publishedEnvironmentGeneration;
    session->nextCellGeneration++;
    return ZR_TRUE;

cleanup:
    if (newBindings != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                session->state->global,
                newBindings,
                sizeof(*newBindings) * totalBindingCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (newSignatures != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(
                session->state->global,
                newSignatures,
                sizeof(*newSignatures) * totalSignatureCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    return ZR_FALSE;
}

static TZrChar *zr_cli_repl_session_build_type_query_source(const TZrChar *expression) {
    static const TZrChar prefix[] = "var __zr_repl_type_query = ";
    const TZrChar *begin;
    const TZrChar *end;
    TZrSize expressionLength;
    TZrSize prefixLength = sizeof(prefix) - 1u;
    TZrChar *source;

    begin = ZrCli_ReplInput_SkipSpace(expression);
    if (begin == ZR_NULL || *begin == '\0') {
        return ZR_NULL;
    }
    end = begin + strlen(begin);
    while (end > begin && ZrCli_ReplInput_IsSpace(end[-1])) {
        --end;
    }
    if (end > begin && end[-1] == ';') {
        --end;
    }
    if (end == begin) {
        return ZR_NULL;
    }

    expressionLength = (TZrSize)(end - begin);
    source = (TZrChar *)malloc(prefixLength + expressionLength + 2u);
    if (source == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(source, prefix, prefixLength);
    memcpy(source + prefixLength, begin, expressionLength);
    source[prefixLength + expressionLength] = ';';
    source[prefixLength + expressionLength + 1u] = '\0';
    return source;
}

static SZrAstNode *zr_cli_repl_session_type_query_expression(SZrAstNode *ast) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL || ast->data.script.statements->count == 0u) {
        return ZR_NULL;
    }

    for (TZrSize index = ast->data.script.statements->count; index > 0u; index--) {
        SZrAstNode *statement = ast->data.script.statements->nodes[index - 1u];
        if (statement != ZR_NULL && statement->type == ZR_AST_VARIABLE_DECLARATION) {
            return statement->data.variableDeclaration.value;
        }
    }
    return ZR_NULL;
}

static TZrBool zr_cli_repl_session_has_binding(
        const ZrCliReplSession *session,
        SZrString *name) {
    if (session == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < session->bindingCount; index++) {
        if (session->bindings[index].name != ZR_NULL &&
            ZrCore_String_Equal(session->bindings[index].name, name)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

int ZrCli_ReplSession_Init(ZrCliReplSession *session) {
    if (session == ZR_NULL) {
        return 1;
    }

    memset(session, 0, sizeof(*session));
    session->global = ZrCli_Project_CreateBareGlobal();
    if (session->global == ZR_NULL) {
        ZrCore_Log_Error(ZR_NULL, "failed to initialize REPL runtime\n");
        return 1;
    }
    session->state = session->global->mainThreadState;
    if (!ZrCli_Project_RegisterStandardModules(session->global) ||
        !ZrCli_Runtime_InjectProcessArguments(session->state, "<repl>", ZR_NULL, 0)) {
        ZrCli_ReplSession_Free(session);
        return 1;
    }

    session->moduleGeneration = 1u;
    session->environmentGeneration = 1u;
    session->nextCellGeneration = 1u;
    session->sourceName = ZrCore_String_CreateFromNative(session->state, "<repl>");
    if (session->sourceName == ZR_NULL ||
        !ZrCore_GcRootHandle_Create(
                session->state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(session->sourceName),
                &session->sourceNameRoot) ||
        !zr_cli_repl_session_make_empty_closure(session)) {
        ZrCli_ReplSession_Free(session);
        return 1;
    }
    return 0;
}

void ZrCli_ReplSession_Free(ZrCliReplSession *session) {
    if (session == ZR_NULL) {
        return;
    }

    if (session->state != ZR_NULL) {
        ZrCore_GcRootHandle_Release(session->state, &session->environmentRoot);
        ZrCore_GcRootHandle_Release(session->state, &session->sourceNameRoot);
        zr_cli_repl_session_free_result_storage(session);
    }
    if (session->global != ZR_NULL) {
        ZrLibrary_NativeRegistry_Free(session->global);
        ZrCore_GlobalState_Free(session->global);
    }
    memset(session, 0, sizeof(*session));
}

int ZrCli_ReplSession_Submit(ZrCliReplSession *session, const TZrChar *code) {
    SZrParserSubmissionContext context;
    SZrParserSubmissionResult result;
    SZrFunction *function;
    SZrClosure *successor = ZR_NULL;
    SZrGcRootHandle functionRoot;
    SZrGcRootHandle successorRoot;
    SZrGcRootHandle priorRoot;
    SZrTypeValue value;
    SZrGcRootHandle valueRoot;
    SZrString *sourceName;
    SZrString *valueText;
    TZrBool success = ZR_FALSE;

    if (session == ZR_NULL || session->state == ZR_NULL || code == ZR_NULL || code[0] == '\0' ||
        !zr_cli_repl_session_context(session, &context)) {
        return 1;
    }

    memset(&result, 0, sizeof(result));
    memset(&functionRoot, 0, sizeof(functionRoot));
    memset(&successorRoot, 0, sizeof(successorRoot));
    memset(&priorRoot, 0, sizeof(priorRoot));
    memset(&valueRoot, 0, sizeof(valueRoot));
    sourceName = zr_cli_repl_session_resolve_source_name(session);
    if (sourceName == ZR_NULL) {
        goto cleanup;
    }
    function = ZrParser_Source_CompileSubmission(
            session->state, code, strlen(code), sourceName, &context, &result);
    if (function == ZR_NULL ||
        !ZrCore_GcRootHandle_Create(
                session->state, ZR_CAST_RAW_OBJECT_AS_SUPER(function), &functionRoot)) {
        goto cleanup;
    }

    successor = ZrCore_Closure_New(session->state, function->closureValueLength);
    if (successor == ZR_NULL ||
        !ZrCore_GcRootHandle_Create(
                session->state, ZR_CAST_RAW_OBJECT_AS_SUPER(successor), &successorRoot)) {
        goto cleanup;
    }
    successor = zr_cli_repl_session_resolve_closure(session, &successorRoot);
    if (successor == ZR_NULL) {
        goto cleanup;
    }
    successor->function = function;
    ZrCore_RawObject_Barrier(
            session->state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(successor),
            ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    ZrCore_Closure_InitValue(session->state, successor);
    if (function->closureValueLength != session->bindingCount + result.bindingCount ||
        !zr_cli_repl_session_copy_prior_captures(session, successor)) {
        goto cleanup;
    }
    ZrCore_GcRootHandle_Release(session->state, &functionRoot);

    if (!zr_cli_repl_session_execute_rooted(session, &successorRoot, &value)) {
        goto cleanup;
    }
    if (value.isGarbageCollectable && value.value.object != ZR_NULL &&
        !ZrCore_GcRootHandle_Create(session->state, value.value.object, &valueRoot)) {
        goto cleanup;
    }
    if (!ZrCore_GcRootHandle_Clone(session->state, &session->environmentRoot, &priorRoot)) {
        goto cleanup;
    }
    successor = zr_cli_repl_session_resolve_closure(session, &successorRoot);
    if (successor == ZR_NULL ||
        !ZrCore_GcRootHandle_Update(
                session->state,
                &session->environmentRoot,
                ZR_CAST_RAW_OBJECT_AS_SUPER(successor))) {
        goto cleanup;
    }
    if (!zr_cli_repl_session_publish_result(session, &result)) {
        SZrClosure *prior = zr_cli_repl_session_resolve_closure(session, &priorRoot);
        if (prior != ZR_NULL) {
            (void)ZrCore_GcRootHandle_Update(
                    session->state,
                    &session->environmentRoot,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(prior));
        }
        goto cleanup;
    }
    session->activeClosure = zr_cli_repl_session_resolve_closure(session, &session->environmentRoot);
    if (session->activeClosure == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_GcRootHandle_Release(session->state, &successorRoot);
    ZrCore_GcRootHandle_Release(session->state, &priorRoot);
    valueText = ZrCore_Value_ConvertToString(session->state, &value);
    if (valueText == ZR_NULL) {
        ZrCore_Log_Error(session->state, "failed to stringify REPL result\n");
    } else {
        ZrCore_Log_Resultf(session->state, "%s\n", ZrCore_String_GetNativeString(valueText));
    }
    success = ZR_TRUE;

cleanup:
    ZrCore_GcRootHandle_Release(session->state, &valueRoot);
    ZrCore_GcRootHandle_Release(session->state, &functionRoot);
    ZrCore_GcRootHandle_Release(session->state, &successorRoot);
    ZrCore_GcRootHandle_Release(session->state, &priorRoot);
    if (!success) {
        ZrParser_SubmissionResult_Free(session->state, &result);
    }
    return success ? 0 : 1;
}

int ZrCli_ReplSession_TypeQuery(ZrCliReplSession *session, const TZrChar *expression) {
    SZrParserSubmissionContext context;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    TZrChar *source = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrAstNode *expr;
    SZrInferredType inferredType;
    SZrString *sourceName;
    TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *typeText;
    TZrBool compilerStateInitialized = ZR_FALSE;
    TZrBool inferredTypeInitialized = ZR_FALSE;
    int exitCode = 1;

    if (session == ZR_NULL || session->state == ZR_NULL || expression == ZR_NULL ||
        *ZrCli_ReplInput_SkipSpace(expression) == '\0' ||
        !zr_cli_repl_session_context(session, &context)) {
        ZrCore_Log_Error(session != ZR_NULL ? session->state : ZR_NULL, "usage: :type <expression>\n");
        return 1;
    }

    source = zr_cli_repl_session_build_type_query_source(expression);
    if (source == ZR_NULL) {
        return 1;
    }
    sourceName = zr_cli_repl_session_resolve_source_name(session);
    if (sourceName == ZR_NULL) {
        goto cleanup;
    }
    ZrParser_State_Init(
            &parserState, session->state, source, strlen(source), sourceName);
    ast = ZrParser_ParseWithState(&parserState);
    if (parserState.hasError || ast == ZR_NULL) {
        ZrParser_State_Free(&parserState);
        goto cleanup;
    }
    ZrParser_State_Free(&parserState);

    expr = zr_cli_repl_session_type_query_expression(ast);
    if (expr == ZR_NULL) {
        ZrCore_Log_Error(session->state, ":type expects an expression, not a statement or declaration\n");
        goto cleanup;
    }
    if (expr->type == ZR_AST_IDENTIFIER_LITERAL &&
        !zr_cli_repl_session_has_binding(session, expr->data.identifier.name)) {
        ZrCore_Log_Error(session->state, "REPL binding is unavailable in the active generation\n");
        goto cleanup;
    }

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, session->state);
    compilerStateInitialized = ZR_TRUE;
    compilerState.currentAst = ast;
    compilerState.scriptAst = ast;
    compilerState.suppressErrorOutput = ZR_FALSE;
    if (!ZrParser_CompilerState_SeedSubmissionContext(&compilerState, &context)) {
        ZrCore_Log_Error(session->state, "REPL binding is unavailable in the active generation\n");
        goto cleanup;
    }

    ZrParser_InferredType_Init(session->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    inferredTypeInitialized = ZR_TRUE;
    if (!ZrParser_ExpressionType_Infer(&compilerState, expr, &inferredType)) {
        ZrCore_Log_Error(session->state, "failed to infer expression type\n");
        goto cleanup;
    }

    typeText = ZrParser_TypeNameString_Get(
            session->state, &inferredType, typeBuffer, sizeof(typeBuffer));
    ZrCore_Log_Resultf(session->state, "Type: %s\n", typeText != ZR_NULL ? typeText : "unknown");
    ZrCli_ReplSemanticFacts_WriteNumericForExpression(session->state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteLogicalForExpression(session->state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteOwnershipForExpression(session->state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteExpressionForExpression(session->state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteReferencesForExpression(session->state, compilerState.semanticContext, expr);
    ZrCli_ReplSemanticFacts_WriteReachabilityForExpression(session->state, compilerState.semanticContext, expr);
    exitCode = 0;

cleanup:
    if (inferredTypeInitialized) {
        ZrParser_InferredType_Free(session->state, &inferredType);
    }
    if (compilerStateInitialized) {
        ZrParser_CompilerState_Free(&compilerState);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(session->state, ast);
    }
    free(source);
    return exitCode;
}

int ZrCli_ReplSession_Reset(ZrCliReplSession *session) {
    if (session == ZR_NULL || session->state == ZR_NULL) {
        return 1;
    }

    ZrCore_GcRootHandle_Release(session->state, &session->environmentRoot);
    session->activeClosure = ZR_NULL;
    zr_cli_repl_session_free_result_storage(session);
    session->moduleGeneration++;
    session->environmentGeneration++;
    session->nextCellGeneration++;
    if (session->moduleGeneration == 0u || session->environmentGeneration == 0u ||
        session->nextCellGeneration == 0u ||
        !zr_cli_repl_session_make_empty_closure(session)) {
        return 1;
    }
    return 0;
}
