#include "semantic_scope_facts.h"

#include "zr_vm_parser/semantic_display.h"
#include "zr_vm_parser/semantic_facts.h"

#include <string.h>

/**
 * @brief 保存一次 AST 事实投影所需的语义上下文与全局声明顺序。
 * @note 仅在公开构建入口的栈帧中创建；递归共享序号，便于候选按源码遍历顺序稳定排序。
 */
typedef struct SZrSemanticScopeFactBuilder {
    SZrSemanticContext *context;
    TZrUInt32 nextDeclarationOrder;
} SZrSemanticScopeFactBuilder;

/** @brief 按 AST 节点身份查找已解析的声明引用事实。
 * @return 仅返回有效符号 ID 的已解析声明；其他情况返回 NULL。
 */
static const SZrSemanticReferenceFact *semantic_scope_facts_find_declaration(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);

        if (fact != ZR_NULL && fact->node == node &&
            fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION && fact->isResolved &&
            fact->symbolId != ZR_SEMANTIC_ID_INVALID) {
            return fact;
        }
    }
    return ZR_NULL;
}

/** @brief 查找与 AST 节点对应且已取得有效类型 ID 的类型符号。
 * @return 不匹配或语义符号数组不可用时返回 NULL。
 */
static const SZrSemanticSymbolRecord *semantic_scope_facts_find_type_declaration(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
            symbol->astNode == node && symbol->typeId != ZR_SEMANTIC_ID_INVALID) {
            return symbol;
        }
    }
    return ZR_NULL;
}

/** @brief 按 AST 节点指针查找符号；符号种类由调用方继续筛选。
 * @return 首个节点身份匹配项；未找到时返回 NULL。
 */
static const SZrSemanticSymbolRecord *semantic_scope_facts_find_symbol_by_node(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL && symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

/** @brief 查找节点对应的泛型参数符号，不把普通变量当作参数。
 * @return 仅匹配参数种类和 AST 节点身份的符号。
 */
static const SZrSemanticSymbolRecord *semantic_scope_facts_find_generic_parameter(
        const SZrSemanticContext *context,
        const SZrAstNode *node) {
    TZrSize index;

    if (context == ZR_NULL || node == ZR_NULL || !context->symbols.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);

        if (symbol != ZR_NULL &&
            symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_PARAMETER &&
            symbol->astNode == node) {
            return symbol;
        }
    }
    return ZR_NULL;
}

/** @brief 复制并发布一个带父级、源码范围、所属符号和静态上下文的词法作用域。
 * @note 静态上下文沿父链继承；根作用域使用无效父 ID。
 */
static TZrSemanticScopeId semantic_scope_facts_publish_scope(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId parentScopeId,
        EZrSemanticScopeKind kind,
        SZrFileRange range,
        TZrSymbolId ownerSymbolId,
        TZrBool isStaticContext) {
    SZrSemanticScopeFact fact;
    const SZrSemanticScopeFact *parent;

    if (builder == ZR_NULL || builder->context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    memset(&fact, 0, sizeof(fact));
    fact.parentScopeId = parentScopeId;
    fact.kind = kind;
    fact.range = range;
    fact.ownerSymbolId = ownerSymbolId;
    parent = ZrParser_Semantic_FindScopeFactById(builder->context, parentScopeId);
    fact.isStaticContext = isStaticContext ||
                           (parent != ZR_NULL && parent->isStaticContext);
    return ZrParser_Semantic_PublishScopeFact(builder->context, &fact);
}

/** @brief 从受支持的接收者成员 AST 节点提取访问级别与静态标记。
 * @note 接口成员签名没有静态语法，因此按实例成员处理。
 */
static TZrBool semantic_scope_facts_receiver_member_metadata(
        const SZrAstNode *node,
        EZrAccessModifier *outAccess,
        TZrBool *outIsStatic) {
    if (node == ZR_NULL || outAccess == ZR_NULL || outIsStatic == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_FIELD:
            *outAccess = node->data.structField.access;
            *outIsStatic = node->data.structField.isStatic;
            return ZR_TRUE;
        case ZR_AST_CLASS_FIELD:
            *outAccess = node->data.classField.access;
            *outIsStatic = node->data.classField.isStatic;
            return ZR_TRUE;
        case ZR_AST_INTERFACE_FIELD_DECLARATION:
            *outAccess = node->data.interfaceFieldDeclaration.access;
            *outIsStatic = ZR_FALSE;
            return ZR_TRUE;
        case ZR_AST_STRUCT_METHOD:
            *outAccess = node->data.structMethod.access;
            *outIsStatic = node->data.structMethod.isStatic;
            return ZR_TRUE;
        case ZR_AST_CLASS_METHOD:
            *outAccess = node->data.classMethod.access;
            *outIsStatic = node->data.classMethod.isStatic;
            return ZR_TRUE;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            *outAccess = node->data.interfaceMethodSignature.access;
            *outIsStatic = ZR_FALSE;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

/** @brief 把已有字段或函数符号投影为所属类型作用域中的接收者成员。
 * @note 没有对应符号或符号种类不受支持时跳过，不按成员名称创建绑定。
 */
static TZrBool semantic_scope_facts_publish_receiver_member(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        const SZrAstNode *node,
        TZrSymbolId ownerSymbolId) {
    const SZrSemanticSymbolRecord *symbol;
    SZrSemanticVisibleSymbolFact fact;
    EZrAccessModifier access;
    TZrBool isStatic;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID ||
        !semantic_scope_facts_receiver_member_metadata(node, &access, &isStatic)) {
        return ZR_TRUE;
    }
    symbol = semantic_scope_facts_find_symbol_by_node(builder->context, node);
    if (symbol == ZR_NULL ||
        (symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FIELD &&
         symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION)) {
        return ZR_TRUE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = symbol->id;
    fact.ownerSymbolId = ownerSymbolId;
    fact.access = access;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = symbol->location;
    fact.definitionRange = symbol->location;
    fact.hasDefinitionRange = ZR_TRUE;
    fact.isHoisted = ZR_TRUE;
    fact.isAccessible = ZR_TRUE;
    fact.isReceiverMember = ZR_TRUE;
    fact.isStatic = isStatic;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

/** @brief 判断变量节点是否为简单标识符直接接收 import 表达式。
 * @return 仅该语法形状返回 true；解构导入由变量发布逻辑单独处理。
 */
static TZrBool semantic_scope_facts_is_direct_import_alias(
        const SZrAstNode *node) {
    const SZrVariableDeclaration *declaration;

    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &node->data.variableDeclaration;
    return declaration->pattern != ZR_NULL &&
           declaration->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
           declaration->pattern->data.identifier.name != ZR_NULL &&
           declaration->value != ZR_NULL &&
           declaration->value->type == ZR_AST_IMPORT_EXPRESSION;
}

/** @brief 判断简单变量是否直接绑定类型字面量表达式。
 * @return 仅用于标记别名，不在此处创建或解析类型符号。
 */
static TZrBool semantic_scope_facts_is_type_value_alias(
        const SZrAstNode *node) {
    const SZrVariableDeclaration *declaration;

    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &node->data.variableDeclaration;
    return declaration->pattern != ZR_NULL &&
           declaration->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
           declaration->pattern->data.identifier.name != ZR_NULL &&
           declaration->value != ZR_NULL &&
           declaration->value->type == ZR_AST_TYPE_LITERAL_EXPRESSION;
}

/** @brief 从直接导入变量的字符串模块路径取得来源 URI。
 * @return 返回 AST 持有的字符串指针；发布可见事实时再由语义上下文复制。
 */
static SZrString *semantic_scope_facts_import_origin_uri(
        const SZrAstNode *node) {
    const SZrVariableDeclaration *declaration;
    const SZrAstNode *modulePath;

    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_NULL;
    }
    declaration = &node->data.variableDeclaration;
    if (declaration->value == ZR_NULL ||
        declaration->value->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_NULL;
    }
    modulePath = declaration->value->data.importExpression.modulePath;
    return modulePath != ZR_NULL && modulePath->type == ZR_AST_STRING_LITERAL
            ? modulePath->data.stringLiteral.value
            : ZR_NULL;
}

/** @brief 读取直接导入模块字符串的 AST 范围，并在失败时清零输出。
 * @return 仅字符串字面量模块路径提供来源范围。
 */
static TZrBool semantic_scope_facts_import_origin_range(
        const SZrAstNode *node,
        SZrFileRange *outRange) {
    const SZrVariableDeclaration *declaration;
    const SZrAstNode *modulePath;

    if (outRange != ZR_NULL) {
        memset(outRange, 0, sizeof(*outRange));
    }
    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION ||
        outRange == ZR_NULL) {
        return ZR_FALSE;
    }
    declaration = &node->data.variableDeclaration;
    if (declaration->value == ZR_NULL ||
        declaration->value->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_FALSE;
    }
    modulePath = declaration->value->data.importExpression.modulePath;
    if (modulePath == ZR_NULL || modulePath->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }
    *outRange = modulePath->location;
    return ZR_TRUE;
}

/** @brief 将节点已有的已解析声明事实复制为指定作用域中的可见符号事实。
 * @return 无对应声明事实或符号时按无候选成功处理；无效输入或发布失败返回 false。
 * @note 函数签名优先取当前语义上下文生成的显示串；来源 URI 的复制由发布 API 负责。
 */
static TZrBool semantic_scope_facts_publish_reference_declaration(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        const SZrAstNode *node,
        TZrSymbolId ownerSymbolId,
        TZrBool isHoisted,
        TZrBool isImport,
        TZrBool isAlias,
        SZrString *externalOriginUri,
        SZrFileRange externalOriginRange,
        TZrBool hasExternalOriginRange) {
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticSymbolRecord *symbol;
    SZrSemanticVisibleSymbolFact fact;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID || node == ZR_NULL) {
        return ZR_FALSE;
    }
    reference = semantic_scope_facts_find_declaration(builder->context, node);
    if (reference == ZR_NULL) {
        return ZR_TRUE;
    }
    symbol = ZrParser_Semantic_FindSymbolById(builder->context, reference->symbolId);
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = reference->symbolId;
    fact.ownerSymbolId = ownerSymbolId;
    fact.access = ZR_ACCESS_PUBLIC;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = reference->declarationRange;
    fact.definitionRange = reference->definitionRange;
    fact.hasDefinitionRange = reference->hasDefinitionRange;
    fact.signatureDisplay = reference->signatureDisplay;
    if (symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        SZrString *signature = ZrParser_SemanticDisplay_PublishCallableSignature(
                builder->context, symbol->id);
        if (signature != ZR_NULL) {
            fact.signatureDisplay = signature;
        }
    }
    fact.externalOriginUri = externalOriginUri;
    fact.externalOriginRange = externalOriginRange;
    fact.hasExternalOriginRange = hasExternalOriginRange;
    fact.isHoisted = isHoisted;
    fact.isAccessible = ZR_TRUE;
    fact.isImport = isImport;
    fact.isAlias = isAlias;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

/** @brief 为普通声明补齐导入与类型别名元数据后委托可见事实发布。
 * @note 绑定身份和范围仍取自既有引用事实，不由此函数按名称推断。
 */
static TZrBool semantic_scope_facts_publish_declaration(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        const SZrAstNode *node,
        TZrSymbolId ownerSymbolId,
        TZrBool isHoisted) {
    TZrBool isImport = semantic_scope_facts_is_direct_import_alias(node);
    SZrFileRange externalOriginRange;
    TZrBool hasExternalOriginRange = isImport &&
            semantic_scope_facts_import_origin_range(node, &externalOriginRange);

    if (!hasExternalOriginRange) {
        memset(&externalOriginRange, 0, sizeof(externalOriginRange));
    }

    return semantic_scope_facts_publish_reference_declaration(
            builder,
            scopeId,
            node,
            ownerSymbolId,
            isHoisted,
            isImport,
            (TZrBool)(isImport || semantic_scope_facts_is_type_value_alias(node)),
            isImport ? semantic_scope_facts_import_origin_uri(node) : ZR_NULL,
            externalOriginRange,
            hasExternalOriginRange);
}

/** @brief 把支持的对象解构项映射到声明事实所用的标识符节点。
 * @return 裸标识符原样返回；非计算且键为标识符的键值项返回键节点。
 */
static SZrAstNode *semantic_scope_facts_destructuring_binding_node(
        SZrAstNode *entry) {
    if (entry == ZR_NULL) {
        return ZR_NULL;
    }
    if (entry->type == ZR_AST_IDENTIFIER_LITERAL) {
        return entry;
    }
    if (entry->type == ZR_AST_KEY_VALUE_PAIR &&
        !entry->data.keyValuePair.keyIsComputed &&
        entry->data.keyValuePair.key != ZR_NULL &&
        entry->data.keyValuePair.key->type == ZR_AST_IDENTIFIER_LITERAL) {
        return entry->data.keyValuePair.key;
    }
    return ZR_NULL;
}

/** @brief 发布普通变量或对象解构中可由引用事实确认的局部绑定。
 * @note 对象模式逐项匹配绑定节点；其他模式以整个变量声明节点查找既有事实。
 * TODO: 详细查询契约只投影精确解析声明事实，缺项会省略（docs/parser-and-semantics/semantic-query-api-foundation.md:256；
 * docs/plans/lsp/optimize/2026-08-24-plan03-task02-source-scopes.md:5-20）；概览虽将“locals”列为
 * VisibleSymbols 覆盖类别（docs/parser-and-semantics/semantic-query-api-foundation.md:1242），却未列绑定语法。
 * 数组解构编译分支分配局部槽位但未按 pattern binding 节点登记事实（zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c:3130-3134；zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c:1015-1047）。
 * 本函数只按对象绑定节点或整体变量声明节点查既有事实（semantic_scope_facts.c:445-448）；VisibleSymbols 只读已发布项
 * （zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c:632-645），故数组绑定目前省略。
 * 数组模式尚无专门查询用例；普通局部用例见 tests/parser/test_semantic_query_symbols.c:885-927。需明确概览是否承诺该模式；
 * 若承诺，发布精确绑定事实并补查询回归。
 */
static TZrBool semantic_scope_facts_publish_variable_declaration(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        SZrAstNode *node,
        TZrSymbolId ownerSymbolId) {
    SZrVariableDeclaration *declaration;
    TZrBool isImport;
    SZrString *externalOriginUri;
    SZrFileRange externalOriginRange;
    TZrBool hasExternalOriginRange;
    TZrSize index;

    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &node->data.variableDeclaration;
    if (declaration->pattern == ZR_NULL ||
        declaration->pattern->type != ZR_AST_DESTRUCTURING_OBJECT) {
        return semantic_scope_facts_publish_declaration(
                builder, scopeId, node, ownerSymbolId, ZR_FALSE);
    }
    isImport = (TZrBool)(declaration->value != ZR_NULL &&
                          declaration->value->type == ZR_AST_IMPORT_EXPRESSION);
    externalOriginUri = isImport ? semantic_scope_facts_import_origin_uri(node) : ZR_NULL;
    hasExternalOriginRange = isImport &&
            semantic_scope_facts_import_origin_range(node, &externalOriginRange);
    if (!hasExternalOriginRange) {
        memset(&externalOriginRange, 0, sizeof(externalOriginRange));
    }
    if (declaration->pattern->data.destructuringObject.keys == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < declaration->pattern->data.destructuringObject.keys->count; index++) {
        SZrAstNode *bindingNode = semantic_scope_facts_destructuring_binding_node(
                declaration->pattern->data.destructuringObject.keys->nodes[index]);

        if (bindingNode != ZR_NULL &&
            !semantic_scope_facts_publish_reference_declaration(
                    builder,
                    scopeId,
                    bindingNode,
                    ownerSymbolId,
                    ZR_FALSE,
                    isImport,
                    isImport,
                    externalOriginUri,
                    externalOriginRange,
                    hasExternalOriginRange)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/** @brief 将 AST 节点对应的类型符号发布到外围作用域。
 * @note 类型声明按提升项处理；缺少已登记类型符号时不额外合成事实。
 */
static TZrBool semantic_scope_facts_publish_type_declaration(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        const SZrAstNode *node) {
    const SZrSemanticSymbolRecord *symbol;
    SZrSemanticVisibleSymbolFact fact;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID || node == ZR_NULL) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_type_declaration(builder->context, node);
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = symbol->id;
    fact.access = ZR_ACCESS_PUBLIC;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = symbol->location;
    fact.definitionRange = symbol->location;
    fact.hasDefinitionRange = ZR_TRUE;
    fact.isHoisted = ZR_TRUE;
    fact.isAccessible = ZR_TRUE;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

/** @brief 为一个受支持的类型或 const-int 泛型参数登记规范类型、符号及可见事实。
 * @note 规范类型按所属符号与参数序号区分；不支持的参数形状跳过。
 */
static TZrBool semantic_scope_facts_publish_generic_parameter(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        TZrSymbolId ownerSymbolId,
        SZrAstNode *node,
        TZrUInt32 ordinal) {
    const SZrSemanticSymbolRecord *symbol;
    SZrParameter *parameter;
    SZrSemanticVisibleSymbolFact fact;
    TZrTypeId typeId;
    TZrSymbolId symbolId;

    if (builder == ZR_NULL || builder->context == ZR_NULL ||
        scopeId == ZR_SEMANTIC_ID_INVALID ||
        ownerSymbolId == ZR_SEMANTIC_ID_INVALID ||
        node == ZR_NULL || node->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }
    parameter = &node->data.parameter;
    if ((parameter->genericKind != ZR_GENERIC_PARAMETER_TYPE &&
         parameter->genericKind != ZR_GENERIC_PARAMETER_CONST_INT) ||
        parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
        return ZR_TRUE;
    }

    typeId = ZrParser_CanonicalType_InternGenericParameter(
            builder->context, ownerSymbolId, ordinal);
    if (typeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_Semantic_RegisterCanonicalType(
                builder->context,
                typeId,
                ZR_SEMANTIC_TYPE_KIND_GENERIC_PARAMETER,
                parameter->name->name,
                node)) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_generic_parameter(builder->context, node);
    if (symbol != ZR_NULL) {
        if (symbol->typeId != typeId) {
            return ZR_FALSE;
        }
        symbolId = symbol->id;
    } else {
        symbolId = ZrParser_Semantic_RegisterSymbol(
                builder->context,
                parameter->name->name,
                ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
                typeId,
                ZR_SEMANTIC_ID_INVALID,
                node,
                parameter->nameLocation);
        if (symbolId == ZR_SEMANTIC_ID_INVALID) {
            return ZR_FALSE;
        }
    }

    memset(&fact, 0, sizeof(fact));
    fact.scopeId = scopeId;
    fact.symbolId = symbolId;
    fact.ownerSymbolId = ownerSymbolId;
    fact.access = ZR_ACCESS_PUBLIC;
    fact.declarationOrder = ++builder->nextDeclarationOrder;
    fact.declarationRange = parameter->nameLocation;
    fact.definitionRange = parameter->nameLocation;
    fact.hasDefinitionRange = ZR_TRUE;
    fact.isAccessible = ZR_TRUE;
    fact.isGenericParameter = ZR_TRUE;
    return ZrParser_Semantic_PublishVisibleSymbolFact(builder->context, &fact);
}

/** @brief 按 AST 顺序发布泛型声明中的参数事实。
 * @return 空泛型声明成功；任一参数发布失败即停止并返回 false。
 */
static TZrBool semantic_scope_facts_publish_generic_parameters(
        SZrSemanticScopeFactBuilder *builder,
        TZrSemanticScopeId scopeId,
        TZrSymbolId ownerSymbolId,
        SZrGenericDeclaration *generic) {
    TZrSize index;

    if (generic == ZR_NULL || generic->params == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < generic->params->count; index++) {
        if (!semantic_scope_facts_publish_generic_parameter(
                    builder,
                    scopeId,
                    ownerSymbolId,
                    generic->params->nodes[index],
                    (TZrUInt32)index)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool semantic_scope_facts_visit_node(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId);

/** @brief 依 AST 数组顺序遍历节点并在首个发布失败处短路。
 * @note NULL 数组按空列表处理；节点本身的 NULL 策略由单节点 visitor 决定。
 */
static TZrBool semantic_scope_facts_visit_nodes(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNodeArray *nodes,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < nodes->count; index++) {
        if (!semantic_scope_facts_visit_node(
                    builder, nodes->nodes[index], parentScopeId, ownerSymbolId)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

/** @brief 为块的完整源码范围发布词法子作用域，再遍历块体。
 * @note 块作用域继承所属函数/类型符号与父级静态上下文。
 */
static TZrBool semantic_scope_facts_visit_block(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSemanticScopeId scopeId;

    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_BLOCK,
            node->location,
            ownerSymbolId,
            ZR_FALSE);
    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_visit_nodes(
                   builder, node->data.block.body, scopeId, ownerSymbolId);
}

/** @brief 为 lambda 建立函数作用域，并投影参数与块体中的嵌套声明。
 * @note 所属符号需从当前上下文按 lambda AST 节点查得；缺失时使用无效 owner。
 */
static TZrBool semantic_scope_facts_visit_lambda(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId) {
    const SZrSemanticSymbolRecord *symbol;
    SZrLambdaExpression *lambda;
    TZrSemanticScopeId scopeId;
    TZrSymbolId ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    TZrSize index;

    if (builder == ZR_NULL || builder->context == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_LAMBDA_EXPRESSION) {
        return ZR_FALSE;
    }
    lambda = &node->data.lambdaExpression;
    symbol = semantic_scope_facts_find_symbol_by_node(builder->context, node);
    if (symbol != ZR_NULL && symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
        symbol->id != ZR_SEMANTIC_ID_INVALID) {
        ownerSymbolId = symbol->id;
    }
    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_FUNCTION,
            node->location,
            ownerSymbolId,
            ZR_FALSE);
    if (scopeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (lambda->params != ZR_NULL) {
        for (index = 0U; index < lambda->params->count; index++) {
            if (!semantic_scope_facts_publish_declaration(
                        builder,
                        scopeId,
                        lambda->params->nodes[index],
                        ownerSymbolId,
                        ZR_FALSE)) {
                return ZR_FALSE;
            }
        }
    }
    return lambda->block == ZR_NULL ||
           semantic_scope_facts_visit_node(builder, lambda->block, scopeId, ownerSymbolId);
}

/** @brief 先在父作用域发布提升的函数声明，再建立函数作用域处理泛型、参数和函数体。
 * @note 函数 owner 来自该 AST 节点的已解析声明事实；没有 owner 时不生成泛型参数事实。
 */
static TZrBool semantic_scope_facts_visit_function(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId enclosingOwnerSymbolId) {
    const SZrSemanticReferenceFact *reference;
    TZrSymbolId ownerSymbolId = ZR_SEMANTIC_ID_INVALID;
    TZrSemanticScopeId scopeId;
    SZrFunctionDeclaration *declaration = &node->data.functionDeclaration;
    TZrSize index;

    if (!semantic_scope_facts_publish_declaration(
                builder, parentScopeId, node, enclosingOwnerSymbolId, ZR_TRUE)) {
        return ZR_FALSE;
    }
    reference = semantic_scope_facts_find_declaration(builder->context, node);
    if (reference != ZR_NULL) {
        ownerSymbolId = reference->symbolId;
    }
    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_FUNCTION,
            node->location,
            ownerSymbolId,
            ZR_FALSE);
    if (scopeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    if (ownerSymbolId != ZR_SEMANTIC_ID_INVALID &&
        !semantic_scope_facts_publish_generic_parameters(
                builder, scopeId, ownerSymbolId, declaration->generic)) {
        return ZR_FALSE;
    }
    if (declaration->params != ZR_NULL) {
        for (index = 0U; index < declaration->params->count; index++) {
            if (!semantic_scope_facts_publish_declaration(
                        builder,
                        scopeId,
                        declaration->params->nodes[index],
                        ownerSymbolId,
                        ZR_FALSE)) {
                return ZR_FALSE;
            }
        }
    }
    return declaration->body == ZR_NULL ||
           semantic_scope_facts_visit_node(builder, declaration->body, scopeId, ownerSymbolId);
}

/** @brief 为结构体、类或接口方法签名建立函数作用域并投影其泛型、参数和方法体。
 * @note 方法须已有函数符号；静态标记进入作用域并由后代作用域继承。
 */
static TZrBool semantic_scope_facts_visit_method(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId) {
    const SZrSemanticSymbolRecord *symbol;
    SZrGenericDeclaration *generic;
    SZrAstNodeArray *params;
    SZrAstNode *body;
    TZrSemanticScopeId scopeId;
    TZrSymbolId ownerSymbolId;
    TZrBool isStatic = ZR_FALSE;
    TZrSize index;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->type) {
        case ZR_AST_STRUCT_METHOD:
            generic = node->data.structMethod.generic;
            params = node->data.structMethod.params;
            body = node->data.structMethod.body;
            isStatic = node->data.structMethod.isStatic;
            break;
        case ZR_AST_CLASS_METHOD:
            generic = node->data.classMethod.generic;
            params = node->data.classMethod.params;
            body = node->data.classMethod.body;
            isStatic = node->data.classMethod.isStatic;
            break;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            generic = node->data.interfaceMethodSignature.generic;
            params = node->data.interfaceMethodSignature.params;
            body = ZR_NULL;
            break;
        default:
            return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_symbol_by_node(builder->context, node);
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->id == ZR_SEMANTIC_ID_INVALID) {
        return ZR_TRUE;
    }
    /* Publishing generic parameters can relocate the symbol array. */
    ownerSymbolId = symbol->id;
    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_FUNCTION,
            node->location,
            ownerSymbolId,
            isStatic);
    if (scopeId == ZR_SEMANTIC_ID_INVALID ||
        !semantic_scope_facts_publish_generic_parameters(
                builder, scopeId, ownerSymbolId, generic)) {
        return ZR_FALSE;
    }
    if (params != ZR_NULL) {
        for (index = 0U; index < params->count; index++) {
            if (!semantic_scope_facts_publish_declaration(
                        builder,
                        scopeId,
                        params->nodes[index],
                        ownerSymbolId,
                        ZR_FALSE)) {
                return ZR_FALSE;
            }
        }
    }
    return body == ZR_NULL ||
           semantic_scope_facts_visit_node(builder, body, scopeId, ownerSymbolId);
}

/** @brief 为类型声明建立类型作用域，再发布泛型参数和成员事实。
 * @note 类型 owner ID 在可能扩容符号数组的子事实发布之前复制保存。
 */
static TZrBool semantic_scope_facts_visit_type(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId) {
    const SZrSemanticSymbolRecord *symbol;
    SZrGenericDeclaration *generic = ZR_NULL;
    SZrAstNodeArray *members = ZR_NULL;
    TZrSemanticScopeId scopeId;
    TZrSymbolId ownerSymbolId;

    if (!semantic_scope_facts_publish_type_declaration(builder, parentScopeId, node)) {
        return ZR_FALSE;
    }
    symbol = semantic_scope_facts_find_type_declaration(builder->context, node);
    if (symbol == ZR_NULL) {
        return ZR_TRUE;
    }
    /* Child publication may grow context->symbols and invalidate symbol. */
    ownerSymbolId = symbol->id;
    scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_TYPE,
            node->location,
            ownerSymbolId,
            ZR_FALSE);
    switch (node->type) {
        case ZR_AST_STRUCT_DECLARATION:
            generic = node->data.structDeclaration.generic;
            members = node->data.structDeclaration.members;
            break;
        case ZR_AST_CLASS_DECLARATION:
            generic = node->data.classDeclaration.generic;
            members = node->data.classDeclaration.members;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            generic = node->data.interfaceDeclaration.generic;
            members = node->data.interfaceDeclaration.members;
            break;
        case ZR_AST_ENUM_DECLARATION:
            members = node->data.enumDeclaration.members;
            break;
        default:
            return ZR_FALSE;
    }
    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_publish_generic_parameters(
                   builder, scopeId, ownerSymbolId, generic) &&
           semantic_scope_facts_visit_nodes(builder, members, scopeId, ownerSymbolId);
}

/** @brief 只把 extern 块中的函数/委托声明及类型声明并入外围作用域。
 * @note 其他 extern 子节点不建立独立词法事实。
 */
static TZrBool semantic_scope_facts_visit_extern_block(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSize index;

    if (node->data.externBlock.declarations == ZR_NULL) {
        return ZR_TRUE;
    }
    for (index = 0U; index < node->data.externBlock.declarations->count; index++) {
        SZrAstNode *declaration = node->data.externBlock.declarations->nodes[index];

        if (declaration == ZR_NULL) {
            continue;
        }
        switch (declaration->type) {
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (!semantic_scope_facts_publish_declaration(
                            builder,
                            parentScopeId,
                            declaration,
                            ownerSymbolId,
                            ZR_TRUE)) {
                    return ZR_FALSE;
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
            case ZR_AST_CLASS_DECLARATION:
            case ZR_AST_INTERFACE_DECLARATION:
            case ZR_AST_ENUM_DECLARATION:
                if (!semantic_scope_facts_visit_type(
                            builder, declaration, parentScopeId)) {
                    return ZR_FALSE;
                }
                break;
            default:
                break;
        }
    }
    return ZR_TRUE;
}

/** @brief 计算循环词法范围：保留循环起点，并在有循环体时延伸到体末端。
 * @note 范围使用 AST 自带的位置与 source 指针，不复制源码缓冲区。
 */
static SZrFileRange semantic_scope_facts_loop_range(
        const SZrAstNode *node,
        const SZrAstNode *body) {
    SZrFileRange range = node->location;

    if (body != ZR_NULL) {
        range.end = body->location.end;
    }
    return range;
}

/** @brief 用一个块作用域覆盖 for 初始化式和循环体。
 * @note 初始化式与循环体共享该作用域，循环体自身的块可再建立内层作用域。
 */
static TZrBool semantic_scope_facts_visit_for_loop(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSemanticScopeId scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_BLOCK,
            semantic_scope_facts_loop_range(node, node->data.forLoop.block),
            ownerSymbolId,
            ZR_FALSE);

    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_visit_node(
                   builder, node->data.forLoop.init, scopeId, ownerSymbolId) &&
           semantic_scope_facts_visit_node(
                   builder, node->data.forLoop.block, scopeId, ownerSymbolId);
}

/** @brief 为 foreach 建立循环作用域，在其中发布迭代绑定并遍历循环体。
 * @note 绑定仅能通过 pattern 对应的既有声明事实发布。
 */
static TZrBool semantic_scope_facts_visit_foreach_loop(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    TZrSemanticScopeId scopeId = semantic_scope_facts_publish_scope(
            builder,
            parentScopeId,
            ZR_SEMANTIC_SCOPE_KIND_BLOCK,
            semantic_scope_facts_loop_range(node, node->data.foreachLoop.block),
            ownerSymbolId,
            ZR_FALSE);

    return scopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_publish_declaration(
                   builder, scopeId, node->data.foreachLoop.pattern, ownerSymbolId, ZR_FALSE) &&
           semantic_scope_facts_visit_node(
                   builder, node->data.foreachLoop.block, scopeId, ownerSymbolId);
}

/**
 * @brief 按已知声明和控制结构递归投影作用域与绑定事实。
 * @note if/switch/try 分支沿用当前父作用域，块节点再创建子作用域；未知节点按叶子跳过。
 * TODO: 详细契约只投影精确声明事实且缺项省略（docs/parser-and-semantics/semantic-query-api-foundation.md:256；
 * docs/plans/lsp/optimize/2026-08-24-plan03-task02-source-scopes.md:5-20）；概览虽把“locals”列为 VisibleSymbols
 * 覆盖类别（docs/parser-and-semantics/semantic-query-api-foundation.md:1242），未逐项界定 catch/using。catch 只遍历 block，
 * using 只遍历 body/else，均不发布 pattern；编译器仍创建对应局部（zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_try.c:54-75；zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c:2216,2291）。
 * using 注册不带声明节点/范围（zr_vm_parser/src/zr_vm_parser/type_system.c:1304-1316）；VisibleSymbols 只消费已发布项
 * （zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c:632-645），故这些绑定当前省略；尚无专门查询用例。
 * 普通局部用例见 tests/parser/test_semantic_query_symbols.c:885-927。需先明确概览的 locals 是否承诺这些模式；若承诺，发布精确绑定事实并补查询回归。
 */
static TZrBool semantic_scope_facts_visit_node(
        SZrSemanticScopeFactBuilder *builder,
        SZrAstNode *node,
        TZrSemanticScopeId parentScopeId,
        TZrSymbolId ownerSymbolId) {
    if (builder == ZR_NULL || builder->context == ZR_NULL || node == ZR_NULL) {
        return builder != ZR_NULL && builder->context != ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return semantic_scope_facts_visit_function(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_EXTERN_BLOCK:
            return semantic_scope_facts_visit_extern_block(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
            return semantic_scope_facts_visit_type(builder, node, parentScopeId);
        case ZR_AST_STRUCT_METHOD:
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return semantic_scope_facts_publish_receiver_member(
                           builder, parentScopeId, node, ownerSymbolId) &&
                   semantic_scope_facts_visit_method(builder, node, parentScopeId);
        case ZR_AST_STRUCT_FIELD:
        case ZR_AST_CLASS_FIELD:
        case ZR_AST_INTERFACE_FIELD_DECLARATION:
            return semantic_scope_facts_publish_receiver_member(
                    builder, parentScopeId, node, ownerSymbolId);
        case ZR_AST_VARIABLE_DECLARATION:
            return semantic_scope_facts_publish_variable_declaration(
                           builder, parentScopeId, node, ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder,
                           node->data.variableDeclaration.value,
                           parentScopeId,
                           ownerSymbolId);
        case ZR_AST_RETURN_STATEMENT:
            return semantic_scope_facts_visit_node(
                    builder,
                    node->data.returnStatement.expr,
                    parentScopeId,
                    ownerSymbolId);
        case ZR_AST_LAMBDA_EXPRESSION:
            return semantic_scope_facts_visit_lambda(builder, node, parentScopeId);
        case ZR_AST_BLOCK:
            return semantic_scope_facts_visit_block(builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_IF_EXPRESSION:
            return semantic_scope_facts_visit_node(
                           builder,
                           node->data.ifExpression.thenExpr,
                           parentScopeId,
                           ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder,
                           node->data.ifExpression.elseExpr,
                           parentScopeId,
                           ownerSymbolId);
        case ZR_AST_WHILE_LOOP:
            return semantic_scope_facts_visit_node(
                    builder, node->data.whileLoop.block, parentScopeId, ownerSymbolId);
        case ZR_AST_FOR_LOOP:
            return semantic_scope_facts_visit_for_loop(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_FOREACH_LOOP:
            return semantic_scope_facts_visit_foreach_loop(
                    builder, node, parentScopeId, ownerSymbolId);
        case ZR_AST_SWITCH_EXPRESSION: {
            TZrSize index;
            SZrSwitchExpression *switchExpression = &node->data.switchExpression;

            if (switchExpression->cases != ZR_NULL) {
                for (index = 0U; index < switchExpression->cases->count; index++) {
                    if (!semantic_scope_facts_visit_node(
                                builder,
                                switchExpression->cases->nodes[index],
                                parentScopeId,
                                ownerSymbolId)) {
                        return ZR_FALSE;
                    }
                }
            }
            return semantic_scope_facts_visit_node(
                    builder, switchExpression->defaultCase, parentScopeId, ownerSymbolId);
        }
        case ZR_AST_SWITCH_CASE:
            return semantic_scope_facts_visit_node(
                    builder, node->data.switchCase.block, parentScopeId, ownerSymbolId);
        case ZR_AST_SWITCH_DEFAULT:
            return semantic_scope_facts_visit_node(
                    builder, node->data.switchDefault.block, parentScopeId, ownerSymbolId);
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: {
            SZrTryCatchFinallyStatement *statement = &node->data.tryCatchFinallyStatement;

            return semantic_scope_facts_visit_node(
                           builder, statement->block, parentScopeId, ownerSymbolId) &&
                   semantic_scope_facts_visit_nodes(
                           builder, statement->catchClauses, parentScopeId, ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder, statement->finallyBlock, parentScopeId, ownerSymbolId);
        }
        case ZR_AST_CATCH_CLAUSE:
            return semantic_scope_facts_visit_node(
                    builder, node->data.catchClause.block, parentScopeId, ownerSymbolId);
        case ZR_AST_USING_STATEMENT:
            return semantic_scope_facts_visit_node(
                           builder, node->data.usingStatement.body, parentScopeId, ownerSymbolId) &&
                   semantic_scope_facts_visit_node(
                           builder, node->data.usingStatement.elseBody, parentScopeId, ownerSymbolId);
        /* TODO: visitor 对未列入 switch 的 call/init AST 类型不会递归其实参。合法 LSP 输入
         * `init task.Job<int>(fn() => { return 7; })` 将 lambda 放在 init 实参中
         * （tests/language_server/test_lsp_interface.c:7313,7347，后者调用 Source_Compile）。
         * zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c:72,86,94 把 args 存入节点；
         * zr_vm_parser/include/zr_vm_parser/ast.h:574-580 保存 args；zr_vm_parser/src/zr_vm_parser/parser/parser_function_syntax.c:117-132 构建 lambda body。
         * 变量 visitor 会递归到初值（zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c:1048-1052），
         * 但该 init 节点落入此 default，不会触发 lambda case（zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c:1059-1060），故不发布 lambda 作用域/参数 facts。
         * VisibleSymbols 只扫描已发布候选（zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c:632-645），
         * 所以该 lambda 内查询没有自己的 scope/parameter candidates。详细契约仅说明精确 facts 缺项省略
         * （docs/parser-and-semantics/semantic-query-api-foundation.md:256），概览泛称 locals
         * （docs/parser-and-semantics/semantic-query-api-foundation.md:1242）；需明确此表达式形态的支持范围，
         * 若承诺则递归 init/call 实参并补可见符号查询回归。
         */
        default:
            return ZR_TRUE;
    }
}

/**
 * @brief 从脚本 AST 与当前语义上下文中的已解析声明事实构建词法作用域快照。
 * @pre 符号和引用事实已针对同一 root 完成分析；每个快照调用一次。
 * @return root/context 非法或任何作用域、绑定发布失败时返回 false。
 * @note 事实追加到 context；递归中途失败不回滚。范围/source 与符号 AST 节点不由本函数持有，
 *       调用方须让 AST 与源码在查询期间保持有效；Reset 清空事实并重置作用域 ID。
 */
TZrBool ZrParser_Semantic_BuildSourceScopeFacts(
        SZrSemanticContext *context,
        SZrAstNode *root) {
    SZrSemanticScopeFactBuilder builder;
    TZrSemanticScopeId moduleScopeId;

    if (context == ZR_NULL || root == ZR_NULL || root->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }
    memset(&builder, 0, sizeof(builder));
    builder.context = context;
    moduleScopeId = semantic_scope_facts_publish_scope(
            &builder,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_SCOPE_KIND_MODULE,
            root->location,
            ZR_SEMANTIC_ID_INVALID,
            ZR_FALSE);
    return moduleScopeId != ZR_SEMANTIC_ID_INVALID &&
           semantic_scope_facts_visit_nodes(
                   &builder,
                   root->data.script.statements,
                   moduleScopeId,
                   ZR_SEMANTIC_ID_INVALID);
}
