#include "compile_statement_union_switch_validation.h"

#include "compile_expression_internal.h"
#include "type_inference_internal.h"

#include "zr_vm_core/string.h"

static TZrBool switch_case_can_use_subject_union_type(SZrAstNode *caseValue) {
    return caseValue != ZR_NULL &&
           (caseValue->type == ZR_AST_IDENTIFIER_LITERAL ||
            caseValue->type == ZR_AST_PRIMARY_EXPRESSION);
}

static TZrBool switch_union_case_covers_variant(SZrCompilerState *cs,
                                                SZrAstNode *caseValue,
                                                SZrString *switchUnionTypeName,
                                                SZrString *variantName) {
    SZrString *caseVariantName = ZR_NULL;
    SZrAstNodeArray *bindings = ZR_NULL;

    if (cs == ZR_NULL || caseValue == ZR_NULL || variantName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    if (switchUnionTypeName != ZR_NULL && switch_case_can_use_subject_union_type(caseValue)) {
        if (!try_resolve_union_variant_pattern_for_type(cs,
                                                        caseValue,
                                                        switchUnionTypeName,
                                                        &caseVariantName,
                                                        &bindings,
                                                        ZR_NULL)) {
            return ZR_FALSE;
        }
    } else if (!try_resolve_union_variant_pattern_expression(cs,
                                                              caseValue,
                                                              &caseVariantName,
                                                              &bindings,
                                                              ZR_NULL)) {
        return ZR_FALSE;
    }
    if (cs->hasError || caseVariantName == ZR_NULL) {
        return ZR_FALSE;
    }

    return ZrCore_String_Equal(caseVariantName, variantName);
}

void compile_switch_validate_union_duplicate_cases(SZrCompilerState *cs,
                                                   SZrSwitchExpression *switchExpression,
                                                   SZrAstNode *unionDeclaration,
                                                   SZrString *switchUnionTypeName) {
    SZrAstNodeArray *variants;

    if (cs == ZR_NULL || switchExpression == ZR_NULL || unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION || cs->hasError ||
        switchExpression->cases == ZR_NULL || switchExpression->cases->nodes == ZR_NULL) {
        return;
    }

    variants = unionDeclaration->data.unionDeclaration.variants;
    if (variants == ZR_NULL || variants->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize variantIndex = 0; variantIndex < variants->count; variantIndex++) {
        SZrAstNode *variantNode = variants->nodes[variantIndex];
        SZrString *variantName;
        TZrBool alreadyCovered = ZR_FALSE;

        if (variantNode == ZR_NULL ||
            variantNode->type != ZR_AST_UNION_VARIANT ||
            variantNode->data.unionVariant.name == ZR_NULL ||
            variantNode->data.unionVariant.name->name == ZR_NULL) {
            continue;
        }

        variantName = variantNode->data.unionVariant.name->name;
        for (TZrSize caseIndex = 0; caseIndex < switchExpression->cases->count; caseIndex++) {
            SZrAstNode *caseNode = switchExpression->cases->nodes[caseIndex];

            if (caseNode == ZR_NULL ||
                caseNode->type != ZR_AST_SWITCH_CASE ||
                !switch_union_case_covers_variant(cs,
                                                  caseNode->data.switchCase.value,
                                                  switchUnionTypeName,
                                                  variantName)) {
                if (cs->hasError) {
                    return;
                }
                continue;
            }

            if (alreadyCovered) {
                TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
                const TZrChar *variantText = ZrCore_String_GetNativeString(variantName);

                snprintf(message,
                         sizeof(message),
                         "Unreachable union switch case; variant '%s' is already covered",
                         variantText != ZR_NULL ? variantText : "<unknown>");
                ZrParser_Compiler_Error(cs, message, caseNode->location);
                return;
            }

            alreadyCovered = ZR_TRUE;
        }
    }
}

TZrBool compile_switch_validate_union_exhaustiveness(SZrCompilerState *cs,
                                                     SZrSwitchExpression *switchExpression,
                                                     SZrAstNode *unionDeclaration,
                                                     SZrString *switchUnionTypeName,
                                                     SZrFileRange location) {
    SZrAstNodeArray *variants;

    if (cs == ZR_NULL || switchExpression == ZR_NULL || unionDeclaration == ZR_NULL ||
        unionDeclaration->type != ZR_AST_UNION_DECLARATION || cs->hasError) {
        return ZR_FALSE;
    }

    variants = unionDeclaration->data.unionDeclaration.variants;
    if (variants == ZR_NULL || variants->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize variantIndex = 0; variantIndex < variants->count; variantIndex++) {
        SZrAstNode *variantNode = variants->nodes[variantIndex];
        SZrString *variantName;
        TZrBool covered = ZR_FALSE;

        if (variantNode == ZR_NULL ||
            variantNode->type != ZR_AST_UNION_VARIANT ||
            variantNode->data.unionVariant.name == ZR_NULL ||
            variantNode->data.unionVariant.name->name == ZR_NULL) {
            continue;
        }

        variantName = variantNode->data.unionVariant.name->name;
        if (switchExpression->cases != ZR_NULL && switchExpression->cases->nodes != ZR_NULL) {
            for (TZrSize caseIndex = 0; caseIndex < switchExpression->cases->count; caseIndex++) {
                SZrAstNode *caseNode = switchExpression->cases->nodes[caseIndex];

                if (caseNode != ZR_NULL &&
                    caseNode->type == ZR_AST_SWITCH_CASE &&
                    switch_union_case_covers_variant(cs,
                                                     caseNode->data.switchCase.value,
                                                     switchUnionTypeName,
                                                     variantName)) {
                    covered = ZR_TRUE;
                    break;
                }
                if (cs->hasError) {
                    return ZR_FALSE;
                }
            }
        }

        if (!covered) {
            if (switchExpression->defaultCase == ZR_NULL) {
                TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
                const TZrChar *variantText = ZrCore_String_GetNativeString(variantName);

                snprintf(message,
                         sizeof(message),
                         "Non-exhaustive union switch; missing variant '%s'",
                         variantText != ZR_NULL ? variantText : "<unknown>");
                ZrParser_Compiler_Error(cs, message, location);
            }
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}
