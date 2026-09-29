#include "zr_vm_parser/const_assignment.h"
#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

/** @brief 判断目标源区间是否完整包围赋值区间。
 * @note 优先比较同源文件偏移；偏移不可用时回退到有效行列，缺少位置证据则拒绝。
 */
static TZrBool const_assignment_range_contains(const SZrFileRange *outer,
                                               const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        (outer->source != ZR_NULL && inner->source != ZR_NULL &&
         !ZrCore_String_Equal(outer->source, inner->source))) {
        return ZR_FALSE;
    }
    if (outer->end.offset > outer->start.offset &&
        inner->end.offset >= inner->start.offset) {
        return outer->start.offset <= inner->start.offset &&
               inner->end.offset <= outer->end.offset;
    }
    if (outer->start.line <= 0 || outer->end.line <= 0 ||
        inner->start.line <= 0 || inner->end.line <= 0) {
        return ZR_FALSE;
    }
    return (outer->start.line < inner->start.line ||
            (outer->start.line == inner->start.line &&
             outer->start.column <= inner->start.column)) &&
           (inner->end.line < outer->end.line ||
            (inner->end.line == outer->end.line &&
             inner->end.column <= outer->end.column));
}

static TZrBool const_assignment_string_equals(SZrString *value,
                                              const TZrChar *expected) {
    const TZrChar *text;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL && strcmp(text, expected) == 0;
}

static SZrAstNodeArray *const_assignment_owner_members(const SZrAstNode *owner) {
    if (owner == ZR_NULL) {
        return ZR_NULL;
    }
    if (owner->type == ZR_AST_CLASS_DECLARATION) {
        return owner->data.classDeclaration.members;
    }
    if (owner->type == ZR_AST_STRUCT_DECLARATION) {
        return owner->data.structDeclaration.members;
    }
    return ZR_NULL;
}

/** @brief 在模块的顶层类或结构体成员中按 AST 节点身份定位字段所有者。
 * @note 只扫描直接的 script 声明，不递归进入其他节点；构造函数例外依赖该所有者。
 */
static const SZrAstNode *const_assignment_find_field_owner(
        const SZrAstNode *moduleRoot,
        const SZrAstNode *targetDeclaration) {
    SZrAstNodeArray *statements;

    if (moduleRoot == ZR_NULL || targetDeclaration == ZR_NULL) {
        return ZR_NULL;
    }
    if (moduleRoot->type == ZR_AST_CLASS_DECLARATION ||
        moduleRoot->type == ZR_AST_STRUCT_DECLARATION) {
        statements = const_assignment_owner_members(moduleRoot);
        for (TZrSize index = 0U;
             statements != ZR_NULL && index < statements->count;
             index++) {
            if (statements->nodes[index] == targetDeclaration) {
                return moduleRoot;
            }
        }
        return ZR_NULL;
    }
    if (moduleRoot->type != ZR_AST_SCRIPT ||
        moduleRoot->data.script.statements == ZR_NULL) {
        return ZR_NULL;
    }

    statements = moduleRoot->data.script.statements;
    for (TZrSize statementIndex = 0U;
         statementIndex < statements->count;
         statementIndex++) {
        const SZrAstNode *candidateOwner = statements->nodes[statementIndex];
        SZrAstNodeArray *members = const_assignment_owner_members(candidateOwner);
        for (TZrSize memberIndex = 0U;
             members != ZR_NULL && memberIndex < members->count;
             memberIndex++) {
            if (members->nodes[memberIndex] == targetDeclaration) {
                return candidateOwner;
            }
        }
    }
    return ZR_NULL;
}

static TZrBool const_assignment_is_constructor(const SZrAstNode *member) {
    if (member == ZR_NULL) {
        return ZR_FALSE;
    }
    if (member->type == ZR_AST_CLASS_META_FUNCTION) {
        return member->data.classMetaFunction.meta != ZR_NULL &&
               const_assignment_string_equals(
                       member->data.classMetaFunction.meta->name,
                       "constructor");
    }
    if (member->type == ZR_AST_STRUCT_META_FUNCTION) {
        return member->data.structMetaFunction.meta != ZR_NULL &&
               const_assignment_string_equals(
                       member->data.structMetaFunction.meta->name,
                       "constructor");
    }
    return ZR_FALSE;
}

/** @brief 限定实例 const 字段的初始化例外位于声明类型自己的构造函数范围内。
 * @note 此处只判 AST 源位置归属，不统计写入次数或分支上的初始化完备性。
 */
static TZrBool const_assignment_is_inside_owner_constructor(
        const SZrAstNode *owner,
        const SZrAstNode *assignment) {
    SZrAstNodeArray *members = const_assignment_owner_members(owner);

    for (TZrSize index = 0U;
         members != ZR_NULL && assignment != ZR_NULL && index < members->count;
         index++) {
        const SZrAstNode *member = members->nodes[index];
        if (const_assignment_is_constructor(member) &&
            const_assignment_range_contains(&member->location,
                                            &assignment->location)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/** @brief 识别隐式字段名或 this.field 这两种当前实例左值形状。
 * @note 只做语法接收者判断，不替代上游名称解析，也不接受其他对象的成员写入。
 */
static TZrBool const_assignment_targets_current_instance(
        const SZrAstNode *assignment) {
    const SZrAstNode *left;
    const SZrPrimaryExpression *primary;

    if (assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_FALSE;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }
    if (left->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &left->data.primaryExpression;
    return primary->property != ZR_NULL &&
           primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
            const_assignment_string_equals(
                    primary->property->data.identifier.name,
                    "this");
}

static SZrString *const_assignment_target_name(const SZrAstNode *assignment) {
    const SZrAstNode *left;
    const SZrPrimaryExpression *primary;
    const SZrAstNode *lastMember;
    const SZrAstNode *property;

    if (assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_NULL;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        return left->data.identifier.name;
    }
    if (left->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_NULL;
    }

    primary = &left->data.primaryExpression;
    if (primary->members == ZR_NULL || primary->members->count == 0U) {
        return ZR_NULL;
    }
    lastMember = primary->members->nodes[primary->members->count - 1U];
    if (lastMember == ZR_NULL ||
        lastMember->type != ZR_AST_MEMBER_EXPRESSION) {
        return ZR_NULL;
    }
    property = lastMember->data.memberExpression.property;
    return property != ZR_NULL && property->type == ZR_AST_IDENTIFIER_LITERAL
                   ? property->data.identifier.name
                   : ZR_NULL;
}

static TZrBool const_assignment_target_range(
        const SZrAstNode *assignment,
        SZrFileRange *outRange) {
    const SZrAstNode *left;

    if (assignment == ZR_NULL || outRange == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_FALSE;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        *outRange = left->location;
        return ZR_TRUE;
    }
    if (left->type == ZR_AST_PRIMARY_EXPRESSION &&
        left->data.primaryExpression.members != ZR_NULL &&
        left->data.primaryExpression.members->count > 0U) {
        const SZrAstNode *lastMember =
                left->data.primaryExpression.members->nodes[
                        left->data.primaryExpression.members->count - 1U];
        if (lastMember != ZR_NULL &&
            lastMember->type == ZR_AST_MEMBER_EXPRESSION &&
            lastMember->data.memberExpression.property != ZR_NULL) {
            *outRange = lastMember->data.memberExpression.property->location;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/** @brief 将 this 与当前类型名接收者分别归类为实例字段和静态字段访问。
 * @note 该分类只供无已解析声明时的上下文兜底查找使用。
 */
static TZrBool const_assignment_context_receiver_kind(
        const SZrTypePrototypeInfo *prototype,
        const SZrAstNode *assignment,
        TZrBool *outIsStatic) {
    const SZrAstNode *left;
    const SZrAstNode *receiver;

    if (prototype == ZR_NULL || outIsStatic == ZR_NULL ||
        assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        assignment->data.assignmentExpression.left == ZR_NULL) {
        return ZR_FALSE;
    }
    left = assignment->data.assignmentExpression.left;
    if (left->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    receiver = left->data.primaryExpression.property;
    if (receiver == ZR_NULL ||
        receiver->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    if (const_assignment_string_equals(
                receiver->data.identifier.name, "this")) {
        *outIsStatic = ZR_FALSE;
        return ZR_TRUE;
    }
    if (prototype->name != ZR_NULL &&
        receiver->data.identifier.name != ZR_NULL &&
        ZrCore_String_Equal(
                prototype->name, receiver->data.identifier.name)) {
        *outIsStatic = ZR_TRUE;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

/** @brief 在当前类型原型中用接收者种类、字段名和静态性消歧字段声明。
 * @note 此兜底路径按字符串名扫描，遇到多个不同声明匹配时拒绝猜测；已解析 SymbolId 应优先提供 AST 节点。
 */
static const SZrAstNode *const_assignment_resolve_context_field(
        const SZrCompilerState *compilerState,
        const SZrAstNode *assignment) {
    const SZrTypePrototypeInfo *prototype;
    SZrString *targetName;
    const SZrAstNode *resolvedDeclaration = ZR_NULL;
    TZrBool targetIsStatic;

    if (compilerState == ZR_NULL ||
        compilerState->currentTypePrototypeInfo == ZR_NULL) {
        return ZR_NULL;
    }
    prototype = compilerState->currentTypePrototypeInfo;
    if (!const_assignment_context_receiver_kind(
                prototype, assignment, &targetIsStatic)) {
        return ZR_NULL;
    }
    targetName = const_assignment_target_name(assignment);
    if (targetName == ZR_NULL || !prototype->members.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&prototype->members, index);
        if (member == ZR_NULL || member->declarationNode == ZR_NULL ||
            (member->memberType != ZR_AST_CLASS_FIELD &&
             member->memberType != ZR_AST_STRUCT_FIELD) ||
            member->isStatic != targetIsStatic ||
            member->name == ZR_NULL ||
            !ZrCore_String_Equal(member->name, targetName)) {
            continue;
        }
        if (resolvedDeclaration != ZR_NULL &&
            resolvedDeclaration != member->declarationNode) {
            return ZR_NULL;
        }
        resolvedDeclaration = member->declarationNode;
    }
    return resolvedDeclaration;
}

/**
 * @brief 从声明 AST 提取 const 赋值检查所需的类别、名称和声明位置。
 * @note targetName 借用声明 AST 的字符串；输出不接管 AST 或字符串所有权。
 * @return false 表示输入无效或声明种类不受支持；true 表示结果已描述，不代表发生违规。
 */
TZrBool ZrParser_ConstAssignment_DescribeTarget(
        const SZrAstNode *targetDeclaration,
        SZrConstAssignmentResult *outResult) {
    if (targetDeclaration == ZR_NULL || outResult == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(outResult, 0, sizeof(*outResult));
    outResult->declarationRange = targetDeclaration->location;
    switch (targetDeclaration->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            if (targetDeclaration->data.variableDeclaration.pattern == ZR_NULL ||
                targetDeclaration->data.variableDeclaration.pattern->type !=
                        ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_FALSE;
            }
            outResult->targetKind = ZR_CONST_ASSIGNMENT_TARGET_LOCAL;
            outResult->targetName = targetDeclaration->data.variableDeclaration
                                            .pattern->data.identifier.name;
            outResult->declarationRange =
                    targetDeclaration->data.variableDeclaration.pattern->location;
            outResult->isConstTarget =
                    targetDeclaration->data.variableDeclaration.isConst;
            break;

        case ZR_AST_PARAMETER:
            outResult->targetKind = ZR_CONST_ASSIGNMENT_TARGET_PARAMETER;
            outResult->targetName = targetDeclaration->data.parameter.name != ZR_NULL
                                            ? targetDeclaration->data.parameter.name->name
                                            : ZR_NULL;
            outResult->declarationRange =
                    targetDeclaration->data.parameter.nameLocation;
            outResult->isConstTarget = targetDeclaration->data.parameter.isConst;
            break;

        case ZR_AST_CLASS_FIELD:
            outResult->targetKind = targetDeclaration->data.classField.isStatic
                                            ? ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD
                                            : ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD;
            outResult->targetName = targetDeclaration->data.classField.name != ZR_NULL
                                            ? targetDeclaration->data.classField.name->name
                                            : ZR_NULL;
            outResult->declarationRange =
                    targetDeclaration->data.classField.nameLocation;
            outResult->isConstTarget = targetDeclaration->data.classField.isConst;
            break;

        case ZR_AST_STRUCT_FIELD:
            outResult->targetKind = targetDeclaration->data.structField.isStatic
                                            ? ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD
                                            : ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD;
            outResult->targetName = targetDeclaration->data.structField.name != ZR_NULL
                                            ? targetDeclaration->data.structField.name->name
                                            : ZR_NULL;
            outResult->isConstTarget = targetDeclaration->data.structField.isConst;
            break;

        default:
            return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 判断赋值是否违反参数、局部变量或字段的 const 写入规则。
 * @note 只有当前实例字段在其声明类或结构体的构造函数范围内可写；true 表示完成判定，违规由 isViolation 标出。
 * @return false 表示赋值或目标声明无法判定；true 表示 outResult 已写入判定结果。
 */
TZrBool ZrParser_ConstAssignment_Evaluate(
        const SZrAstNode *moduleRoot,
        const SZrAstNode *assignment,
        const SZrAstNode *targetDeclaration,
        SZrConstAssignmentResult *outResult) {
    const SZrAstNode *owner;

    if (assignment == ZR_NULL ||
        assignment->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        !ZrParser_ConstAssignment_DescribeTarget(
                targetDeclaration, outResult)) {
        return ZR_FALSE;
    }

    outResult->assignmentRange = assignment->location;
    if (!outResult->isConstTarget) {
        return ZR_TRUE;
    }
    if (outResult->targetKind !=
            ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD) {
        outResult->isViolation = ZR_TRUE;
        return ZR_TRUE;
    }

    owner = const_assignment_find_field_owner(moduleRoot, targetDeclaration);
    outResult->isViolation =
            owner == ZR_NULL ||
            !const_assignment_targets_current_instance(assignment) ||
            !const_assignment_is_inside_owner_constructor(owner, assignment);
    return ZR_TRUE;
}

/**
 * @brief 以已解析声明优先、当前类型上下文兜底的方式执行 const 赋值判定。
 * @note resolvedTargetDeclaration 非空时不按名称重新选择；为空时才使用原型字段查找，再复用统一规则。
 */
TZrBool ZrParser_ConstAssignment_EvaluateContext(
        const SZrCompilerState *compilerState,
        const SZrAstNode *moduleRoot,
        const SZrAstNode *assignment,
        const SZrAstNode *resolvedTargetDeclaration,
        SZrConstAssignmentResult *outResult) {
    const SZrAstNode *targetDeclaration = resolvedTargetDeclaration;

    if (targetDeclaration == ZR_NULL) {
        targetDeclaration = const_assignment_resolve_context_field(
                compilerState, assignment);
    }
    return ZrParser_ConstAssignment_Evaluate(
            moduleRoot, assignment, targetDeclaration, outResult);
}

/**
 * @brief 将已确认的 const 写入违规构造成带声明关联位置和无自动修复原因的结构化诊断。
 * @note 本函数只构造诊断，不发布语义事实；输出诊断仍由调用者释放或转交事实容器。
 */
TZrBool ZrParser_ConstAssignment_BuildDiagnostic(
        SZrState *state,
        const SZrConstAssignmentResult *result,
        SZrStructuredDiagnostic *outDiagnostic) {
    TZrChar message[192];
    const TZrChar *name;
    const TZrChar *cause;
    const TZrChar *suggestion;

    if (state == ZR_NULL || result == ZR_NULL || outDiagnostic == ZR_NULL ||
        !result->isConstTarget || !result->isViolation) {
        return ZR_FALSE;
    }

    name = result->targetName != ZR_NULL
                   ? ZrCore_String_GetNativeString(result->targetName)
                   : ZR_NULL;
    switch (result->targetKind) {
        case ZR_CONST_ASSIGNMENT_TARGET_PARAMETER:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to const parameter '%s'"
                             : "Cannot assign to const parameter",
                     name != ZR_NULL ? name : "");
            cause = "A const parameter is immutable for the entire callable body.";
            suggestion = "Pass a different value or introduce a mutable local copy.";
            break;

        case ZR_CONST_ASSIGNMENT_TARGET_LOCAL:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to const variable '%s' after declaration"
                             : "Cannot assign to const variable after declaration",
                     name != ZR_NULL ? name : "");
            cause = "A const local is initialized by its declaration and cannot be assigned again.";
            suggestion = "Use a mutable variable when later assignment is required.";
            break;

        case ZR_CONST_ASSIGNMENT_TARGET_STATIC_FIELD:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to const static field '%s'"
                             : "Cannot assign to const static field",
                     name != ZR_NULL ? name : "");
            cause = "A const static field cannot be mutated after its declaration initializer.";
            suggestion = "Keep the field immutable or redesign the state as an explicitly mutable static field.";
            break;

        case ZR_CONST_ASSIGNMENT_TARGET_INSTANCE_FIELD:
            snprintf(message,
                     sizeof(message),
                     name != ZR_NULL
                             ? "Cannot assign to immutable field '%s' outside initialization"
                             : "Cannot assign to immutable field outside initialization",
                     name != ZR_NULL ? name : "");
            cause = "An immutable instance field can only initialize the current instance inside its declaring type's constructor.";
            suggestion = "Move the assignment into the constructor or make the field mutable.";
            break;

        default:
            return ZR_FALSE;
    }

    if (!ZrParser_DiagnosticBuilder_Build(
                state,
                outDiagnostic,
                ZR_STRUCTURED_DIAGNOSTIC_ERROR,
                result->assignmentRange,
                "const_assignment",
                message,
                cause,
                suggestion) ||
        !ZrParser_StructuredDiagnostic_AddRelatedInformation(
                state,
                outDiagnostic,
                result->declarationRange,
                "Immutable declaration is here") ||
        !ZrParser_StructuredDiagnostic_SetNoFixReason(
                outDiagnostic,
                ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION)) {
        ZrParser_StructuredDiagnostic_Free(state, outDiagnostic);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 从赋值左值的语义引用取得规范符号 ID，生成并追加 const 写入诊断事实。
 * @note 有引用时按 SymbolId 取回声明 AST；仅 SymbolAt 未命中时才退回当前类型的名称查找。AppendDiagnostic 复制事实后释放本地诊断。
 * @return true 仅表示违规诊断事实已追加；无违规、引用记录不完整或发布失败均返回 false。
 */
TZrBool ZrParser_ConstAssignment_PublishDiagnostic(
        SZrCompilerState *compilerState,
        const SZrAstNode *moduleRoot,
        SZrAstNode *assignment) {
    SZrFileRange targetRange;
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticSymbolQuery query;
    const SZrSemanticSymbolRecord *symbol = ZR_NULL;
    const SZrAstNode *targetDeclaration = ZR_NULL;
    SZrConstAssignmentResult result;
    SZrStructuredDiagnostic diagnostic;
    SZrSemanticDiagnosticFact fact;
    TZrBool appended;

    if (compilerState == ZR_NULL || compilerState->state == ZR_NULL ||
        compilerState->semanticContext == ZR_NULL || moduleRoot == ZR_NULL ||
        !const_assignment_target_range(assignment, &targetRange)) {
        return ZR_FALSE;
    }

    ZrParser_SemanticQueryScope_Module(&scope);
    memset(&query, 0, sizeof(query));
    if (ZrParser_SemanticQuery_SymbolAt(
                compilerState->semanticContext,
                targetRange,
                &scope,
                &query)) {
        symbol = ZrParser_Semantic_FindSymbolById(
                compilerState->semanticContext, query.symbolId);
        if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
            return ZR_FALSE;
        }
        targetDeclaration = symbol->astNode;
    }
    if (!ZrParser_ConstAssignment_EvaluateContext(
                compilerState,
                moduleRoot,
                assignment,
                targetDeclaration,
                &result) ||
        !result.isConstTarget || !result.isViolation) {
        return ZR_FALSE;
    }

    ZrParser_StructuredDiagnostic_Init(&diagnostic);
    if (!ZrParser_ConstAssignment_BuildDiagnostic(
                compilerState->state, &result, &diagnostic)) {
        return ZR_FALSE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = assignment;
    fact.diagnostic = diagnostic;
    appended = ZrParser_SemanticFacts_AppendDiagnostic(
            compilerState->semanticContext, &fact);
    ZrParser_StructuredDiagnostic_Free(compilerState->state, &diagnostic);
    return appended;
}
