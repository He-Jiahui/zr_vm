#ifndef ZR_VM_PARSER_TYPE_ENVIRONMENT_CONTEXT_LOOKUP_H
#define ZR_VM_PARSER_TYPE_ENVIRONMENT_CONTEXT_LOOKUP_H

#include "zr_vm_parser/type_system.h"

static inline TZrBool type_environment_context_equal(
        SZrSyntaxContext left, SZrSyntaxContext right,
        EZrSyntaxNamespace namespaceKind, TZrBool *outEqual) {
    SZrSyntaxContext normalizedLeft;
    SZrSyntaxContext normalizedRight;

    if (outEqual == ZR_NULL) {
        return ZR_FALSE;
    }
    *outEqual = ZR_FALSE;
    if (!ZrParser_SyntaxContext_IsValid(left) || !ZrParser_SyntaxContext_IsValid(right)) {
        return ZR_FALSE;
    }
    if (ZrParser_SyntaxContext_Equal(left, right)) {
        *outEqual = ZR_TRUE;
        return ZR_TRUE;
    }
    normalizedLeft = ZrParser_SyntaxContext_Normalize(left, namespaceKind);
    if (!ZrParser_SyntaxContext_IsValid(normalizedLeft)) {
        return ZR_FALSE;
    }
    normalizedRight = ZrParser_SyntaxContext_Normalize(right, namespaceKind);
    if (!ZrParser_SyntaxContext_IsValid(normalizedRight)) {
        return ZR_FALSE;
    }
    *outEqual = ZrParser_SyntaxContext_Equal(normalizedLeft, normalizedRight);
    return ZR_TRUE;
}

TZrBool type_environment_find_current_variable_binding(
        const SZrTypeEnvironment *env, const SZrString *name,
        SZrSyntaxContext syntaxContext, const SZrTypeBinding **outBinding);
TZrBool type_environment_find_variable_binding(
        const SZrTypeEnvironment *env, const SZrString *name,
        SZrSyntaxContext syntaxContext, const SZrTypeBinding **outBinding);

#endif
