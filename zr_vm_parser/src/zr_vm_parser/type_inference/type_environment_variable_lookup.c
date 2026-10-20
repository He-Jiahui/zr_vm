#include "type_inference/type_environment_context_lookup.h"

#include "zr_vm_core/string.h"

TZrBool type_environment_find_current_variable_binding(
        const SZrTypeEnvironment *env, const SZrString *name,
        SZrSyntaxContext syntaxContext, const SZrTypeBinding **outBinding) {
    if (outBinding == ZR_NULL) {
        return ZR_FALSE;
    }
    *outBinding = ZR_NULL;
    if (env == ZR_NULL || name == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < env->variableTypes.length; index++) {
        const SZrTypeBinding *binding = ZrCore_Array_Get(
                (SZrArray *)&env->variableTypes, index);
        TZrBool equal;

        if (binding == ZR_NULL || binding->name == ZR_NULL ||
            !ZrCore_String_Equal(binding->name, (SZrString *)name)) {
            continue;
        }
        if (!type_environment_context_equal(binding->syntaxContext, syntaxContext,
                    ZR_SYNTAX_NAMESPACE_LOCAL, &equal)) {
            return ZR_FALSE;
        }
        if (equal) {
            *outBinding = binding;
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

TZrBool type_environment_find_variable_binding(
        const SZrTypeEnvironment *env, const SZrString *name,
        SZrSyntaxContext syntaxContext, const SZrTypeBinding **outBinding) {
    if (outBinding == ZR_NULL) {
        return ZR_FALSE;
    }
    *outBinding = ZR_NULL;
    if (env == ZR_NULL || name == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }
    for (const SZrTypeEnvironment *current = env; current != ZR_NULL;
         current = current->parent) {
        if (!type_environment_find_current_variable_binding(
                    current, name, syntaxContext, outBinding)) {
            return ZR_FALSE;
        }
        if (*outBinding != ZR_NULL) {
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

const SZrTypeBinding *ZrParser_TypeEnvironment_FindVariableBinding(
        SZrTypeEnvironment *env, SZrString *name) {
    return ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            env, name, ZrParser_SyntaxContext_Root());
}

const SZrTypeBinding *ZrParser_TypeEnvironment_FindVariableBindingWithContext(
        SZrTypeEnvironment *env, SZrString *name, SZrSyntaxContext syntaxContext) {
    const SZrTypeBinding *binding;

    if (!type_environment_find_variable_binding(env, name, syntaxContext, &binding)) {
        return ZR_NULL;
    }
    return binding;
}

TZrBool ZrParser_TypeEnvironment_LookupVariable(
        SZrState *state, SZrTypeEnvironment *env, SZrString *name, SZrInferredType *result) {
    return ZrParser_TypeEnvironment_LookupVariableWithContext(
            state, env, name, ZrParser_SyntaxContext_Root(), result);
}

TZrBool ZrParser_TypeEnvironment_LookupVariableWithContext(
        SZrState *state, SZrTypeEnvironment *env, SZrString *name,
        SZrSyntaxContext syntaxContext, SZrInferredType *result) {
    const SZrTypeBinding *binding;

    if (state == ZR_NULL || result == ZR_NULL ||
        !type_environment_find_variable_binding(env, name, syntaxContext, &binding) ||
        binding == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Copy(state, result, &binding->type);
    return ZR_TRUE;
}
