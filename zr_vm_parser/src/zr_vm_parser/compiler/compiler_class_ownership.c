#include "compiler_internal.h"
#include "compile_expression_internal.h"

static TZrBool compiler_class_ownership_is_field(const SZrTypeMemberInfo *memberInfo) {
    return memberInfo != ZR_NULL &&
           (memberInfo->memberType == ZR_AST_STRUCT_FIELD ||
            memberInfo->memberType == ZR_AST_CLASS_FIELD);
}

static TZrBool compiler_class_ownership_has_shared_field_to(
        SZrTypePrototypeInfo *info,
        SZrString *targetTypeName) {
    if (info == ZR_NULL || targetTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0u; index < info->members.length; index++) {
        SZrTypeMemberInfo *memberInfo =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        if (compiler_class_ownership_is_field(memberInfo) &&
            !memberInfo->isStatic &&
            memberInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_SHARED &&
            memberInfo->fieldTypeName != ZR_NULL &&
            ZrCore_String_Equal(memberInfo->fieldTypeName, targetTypeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void compiler_class_ownership_publish_cycle_warning(
        SZrCompilerState *cs,
        const SZrTypeMemberInfo *memberInfo) {
    SZrStructuredDiagnostic diagnostic;
    SZrSemanticDiagnosticFact fact;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || memberInfo == ZR_NULL ||
        memberInfo->declarationNode == ZR_NULL) {
        return;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_DiagnosticBuilder_Build(
                cs->state,
                &diagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_WARNING,
                memberInfo->declarationNode->location,
                "resource_shared_strong_cycle",
                "Shared resource fields form a strong ownership cycle",
                "The process-local resource field graph contains a Shared edge back to this resource type.",
                "Use Weak for the back-reference, or use a GC class when arbitrary strong cycles are required.")) {
        return;
    }
    if (!ZrParser_StructuredDiagnostic_SetNoFixReason(
                &diagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
        return;
    }

    memset(&fact, 0, sizeof(fact));
    fact.node = memberInfo->declarationNode;
    fact.diagnostic = diagnostic;
    (void)ZrParser_SemanticFacts_AppendDiagnostic(cs->semanticContext, &fact);
    ZrParser_StructuredDiagnostic_Free(cs->state, &diagnostic);
}

void compiler_class_lint_process_local_shared_cycles(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *info) {
    if (cs == ZR_NULL || info == ZR_NULL ||
        (info->modifierFlags & ZR_DECLARATION_MODIFIER_RESOURCE) == 0u ||
        info->isImportedNative) {
        return;
    }

    for (TZrSize index = 0u; index < info->members.length; index++) {
        SZrTypeMemberInfo *memberInfo =
                (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        SZrTypePrototypeInfo *targetInfo;

        if (!compiler_class_ownership_is_field(memberInfo) || memberInfo->isStatic ||
            memberInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_SHARED ||
            memberInfo->fieldTypeName == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(memberInfo->fieldTypeName, info->name)) {
            compiler_class_ownership_publish_cycle_warning(cs, memberInfo);
            continue;
        }

        targetInfo = find_compiler_type_prototype(cs, memberInfo->fieldTypeName);
        if (targetInfo != ZR_NULL && !targetInfo->isImportedNative &&
            (targetInfo->modifierFlags & ZR_DECLARATION_MODIFIER_RESOURCE) != 0u &&
            compiler_class_ownership_has_shared_field_to(targetInfo, info->name)) {
            compiler_class_ownership_publish_cycle_warning(cs, memberInfo);
        }
    }
}
