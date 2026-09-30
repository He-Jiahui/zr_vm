#include "dataflow_ownership_regions.h"

#include <string.h>

#include "zr_vm_parser/semantic.h"

/* 仅同源文件的范围才能建立引用归属；来源名可为内容相等但非同一对象的字符串。 */
static TZrBool ownership_region_same_source(SZrString *left, SZrString *right) {
    return left == right ||
           (left != ZR_NULL &&
            right != ZR_NULL &&
            ZrCore_String_Equal(left, right));
}

/* 以 AST 与语义事实共享的来源和坐标范围匹配引用。 */
static TZrBool ownership_region_range_contains(const SZrFileRange *outer,
                                                const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !ownership_region_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }
    /* 两端都有非零 offset 信息时按字节边界比较，否则回退到行列坐标。 */
    if ((outer->start.offset > 0 || outer->end.offset > 0) &&
        (inner->start.offset > 0 || inner->end.offset > 0)) {
        return inner->start.offset >= outer->start.offset && inner->end.offset <= outer->end.offset;
    }
    if (inner->start.line < outer->start.line || inner->end.line > outer->end.line) {
        return ZR_FALSE;
    }
    if (inner->start.line == outer->start.line && inner->start.column < outer->start.column) {
        return ZR_FALSE;
    }
    return inner->end.line != outer->end.line || inner->end.column <= outer->end.column;
}

/* 优先用 AST 节点身份匹配；派生节点不同时再用同源范围包含补足。 */
static TZrBool ownership_region_node_contains_reference(
        SZrAstNode *node,
        const SZrSemanticReferenceFact *fact) {
    return node != ZR_NULL &&
           fact != ZR_NULL &&
           (node == fact->node || ownership_region_range_contains(&node->location, &fact->range));
}

/* 从有效的引用事实中筛选指定 kind 和 target 范围，供区域绑定复用。 */
static const SZrSemanticReferenceFact *ownership_region_find_reference(
        const SZrSemanticContext *context,
        SZrAstNode *node,
        EZrSemanticReferenceKind kind) {
    const SZrSemanticReferenceFact *result = ZR_NULL;
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == kind &&
            fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID &&
            ownership_region_node_contains_reference(node, fact)) {
            /* TODO: 复合 target 可能同时覆盖 receiver 与 member READ；最小 symbolId 是否对应实际 owner，
             * 还需核实 member fact 与 ownership symbol map 的身份关系。对照 type_inference_member_facts.c、
             * dataflow_ownership_symbols.c，并在 test_ownership_diagnostics_region_cases.h 添加成员借用用例。 */
            if (result == ZR_NULL || fact->symbolId < result->symbolId) {
                result = fact;
            }
        }
    }
    return result;
}

const SZrSemanticReferenceFact *ZrParser_DataflowOwnership_ConstructTargetRead(
        const SZrSemanticContext *context,
        SZrAstNode *constructNode) {
    if (constructNode == ZR_NULL || constructNode->type != ZR_AST_CONSTRUCT_EXPRESSION) {
        return ZR_NULL;
    }
    return ownership_region_find_reference(context,
                                           constructNode->data.constructExpression.target,
                                           ZR_SEMANTIC_REFERENCE_READ);
}

/* 把 degrade intrinsic 投影为区域绑定共用的 builtin 与 target 视图。 */
static TZrBool ownership_region_current_member_projection(
        SZrAstNode *node,
        EZrOwnershipBuiltinKind *outBuiltinKind,
        SZrAstNode **outTarget) {
    if (outBuiltinKind != ZR_NULL) {
        *outBuiltinKind = ZR_OWNERSHIP_BUILTIN_KIND_NONE;
    }
    if (outTarget != ZR_NULL) {
        *outTarget = ZR_NULL;
    }
    if (node == ZR_NULL ||
        node->type != ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION ||
        node->data.ownershipIntrinsicExpression.operation !=
                ZR_OWNERSHIP_INTRINSIC_DEGRADE ||
        node->data.ownershipIntrinsicExpression.argument == ZR_NULL) {
        return ZR_FALSE;
    }
    if (outBuiltinKind != ZR_NULL) {
        *outBuiltinKind = ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE;
    }
    if (outTarget != ZR_NULL) {
        *outTarget = node->data.ownershipIntrinsicExpression.argument;
    }
    return ZR_TRUE;
}

TZrBool ZrParser_DataflowOwnership_StatementRegionBinding(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        SZrDataflowOwnershipRegionBinding *outBinding) {
    SZrAstNode *constructNode;
    SZrAstNode *aliasNode;
    SZrAstNode *ownerNode = ZR_NULL;
    EZrSemanticReferenceKind aliasReferenceKind;
    EZrOwnershipBuiltinKind builtinKind;

    if (outBinding != ZR_NULL) {
        memset(outBinding, 0, sizeof(*outBinding));
    }
    if (context == ZR_NULL ||
        statement == ZR_NULL ||
        outBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (statement->type == ZR_AST_VARIABLE_DECLARATION) {
        constructNode = statement->data.variableDeclaration.value;
        aliasNode = statement->data.variableDeclaration.pattern;
        aliasReferenceKind = ZR_SEMANTIC_REFERENCE_DECLARATION;
        outBinding->isDeclaration = ZR_TRUE;
    } else if (statement->type == ZR_AST_EXPRESSION_STATEMENT &&
               statement->data.expressionStatement.expr != ZR_NULL &&
               statement->data.expressionStatement.expr->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
               statement->data.expressionStatement.expr->data.assignmentExpression.op.op != ZR_NULL &&
               strcmp(statement->data.expressionStatement.expr->data.assignmentExpression.op.op,
                      "=") == 0) {
        SZrAssignmentExpression *assignment =
                &statement->data.expressionStatement.expr->data.assignmentExpression;
        constructNode = assignment->right;
        aliasNode = assignment->left;
        aliasReferenceKind = ZR_SEMANTIC_REFERENCE_WRITE;
    } else {
        return ZR_FALSE;
    }
    if (constructNode == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 普通 borrow/loan 构造和 degrade intrinsic 使用不同 AST，先归一为同一绑定输入。 */
    if (constructNode->type == ZR_AST_CONSTRUCT_EXPRESSION) {
        builtinKind = constructNode->data.constructExpression.builtinKind;
        ownerNode = constructNode->data.constructExpression.target;
    } else if (!ownership_region_current_member_projection(
                       constructNode,
                       &builtinKind,
                       &ownerNode)) {
        return ZR_FALSE;
    }
    if (builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_BORROW &&
        builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_LOAN &&
        builtinKind != ZR_OWNERSHIP_BUILTIN_KIND_DEGRADE) {
        return ZR_FALSE;
    }

    outBinding->aliasReference = ownership_region_find_reference(
            context,
            aliasNode,
            aliasReferenceKind);
    outBinding->ownerReference = ownership_region_find_reference(
            context,
            ownerNode,
            ZR_SEMANTIC_REFERENCE_READ);
    if (outBinding->aliasReference == ZR_NULL || outBinding->ownerReference == ZR_NULL) {
        memset(outBinding, 0, sizeof(*outBinding));
        return ZR_FALSE;
    }

    outBinding->constructNode = constructNode;
    if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_BORROW) {
        outBinding->qualifier = ZR_OWNERSHIP_QUALIFIER_BORROWED;
    } else if (builtinKind == ZR_OWNERSHIP_BUILTIN_KIND_LOAN) {
        outBinding->qualifier = ZR_OWNERSHIP_QUALIFIER_LOANED;
    } else {
        outBinding->qualifier = ZR_OWNERSHIP_QUALIFIER_WEAK;
    }
    return ZR_TRUE;
}

/* 取出 CFG 语句承载的表达式；using 资源由释放分类器的专门分支处理。 */
static SZrAstNode *ownership_region_statement_expression(SZrAstNode *statement) {
    if (statement == ZR_NULL) {
        return ZR_NULL;
    }
    switch (statement->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            return statement->data.variableDeclaration.value;
        case ZR_AST_EXPRESSION_STATEMENT:
            return statement->data.expressionStatement.expr;
        case ZR_AST_RETURN_STATEMENT:
            return statement->data.returnStatement.expr;
        case ZR_AST_THROW_STATEMENT:
            return statement->data.throwStatement.expr;
        case ZR_AST_OUT_STATEMENT:
            return statement->data.outStatement.expr;
        default:
            return ZR_NULL;
    }
}

/* BUG: 这里只识别表达式根部的 DROP，以及简单赋值右侧的 DROP；复合节点内的释放不会被识别。
 * 可用已有 Shared<Resource>、ref owner 场景扩展为 var boxed = [drop(owner)]; borrowed;：数组推导接受
 * Array<Null>，lowering 也编译元素，但 owner 没有进入 RELEASED 状态，后续 borrowed 读取漏发 borrow_escape。
 * 合法输入、状态传播与现有回归入口见覆盖台账。 */
static TZrBool ownership_region_expression_releases_read(
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression->type == ZR_AST_CONSTRUCT_EXPRESSION) {
        return expression->data.constructExpression.builtinKind ==
                       ZR_OWNERSHIP_BUILTIN_KIND_DROP &&
               ownership_region_node_contains_reference(
                       expression->data.constructExpression.target,
                       fact);
    }
    if (expression->type == ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION) {
        return expression->data.ownershipIntrinsicExpression.operation ==
                       ZR_OWNERSHIP_INTRINSIC_DROP &&
               ownership_region_node_contains_reference(
                       expression->data.ownershipIntrinsicExpression.argument,
                       fact);
    }
    if (expression->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
        expression->data.assignmentExpression.op.op != ZR_NULL &&
        strcmp(expression->data.assignmentExpression.op.op, "=") == 0) {
        return ownership_region_expression_releases_read(
                expression->data.assignmentExpression.right,
                fact);
    }
    return ZR_FALSE;
}

TZrBool ZrParser_DataflowOwnership_StatementReleasesRead(
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    /* BUG: using 把 resource 子树内的 READ 全视为 cleanup 对象。合法的
     * Shared<Resource> 按值函数 sharedReturn(owner) 返回独立共享副本，调用方原 owner 仍有自己的强引用；
     * 编译器为 call-result 建临时 slot 并只对该 slot 注册 DROP，数据流这里却将原 owner 的 READ 标成 RELEASED，
     * 使其仍有效的 borrowed alias 在作用域后误报 borrow_escape。using 与 call-result 目前只有分立测试；
     * 组合路径由 parser/type/copy/cleanup/CFG/diagnostic 静态闭环，完整证据与待补回归入口见覆盖台账。
     */
    if (statement != ZR_NULL && statement->type == ZR_AST_USING_STATEMENT) {
        return ownership_region_node_contains_reference(
                statement->data.usingStatement.resource,
                fact);
    }
    return ownership_region_expression_releases_read(
            ownership_region_statement_expression(statement),
            fact);
}
