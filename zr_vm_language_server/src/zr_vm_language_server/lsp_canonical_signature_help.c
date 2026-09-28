#include "lsp_canonical_signature_help.h"

#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

/* 参数标签与 parser 的 canonical 传参约束保持一致；scoped 只在引用不能越过当前函数时显示。 */
static const TZrChar *canonical_signature_help_passing_prefix(
        const SZrCanonicalParameterContract *contract) {
    if (contract == ZR_NULL) {
        return "";
    }
    switch (contract->passingForm) {
        case ZR_CANONICAL_PASSING_IN: return "in ";
        case ZR_CANONICAL_PASSING_REF:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref "
                           : "ref ";
        case ZR_CANONICAL_PASSING_REF_READONLY:
            return contract->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                           ? "scoped ref readonly "
                           : "ref readonly ";
        case ZR_CANONICAL_PASSING_OUT: return "out ";
        case ZR_CANONICAL_PASSING_VALUE:
        default: return "";
    }
}

/* 非值参数的 typeId 是引用包装，标签中展示被引用值类型以免与传参前缀重复。 */
static TZrTypeId canonical_signature_help_parameter_value_type_id(
        const SZrSemanticContext *context,
        const SZrCanonicalParameterContract *contract) {
    const SZrCanonicalTypeNode *type;

    if (context == ZR_NULL || contract == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    if (contract->passingForm == ZR_CANONICAL_PASSING_VALUE) {
        return contract->typeId;
    }
    type = ZrParser_CanonicalType_Find(context, contract->typeId);
    return type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_REF
                   ? type->data.refType.pointeeTypeId
                   : ZR_SEMANTIC_ID_INVALID;
}

/* 外层签名已由 PopulateFromLabel 建好；此处只从同一语义快照补齐参数契约和实参事实。
 * 中途失败由 Resolve 统一释放已附加的参数，避免把半成品签名交给客户端。 */
static TZrBool canonical_signature_help_append_parameters(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrCanonicalTypeNode *functionType,
        SZrAstNodeArray *argumentNodes,
        SZrLspSignatureHelp *help) {
    SZrLspSignatureInformation **signaturePtr;
    SZrLspSignatureInformation *signature;
    TZrSize index;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || functionType == ZR_NULL ||
        functionType->kind != ZR_CANONICAL_TYPE_FUNCTION || help == ZR_NULL ||
        help->signatures.length == 0) {
        return ZR_FALSE;
    }

    signaturePtr = (SZrLspSignatureInformation **)ZrCore_Array_Get(
            &help->signatures,
            0);
    signature = signaturePtr != ZR_NULL ? *signaturePtr : ZR_NULL;
    if (signature == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0;
         index < functionType->data.function.parameterContracts.length;
         index++) {
        const SZrCanonicalParameterContract *contract =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&functionType->data.function.parameterContracts,
                        index);
        SZrLspParameterInformation *parameter;
        TZrTypeId valueTypeId;
        TZrChar typeLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        TZrChar parameterLabel[ZR_LSP_TEXT_BUFFER_LENGTH];
        int written;

        valueTypeId = canonical_signature_help_parameter_value_type_id(
                analyzer->semanticContext,
                contract);
        if (valueTypeId == ZR_SEMANTIC_ID_INVALID ||
            !ZrParser_CanonicalType_Format(
                    analyzer->semanticContext,
                    valueTypeId,
                    typeLabel,
                    sizeof(typeLabel))) {
            return ZR_FALSE;
        }
        written = snprintf(
                parameterLabel,
                sizeof(parameterLabel),
                "%s%s",
                canonical_signature_help_passing_prefix(contract),
                typeLabel);
        if (written < 0 || (TZrSize)written >= sizeof(parameterLabel)) {
            return ZR_FALSE;
        }

        parameter = (SZrLspParameterInformation *)ZrCore_Memory_RawMalloc(
                state->global,
                sizeof(SZrLspParameterInformation));
        if (parameter == ZR_NULL) {
            return ZR_FALSE;
        }
        memset(parameter, 0, sizeof(*parameter));
        parameter->label = ZrCore_String_Create(
                state,
                parameterLabel,
                (TZrSize)written);
        if (parameter->label == ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, parameter, sizeof(*parameter));
            return ZR_FALSE;
        }
        parameter->documentation =
                argumentNodes != ZR_NULL && index < argumentNodes->count
                        ? ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
                                  state,
                                  analyzer,
                                  argumentNodes->nodes[index])
                        : ZR_NULL;
        ZrCore_Array_Push(state, &signature->parameters, &parameter);
    }

    return ZR_TRUE;
}

/* 源码调用和构造调用共用 parser 的 CallAt/FormatCall；拒绝从 AST 名称重建缺失的规范事实。 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrAstNodeArray *argumentNodes,
        TZrInt32 activeParameter,
        SZrLspSignatureHelp **result) {
    SZrParserSemanticCallQuery query;
    const SZrCanonicalTypeNode *functionType;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(analyzer->semanticContext, position, ZR_NULL, &query) ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext, &query, label, sizeof(label))) {
        return ZR_FALSE;
    }
    functionType = ZrParser_CanonicalType_Find(
            analyzer->semanticContext,
            query.callableTypeId);
    if (functionType == ZR_NULL ||
        !ZrLanguageServer_LspSignatureHelp_PopulateFromLabel(state,
                                                             analyzer,
                                                             label,
                                                             ZR_NULL,
                                                             argumentNodes,
                                                             ZR_NULL,
                                                             activeParameter,
                                                             result) ||
        !canonical_signature_help_append_parameters(
                state,
                analyzer,
                functionType,
                argumentNodes,
                *result)) {
        if (result != ZR_NULL && *result != ZR_NULL) {
            ZrLanguageServer_LspSignatureHelp_Free(state, *result);
            *result = ZR_NULL;
        }
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/* hover 的命中范围取已解析的调用引用，而非光标点；失败时保留调用方原范围。 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_TryGetResolvedCallReferenceRange(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrFileRange *result) {
    SZrParserSemanticCallQuery query;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, position, ZR_NULL, &query) ||
        !query.hasResolvedTarget || query.reference == ZR_NULL ||
        !query.reference->isResolved) {
        return ZR_FALSE;
    }
    *result = query.reference->range;
    return ZR_TRUE;
}

/* 已知可调用值的 hover 路径先行；此处阻止剩余未解析非成员调用进入弱来源兜底。 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_HasUnavailableLocalCall(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position) {
    const SZrSemanticReferenceFact *reference;
    TZrSize index;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }
    reference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
            analyzer->semanticContext,
            position,
            ZR_SEMANTIC_REFERENCE_CALL);
    if (reference == ZR_NULL || reference->isResolved) {
        return ZR_FALSE;
    }
    for (index = 0U;
         index < analyzer->semanticContext->expressionFacts.length;
         index++) {
        const SZrSemanticExpressionFact *expression =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(
                        &analyzer->semanticContext->expressionFacts,
                        index);
        if (expression != ZR_NULL && !expression->isMemberCall &&
            expression->callTargetRange.source == reference->range.source &&
            expression->callTargetRange.start.offset == reference->range.start.offset &&
            expression->callTargetRange.end.offset == reference->range.end.offset) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 已解析的接收者调用优先沿 parser 的引用和函数契约生成 hover，与签名帮助共享标签来源。 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveReceiverHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result) {
    SZrParserSemanticCallQuery query;
    const SZrCanonicalTypeNode *functionType;
    SZrLspHover *hover;
    SZrString *content;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar markdown[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    int written;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || uri == ZR_NULL || result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, position, ZR_NULL, &query) ||
        !query.hasResolvedTarget || query.reference == ZR_NULL ||
        !query.reference->isResolved || query.reference->hasExternalTarget ||
        position.start.offset < query.reference->range.start.offset ||
        position.start.offset > query.reference->range.end.offset ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext, &query, label, sizeof(label))) {
        return ZR_FALSE;
    }
    functionType = ZrParser_CanonicalType_Find(
            analyzer->semanticContext,
            query.callableTypeId);
    if (functionType == ZR_NULL ||
        functionType->kind != ZR_CANONICAL_TYPE_FUNCTION ||
        functionType->data.function.receiverEffect == ZR_CANONICAL_RECEIVER_NONE) {
        return ZR_FALSE;
    }

    written = snprintf(markdown, sizeof(markdown), "**call**\n\nSignature: %s", label);
    if (written < 0 || (TZrSize)written >= sizeof(markdown)) {
        return ZR_FALSE;
    }
    content = ZrCore_String_Create(state, markdown, (TZrSize)written);
    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrLspHover));
    if (content == ZR_NULL || hover == ZR_NULL) {
        if (hover != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, hover, sizeof(*hover));
        }
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1U);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            uri,
            query.reference->range);
    *result = hover;
    return ZR_TRUE;
}

/* 可调用值可能没有外部声明身份；只借用 parser 已记录的调用类型与标签展示 hover。 */
TZrBool ZrLanguageServer_LspCanonicalSignatureHelp_ResolveExternalCallableHover(
        SZrState *state,
        SZrLspContext *context,
        SZrSemanticAnalyzer *analyzer,
        SZrString *uri,
        SZrFileRange position,
        SZrLspHover **result) {
    SZrParserSemanticCallQuery query;
    const SZrCanonicalTypeNode *functionType;
    SZrLspHover *hover;
    SZrString *content;
    TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar markdown[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    int written;

    if (state == ZR_NULL || context == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || uri == ZR_NULL || result == ZR_NULL ||
        !ZrParser_SemanticQuery_CallAt(
                analyzer->semanticContext, position, ZR_NULL, &query) ||
        query.expression == ZR_NULL || query.reference == ZR_NULL ||
        query.expression->isMemberCall || query.reference->isResolved ||
        query.hasResolvedTarget ||
        position.start.offset < query.reference->range.start.offset ||
        position.start.offset > query.reference->range.end.offset ||
        !ZrParser_SemanticQuery_FormatCall(
                analyzer->semanticContext, &query, label, sizeof(label))) {
        return ZR_FALSE;
    }
    /* TODO: 这里只凭“未解析、非成员调用、函数类型”识别可调用值；
     * 需核对 parser 是否也会为未解析的本地直接调用给出同形事实，
     * 并在 GetHover 中先于 HasUnavailableLocalCall 的路径补充区分测试。 */
    functionType = ZrParser_CanonicalType_Find(
            analyzer->semanticContext,
            query.callableTypeId);
    if (functionType == ZR_NULL ||
        functionType->kind != ZR_CANONICAL_TYPE_FUNCTION) {
        return ZR_FALSE;
    }

    written = snprintf(markdown, sizeof(markdown), "**call**\n\nSignature: %s", label);
    if (written < 0 || (TZrSize)written >= sizeof(markdown)) {
        return ZR_FALSE;
    }
    content = ZrCore_String_Create(state, markdown, (TZrSize)written);
    hover = (SZrLspHover *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrLspHover));
    if (content == ZR_NULL || hover == ZR_NULL) {
        if (hover != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, hover, sizeof(*hover));
        }
        return ZR_FALSE;
    }

    ZrCore_Array_Init(state, &hover->contents, sizeof(SZrString *), 1U);
    ZrCore_Array_Push(state, &hover->contents, &content);
    hover->range = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(
            context,
            uri,
            query.reference->range);
    *result = hover;
    return ZR_TRUE;
}
