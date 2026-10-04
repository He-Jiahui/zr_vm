/**
 * @file
 * @brief 在分析事实与查询展示之间投影规范身份、源注解及已发布文档/签名。
 * @pre 原生容器、AST 和 VM 字符串由实际 owner 保持有效；发布与读取使用同一快照并串行完成。
 * @note 本文件不负责按展示名解析类型，也不为原生事实中的 VM 指针登记 GC 根。
 * Reset 清空事实并重用 ID；复制的 VM 文本和生成签名须另行维持 GC 可达，具体根交接见发布块 TODO。
 */
#include "zr_vm_parser/semantic_display.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"

/**
 * @brief 为展示入口建立空输出起点，避免沿用调用方的旧标签。
 * @return 输出为空指针或零容量时返回 false；可写非零输出首字节置空后返回 true。
 * @note 这里只定义入口状态，不保证调用它的 formatter 在后续失败时仍留下空文本。
 */
static TZrBool semantic_display_prepare_buffer(TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0U) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    return ZR_TRUE;
}

/**
 * @brief 将匹配声明事实的 VM 签名复制为调用方持有的完整字节文本。
 * @pre value 在读取期间保持 GC 可达；输出区不与字符串存储重叠。
 * @note 不借出内部字符指针；容量不足不复制截断签名。FormatSymbol 仅以非 NULL 签名指针调用；零长度文本仍可完整复制成功。
 */
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

/**
 * @brief 为 FormatSymbol 选择当前符号的已解析声明签名，保持重载身份隔离。
 * @return 无匹配事实时返回 NULL；匹配时借用第一个同 SymbolId/TypeId 的声明签名。
 * @pre 调用期间符号、引用事实和 VM 字符串保持有效；不得把旧快照身份带入此查询。
 */
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

/**
 * @brief 为属性展示确认访问器身份可作为已登记的规范函数使用。
 * @note 只核对函数符号及其规范函数类型，不证明它与该属性的完整签名关系。
 */
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

/**
 * @brief 为泛型片段与签名共享有界追加约束，保持当前前缀的 NUL 终止。
 * @pre offset 描述本地有效输出前缀且小于 bufferSize；text 是仍有效的 NUL 文本，不与目标重叠。
 * @note 失败不追加本片段；此前已追加的前缀不回滚，签名构造者会放弃整个本地结果。
 */
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

/**
 * @brief 比较别名的精确使用位置键，允许来源字符串副本表示同一文件。
 * @pre 非空来源字符串仍有效；比较起止 offset、line、column 与来源内容。
 * @note 同一 source 指针也视为相同，包括双方为空；公开发布另行要求来源非空。
 */
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
 * @brief 为推导得到的规范类型保存一个源码位置的可选拼写，不改变类型身份。
 * @pre 输入字符串在复制期间有效；context 的原生事实容器已成功初始化。
 * @return 首次或同键同文发布返回 true；无效输入、冲突或可报告复制失败返回 false。
 * @note 来源与别名字节通过 VM 字符串创建保存；同键冲突保留原事实，规范格式不读取这些别名。
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
        /** 同一类型/完整使用位置只接受同文发布，避免访问顺序改变已经发布的源拼写。 */
        if (existing != ZR_NULL && existing->typeId == typeId &&
            semantic_display_ranges_equal(&existing->useRange, useRange)) {
            return ZrCore_String_Equal(existing->alias, alias);
        }
    }

    /**
     * @brief 通过 VM 字符串创建保存来源与别名字节，逻辑归属仍限于当前快照。
     * @note 原生事实数组仅保存指针，不建立 GC 根；短串可复用驻留对象，长串可为独立副本。
     * @todo TODO: 核对别名、文档和生成签名的长串根交接：从 SemanticContext_New、发布及宿主 trace
     * 注册入口检查，以发布后 FullGC 再查询并 Reset/Free 的用例验证保活与释放交接。
     */
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
    /**
     * @note BUG: 合法 state 下别名数组初始申请失败仍可由 Context_New 返回有效外层 context；
     * 后续有效发布在 Push 断言终止，关闭断言时向空 head 写入。增长申请失败也会丢失旧 head 后继续复制。
     * 静态链入口：semantic.c:91,139-141；memory.h:37-40；array.h:29-42,73-89。尚未执行 OOM 注入。
     */
    ZrCore_Array_Push(context->state, &context->typeDisplayAliasFacts, &fact);
    return ZR_TRUE;
}

/**
 * @brief 按当前规范类型及完整源码位置借用已发布的别名。
 * @return 无匹配返回 NULL；结果不转移所有权，也不延长 VM 字符串的 GC 寿命。
 * @note Reset 会清空事实并重用 ID，借用结果不得跨快照继续作为当前事实使用。
 * @todo TODO: 仓内直接查询仅见 parser 用例；从 LSP inlay/completion/hover 核对生产读取接入，再承诺用户可见别名。
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

/**
 * @brief 为签名构造借用受支持函数、extern 与方法声明的参数列表。
 * @note 不按未知 AST 形状猜测字段，未知或空声明返回 NULL；构造者仍可接受规范参数数目为零的 NULL 列表。
 */
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

/**
 * @brief 从函数或方法的源声明借用泛型参数名和修饰，供签名片段展示。
 * @note extern、未知或空声明没有本映射的泛型列表；此查询不建立或验证规范泛型身份。
 */
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

/**
 * @brief 借用关联声明的返回类型注解，供签名构造对照显式解析事实。
 * @pre declaration 非空且关联 AST 仍有效；调用者已经确认符号持有 AST。
 * @note 未支持的节点返回 NULL，随后可由规范返回 TypeId 单独格式化。
 */
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

/**
 * @brief 在签名类型位置尊重同一注解节点的负面解析事实，避免规范名字掩盖未解析状态。
 * @pre context、输出区和关联 AST/事实有效；当前调用者提供本地独立缓冲区。
 * @note 存在未解析或 TypeId 冲突事实时展示 cannot infer exact type；没有该事实时仍采用规范类型。
 * @return 降级文字也可成功；容量失败可能留下部分降级文本，构造者必须放弃整个签名。
 */
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

/**
 * @brief 为源侧签名展示泛型声明参数名、const-int 和已知方差前缀。
 * @pre 调用者保持 AST 参数数组与名称字符串有效；输出约束同 semantic_display_append。
 * @note 空列表省略片段；节点、名称或 genericKind 不可展示时失败并保留已有前缀。
 * 已知 IN/OUT 方差添加对应前缀，其它方差值不添加；此处不是完整 AST 合法性验证器。
 */
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

/**
 * @brief 将已验证调用契约的 passing/escape 信息投影为源侧参数前缀。
 * @pre CreateCallableSignature 已验证契约；本映射本身不验证所有 escape 字段组合。
 * @return 返回静态文字，无所有权转移；空契约或未知 passing form 返回 NULL。
 */
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
 * @brief 将函数规范调用契约与关联声明名称组合成供查询展示的 VM 签名。
 * @pre context/state、AST 与引用事实保持有效；关联节点应为受支持的函数、extern 或方法声明。
 * @return 身份、参数契约、形状、格式窗口或可报告创建失败返回 NULL；非空结果由使用者维持 GC 可达。
 * @note 未解析或冲突的显式类型可生成降级文字，成功不保证所有注解已解析。
 * 本函数不发布事实；零参数时没有完整 AST kind 拒绝保证，源名称也不决定规范身份。
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
    /** 参数名和显式注解来自源 AST；注解的负面解析事实可使相应位置降级。
     * 参数数量和已访问节点须匹配调用契约；零参数时不构成对所有未知 AST kind 的拒绝保证。 */
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
 * @brief 在 source-scope 分析期共享生成签名，供引用与可见符号查询复用。
 * @pre 声明/类型事实已发布，写入与查询串行；创建约束同 CreateCallableSignature。
 * @return 创建失败不改原签名；成功返回 VM 字符串，即使没有匹配引用。
 * @note 只更新同 SymbolId/TypeId 的已解析引用；旧展示视图可能被替换，不跨快照保留借用结果。
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
    /** 只共享同 symbol/type 的已解析引用，避免重载、特化类型或未解析事实借用错误签名。 */
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
 * @brief 以符号身份登记复制的文档，供 completion 与 hover 共享同一事实。
 * @pre context/state、原生事实容器和输入 VM 字符串有效。
 * @return 首次或同键同文成功；冲突、无效身份或可报告复制失败返回 false，原事实不被覆盖。
 * @note 文档可为空文本；VM 复制不提供独立 GC 根。
 * @todo TODO: 仓内发布者目前只有测试；从注释提取、semantic analyzer 与 source-scope 构造入口核对生产发布接入。
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
    /** 文档按符号身份保持幂等：同文重复接受，冲突保留原事实，不按显示名合并。 */
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
    /**
     * @note BUG: 文档数组初始申请失败未被 Context_New 传播；后续有效发布到达空 head 的 Push，
     * 断言终止或空地址写入。增长申请失败同样可能覆盖 head 后继续复制，发布接口不能统一返回 false。
     * 静态链入口：semantic.c:95,139-141；memory.h:37-40；array.h:29-42,73-89。尚未执行 OOM 注入。
     */
    ZrCore_Array_Push(context->state, &context->documentationFacts, &fact);
    return ZR_TRUE;
}

/**
 * @brief 为 completion/hover 按精确符号借用当前快照的文档。
 * @return 无有效符号或未发布事实时返回 NULL；结果不得释放、修改或跨快照保留。
 * @note context 的原生数组不延长文档字符串的 GC 寿命，使用者保持实际 VM owner/root 有效。
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
 * @brief 以规范 TypeId 展示类型结构，供符号及属性标签复用。
 * @pre 输出为独立可写字节区，容量包含 NUL；类型图及其 VM 名称保持有效。
 * @return 完整格式写入成功返回 true；失败时可写非零输出清空。
 * @note use-site 别名通过独立查询提供，不参与此规范格式。
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
 * @brief 为已登记符号优先展示匹配声明签名，否则组合名称与规范类型。
 * @pre 当前快照及 VM 文本有效；输出区独立可写，容量包含 NUL。
 * @return 完整标签写入返回 true；身份、类型或容量不足返回 false。
 * @note 输出只在入口清空；snprintf 失败可能留下截断文本，必须忽略 false 的内容。
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
 * @brief 为调用方属性契约展示类型、接收者/引用修饰及访问器存在性。
 * @pre 契约与引用的符号/类型属于当前有效快照；输出区独立可写，容量包含 NUL。
 * @return 属性 symbol/type、枚举及各已提供访问器的规范函数身份可用且完整写入时返回 true。
 * @note 至少一个访问器必须存在；不验证完整属性签名关系或事实表成员身份。
 * 入口清空后仍可能留下截断文本，调用方必须忽略 false 的内容。
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
