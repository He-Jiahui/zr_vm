#include "dataflow_ownership_moves.h"

#include <string.h>

#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_parser/semantic.h"

/* 调用实参与形参按名字配对时，只有两个已命名的字符串才构成有效匹配。 */
static TZrBool ownership_move_names_equal(SZrString *left, SZrString *right) {
    return left != ZR_NULL &&
           right != ZR_NULL &&
           (left == right || ZrCore_String_Equal(left, right));
}

/* 仅正偏移表示当前范围可用字节坐标比较；零值继续走行列回退。 */
static TZrBool ownership_move_has_offset(const SZrFilePosition *position) {
    return position != ZR_NULL && position->offset > 0;
}

/* 文件名可能来自不同分配，故接受同一指针（包括两个空值）或非空字符串值相等。 */
static TZrBool ownership_move_same_source(SZrString *left, SZrString *right) {
    return left == right ||
           (left != ZR_NULL &&
            right != ZR_NULL &&
            ZrCore_String_Equal(left, right));
}

/* 事实位置只在同一源码内比较；双方都有字节坐标时优先用偏移边界判断包含关系。 */
static TZrBool ownership_move_range_contains(const SZrFileRange *outer,
                                              const SZrFileRange *inner) {
    if (outer == ZR_NULL || inner == ZR_NULL ||
        !ownership_move_same_source(outer->source, inner->source)) {
        return ZR_FALSE;
    }
    if ((ownership_move_has_offset(&outer->start) || ownership_move_has_offset(&outer->end)) &&
        (ownership_move_has_offset(&inner->start) || ownership_move_has_offset(&inner->end))) {
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

/* AST 节点优先按身份匹配，克隆或细粒度事实则用源码范围关联。 */
static TZrBool ownership_move_node_contains_fact(SZrAstNode *node,
                                                  const SZrSemanticReferenceFact *fact) {
    return node != ZR_NULL &&
           fact != ZR_NULL &&
           (node == fact->node || ownership_move_range_contains(&node->location, &fact->range));
}

/*
 * 只接受已解析且带有效符号身份的 CALL 事实，以免按语法形状猜测形参契约。
 * BUG: 合法实例方法形状为 `resource class Resource {}; class Box { fn consume(value: Unique<Resource>) {} fn observe(watcher: ref Weak<Resource>) {} }`。
 * `fn use(box: Box, owner: Unique<Resource>) { box.consume(owner); drop(owner); }` 中同类型 VALUE 实参应移动，
 * 但 CALL 事实指向方法名属性叶，本函数收到完整 primary 后未匹配该事实，后续 owner 读取可能漏报。
 * 同一 Box 的 weak 形状 `fn useWeak(box: Box, owner: Shared<Resource>) { var weakAlias = degrade(owner); drop(owner); box.observe(ref weakAlias); }` 也会漏分类。
 * Unique/Shared/Weak 包装保留所有权限定；方法参数元数据保存类型与模式，ref weak 接受同类型可写实参及 ref 标记。
 * 方法 fixture 的 Shared 实参仅证明成员调用形状；同类型 Unique VALUE 与同类型 Weak REF 才闭合为可接受调用。
 * compiler 与 LSP 的诊断最终都使用此 ownership driver，未发现绕过该结果的诊断分支。
 * 证据：parser_types.c:82-105,1488-1566、parser_reference_syntax.c:11-59、compiler_class.c:1168-1170、
 * type_inference_call_semantic_facts.c:773-838、type_inference_native.c:2633-2686、
 * type_inference_passing_modes.c:47-63,121-230、test_semantic_analyzer.c:3957-3965、
 * test_ownership_diagnostics.c:520-598、compiler/compiler_semantic_query_diagnostics.c:112、
 * semantic_analyzer_query_diagnostics.c:165。
 * 当前结论来自闭合静态链，未运行测试，也没有同类型实例方法移动的专门复现。
 */
static const SZrSemanticReferenceFact *ownership_move_find_call_reference(
        const SZrSemanticContext *context,
        const SZrAstNode *callOwner) {
    TZrSize index;

    if (context == ZR_NULL || callOwner == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_CALL &&
            (fact->node == callOwner ||
             (callOwner->type == ZR_AST_PRIMARY_EXPRESSION &&
              fact->node == callOwner->data.primaryExpression.property)) &&
            fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            return fact;
        }
    }
    return ZR_NULL;
}

/* 成员调用的 canonical contract role 附着在成员或其属性节点上。 */
static const SZrSemanticReferenceFact *ownership_move_find_member_call_reference(
        const SZrSemanticContext *context,
        const SZrAstNode *memberNode) {
    TZrSize index;

    if (context == ZR_NULL || memberNode == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (fact != ZR_NULL &&
            fact->kind == ZR_SEMANTIC_REFERENCE_CALL &&
            fact->isResolved &&
            (fact->node == memberNode ||
             (memberNode->type == ZR_AST_MEMBER_EXPRESSION &&
              fact->node == memberNode->data.memberExpression.property))) {
            return fact;
        }
    }
    return ZR_NULL;
}

/* 将调用事实中的符号身份还原为当前语义上下文的声明记录。 */
static const SZrSemanticSymbolRecord *ownership_move_find_symbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || !context->symbols.isValid || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols,
                        index);
        if (symbol != ZR_NULL && symbol->id == symbolId) {
            return symbol;
        }
    }
    return ZR_NULL;
}

/* 统一不同可调用声明的固定参数与可变参数入口，供调用参数映射使用。 */
static SZrAstNodeArray *ownership_move_callable_parameters(SZrAstNode *node,
                                                           SZrParameter **outVariadic) {
    if (outVariadic != ZR_NULL) {
        *outVariadic = ZR_NULL;
    }
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.functionDeclaration.args;
            return node->data.functionDeclaration.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.externFunctionDeclaration.args;
            return node->data.externFunctionDeclaration.params;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.externDelegateDeclaration.args;
            return node->data.externDelegateDeclaration.params;
        case ZR_AST_STRUCT_METHOD:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.structMethod.args;
            return node->data.structMethod.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.structMetaFunction.args;
            return node->data.structMetaFunction.params;
        case ZR_AST_CLASS_METHOD:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.classMethod.args;
            return node->data.classMethod.params;
        case ZR_AST_CLASS_META_FUNCTION:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.classMetaFunction.args;
            return node->data.classMetaFunction.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.interfaceMethodSignature.args;
            return node->data.interfaceMethodSignature.params;
        case ZR_AST_INTERFACE_META_SIGNATURE:
            if (outVariadic != ZR_NULL) *outVariadic = node->data.interfaceMetaSignature.args;
            return node->data.interfaceMetaSignature.params;
        default:
            return ZR_NULL;
    }
}

/* 按调用参数序号读取可选的命名实参标签；缺失或无效数组表示位置实参。 */
static SZrString *ownership_move_argument_name(const SZrFunctionCall *call, TZrSize index) {
    SZrString **name;

    if (call == ZR_NULL || call->argNames == ZR_NULL || !call->argNames->isValid ||
        index >= call->argNames->length) {
        return ZR_NULL;
    }
    name = (SZrString **)ZrCore_Array_Get(call->argNames, index);
    return name != ZR_NULL ? *name : ZR_NULL;
}

/* 位置实参按序号匹配，命名实参按名称匹配；仅未命名尾参可回退到 variadic。 */
static const SZrParameter *ownership_move_parameter_at(SZrAstNodeArray *parameters,
                                                        SZrParameter *variadic,
                                                        const SZrFunctionCall *call,
                                                        TZrSize argumentIndex) {
    SZrString *argumentName = ownership_move_argument_name(call, argumentIndex);
    TZrSize index;

    if (parameters != ZR_NULL && argumentName == ZR_NULL && argumentIndex < parameters->count) {
        SZrAstNode *parameterNode = parameters->nodes[argumentIndex];
        return parameterNode != ZR_NULL && parameterNode->type == ZR_AST_PARAMETER
                       ? &parameterNode->data.parameter
                       : ZR_NULL;
    }
    for (index = 0; parameters != ZR_NULL && argumentName != ZR_NULL &&
                    index < parameters->count; index++) {
        SZrAstNode *parameterNode = parameters->nodes[index];
        if (parameterNode != ZR_NULL &&
            parameterNode->type == ZR_AST_PARAMETER &&
            parameterNode->data.parameter.name != ZR_NULL &&
            ownership_move_names_equal(parameterNode->data.parameter.name->name, argumentName)) {
            return &parameterNode->data.parameter;
        }
    }
    return argumentName == ZR_NULL ? variadic : ZR_NULL;
}

/* 区分值本身与成员投影：只有事实节点或无成员 primary 的属性才算直接来源。 */
static TZrBool ownership_move_expression_is_direct_reference(
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (expression == fact->node) {
        return ZR_TRUE;
    }
    return expression->type == ZR_AST_PRIMARY_EXPRESSION &&
           (expression->data.primaryExpression.members == ZR_NULL ||
            expression->data.primaryExpression.members->count == 0) &&
           expression->data.primaryExpression.property == fact->node;
}

/* 从已解析调用事实沿符号声明找到实参对应形参；任何缺失环节都不推断参数模式。 */
static const SZrParameter *ownership_move_argument_parameter(
        const SZrSemanticContext *context,
        SZrAstNode *callOwner,
        const SZrFunctionCall *call,
        TZrSize argumentIndex) {
    const SZrSemanticReferenceFact *callReference =
            ownership_move_find_call_reference(context, callOwner);
    const SZrSemanticSymbolRecord *symbol;
    SZrAstNodeArray *parameters;
    SZrParameter *variadic;

    if (callReference == ZR_NULL) {
        return ZR_NULL;
    }
    symbol = ownership_move_find_symbol(context, callReference->symbolId);
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }
    parameters = ownership_move_callable_parameters(symbol->astNode, &variadic);
    return ownership_move_parameter_at(parameters, variadic, call, argumentIndex);
}

/* 唯有 VALUE 形参会消费直接传入的 unique 值，引用类参数由别处保留别名语义。 */
static TZrBool ownership_move_argument_is_by_value(const SZrSemanticContext *context,
                                                    SZrAstNode *callOwner,
                                                    const SZrFunctionCall *call,
                                                    TZrSize argumentIndex) {
    const SZrParameter *parameter = ownership_move_argument_parameter(
            context,
            callOwner,
            call,
            argumentIndex);
    return parameter != ZR_NULL && parameter->passingMode == ZR_PARAMETER_PASSING_MODE_VALUE;
}

/* 调度器契约以 canonical role 标识唯一被消费的首个实参，不依赖成员拼写。 */
static TZrBool ownership_move_argument_is_consumed_by_contract(
        const SZrSemanticContext *context,
        const SZrAstNode *memberNode,
        TZrSize argumentIndex) {
    const SZrSemanticReferenceFact *callReference =
            ownership_move_find_member_call_reference(context, memberNode);

    return callReference != ZR_NULL &&
           callReference->contractRole == ZR_MEMBER_CONTRACT_ROLE_TASK_SCHEDULER_SCHEDULE &&
           argumentIndex == 0U;
}

/* weak 值作为借用或源码引用形参传递时，接收方观察的是可能已失效的别名。 */
static TZrBool ownership_move_argument_requires_weak_wake(
        const SZrSemanticContext *context,
        SZrAstNode *callOwner,
        const SZrFunctionCall *call,
        TZrSize argumentIndex) {
    const SZrParameter *parameter = ownership_move_argument_parameter(
            context,
            callOwner,
            call,
            argumentIndex);
    return parameter != ZR_NULL &&
           ((parameter->typeInfo != ZR_NULL &&
             parameter->typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_IN ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_REF ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_REF_READONLY ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF ||
            parameter->sourcePassingForm == ZR_PARAMETER_SOURCE_SCOPED_REF_READONLY);
}

static TZrBool ownership_move_expression_contains_call(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact);

/* 在 primary 的调用链中找直接传入来源事实的消费实参，并递归检查嵌套实参。 */
static TZrBool ownership_move_primary_contains_call(const SZrSemanticContext *context,
                                                     SZrAstNode *primaryNode,
                                                     const SZrSemanticReferenceFact *fact) {
    SZrAstNodeArray *members;
    TZrSize memberIndex;

    if (primaryNode == ZR_NULL || primaryNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    members = primaryNode->data.primaryExpression.members;
    for (memberIndex = 0; members != ZR_NULL && memberIndex < members->count; memberIndex++) {
        SZrAstNode *member = members->nodes[memberIndex];
        SZrFunctionCall *call;
        SZrAstNode *callMember;
        TZrSize argumentIndex;

        if (member == ZR_NULL || member->type != ZR_AST_FUNCTION_CALL) {
            continue;
        }
        call = &member->data.functionCall;
        callMember = memberIndex > 0U ? members->nodes[memberIndex - 1U] : ZR_NULL;
        for (argumentIndex = 0; call->args != ZR_NULL && argumentIndex < call->args->count;
             argumentIndex++) {
            SZrAstNode *argument = call->args->nodes[argumentIndex];
            /*
             * 这里有两个独立的消费契约：普通 VALUE 形参按声明模式消费来源，
             * scheduler 的 canonical role 则将首参定义为消费项，不依赖参数名或拼写。
             * 两者都只在实参直接引用当前来源时成立；成员 CALL 事实由其属性节点定位。
             */
            if (ownership_move_expression_is_direct_reference(argument, fact) &&
                (ownership_move_argument_is_by_value(context,
                                                     primaryNode,
                                                     call,
                                                     argumentIndex) ||
                 ownership_move_argument_is_consumed_by_contract(context,
                                                                  callMember,
                                                                  argumentIndex))) {
                return ZR_TRUE;
            }
            if (ownership_move_expression_contains_call(context, argument, fact)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool ownership_weak_expression_requires_wake(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact);

/* 读取编译器为接收者节点发布的 weak-wake guard，判断调用前是否已有有效唤醒。 */
static TZrBool ownership_weak_primary_has_receiver_guard(
        const SZrSemanticContext *context,
        const SZrAstNodeArray *members,
        TZrSize callIndex) {
    SZrAstNode *guardedSegment;
    const SZrReceiverGuardFact *guard;

    if (context == ZR_NULL || members == ZR_NULL ||
        members->nodes == ZR_NULL || callIndex >= members->count) {
        return ZR_FALSE;
    }
    guardedSegment = callIndex > 0u ? members->nodes[callIndex - 1u]
                                    : members->nodes[callIndex];
    guard = ZrParser_SemanticFacts_FindReceiverGuardByNode(
            context, guardedSegment);
    return guard != ZR_NULL && guard->kind == ZR_RECEIVER_GUARD_WEAK_WAKE;
}

/* 未受 guard 的 weak receiver 调用和传给借用形参的 weak 实参都要求 wake。 */
static TZrBool ownership_weak_primary_requires_wake(
        const SZrSemanticContext *context,
        SZrAstNode *primaryNode,
        const SZrSemanticReferenceFact *fact) {
    SZrAstNodeArray *members;
    TZrSize memberIndex;

    if (primaryNode == ZR_NULL || primaryNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }
    members = primaryNode->data.primaryExpression.members;
    for (memberIndex = 0; members != ZR_NULL && memberIndex < members->count; memberIndex++) {
        SZrAstNode *member = members->nodes[memberIndex];
        SZrFunctionCall *call;
        TZrSize argumentIndex;

        if (member != ZR_NULL &&
            member->type == ZR_AST_FUNCTION_CALL &&
            ownership_move_node_contains_fact(
                    primaryNode->data.primaryExpression.property,
                    fact) &&
            !ownership_weak_primary_has_receiver_guard(
                    context, members, memberIndex)) {
            return ZR_TRUE;
        }
        if (member == ZR_NULL || member->type != ZR_AST_FUNCTION_CALL) {
            continue;
        }
        call = &member->data.functionCall;
        for (argumentIndex = 0; call->args != ZR_NULL && argumentIndex < call->args->count;
             argumentIndex++) {
            SZrAstNode *argument = call->args->nodes[argumentIndex];
            if (ownership_move_expression_is_direct_reference(argument, fact) &&
                ownership_move_argument_requires_weak_wake(context,
                                                              primaryNode,
                                                              call,
                                                              argumentIndex)) {
                return ZR_TRUE;
            }
            if (ownership_weak_expression_requires_wake(context, argument, fact)) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

/*
 * 在表达式树中汇总需要 wake 的 weak 读取；显式 wake 包住同一事实时终止待唤醒判定。
 * BUG: 合法形状为 `resource class Resource {}; struct WatcherBox { pub @constructor(watcher: ref Weak<Resource>) {} };`
 * 与 `fn use(owner: Shared<Resource>) { var weakAlias = degrade(owner); drop(owner); init WatcherBox(ref weakAlias); }`。
 * 构造器只接收且不保存该借用；Weak<Resource> 保留 weak qualifier，形参声明为 REF，实参是非临时变量。
 * parse_type 识别 Weak 包装器，参数解析保存 REF passingMode，init 参数解析保存调用端 ref marker。
 * struct constructor contract 从声明类型和 passingMode 构造；值构造 binder 以实参 typeId 与 REF marker 匹配。
 * type inference 将 STRUCT_INIT 的 target 设为类型名节点，并为构造实参发布 argument mapping。
 * 当前 walker 没有 STRUCT_INIT 分支；release 已先发生，driver 仅在 wake 分类为真时记违规。
 * 证据：parser_types.c:82-105,1488-1566、parser_call_arguments.c:4-24、parser_struct_init.c:65-97、
 * compiler_struct.c:133-176,320-403、compile_expression_value_construct.c:5-22,430-464、
 * canonical_type_adapter.c:405-428、canonical_type_definition.c:928-935、
 * type_inference_call_semantic_facts.c:841-913,915-1053、dataflow_ownership.c:324-387。
 * test_ownership_diagnostics_region_cases.h:388-393 只覆盖普通 weak/ref owner-release；无构造器组合 fixture，本次未运行测试。
 */
static TZrBool ownership_weak_expression_requires_wake(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (expression->type) {
        case ZR_AST_PRIMARY_EXPRESSION:
            return ownership_weak_primary_requires_wake(context, expression, fact);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ownership_weak_expression_requires_wake(
                           context, expression->data.assignmentExpression.left, fact) ||
                   ownership_weak_expression_requires_wake(
                           context, expression->data.assignmentExpression.right, fact);
        case ZR_AST_BINARY_EXPRESSION:
            return ownership_weak_expression_requires_wake(
                           context, expression->data.binaryExpression.left, fact) ||
                   ownership_weak_expression_requires_wake(
                           context, expression->data.binaryExpression.right, fact);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ownership_weak_expression_requires_wake(
                           context, expression->data.logicalExpression.left, fact) ||
                   ownership_weak_expression_requires_wake(
                           context, expression->data.logicalExpression.right, fact);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ownership_weak_expression_requires_wake(
                           context, expression->data.conditionalExpression.test, fact) ||
                   ownership_weak_expression_requires_wake(
                           context, expression->data.conditionalExpression.consequent, fact) ||
                   ownership_weak_expression_requires_wake(
                           context, expression->data.conditionalExpression.alternate, fact);
        case ZR_AST_UNARY_EXPRESSION:
            return ownership_weak_expression_requires_wake(
                    context, expression->data.unaryExpression.argument, fact);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ownership_weak_expression_requires_wake(
                    context, expression->data.typeCastExpression.expression, fact);
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
            /*
             * 显式 wake 把同一读事实归入受控唤醒路径；对此分支直接返回“不要求 wake”，
             * 避免继续把该事实分类为待唤醒。是否仍有 owner-release 违规由 CFG 状态另行判断。
             */
            if (expression->data.ownershipIntrinsicExpression.operation ==
                    ZR_OWNERSHIP_INTRINSIC_WAKE &&
                ownership_move_node_contains_fact(
                        expression->data.ownershipIntrinsicExpression.argument,
                        fact)) {
                return ZR_FALSE;
            }
            return ownership_weak_expression_requires_wake(
                    context,
                    expression->data.ownershipIntrinsicExpression.argument,
                    fact);
        default:
            return ZR_FALSE;
    }
}

/*
 * 递归识别表达式中会消耗来源的调用或 ownership intrinsic；只分类，不改写语义事实。
 * BUG: `resource class Resource {}; class Box { pub @constructor(value: Unique<Resource>) { drop(value); } }`
 * 配合 `fn use(owner: Unique<Resource>) { var box = new Box(owner); drop(owner); }` 是合法 boxed-class 形状；
 * `struct ValueBox { pub @constructor(value: Unique<Resource>) { drop(value); } }` 加 `init ValueBox(owner)` 同型。
 * 两个构造器形参默认 VALUE，owner 与形参同为 Unique<Resource>；new 目标为普通 boxed class，init 使用可构造 struct。
 * 声明和构造实参都保留 Unique qualifier；init contract 按同一 typeId 和 NONE marker 匹配，不存在 ownership mismatch。
 * type inference 为构造 AST 发布 CALL 事实和实参映射；本递归没有 CONSTRUCT 或 STRUCT_INIT 分支。
 * 所以构造实参 read 不会置 MOVED；后续 `drop(owner)` 的 read 可能漏报 use_after_move，且无独立构造 move fallback。
 * 证据：parser_types.c:82-105、compiler_class.c:1200-1234、type_inference_core.c:5001-5058、
 * compile_expression.c:2715-2723、compile_expression_call.c:1297-1371、compiler_struct.c:133-176,320-403、
 * compile_expression_value_construct.c:419-464、canonical_type_definition.c:928-935、
 * type_inference_call_semantic_facts.c:886-913,915-1053、dataflow_ownership.c:319-411、
 * compiler/compiler_semantic_query_diagnostics.c:112,118、semantic_analyzer_analysis.c:224、
 * semantic_analyzer_query_diagnostics.c:165。
 * test_type_inference.c:4864-4871 与 test_value_type_runtime.c:332-340 仅分别覆盖常规 new/init 构造。
 * test_ownership_diagnostics.c:520-598 覆盖普通 VALUE Unique 移动；无构造器组合 fixture，本次未运行测试。
 */
static TZrBool ownership_move_expression_contains_call(
        const SZrSemanticContext *context,
        SZrAstNode *expression,
        const SZrSemanticReferenceFact *fact) {
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (expression->type) {
        case ZR_AST_PRIMARY_EXPRESSION:
            return ownership_move_primary_contains_call(context, expression, fact);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.assignmentExpression.left, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.assignmentExpression.right, fact);
        case ZR_AST_BINARY_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.binaryExpression.left, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.binaryExpression.right, fact);
        case ZR_AST_LOGICAL_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.logicalExpression.left, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.logicalExpression.right, fact);
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return ownership_move_expression_contains_call(
                           context, expression->data.conditionalExpression.test, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.conditionalExpression.consequent, fact) ||
                   ownership_move_expression_contains_call(
                           context, expression->data.conditionalExpression.alternate, fact);
        case ZR_AST_UNARY_EXPRESSION:
            return ownership_move_expression_contains_call(
                    context, expression->data.unaryExpression.argument, fact);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ownership_move_expression_contains_call(
                    context, expression->data.typeCastExpression.expression, fact);
        case ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION:
            if ((expression->data.ownershipIntrinsicExpression.operation ==
                         ZR_OWNERSHIP_INTRINSIC_SHARE ||
                 expression->data.ownershipIntrinsicExpression.operation ==
                         ZR_OWNERSHIP_INTRINSIC_INTO_GC ||
                 expression->data.ownershipIntrinsicExpression.operation ==
                         ZR_OWNERSHIP_INTRINSIC_DROP) &&
                ownership_move_node_contains_fact(
                        expression->data.ownershipIntrinsicExpression.argument,
                        fact)) {
                return ZR_TRUE;
            }
            return ownership_move_expression_contains_call(
                    context,
                    expression->data.ownershipIntrinsicExpression.argument,
                    fact);
        default:
            return ZR_FALSE;
    }
}

/* LHS 已写回同一符号时是自赋值，不把 RHS 的直接引用误判为所有权转移。 */
static TZrBool ownership_move_assignment_targets_source(const SZrSemanticContext *context,
                                                         SZrAstNode *target,
                                                         TZrSymbolId sourceSymbolId) {
    TZrSize index;

    if (context == ZR_NULL || target == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *candidate =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        if (candidate != ZR_NULL &&
            candidate->kind == ZR_SEMANTIC_REFERENCE_WRITE &&
            candidate->symbolId == sourceSymbolId &&
            ownership_move_node_contains_fact(target, candidate)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 仅提取这些语句携带的值表达式；结构控制语句不在此处展开。 */
static SZrAstNode *ownership_move_statement_expression(SZrAstNode *statement) {
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

/* 只读分类语句中的来源消费；返回值由所有权驱动结合 qualifier 应用状态转移。 */
TZrBool ZrParser_DataflowOwnership_StatementMovesRead(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    SZrAstNode *expression;

    if (context == ZR_NULL || statement == ZR_NULL || fact == ZR_NULL) {
        return ZR_FALSE;
    }
    if (statement->type == ZR_AST_VARIABLE_DECLARATION &&
        ownership_move_expression_is_direct_reference(
                statement->data.variableDeclaration.value,
                fact)) {
        return ZR_TRUE;
    }

    expression = ownership_move_statement_expression(statement);
    if (expression != ZR_NULL &&
        expression->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
        expression->data.assignmentExpression.op.op != ZR_NULL &&
        strcmp(expression->data.assignmentExpression.op.op, "=") == 0 &&
        ownership_move_expression_is_direct_reference(
                expression->data.assignmentExpression.right,
                fact) &&
        !ownership_move_assignment_targets_source(
                context,
                expression->data.assignmentExpression.left,
                fact->symbolId)) {
        return ZR_TRUE;
    }
    return ownership_move_expression_contains_call(context, expression, fact);
}

/* 只读分类待唤醒的 weak 读取；违规与 owner 状态仍由 CFG 所有权驱动发布。 */
TZrBool ZrParser_DataflowOwnership_StatementWeakReadRequiresWake(
        const SZrSemanticContext *context,
        SZrAstNode *statement,
        const SZrSemanticReferenceFact *fact) {
    return ownership_weak_expression_requires_wake(
            context,
            ownership_move_statement_expression(statement),
            fact);
}
