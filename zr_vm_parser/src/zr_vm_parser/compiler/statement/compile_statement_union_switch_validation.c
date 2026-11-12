#include "compiler/statement/compile_statement_union_switch_validation.h"

#include "compiler/expression/compile_expression_internal.h"
#include "type_inference/type_inference_internal.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"

/* 仅裸标识符或 primary/member-path 形态可以在显式解析失败后借 switch subject 类型补足 union 上下文。 */
static TZrBool switch_case_can_use_subject_union_type(SZrAstNode *caseValue) {
    return caseValue != ZR_NULL &&
           (caseValue->type == ZR_AST_IDENTIFIER_LITERAL ||
            caseValue->type == ZR_AST_PRIMARY_EXPRESSION);
}

/*
 * resolver 输出的 bindings 有两种所有权：struct payload pattern 由 build_union_struct_pattern_bindings 新建独立容器，
 * 归消费方释放；tuple payload pattern 直接返回 case AST 的 call->args，归 ZrParser_Ast_Free 释放，校验之后的
 * lowering 还要读取它。本文件只把标识符或 primary 表达式形态的 case 交给 resolver，此时 bindings 只会随 primary
 * 表达式输出，借用来源只有其成员里的调用实参；无法确认归属的形态按借用处理，宁可漏放也不释放 AST 的数组。
 */
static TZrBool switch_union_case_owns_bindings(SZrAstNode *caseValue, SZrAstNodeArray *bindings) {
    SZrAstNodeArray *members;

    if (caseValue == ZR_NULL || bindings == ZR_NULL || caseValue->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    members = caseValue->data.primaryExpression.members;
    if (members != ZR_NULL && members->nodes != ZR_NULL) {
        for (TZrSize index = 0; index < members->count; index++) {
            SZrAstNode *member = members->nodes[index];

            if (member != ZR_NULL &&
                member->type == ZR_AST_FUNCTION_CALL &&
                member->data.functionCall.args == bindings) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

/* 按 subject union 上下文解析 case，再把解析出的 variant 名与当前声明 variant 对照。 */
static TZrBool switch_union_case_covers_variant(SZrCompilerState *cs,
                                                SZrAstNode *caseValue,
                                                SZrString *switchUnionTypeName,
                                                SZrString *variantName) {
    SZrString *caseVariantName = ZR_NULL;
    SZrAstNodeArray *bindings = ZR_NULL;
    TZrBool resolved;
    TZrBool covers;

    if (cs == ZR_NULL || caseValue == ZR_NULL || variantName == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    if (switchUnionTypeName != ZR_NULL && switch_case_can_use_subject_union_type(caseValue)) {
        resolved = try_resolve_union_variant_pattern_for_type(cs,
                                                              caseValue,
                                                              switchUnionTypeName,
                                                              &caseVariantName,
                                                              &bindings,
                                                              ZR_NULL);
    } else {
        resolved = try_resolve_union_variant_pattern_expression(cs,
                                                                caseValue,
                                                                &caseVariantName,
                                                                &bindings,
                                                                ZR_NULL);
    }

    /*
     * TODO: for_type 会先接受显式 Other.Variant，再把解析出的 variantName 返回；此处只保留名称，无法区分
     * subject union 与同名的外部 variant。lowering 当前比较 __zr_unionVariant 字符串并把解析出的 variant AST
     * 交给 payload 类型注册。下一步：明确跨 union 限定模式的契约，为同名但 payload 类型不同的 union 加回归例，
     * 并统一 resolver、覆盖校验、lowering 与 CFG 使用的 variant identity。
     */
    /* resolver 的 true 既可能表示 pattern 已识别，也可能表示已报告诊断；须同时检查 error 状态和输出名。 */
    covers = (TZrBool)(resolved && !cs->hasError && caseVariantName != ZR_NULL &&
                       ZrCore_String_Equal(caseVariantName, variantName));
    /* 校验只比较名称；struct pattern 的独立容器在此配对释放，duplicate 与 exhaustiveness 两轮扫描都经过这里。 */
    if (switch_union_case_owns_bindings(caseValue, bindings)) {
        ZrParser_AstNodeArray_Free(cs->state, bindings);
    }
    return covers;
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

    /* 按声明 variant 逐一扫描 case；记录首个命中后，第二个命中就是应在 lowering 前拒绝的分支。 */
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

        /* default 只处理运行时未命中的值；它不补足 AST 的显式覆盖标记，CFG 仍需保留 default 可达性。 */
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
