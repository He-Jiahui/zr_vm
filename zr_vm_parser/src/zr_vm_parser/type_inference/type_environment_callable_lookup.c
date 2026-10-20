#include "type_inference/type_environment_callable_internal.h"
#include "type_inference/type_environment_context_lookup.h"

#include "zr_vm_core/string.h"

TZrBool type_environment_callable_context_equal(
        SZrSyntaxContext left,
        SZrSyntaxContext right,
        TZrBool isCallableValueBinding,
        TZrBool *outEqual) {
    EZrSyntaxNamespace namespaceKind = isCallableValueBinding
                                              ? ZR_SYNTAX_NAMESPACE_LOCAL
                                              : ZR_SYNTAX_NAMESPACE_ITEM;

    return type_environment_context_equal(left, right, namespaceKind, outEqual);
}

TZrBool type_environment_callable_matches_signature(
        const SZrFunctionTypeInfo *functionInfo,
        const SZrInferredType *returnType,
        const SZrArray *parameterTypes) {
    if (functionInfo == ZR_NULL || returnType == ZR_NULL ||
        !ZrParser_InferredType_Equal(&functionInfo->returnType, returnType)) {
        return ZR_FALSE;
    }
    if (parameterTypes == ZR_NULL) {
        return functionInfo->paramTypes.length == 0U;
    }
    if (functionInfo->paramTypes.length != parameterTypes->length) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < parameterTypes->length; index++) {
        const SZrInferredType *existing = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)&functionInfo->paramTypes, index);
        const SZrInferredType *candidate = (const SZrInferredType *)ZrCore_Array_Get(
                (SZrArray *)parameterTypes, index);

        if (existing == ZR_NULL || candidate == ZR_NULL) {
            if (existing != candidate) {
                return ZR_FALSE;
            }
        } else if (!ZrParser_InferredType_Equal(existing, candidate)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool callable_lookup_has_local_value(
        const SZrTypeEnvironment *env,
        SZrString *name,
        SZrSyntaxContext context,
        TZrBool *outHasLocalValue) {
    const SZrTypeBinding *binding;

    *outHasLocalValue = ZR_FALSE;
    if (!type_environment_find_current_variable_binding(env, name, context, &binding)) {
        return ZR_FALSE;
    }
    *outHasLocalValue = binding != ZR_NULL;
    return ZR_TRUE;
}

static TZrBool callable_lookup_matches(
        const SZrFunctionTypeInfo *candidate,
        SZrString *name,
        SZrSyntaxContext context,
        TZrBool hasLocalValue,
        TZrBool *outMatches) {
    *outMatches = ZR_FALSE;
    if (candidate == ZR_NULL || candidate->name == ZR_NULL ||
        !ZrCore_String_Equal(candidate->name, name) ||
        (hasLocalValue && !candidate->isCallableValueBinding)) {
        return ZR_TRUE;
    }
    return type_environment_callable_context_equal(
            candidate->syntaxContext, context, candidate->isCallableValueBinding, outMatches);
}

static TZrBool callable_lookup_contains_overload(
        const SZrArray *results,
        const SZrFunctionTypeInfo *candidate,
        TZrBool *outContains) {
    *outContains = ZR_FALSE;
    for (TZrSize index = 0U; index < results->length; index++) {
        const SZrFunctionTypeInfo *const *entry =
                (const SZrFunctionTypeInfo *const *)ZrCore_Array_Get(
                        (SZrArray *)results, index);
        const SZrFunctionTypeInfo *existing = entry != ZR_NULL ? *entry : ZR_NULL;
        TZrBool equal;

        if (existing == candidate) {
            *outContains = ZR_TRUE;
            return ZR_TRUE;
        }
        if (existing != ZR_NULL && existing->name != ZR_NULL &&
            ZrCore_String_Equal(existing->name, candidate->name) &&
            existing->isCallableValueBinding == candidate->isCallableValueBinding) {
            if (!type_environment_callable_context_equal(
                    existing->syntaxContext, candidate->syntaxContext,
                    candidate->isCallableValueBinding, &equal)) {
                return ZR_FALSE;
            }
            if (equal && existing->genericParameters.length == candidate->genericParameters.length &&
                type_environment_callable_matches_signature(
                    existing, &candidate->returnType, &candidate->paramTypes)) {
                *outContains = ZR_TRUE;
                return ZR_TRUE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TypeEnvironment_LookupFunctionWithContext(
        SZrTypeEnvironment *env,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        SZrFunctionTypeInfo **result) {
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    *result = ZR_NULL;
    if (env == ZR_NULL || name == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }
    for (SZrTypeEnvironment *current = env; current != ZR_NULL;
         current = current->parent) {
        TZrBool hasLocalValue;

        if (!callable_lookup_has_local_value(current, name, syntaxContext, &hasLocalValue)) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0U; index < current->functionReturnTypes.length; index++) {
            SZrFunctionTypeInfo **entry = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
                    &current->functionReturnTypes, index);
            TZrBool matches;

            if (entry == ZR_NULL) {
                continue;
            }
            if (!callable_lookup_matches(*entry, name, syntaxContext, hasLocalValue, &matches)) {
                return ZR_FALSE;
            }
            if (matches) {
                *result = *entry;
                return ZR_TRUE;
            }
        }
        if (hasLocalValue) {
            return ZR_FALSE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeEnvironment_LookupFunction(
        SZrTypeEnvironment *env,
        SZrString *name,
        SZrFunctionTypeInfo **result) {
    return ZrParser_TypeEnvironment_LookupFunctionWithContext(
            env, name, ZrParser_SyntaxContext_Root(), result);
}

TZrBool ZrParser_TypeEnvironment_LookupFunctionsWithContext(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *name,
        SZrSyntaxContext syntaxContext,
        SZrArray *results) {
    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || results == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(state, results, sizeof(SZrFunctionTypeInfo *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    for (SZrTypeEnvironment *current = env; current != ZR_NULL;
         current = current->parent) {
        TZrBool hasLocalValue;

        if (!callable_lookup_has_local_value(current, name, syntaxContext, &hasLocalValue)) {
            goto failed;
        }

        for (TZrSize index = 0U; index < current->functionReturnTypes.length; index++) {
            SZrFunctionTypeInfo **entry = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
                    &current->functionReturnTypes, index);
            TZrBool matches;
            TZrBool contains;

            if (entry == ZR_NULL) {
                continue;
            }
            if (!callable_lookup_matches(*entry, name, syntaxContext, hasLocalValue, &matches)) {
                goto failed;
            }
            if (!matches) {
                continue;
            }
            if (!callable_lookup_contains_overload(results, *entry, &contains)) {
                goto failed;
            }
            if (!contains) {
                ZrCore_Array_Push(state, results, entry);
            }
        }
        if (hasLocalValue) {
            break;
        }
    }
    if (results->length > 0U) {
        return ZR_TRUE;
    }
failed:
    ZrCore_Array_Free(state, results);
    ZrCore_Array_Construct(results);
    return ZR_FALSE;
}

TZrBool ZrParser_TypeEnvironment_LookupFunctions(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *name,
        SZrArray *results) {
    return ZrParser_TypeEnvironment_LookupFunctionsWithContext(
            state, env, name, ZrParser_SyntaxContext_Root(), results);
}
