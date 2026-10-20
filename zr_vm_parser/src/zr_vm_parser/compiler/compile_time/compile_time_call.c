#include "compiler/compile_time/compile_time_call.h"

#include "compiler/tool/compile_tool_execution_scope.h"
#include "compiler/compile_time/comptime_runtime_contract.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

TZrBool ct_select_supplied_call_arg(SZrFunctionCall *call,
                                    SZrParameter *param,
                                    TZrSize paramIndex,
                                    SZrAstNode **outArgument) {
    TZrSize positionalCount = 0;

    if (outArgument == ZR_NULL) {
        return ZR_FALSE;
    }
    *outArgument = ZR_NULL;
    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL &&
        param != ZR_NULL && param->name != ZR_NULL && param->name->name != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                ZrCore_String_Equal(*argNamePtr, param->name->name)) {
                *outArgument = call->args->nodes[i];
                return ZR_TRUE;
            }
        }

        if (paramIndex < positionalCount) {
            *outArgument = call->args->nodes[paramIndex];
            return ZR_TRUE;
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        *outArgument = call->args->nodes[paramIndex];
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool ct_eval_source_call_arg(SZrCompilerState *cs,
                                       SZrCompileTimeFunction *func,
                                       SZrFunctionCall *call,
                                       SZrParameter *param,
                                       TZrSize paramIndex,
                                       SZrCompileTimeFrame *callerFrame,
                                       SZrCompileTimeFrame *calleeFrame,
                                       SZrTypeValue *result) {
    SZrAstNode *argument;
    SZrCompileToolExecutionScope defaultScope;
    TZrBool success;

    if (ct_select_supplied_call_arg(call, param, paramIndex, &argument)) {
        return evaluate_compile_time_expression_internal(
                cs, argument, callerFrame, result);
    }

    if (!ZrParser_CompileToolExecutionScope_EnterFunction(
                cs, func, &defaultScope)) {
        return ZR_FALSE;
    }
    success = ct_eval_call_arg(
            cs, ZR_NULL, param, paramIndex, calleeFrame, result);
    ZrParser_CompileToolExecutionScope_Leave(cs, &defaultScope);
    return success;
}

static TZrBool ct_eval_runtime_projected_call_arg(SZrCompilerState *cs,
                                                  SZrCompileTimeFunction *func,
                                                  SZrFunctionCall *call,
                                                  SZrString *paramName,
                                                  TZrSize paramIndex,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result) {
    TZrSize positionalCount = 0;

    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && call->args != ZR_NULL && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (paramName != ZR_NULL) {
            for (TZrSize i = 0; i < call->argNames->length && call->args != ZR_NULL && i < call->args->count; i++) {
                SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
                if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                    ZrCore_String_Equal(*argNamePtr, paramName)) {
                    return evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, result);
                }
            }
        }

        if (call->args != ZR_NULL && paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (func != ZR_NULL &&
        paramIndex < func->paramHasDefaultValues.length &&
        paramIndex < func->paramDefaultValues.length) {
        TZrBool *hasDefaultPtr = (TZrBool *)ZrCore_Array_Get(&func->paramHasDefaultValues, paramIndex);
        SZrTypeValue *defaultValue = (SZrTypeValue *)ZrCore_Array_Get(&func->paramDefaultValues, paramIndex);
        if (hasDefaultPtr != ZR_NULL && *hasDefaultPtr && defaultValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
            ZrCore_Value_Copy(cs->state, result, defaultValue);
            return ZR_TRUE;
        }
    }

    ct_error_name(cs, paramName, "Missing compile-time argument for parameter: ", (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_invoke_runtime_callable_with_values(SZrCompilerState *cs,
                                                      SZrAstNode *callSite,
                                                      const SZrTypeValue *callableValue,
                                                      TZrSize argCount,
                                                      const SZrTypeValue *argValues,
                                                      SZrTypeValue *result) {
    SZrState *state;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;

    if (cs == ZR_NULL || callableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    state = cs->state;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, argCount + 1, base, base, &baseAnchor);
    state->stackTop.valuePointer = base;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), callableValue);
    state->stackTop.valuePointer = base + 1;

    for (TZrSize i = 0; i < argCount; i++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base + 1 + i));
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base + 1 + i), &argValues[i]);
        state->stackTop.valuePointer = base + 2 + i;
    }

    base = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Runtime callable failed during compile-time evaluation",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    return ZR_TRUE;
}

static TZrBool ct_call_runtime_projected_compile_time_function(SZrCompilerState *cs,
                                                               SZrAstNode *callSite,
                                                               SZrCompileTimeFunction *func,
                                                               SZrFunctionCall *call,
                                                               SZrCompileTimeFrame *frame,
                                                               SZrTypeValue *result) {
    SZrTypeValue callableValue;
    SZrTypeValue *argValues = ZR_NULL;
    TZrSize parameterCount;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterCount = func->paramNames.length;
    if (!ct_value_from_compile_time_function(cs, func, &callableValue)) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Failed to resolve runtime projection for compile-time function",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->args != ZR_NULL && call->args->count > parameterCount) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Too many arguments for compile-time function call",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (parameterCount > 0) {
        argValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                    sizeof(SZrTypeValue) * parameterCount,
                                                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (argValues == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < parameterCount; index++) {
            SZrString **paramNamePtr = (SZrString **)ZrCore_Array_Get(&func->paramNames, index);
            if (!ct_eval_runtime_projected_call_arg(cs,
                                                    func,
                                                    call,
                                                    paramNamePtr != ZR_NULL ? *paramNamePtr : ZR_NULL,
                                                    index,
                                                    frame,
                                                    &argValues[index]) ||
                (frame != ZR_NULL &&
                 !ct_frame_root_value(cs, frame, &argValues[index]))) {
                goto cleanup;
            }
        }
    }

    success = ct_invoke_runtime_callable_with_values(cs,
                                                     callSite,
                                                     &callableValue,
                                                     parameterCount,
                                                     argValues,
                                                     result);
    if (success && frame != ZR_NULL &&
        !ct_frame_root_value(cs, frame, result)) {
        success = ZR_FALSE;
    }

cleanup:
    if (argValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argValues,
                                      sizeof(SZrTypeValue) * parameterCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ct_call_function(SZrCompilerState *cs,
                              SZrAstNode *callSite,
                              SZrCompileTimeFunction *func,
                              SZrFunctionCall *call,
                              SZrCompileTimeFrame *parentFrame,
                              SZrTypeValue *result) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;
    TZrBool enteredCall = ZR_FALSE;
    TZrBool cacheable;
    SZrComptimeCacheKey cacheKey;
    TZrUInt64 diagnosticCountBefore;
    SZrCompileToolExecutionScope executionScope;
    TZrBool enteredExecutionScope = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (func->isRuntimeProjection) {
        return ct_call_runtime_projected_compile_time_function(cs, callSite, func, call, parentFrame, result);
    }

    if (func->declaration == ZR_NULL || func->declaration->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    if (!ZrParser_CompileToolExecutionScope_EnterFunction(
                cs, func, &executionScope)) {
        return ZR_FALSE;
    }
    enteredExecutionScope = ZR_TRUE;

    decl = &func->declaration->data.functionDeclaration;
    ct_frame_init(cs, &frame, ZR_NULL);
    cacheable = ZrParser_ComptimeCache_BeginKey(cs, func, &cacheKey);
    ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
    enteredExecutionScope = ZR_FALSE;

    if (decl->params != ZR_NULL) {
        for (TZrSize i = 0; i < decl->params->count; i++) {
            SZrAstNode *paramNode = decl->params->nodes[i];
            SZrParameter *param;
            SZrTypeValue argValue;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            param = &paramNode->data.parameter;
            if (!ct_eval_source_call_arg(
                        cs, func, call, param, i, parentFrame, &frame, &argValue)) {
                goto cleanup;
            }
            if (cacheable &&
                !ZrParser_ComptimeCache_MixValue(&cacheKey, &argValue)) {
                cacheable = ZR_FALSE;
            }
            if (param->name == ZR_NULL || param->name->name == ZR_NULL ||
                !ct_frame_declare_with_context(
                        cs,
                        &frame,
                        param->name->name,
                        param->name->syntaxContext,
                        &argValue,
                        param->isConst)) {
                goto cleanup;
            }
        }
    }

    if (!ZrParser_CompileToolExecutionScope_EnterFunction(
                cs, func, &executionScope)) {
        goto cleanup;
    }
    enteredExecutionScope = ZR_TRUE;

    if (call != ZR_NULL && call->args != ZR_NULL && !call->hasNamedArgs) {
        TZrSize expectedArgs = decl->params != ZR_NULL ? decl->params->count : 0;
        if (call->args->count > expectedArgs) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Too many arguments for compile-time function call", callSite->location);
            goto cleanup;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time function body is null", callSite->location);
        goto cleanup;
    }

    if (cacheable && ZrParser_ComptimeCache_Lookup(
                             cs, &cacheKey, result)) {
        success = ZR_TRUE;
        goto cleanup;
    }
    if (!ZrParser_ComptimeRuntime_EnterCall(
                cs,
                callSite != ZR_NULL ? callSite->location : func->location)) {
        goto cleanup;
    }
    enteredCall = ZR_TRUE;
    diagnosticCountBefore = cs->comptimeBudget.usage.diagnosticCount;

    success = decl->body->type == ZR_AST_BLOCK
                  ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, result)
                  : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, result);
    if (success && !didReturn) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (success && cacheable &&
        diagnosticCountBefore ==
                cs->comptimeBudget.usage.diagnosticCount) {
        ZrParser_ComptimeCache_Store(cs, &cacheKey, result);
    }

cleanup:
    if (enteredCall) {
        ZrParser_ComptimeRuntime_LeaveCall(cs);
    }
    if (success && didReturn && parentFrame != ZR_NULL && result != ZR_NULL &&
        !ct_frame_root_value(cs, parentFrame, result)) {
        success = ZR_FALSE;
    }
    ct_frame_free(cs, &frame);
    if (enteredExecutionScope) {
        ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
    }
    return success;
}

