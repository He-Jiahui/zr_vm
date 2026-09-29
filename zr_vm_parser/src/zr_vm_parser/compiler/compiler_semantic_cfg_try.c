#include "compiler_internal.h"
#include "type_inference_internal.h"

/**
 * @brief 从 catch clause 取出唯一参数节点，供 try 能力预检核对绑定名与类型。
 * @return 仅当 catch pattern 恰有一个节点时返回该节点；复杂 destructuring 由上层回退。
 */
static const SZrAstNode *compiler_semantic_cfg_catch_parameter(
        const SZrAstNode *catchClause) {
    if (catchClause == ZR_NULL ||
        catchClause->type != ZR_AST_CATCH_CLAUSE ||
        catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_NULL;
    }
    return catchClause->data.catchClause.pattern->nodes[0];
}

/**
 * @brief 识别没有运行时语句的 catch block，作为安全的空 handler 形态。
 * @return 仅 AST_BLOCK 且 body 为空或无元素时返回 true。
 */
static TZrBool compiler_semantic_cfg_is_empty_block(
        const SZrAstNode *node) {
    return (TZrBool)(node != ZR_NULL && node->type == ZR_AST_BLOCK &&
                    (node->data.block.body == ZR_NULL ||
                     node->data.block.body->count == 0U));
}

/**
 * @brief 从标识符或不带 postfix member 的 primary 中取纯名称。
 * @note 属性访问、调用和带成员链的 primary 不视为单一 catch/local 绑定读取。
 */
static SZrString *compiler_semantic_cfg_simple_identifier_name(
        const SZrAstNode *node) {
    const SZrAstNode *identifier = node;

    if (identifier != ZR_NULL &&
        identifier->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (identifier->data.primaryExpression.members != ZR_NULL &&
            identifier->data.primaryExpression.members->count != 0U) {
            return ZR_NULL;
        }
        identifier = identifier->data.primaryExpression.property;
    }
    if (identifier == ZR_NULL ||
        identifier->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }
    return identifier->data.identifier.name;
}

/**
 * @brief 判断单条表达式语句是否只读取指定 catch 参数绑定。
 * @pre bindingName 是当前 catch 参数名；不允许将成员访问或更大表达式缩减成绑定读取。
 */
static TZrBool compiler_semantic_cfg_is_catch_binding_read(
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrAstNode *expression;
    SZrString *identifierName;

    if (node == ZR_NULL || node->type != ZR_AST_EXPRESSION_STATEMENT ||
        bindingName == ZR_NULL) {
        return ZR_FALSE;
    }
    expression = node->data.expressionStatement.expr;
    if (expression == ZR_NULL) {
        return ZR_FALSE;
    }
    identifierName = compiler_semantic_cfg_simple_identifier_name(expression);
    return (TZrBool)(identifierName != ZR_NULL &&
                    ZrCore_String_Equal(identifierName, bindingName));
}

/**
 * @brief 确认名称已映射到有 place 且当前有值的局部 SemanticIR slot。
 * @note 供 prior-local/call-assignment 白名单排除未初始化或仅 legacy bytecode 的位置。
 */
static TZrBool compiler_semantic_cfg_has_initialized_local(
        SZrCompilerState *cs, SZrString *name) {
    TZrUInt32 stackSlot;
    SZrCompilerSemanticIrSlot *slot;

    if (cs == ZR_NULL || name == ZR_NULL ||
        (stackSlot = find_local_var(cs, name)) == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    slot = compiler_semantic_ir_find_slot(cs, stackSlot);
    return (TZrBool)(slot != ZR_NULL &&
                    slot->placeId != ZR_PLACE_ID_INVALID &&
                    slot->valueId != ZR_VALUE_ID_INVALID);
}

/**
 * @brief 识别 catch block 中对已初始化、且不同于 catch 参数的单一局部读取。
 * @return 仅有真实可读取 slot 的简单标识符表达式通过；其他 handler 结构由外层拒绝。
 */
static TZrBool compiler_semantic_cfg_is_prior_local_read(
        SZrCompilerState *cs, const SZrAstNode *node,
        SZrString *bindingName) {
    SZrString *name;

    if (node == ZR_NULL || node->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_FALSE;
    }
    name = compiler_semantic_cfg_simple_identifier_name(
            node->data.expressionStatement.expr);
    return (TZrBool)(name != ZR_NULL && bindingName != ZR_NULL &&
                    !ZrCore_String_Equal(name, bindingName) &&
                    compiler_semantic_cfg_has_initialized_local(cs, name));
}

/**
 * @brief 限定可静态判定的 catch return 结果为 void、字面值或原 catch 绑定。
 * @note 复杂表达式可能含额外控制流/调用副作用，不用作当前直接 abrupt handler 快捷路径。
 */
static TZrBool compiler_semantic_cfg_is_canonical_catch_return(
        const SZrAstNode *expression,
        SZrString *bindingName) {
    SZrString *identifierName;

    if (expression == ZR_NULL) {
        return ZR_TRUE;
    }
    switch (expression->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return ZR_TRUE;
        default:
            break;
    }
    identifierName = compiler_semantic_cfg_simple_identifier_name(
            expression);
    return (TZrBool)(identifierName != ZR_NULL &&
                    bindingName != ZR_NULL &&
                    ZrCore_String_Equal(identifierName, bindingName));
}

/**
 * @brief 识别仅由一条 canonical return 组成的直接 catch handler。
 * @pre node 是 catch block；嵌套或多语句控制流不属于此 abrupt 形态。
 */
static TZrBool compiler_semantic_cfg_is_direct_catch_return(
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrAstNode *statement;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL ||
        node->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    statement = node->data.block.body->nodes[0];
    return (TZrBool)(statement != ZR_NULL &&
                    statement->type == ZR_AST_RETURN_STATEMENT &&
                    compiler_semantic_cfg_is_canonical_catch_return(
                            statement->data.returnStatement.expr,
                            bindingName));
}

/**
 * @brief 识别单语句直接 catch return，或直接 throw 回 catch 参数的 handler。
 * @note 该窄形态用于终止性预判；它不替代一般 catch body flow 分析。
 */
static TZrBool compiler_semantic_cfg_is_direct_catch_abrupt(
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrAstNode *statement;
    SZrString *identifierName;

    if (node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL ||
        node->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    statement = node->data.block.body->nodes[0];
    if (statement == ZR_NULL) {
        return ZR_FALSE;
    }
    if (compiler_semantic_cfg_is_direct_catch_return(
                node, bindingName)) {
        return ZR_TRUE;
    }
    if (statement->type != ZR_AST_THROW_STATEMENT ||
        statement->data.throwStatement.expr == ZR_NULL) {
        return ZR_FALSE;
    }
    identifierName = compiler_semantic_cfg_simple_identifier_name(
            statement->data.throwStatement.expr);
    return (TZrBool)(identifierName != ZR_NULL &&
                    bindingName != ZR_NULL &&
                    ZrCore_String_Equal(identifierName, bindingName));
}

/**
 * @brief 确认名称未被运行时变量、普通/编译时函数或 compiler type prototype 占用。
 * @note catch-local alias 预检据此区分其特殊绑定形态与已有外部名称解析。
 */
static TZrBool compiler_semantic_cfg_identifier_name_is_unbound(
        SZrCompilerState *cs,
        SZrString *name) {
    SZrFunctionTypeInfo *functionInfo = ZR_NULL;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL ||
        ZrParser_TypeEnvironment_FindVariableBinding(
                cs->typeEnv, name) != ZR_NULL ||
        ZrParser_TypeEnvironment_LookupFunction(
                cs->typeEnv, name, &functionInfo)) {
        return ZR_FALSE;
    }
    functionInfo = ZR_NULL;
    if (cs->compileTimeTypeEnv != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupFunction(
                cs->compileTimeTypeEnv, name, &functionInfo)) {
        return ZR_FALSE;
    }
    return (TZrBool)(find_compiler_type_prototype_inference(cs, name) ==
                    ZR_NULL);
}

/**
 * @brief 识别受限的 catch 参数别名流程：声明一个简单局部，再单独读取它。
 * @pre block 恰含两条语句；名称未与现存变量/函数/type prototype 冲突，initializer 直接来自 catch 绑定。
 * @return 仅接受无显式类型、非 const、private 的局部声明及其随后读取。
 */
static TZrBool compiler_semantic_cfg_is_catch_local_flow(
        SZrCompilerState *cs,
        const SZrAstNode *node,
        SZrString *bindingName) {
    const SZrVariableDeclaration *declaration;
    SZrString *localName;
    SZrString *initializerName;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL ||
        node == ZR_NULL || node->type != ZR_AST_BLOCK ||
        node->data.block.body == ZR_NULL ||
        node->data.block.body->count != 2U ||
        bindingName == ZR_NULL ||
        node->data.block.body->nodes[0] == ZR_NULL ||
        node->data.block.body->nodes[0]->type !=
                ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }
    declaration =
            &node->data.block.body->nodes[0]->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        declaration->typeInfo != ZR_NULL || declaration->value == ZR_NULL ||
        declaration->isConst ||
        declaration->accessModifier != ZR_ACCESS_PRIVATE) {
        return ZR_FALSE;
    }
    localName = declaration->pattern->data.identifier.name;
    initializerName = compiler_semantic_cfg_simple_identifier_name(
            declaration->value);
    return (TZrBool)(localName != ZR_NULL && initializerName != ZR_NULL &&
                    !ZrCore_String_Equal(localName, bindingName) &&
                    compiler_semantic_cfg_identifier_name_is_unbound(
                            cs, bindingName) &&
                    compiler_semantic_cfg_identifier_name_is_unbound(
                            cs, localName) &&
                    ZrCore_String_Equal(initializerName, bindingName) &&
                    compiler_semantic_cfg_is_catch_binding_read(
                            node->data.block.body->nodes[1], localName));
}

/**
 * @brief 以有限白名单判断 catch handler 是否能安全纳入当前语义 CFG lowering。
 * @note 支持空块、简单绑定/先前局部读取、受限 alias 流程和可证明的直接 abrupt；abrupt 快捷路径要求无活动 ownership cleanup，函数体内还拒绝直接 return。
 */
static TZrBool compiler_semantic_cfg_is_supported_catch_block(
        SZrCompilerState *cs,
        const SZrAstNode *node,
        SZrString *bindingName) {
    if (compiler_semantic_cfg_is_empty_block(node)) {
        return ZR_TRUE;
    }
    if (node != ZR_NULL && node->type == ZR_AST_BLOCK &&
        node->data.block.body != ZR_NULL &&
        node->data.block.body->count == 1U &&
        (compiler_semantic_cfg_is_catch_binding_read(
                 node->data.block.body->nodes[0], bindingName) ||
         compiler_semantic_cfg_is_prior_local_read(
                 cs, node->data.block.body->nodes[0], bindingName))) {
        return ZR_TRUE;
    }
    if (compiler_semantic_cfg_is_direct_catch_abrupt(
                node, bindingName)) {
        return (TZrBool)(
                !compiler_has_active_scope_ownership_cleanups(cs) &&
                !(cs->currentFunctionNode != ZR_NULL &&
                  compiler_semantic_cfg_is_direct_catch_return(
                          node, bindingName)));
    }
    return compiler_semantic_cfg_is_catch_local_flow(
            cs, node, bindingName);
}

/**
 * @brief 告诉 try compiler 某个 catch handler 是否以受支持的直接 return/throw 终止。
 * @pre node 是 try/catch/finally AST，catchIndex 指向有唯一具名参数的 clause。
 * @return 只识别单语句直接 abrupt handler；false 表示没有命中此专用终止预判，不代表语法错误或必然落出。
 */
TZrBool compiler_semantic_cfg_try_catch_handler_terminates(
        const SZrAstNode *node,
        TZrSize catchIndex) {
    const SZrTryCatchFinallyStatement *statement;
    const SZrAstNode *catchClause;
    const SZrAstNode *parameter;

    if (node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    if (statement->catchClauses == ZR_NULL ||
        catchIndex >= statement->catchClauses->count) {
        return ZR_FALSE;
    }
    catchClause = statement->catchClauses->nodes[catchIndex];
    if (catchClause == ZR_NULL ||
        catchClause->type != ZR_AST_CATCH_CLAUSE ||
        catchClause->data.catchClause.pattern == ZR_NULL ||
        catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_FALSE;
    }
    parameter = catchClause->data.catchClause.pattern->nodes[0];
    return (TZrBool)(parameter != ZR_NULL &&
                    parameter->type == ZR_AST_PARAMETER &&
                    parameter->data.parameter.name != ZR_NULL &&
                    parameter->data.parameter.name->name != ZR_NULL &&
                    compiler_semantic_cfg_is_direct_catch_abrupt(
                            catchClause->data.catchClause.block,
                            parameter->data.parameter.name->name));
}

/**
 * @brief 检查参数是否为简单标识符或数值/布尔字面值，而无 postfix 求值链。
 * @note 用于选择可证明的直接调用形态；完整类型与运行时 value producer 另由 exactness 检查核对。
 */
static TZrBool compiler_semantic_cfg_is_simple_value_argument(
        const SZrAstNode *node) {
    const SZrAstNode *value = node;

    if (value != ZR_NULL && value->type == ZR_AST_PRIMARY_EXPRESSION) {
        if (value->data.primaryExpression.members != ZR_NULL &&
            value->data.primaryExpression.members->count != 0U) {
            return ZR_FALSE;
        }
        value = value->data.primaryExpression.property;
    }
    return (TZrBool)(
            value != ZR_NULL &&
            (value->type == ZR_AST_IDENTIFIER_LITERAL ||
             value->type == ZR_AST_INTEGER_LITERAL ||
             value->type == ZR_AST_BOOLEAN_LITERAL ||
             value->type == ZR_AST_FLOAT_LITERAL));
}

/**
 * @brief 对直接调用的语法参数做粗粒度白名单筛选，排除 named/特殊 marker 和复杂表达式。
 * @return 最多三个简单 value 参数且 marker 对齐并为普通参数时返回 true。
 */
static TZrBool compiler_semantic_cfg_call_has_supported_arguments(
        const SZrFunctionCall *call) {
    TZrSize index;

    if (call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (call->args == ZR_NULL || call->args->count == 0U) {
        return (TZrBool)(call->argumentMarkers == ZR_NULL ||
                        call->argumentMarkers->length == 0U);
    }
    if (call->args->count > 3U ||
        (call->argumentMarkers != ZR_NULL &&
         call->argumentMarkers->length != call->args->count)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < call->args->count; index++) {
        const SZrCallArgumentSyntax *syntax = ZR_NULL;

        if (!compiler_semantic_cfg_is_simple_value_argument(
                    call->args->nodes[index])) {
            return ZR_FALSE;
        }
        if (call->argumentMarkers != ZR_NULL) {
            syntax = (const SZrCallArgumentSyntax *)ZrCore_Array_Get(
                    call->argumentMarkers, index);
            if (syntax == ZR_NULL ||
                syntax->marker != ZR_CALL_ARGUMENT_MARKER_NONE) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 返回可供异常边建模的直接 identifier(...) call 节点。
 * @pre node 是 primary expression；只支持一个 direct call member、普通参数、非泛型且无 named args。
 * @return 仅限调用形状通过语法预检时返回 call AST；成员/optional/链式调用返回 null。
 */
const SZrAstNode *compiler_semantic_cfg_supported_direct_call(
        const SZrAstNode *node) {
    const SZrAstNode *callNode;
    const SZrFunctionCall *call;

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        node->data.primaryExpression.property == ZR_NULL ||
        node->data.primaryExpression.property->type !=
                ZR_AST_IDENTIFIER_LITERAL ||
        node->data.primaryExpression.members == ZR_NULL ||
        node->data.primaryExpression.members->count != 1U) {
        return ZR_NULL;
    }
    callNode = node->data.primaryExpression.members->nodes[0];
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_NULL;
    }
    call = &callNode->data.functionCall;
    if (!compiler_semantic_cfg_call_has_supported_arguments(call) ||
        (call->genericArguments != ZR_NULL &&
         call->genericArguments->count != 0U) ||
        call->hasNamedArgs ||
        call->accessMode != ZR_POSTFIX_ACCESS_DIRECT) {
        return ZR_NULL;
    }
    return callNode;
}

/**
 * @brief 识别“已初始化局部 = 受支持直接调用”，供 try/finally 将调用结果与目标写入关联。
 * @pre expression 为普通赋值且左侧已登记为可读局部；赋值右侧仍需通过 direct-call 白名单。
 * @return 成功时返回右侧 call AST，其他赋值形式返回 null。
 */
const SZrAstNode *compiler_semantic_cfg_supported_local_assignment_call(
        SZrCompilerState *cs, const SZrAstNode *expression) {
    const SZrAstNode *left;

    if (expression == ZR_NULL ||
        expression->type != ZR_AST_ASSIGNMENT_EXPRESSION ||
        expression->data.assignmentExpression.op.op == ZR_NULL ||
        strcmp(expression->data.assignmentExpression.op.op, "=") != 0) {
        return ZR_NULL;
    }
    left = expression->data.assignmentExpression.left;
    if (left == ZR_NULL || left->type != ZR_AST_IDENTIFIER_LITERAL ||
        !compiler_semantic_cfg_has_initialized_local(
                cs, left->data.identifier.name)) {
        return ZR_NULL;
    }
    return compiler_semantic_cfg_supported_direct_call(
            expression->data.assignmentExpression.right);
}

/**
 * @brief 预检 try/catch 能否由当前 source CFG 路径完整表达，避免半建图后才发现 handler 不受支持。
 * @pre 由 try compiler 在建立 catch CFG 前调用；函数只读 AST/type/local facts，不提交 CFG 状态。
 * @return 仅接受无 finally、单条受支持 protected direct call/赋值调用及可解析 catch handler 的形态。
 * @note catch-all 必须最后出现；失败让调用方放弃已有 CFG 并阻止启动提升，保留 legacy 编译路径。
 */
TZrBool compiler_semantic_cfg_try_catch_is_supported(
        SZrCompilerState *cs,
        const SZrAstNode *node) {
    const SZrTryCatchFinallyStatement *statement;
    const SZrAstNode *protectedStatement;
    const SZrAstNode *protectedCall;
    TZrSize index;

    if (cs == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TRY_CATCH_FINALLY_STATEMENT) {
        return ZR_FALSE;
    }
    statement = &node->data.tryCatchFinallyStatement;
    /* This source-CFG route models catches around one invoke; finally has its
     * own lowering/preflight and mixed try/catch/finally falls back as a unit. */
    if (statement->finallyBlock != ZR_NULL ||
        statement->catchClauses == ZR_NULL ||
        statement->catchClauses->count == 0U ||
        statement->block == ZR_NULL || statement->block->type != ZR_AST_BLOCK ||
        statement->block->data.block.body == ZR_NULL ||
        statement->block->data.block.body->count != 1U) {
        return ZR_FALSE;
    }
    /* Keeping the protected region to one call site gives invoke lowering a
     * single owning exceptional edge and an unambiguous catch entry. */
    protectedStatement = statement->block->data.block.body->nodes[0];
    protectedCall = protectedStatement == ZR_NULL ||
                            protectedStatement->type !=
                                    ZR_AST_EXPRESSION_STATEMENT ||
                            protectedStatement->data.expressionStatement.expr ==
                                    ZR_NULL
                    ? ZR_NULL
                    : compiler_semantic_cfg_supported_direct_call(
                              protectedStatement->data.expressionStatement.expr);
    if (protectedCall == ZR_NULL && protectedStatement != ZR_NULL &&
        protectedStatement->type == ZR_AST_EXPRESSION_STATEMENT) {
        protectedCall = compiler_semantic_cfg_supported_local_assignment_call(
                cs, protectedStatement->data.expressionStatement.expr);
    }
    if (protectedStatement == ZR_NULL ||
        protectedStatement->type != ZR_AST_EXPRESSION_STATEMENT ||
        protectedStatement->data.expressionStatement.expr == ZR_NULL ||
        protectedCall == ZR_NULL ||
        (protectedCall->data.functionCall.args != ZR_NULL &&
         protectedCall->data.functionCall.args->count > 1U)) {
        return ZR_FALSE;
    }
    /* 每个 handler 都须先通过类型、绑定和 body 白名单，catch-all 只能终止 dispatch 列表。 */
    for (index = 0U; index < statement->catchClauses->count; index++) {
        const SZrAstNode *catchClause =
                statement->catchClauses->nodes[index];
        const SZrAstNode *parameter =
                compiler_semantic_cfg_catch_parameter(catchClause);

        if (catchClause == ZR_NULL ||
            catchClause->type != ZR_AST_CATCH_CLAUSE ||
            parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
            parameter->data.parameter.name == ZR_NULL ||
            parameter->data.parameter.name->name == ZR_NULL ||
            !compiler_semantic_cfg_catch_type_is_resolvable(
                    cs, parameter->data.parameter.typeInfo) ||
            !compiler_semantic_cfg_is_supported_catch_block(
                    cs,
                    catchClause->data.catchClause.block,
                    parameter->data.parameter.name->name) ||
            (parameter->data.parameter.typeInfo == ZR_NULL &&
             index + 1U != statement->catchClauses->count)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/**
 * @brief 在活动 catch invoke 建图时证明参数 IR values 与已解析 value-mode 签名逐项一致。
 * @pre call arguments 已编译进 firstArgumentSlot 起始的连续 slots；resolvedSignature 对应同一 callee。
 * @return catch 约束下仅接受非 nullable、无 ownership/bridge/reference qualifier 的数值/布尔值，且其 producer 为 LOAD/CONSTANT。
 * @note 没有活动 catch block 时不增加限制；失败由 call lowering 放弃 source CFG，而不直接报告语言类型错误。
 */
TZrBool compiler_semantic_cfg_try_call_arguments_are_exact(
        SZrCompilerState *cs,
        const SZrFunctionCall *call,
        const SZrResolvedCallSignature *resolvedSignature,
        TZrUInt32 firstArgumentSlot) {
    TZrSize index;
    SZrInferredType actualType;

    if (cs == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cs->preSemanticIrCfgCatchBlock ==
        ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_TRUE;
    }
    if (call->args == ZR_NULL || call->args->count == 0U) {
        return ZR_TRUE;
    }
    if (call->args->count > 3U || resolvedSignature == ZR_NULL ||
        resolvedSignature->parameterTypes.length != call->args->count ||
        resolvedSignature->parameterPassingModes.length != call->args->count ||
        firstArgumentSlot == ZR_PARSER_SLOT_NONE) {
        return ZR_FALSE;
    }
    /* Invoke 分支要求 SSA 参数与解析签名精确匹配，且参数值来自可追踪的 load/constant。 */
    for (index = 0U; index < call->args->count; index++) {
        const SZrInferredType *expectedType =
                (const SZrInferredType *)ZrCore_Array_Get(
                        (SZrArray *)&resolvedSignature->parameterTypes,
                        index);
        const EZrParameterPassingMode *passingMode =
                (const EZrParameterPassingMode *)ZrCore_Array_Get(
                        (SZrArray *)&resolvedSignature->parameterPassingModes,
                        index);
        TZrValueId argumentValueId;
        const SZrSemanticIrValue *argumentValue;
        const SZrSemanticIrInstruction *argumentDefinition;

        if (expectedType == ZR_NULL || passingMode == ZR_NULL ||
            *passingMode != ZR_PARAMETER_PASSING_MODE_VALUE) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Init(
                cs->state, &actualType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrParser_ExpressionType_Infer(
                    cs, call->args->nodes[index], &actualType) ||
            (!ZR_VALUE_IS_TYPE_INT(actualType.baseType) &&
             actualType.baseType != ZR_VALUE_TYPE_BOOL &&
             !ZR_VALUE_IS_TYPE_FLOAT(actualType.baseType)) ||
            actualType.isNullable ||
            actualType.ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
            actualType.gcBridgeKind != ZR_GC_BRIDGE_NONE ||
            actualType.referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
            !ZrParser_InferredType_Equal(&actualType, expectedType)) {
            ZrParser_InferredType_Free(cs->state, &actualType);
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(cs->state, &actualType);

        argumentValueId = compiler_semantic_ir_slot_value(
                cs, firstArgumentSlot + (TZrUInt32)index);
        argumentValue = ZrParser_SemanticIr_Value(
                &cs->preSemanticIr, argumentValueId);
        if (argumentValue == ZR_NULL ||
            argumentValue->definitionInstructionId ==
                    ZR_SEMANTIC_INSTRUCTION_ID_INVALID) {
            return ZR_FALSE;
        }
        argumentDefinition = ZrParser_SemanticIr_InstructionAt(
                &cs->preSemanticIr,
                argumentValue->definitionInstructionId - 1U);
        if (argumentDefinition == ZR_NULL ||
            (argumentDefinition->opcode != ZR_SEMANTIC_IR_LOAD &&
             argumentDefinition->opcode != ZR_SEMANTIC_IR_CONSTANT) ||
            argumentDefinition->resultValueId != argumentValueId) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}
