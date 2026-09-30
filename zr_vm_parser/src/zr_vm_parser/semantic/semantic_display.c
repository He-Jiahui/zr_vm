#include "zr_vm_parser/semantic_display.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"

/* 清空输出区，让失败的格式化入口不会遗留调用方旧文本。 */
static TZrBool semantic_display_prepare_buffer(TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0U) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    return ZR_TRUE;
}

/* 按 VM 字符串的字节长度复制到调用方 buffer，不转移内部字符指针。 */
static TZrBool semantic_display_copy_string(
        SZrString *value,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const TZrChar *text;
    TZrSize length;

    if (value == ZR_NULL || !semantic_display_prepare_buffer(buffer, bufferSize)) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    length = ZrCore_String_GetByteLength(value);
    if (text == ZR_NULL || length + 1U > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return ZR_TRUE;
}

/* 只复用同一 symbol 与 canonical type 的声明签名 fact。 */
static SZrString *semantic_display_declaration_signature(
        const SZrSemanticContext *context,
        const SZrSemanticSymbolRecord *symbol) {
    TZrSize index;

    if (context == ZR_NULL || symbol == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            fact->isResolved && fact->symbolId == symbol->id &&
            fact->typeId == symbol->typeId && fact->signatureDisplay != ZR_NULL) {
            return fact->signatureDisplay;
        }
    }
    return ZR_NULL;
}

/* property accessor 必须同时是 function symbol 和 canonical function type。 */
static TZrBool semantic_display_is_function_symbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    const SZrSemanticSymbolRecord *symbol =
            ZrParser_Semantic_FindSymbolById(context, symbolId);
    const SZrCanonicalTypeNode *type;

    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    type = ZrParser_CanonicalType_Find(context, symbol->typeId);
    return (TZrBool)(symbol != ZR_NULL &&
                     type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_FUNCTION);
}

/* 追加完整文本片段并维护 offset/NUL 不变量，供 generic 与签名格式化共用。 */
static TZrBool semantic_display_append(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset,
        const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    length = strlen(text);
    if (*offset + length + 1U > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer + *offset, text, length);
    *offset += length;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

/* use-site key 比较位置坐标，并按 source 字符串内容等价而非指针地址。 */
static TZrBool semantic_display_ranges_equal(
        const SZrFileRange *left,
        const SZrFileRange *right) {
    if (left == ZR_NULL || right == ZR_NULL ||
        left->start.offset != right->start.offset ||
        left->start.line != right->start.line ||
        left->start.column != right->start.column ||
        left->end.offset != right->end.offset ||
        left->end.line != right->end.line ||
        left->end.column != right->end.column) {
        return ZR_FALSE;
    }
    return (TZrBool)(left->source == right->source ||
                     (left->source != ZR_NULL && right->source != ZR_NULL &&
                      ZrCore_String_Equal(left->source, right->source)));
}

/**
 * @brief 将 source alias 按 canonical TypeId 与精确 use-site range 固化到当前快照。
 * @pre context、已注册 TypeId、非空 source range 和 alias 必须有效。
 * @return 相同键的首次或同文发布返回 true；冲突或无效输入返回 false。
 * @note 发布会复制 source 与 alias 文本；canonical type formatting 不读取这些别名。
 */
TZrBool ZrParser_SemanticTypeDisplayAlias_Publish(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrFileRange *useRange,
        SZrString *alias) {
    SZrSemanticTypeDisplayAliasFact fact;
    TZrNativeString aliasText;
    TZrNativeString sourceText;
    TZrSize index;

    if (context == ZR_NULL || context->state == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL ||
        useRange == ZR_NULL || useRange->source == ZR_NULL ||
        useRange->end.offset <= useRange->start.offset || alias == ZR_NULL ||
        ZrCore_String_GetByteLength(alias) == 0U) {
        return ZR_FALSE;
    }
    aliasText = ZrCore_String_GetNativeString(alias);
    sourceText = ZrCore_String_GetNativeString(useRange->source);
    if (aliasText == ZR_NULL || sourceText == ZR_NULL ||
        ZrCore_String_GetByteLength(useRange->source) == 0U) {
        return ZR_FALSE;
    }
    for (index = 0U; index < context->typeDisplayAliasFacts.length; ++index) {
        const SZrSemanticTypeDisplayAliasFact *existing =
                (const SZrSemanticTypeDisplayAliasFact *)ZrCore_Array_Get(
                        &context->typeDisplayAliasFacts, index);
        /* 相同 TypeId/range 必须得到同一源码别名，避免一处显示随访问顺序改变。 */
        if (existing != ZR_NULL && existing->typeId == typeId &&
            semantic_display_ranges_equal(&existing->useRange, useRange)) {
            return ZrCore_String_Equal(existing->alias, alias);
        }
    }

    /* 保存前复制 parser/source 字符串，事实只依赖快照 state 的 GC 生命周期。 */
    fact.typeId = typeId;
    fact.useRange = *useRange;
    fact.useRange.source = ZrCore_String_Create(
            context->state,
            sourceText,
            ZrCore_String_GetByteLength(useRange->source));
    fact.alias = ZrCore_String_Create(
            context->state, aliasText, ZrCore_String_GetByteLength(alias));
    if (fact.useRange.source == ZR_NULL || fact.alias == ZR_NULL) {
        return ZR_FALSE;
    }
    /* BUG: 若 Context_New 初始化该数组时分配失败，它不检查空 head；有效发布仍到此处，
     * Array_Push 会在 head 断言处终止；关闭断言时则继续向空指针写入。见 semantic.c:68-75,113 与 array.h:36-42,73-89。 */
    ZrCore_Array_Push(context->state, &context->typeDisplayAliasFacts, &fact);
    return ZR_TRUE;
}

/**
 * @brief 查询指定 canonical type 与源代码 use-site 对应的 source alias。
 * @return 返回当前快照借用的字符串；context 生命周期结束后不得保留。
 * @note canonical formatter 不叠加这组事实。
 * TODO: 核查 lsp_inlay_hints.c 与 semantic LSP completion/hover 是否需要读取它；仓内查询目前只见 parser fixtures。
 */
SZrString *ZrParser_SemanticQuery_TypeDisplayAliasAt(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrFileRange *useRange) {
    TZrSize index;

    if (context == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL ||
        useRange == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->typeDisplayAliasFacts.length; ++index) {
        const SZrSemanticTypeDisplayAliasFact *fact =
                (const SZrSemanticTypeDisplayAliasFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->typeDisplayAliasFacts, index);
        if (fact != ZR_NULL && fact->typeId == typeId &&
            semantic_display_ranges_equal(&fact->useRange, useRange)) {
            return fact->alias;
        }
    }
    return ZR_NULL;
}

/* 仅映射签名展示支持的 callable AST 变体，未知 owner 不猜参数字段。 */
static const SZrAstNodeArray *semantic_display_callable_parameters(
        const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.params;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return declaration->data.externDelegateDeclaration.params;
        case ZR_AST_CLASS_METHOD:
            return declaration->data.classMethod.params;
        case ZR_AST_STRUCT_METHOD:
            return declaration->data.structMethod.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declaration->data.interfaceMethodSignature.params;
        default:
            return ZR_NULL;
    }
}

/* generic 名称来自声明 AST；canonical generic identity 由类型图校验。 */
static const SZrGenericDeclaration *semantic_display_callable_generic(
        const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.generic;
        case ZR_AST_CLASS_METHOD:
            return declaration->data.classMethod.generic;
        case ZR_AST_STRUCT_METHOD:
            return declaration->data.structMethod.generic;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declaration->data.interfaceMethodSignature.generic;
        default:
            return ZR_NULL;
    }
}

/* 从对应 callable 声明节点取源返回类型，用于保留声明显示信息。 */
static const SZrType *semantic_display_callable_return_type(
        const SZrAstNode *declaration) {
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.returnType;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.returnType;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return declaration->data.externDelegateDeclaration.returnType;
        case ZR_AST_CLASS_METHOD:
            return declaration->data.classMethod.returnType;
        case ZR_AST_STRUCT_METHOD:
            return declaration->data.structMethod.returnType;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declaration->data.interfaceMethodSignature.returnType;
        default:
            return ZR_NULL;
    }
}

/* unresolved 或冲突的源类型 fact 要显式降级，不能由 canonical 输出掩盖。 */
static TZrBool semantic_display_declared_type(
        const SZrSemanticContext *context,
        const SZrType *typeUse,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    if (typeUse != ZR_NULL && typeUse->name != ZR_NULL) {
        for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
            const SZrSemanticReferenceFact *fact =
                    (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            (SZrArray *)&context->referenceFacts, index);
            if (fact == ZR_NULL || fact->kind != ZR_SEMANTIC_REFERENCE_TYPE ||
                fact->node != typeUse->name) {
                continue;
            }
            if (!fact->isResolved || fact->typeId != typeId) {
                int length = snprintf(buffer, bufferSize, "cannot infer exact type");
                return length >= 0 && (TZrSize)length < bufferSize;
            }
        }
    }
    return ZrParser_CanonicalType_Format(context, typeId, buffer, bufferSize);
}

/* generic 显示保留 AST 名称与 variance；未知节点形状或 kind 使整段失败。 */
static TZrBool semantic_display_append_generic_clause(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset,
        const SZrGenericDeclaration *generic) {
    TZrSize index;

    if (generic == ZR_NULL || generic->params == ZR_NULL ||
        generic->params->count == 0U) {
        return ZR_TRUE;
    }
    if (!semantic_display_append(buffer, bufferSize, offset, "<")) {
        return ZR_FALSE;
    }
    for (index = 0U; index < generic->params->count; index++) {
        const SZrAstNode *node = generic->params->nodes[index];
        const SZrParameter *parameter;
        const TZrChar *name;

        if (node == ZR_NULL || node->type != ZR_AST_PARAMETER) {
            return ZR_FALSE;
        }
        parameter = &node->data.parameter;
        if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
            return ZR_FALSE;
        }
        name = ZrCore_String_GetNativeString(parameter->name->name);
        if (name == ZR_NULL || name[0] == '\0' ||
            (index > 0U &&
             !semantic_display_append(buffer, bufferSize, offset, ", "))) {
            return ZR_FALSE;
        }
        if (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT) {
            if (!semantic_display_append(buffer, bufferSize, offset, "const ") ||
                !semantic_display_append(buffer, bufferSize, offset, name) ||
                !semantic_display_append(buffer, bufferSize, offset, ": int")) {
                return ZR_FALSE;
            }
            continue;
        }
        if (parameter->genericKind != ZR_GENERIC_PARAMETER_TYPE ||
            (parameter->variance == ZR_GENERIC_VARIANCE_IN &&
             !semantic_display_append(buffer, bufferSize, offset, "in ")) ||
            (parameter->variance == ZR_GENERIC_VARIANCE_OUT &&
             !semantic_display_append(buffer, bufferSize, offset, "out ")) ||
            !semantic_display_append(buffer, bufferSize, offset, name)) {
            return ZR_FALSE;
        }
    }
    return semantic_display_append(buffer, bufferSize, offset, ">");
}

/* 参数 passing 与 escape 前缀来自 canonical contract，而非 AST 的表面写法。 */
static const TZrChar *semantic_display_passing_prefix(
        const SZrCanonicalParameterContract *contract) {
    if (contract == ZR_NULL) {
        return ZR_NULL;
    }
    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_IN:
            return "in ";
        case ZR_CANONICAL_PASSING_REF:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                    ? "scoped ref "
                    : "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                    ? "scoped ref readonly "
                    : "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT:
            return "out ";
        case ZR_CANONICAL_PASSING_VALUE:
            return "";
        default:
            return ZR_NULL;
    }
}

/**
 * @brief 由精确 function symbol、canonical contracts 与声明 AST 创建 callable label。
 * @pre symbol 必须解析到 function symbol、canonical function type 和受支持的声明节点。
 * @return 成功返回 context state 拥有的 VM 字符串；身份、contract 或格式不匹配时返回 NULL。
 * @note canonical contract 决定类型语义，AST 只提供源名称；未知 contract fail closed。
 */
SZrString *ZrParser_SemanticDisplay_CreateCallableSignature(
        SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *functionType;
    const SZrAstNodeArray *parameters;
    const SZrGenericDeclaration *generic;
    const TZrUInt32 supportedEffects =
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS |
            ZR_CANONICAL_CALLABLE_EFFECT_ASYNC |
            ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR;
    const TZrChar *receiverPrefix = "";
    TZrChar buffer[1024];
    TZrChar typeBuffer[256];
    TZrSize offset = 0U;
    TZrSize index;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    symbol = ZrParser_Semantic_FindSymbolById(context, symbolId);
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->name == ZR_NULL || symbol->astNode == ZR_NULL ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    functionType = ZrParser_CanonicalType_Find(context, symbol->typeId);
    if (functionType == ZR_NULL || functionType->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        (TZrInt32)functionType->data.function.receiverEffect <
                (TZrInt32)ZR_CANONICAL_RECEIVER_NONE ||
        functionType->data.function.receiverEffect > ZR_CANONICAL_RECEIVER_MUTABLE ||
        (functionType->data.function.effectFlags & ~supportedEffects) != 0U) {
        return ZR_NULL;
    }
    /* AST 只供展示；参数数量和节点形状必须与 canonical contract 一一对应。 */
    parameters = semantic_display_callable_parameters(symbol->astNode);
    if ((parameters == ZR_NULL &&
         functionType->data.function.parameterContracts.length != 0U) ||
        (parameters != ZR_NULL &&
         parameters->count != functionType->data.function.parameterContracts.length)) {
        return ZR_NULL;
    }
    generic = semantic_display_callable_generic(symbol->astNode);
    if (functionType->data.function.receiverEffect ==
        ZR_CANONICAL_RECEIVER_READONLY) {
        receiverPrefix = "const fn ";
    } else if (functionType->data.function.receiverEffect ==
               ZR_CANONICAL_RECEIVER_MUTABLE) {
        receiverPrefix = "fn ";
    }
    buffer[0] = '\0';
    if (((functionType->data.function.effectFlags &
          ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0U &&
         !semantic_display_append(buffer, sizeof(buffer), &offset, "async ")) ||
        ((functionType->data.function.effectFlags &
          ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR) != 0U &&
         !semantic_display_append(buffer, sizeof(buffer), &offset, "generator ")) ||
        !semantic_display_append(buffer, sizeof(buffer), &offset, receiverPrefix) ||
        !semantic_display_append(buffer,
                                 sizeof(buffer),
                                 &offset,
                                 ZrCore_String_GetNativeString(symbol->name)) ||
        !semantic_display_append_generic_clause(
                buffer, sizeof(buffer), &offset, generic) ||
        !semantic_display_append(buffer, sizeof(buffer), &offset, "(")) {
        return ZR_NULL;
    }
    for (index = 0U;
         index < functionType->data.function.parameterContracts.length;
         index++) {
        const SZrCanonicalParameterContract *contract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&functionType->data.function.parameterContracts,
                        index);
        const SZrCanonicalTypeNode *contractType;
        const SZrAstNode *parameterNode = parameters->nodes[index];
        const SZrParameter *parameter;
        const TZrChar *name;
        TZrTypeId displayTypeId;

        if (!ZrParser_CanonicalType_ValidateParameterContract(context, contract) ||
            parameterNode == ZR_NULL ||
            parameterNode->type != ZR_AST_PARAMETER) {
            return ZR_NULL;
        }
        parameter = &parameterNode->data.parameter;
        if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
            return ZR_NULL;
        }
        name = ZrCore_String_GetNativeString(parameter->name->name);
        displayTypeId = contract->typeId;
        if (contract->passingForm != ZR_CANONICAL_PASSING_VALUE) {
            contractType = ZrParser_CanonicalType_Find(context, contract->typeId);
            if (contractType == ZR_NULL || contractType->kind != ZR_CANONICAL_TYPE_REF) {
                return ZR_NULL;
            }
            displayTypeId = contractType->data.refType.pointeeTypeId;
        }
        if (name == ZR_NULL || name[0] == '\0' ||
            !semantic_display_declared_type(
                    context, parameter->typeInfo, displayTypeId, typeBuffer, sizeof(typeBuffer)) ||
            (index > 0U &&
             !semantic_display_append(buffer, sizeof(buffer), &offset, ", ")) ||
            !semantic_display_append(buffer, sizeof(buffer), &offset, name) ||
            !semantic_display_append(buffer, sizeof(buffer), &offset, ": ") ||
            !semantic_display_append(
                    buffer,
                    sizeof(buffer),
                    &offset,
                    semantic_display_passing_prefix(contract)) ||
            !semantic_display_append(buffer, sizeof(buffer), &offset, typeBuffer)) {
            return ZR_NULL;
        }
    }
    if (!semantic_display_declared_type(
                context, semantic_display_callable_return_type(symbol->astNode),
                functionType->data.function.returnTypeId, typeBuffer, sizeof(typeBuffer)) ||
        !semantic_display_append(buffer, sizeof(buffer), &offset, "): ") ||
        !semantic_display_append(buffer, sizeof(buffer), &offset, typeBuffer) ||
        ((functionType->data.function.effectFlags &
          ZR_CANONICAL_CALLABLE_EFFECT_THROWS) != 0U &&
         !semantic_display_append(buffer, sizeof(buffer), &offset, " throws"))) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(context->state, buffer, offset);
}

/**
 * @brief 创建 callable label 并发布给当前快照中相同 SymbolId/TypeId 的解析引用。
 * @return 成功返回 context state 拥有的 VM 字符串；创建失败时不修改引用 facts。
 */
SZrString *ZrParser_SemanticDisplay_PublishCallableSignature(
        SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    SZrString *signature = ZrParser_SemanticDisplay_CreateCallableSignature(context, symbolId);
    const SZrSemanticSymbolRecord *symbol;

    if (signature == ZR_NULL) {
        return ZR_NULL;
    }
    symbol = ZrParser_Semantic_FindSymbolById(context, symbolId);
    /* 精确身份匹配避免同名 overload 之间串用声明签名。 */
    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *fact = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &context->referenceFacts, index);
        if (fact != ZR_NULL && fact->isResolved && fact->symbolId == symbolId &&
            fact->typeId == symbol->typeId) {
            fact->signatureDisplay = signature;
        }
    }
    return signature;
}

/**
 * @brief 以 SymbolId 将 documentation 文本复制到当前 semantic snapshot。
 * @return 首次或同文重复发布返回 true；冲突、无效 symbol 或复制失败返回 false。
 * @pre context、已注册 SymbolId 和有效 documentation 字符串必须存在。
 * @note TODO: 核查 semantic_analyzer.c 与 lsp_interface_support.c 的文档提取/快照接线；仓内发布调用目前只见测试。
 */
TZrBool ZrParser_SemanticDocumentation_Publish(
        SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrString *documentation) {
    SZrSemanticDocumentationFact fact;
    TZrNativeString text;
    TZrSize index;

    if (context == ZR_NULL || context->state == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID || documentation == ZR_NULL ||
        ZrParser_Semantic_FindSymbolById(context, symbolId) == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 同一 SymbolId 的文本不可静默替换，否则 completion 与 hover 会看到不同版本。 */
    for (index = 0U; index < context->documentationFacts.length; ++index) {
        const SZrSemanticDocumentationFact *existing =
                (const SZrSemanticDocumentationFact *)ZrCore_Array_Get(
                        &context->documentationFacts, index);
        if (existing != ZR_NULL && existing->symbolId == symbolId) {
            return ZrCore_String_Equal(existing->documentation, documentation);
        }
    }
    text = ZrCore_String_GetNativeString(documentation);
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }
    fact.symbolId = symbolId;
    fact.documentation = ZrCore_String_Create(
            context->state,
            text,
            ZrCore_String_GetByteLength(documentation));
    if (fact.documentation == ZR_NULL) {
        return ZR_FALSE;
    }
    /* BUG: 若 Context_New 初始化该数组时分配失败，它不检查空 head；有效发布仍到此处，
     * Array_Push 会在 head 断言处终止；关闭断言时则继续向空指针写入。见 semantic.c:72-75,113 与 array.h:36-42,73-89。 */
    ZrCore_Array_Push(context->state, &context->documentationFacts, &fact);
    return ZR_TRUE;
}

/**
 * @brief 按 SymbolId 查询当前 semantic snapshot 的 documentation。
 * @return 返回快照借用的 VM 字符串；调用方不得跨 Reset/Free 或 generation 留存。
 */
SZrString *ZrParser_SemanticQuery_DocumentationOfSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID ||
        ZrParser_Semantic_FindSymbolById(context, symbolId) == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->documentationFacts.length; ++index) {
        const SZrSemanticDocumentationFact *fact =
                (const SZrSemanticDocumentationFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->documentationFacts, index);
        if (fact != ZR_NULL && fact->symbolId == symbolId) {
            return fact->documentation;
        }
    }
    return ZR_NULL;
}

/**
 * @brief 将已注册 canonical TypeId 写为 canonical 类型显示文本。
 * @return 完整文本适配调用方 buffer 时返回 true；无效身份或空间不足时返回 false，可写 buffer 会预先清空。
 * @note source alias 通过独立 use-site query，不改变此 canonical 输出。
 */
TZrBool ZrParser_SemanticDisplay_FormatType(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    if (!semantic_display_prepare_buffer(buffer, bufferSize) || context == ZR_NULL ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    return ZrParser_CanonicalType_Format(context, typeId, buffer, bufferSize);
}

/**
 * @brief 将已注册 SymbolId 写为声明签名，缺少签名时写为名称与 canonical 类型。
 * @return 完整文本适配调用方 buffer 且身份有效时返回 true，否则返回 false 并清空输出。
 */
TZrBool ZrParser_SemanticDisplay_FormatSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticSymbolRecord *symbol;
    SZrString *signature;
    const TZrChar *name;
    TZrChar typeBuffer[512];
    int written;

    if (!semantic_display_prepare_buffer(buffer, bufferSize) || context == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    symbol = ZrParser_Semantic_FindSymbolById(context, symbolId);
    if (symbol == ZR_NULL || symbol->name == ZR_NULL ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    signature = semantic_display_declaration_signature(context, symbol);
    if (signature != ZR_NULL) {
        return semantic_display_copy_string(signature, buffer, bufferSize);
    }
    if (!ZrParser_SemanticDisplay_FormatType(
                context, symbol->typeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }
    name = ZrCore_String_GetNativeString(symbol->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    written = snprintf(buffer, bufferSize, "%s: %s", name, typeBuffer);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}

/**
 * @brief 验证 property contract 后写出 static/receiver/reference/accessor label。
 * @pre property symbol/type 必须一致，且至少有一个 accessor/init symbol 为 canonical function。
 * @return 完整文本适配调用方 buffer 时返回 true；身份、contract 或空间无效时返回 false。
 */
TZrBool ZrParser_SemanticDisplay_FormatProperty(
        const SZrSemanticContext *context,
        const SZrSemanticPropertyContract *property,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticSymbolRecord *symbol;
    const TZrChar *name;
    const TZrChar *staticPrefix;
    const TZrChar *receiverPrefix;
    const TZrChar *referencePrefix;
    const TZrChar *getter;
    const TZrChar *setter;
    const TZrChar *initializer;
    TZrChar typeBuffer[512];
    int written;

    if (!semantic_display_prepare_buffer(buffer, bufferSize) || context == ZR_NULL ||
        property == ZR_NULL || property->propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        property->propertyTypeId == ZR_SEMANTIC_ID_INVALID ||
        (TZrInt32)property->receiverEffect < (TZrInt32)ZR_CANONICAL_RECEIVER_NONE ||
        property->receiverEffect > ZR_CANONICAL_RECEIVER_MUTABLE ||
        (TZrInt32)property->referenceAccess < (TZrInt32)ZR_REFERENCE_ACCESS_NONE ||
        property->referenceAccess > ZR_REFERENCE_ACCESS_READONLY) {
        return ZR_FALSE;
    }
    symbol = ZrParser_Semantic_FindSymbolById(context, property->propertySymbolId);
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_PROPERTY ||
        symbol->typeId != property->propertyTypeId ||
        (property->getterSymbolId == ZR_SEMANTIC_ID_INVALID &&
         property->setterSymbolId == ZR_SEMANTIC_ID_INVALID &&
         property->initializerSymbolId == ZR_SEMANTIC_ID_INVALID) ||
        (property->getterSymbolId != ZR_SEMANTIC_ID_INVALID &&
         !semantic_display_is_function_symbol(context, property->getterSymbolId)) ||
        (property->setterSymbolId != ZR_SEMANTIC_ID_INVALID &&
         !semantic_display_is_function_symbol(context, property->setterSymbolId)) ||
        (property->initializerSymbolId != ZR_SEMANTIC_ID_INVALID &&
         !semantic_display_is_function_symbol(context, property->initializerSymbolId))) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticDisplay_FormatType(
                context, property->propertyTypeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }
    name = ZrCore_String_GetNativeString(symbol->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    staticPrefix = property->isStatic ? "static " : "";
    receiverPrefix = property->receiverEffect == ZR_CANONICAL_RECEIVER_READONLY ? "const " : "";
    referencePrefix = property->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE
                              ? "ref "
                              : property->referenceAccess == ZR_REFERENCE_ACCESS_READONLY
                                      ? "ref readonly "
                                      : "";
    getter = property->getterSymbolId != ZR_SEMANTIC_ID_INVALID ? " get;" : "";
    setter = property->setterSymbolId != ZR_SEMANTIC_ID_INVALID ? " set;" : "";
    initializer = property->initializerSymbolId != ZR_SEMANTIC_ID_INVALID ? " init;" : "";
    written = snprintf(buffer,
                       bufferSize,
                       "%s%s%sproperty %s: %s {%s%s%s }",
                       staticPrefix,
                       receiverPrefix,
                       referencePrefix,
                       name,
                       typeBuffer,
                       getter,
                       setter,
                       initializer);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}
