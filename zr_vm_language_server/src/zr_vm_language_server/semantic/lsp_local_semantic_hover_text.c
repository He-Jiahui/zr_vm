#include "semantic/lsp_local_semantic_hover_text.h"

#include "semantic/lsp_local_semantic_expression_text.h"
#include "semantic/lsp_numeric_range_text.h"

#include "zr_vm_language_server/conf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static TZrBool lsp_local_hover_text_append_format(TZrChar *buffer,
                                                  TZrSize bufferSize,
                                                  TZrSize *used,
                                                  const TZrChar *format,
                                                  ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || used == ZR_NULL || format == ZR_NULL || *used >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);

    if (written < 0 || (TZrSize)written >= bufferSize - *used) {
        buffer[bufferSize - 1u] = '\0';
        return ZR_FALSE;
    }

    *used += (TZrSize)written;
    return ZR_TRUE;
}

static const TZrChar *lsp_local_hover_text_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool lsp_local_hover_text_append_numeric(TZrChar *buffer,
                                                   TZrSize bufferSize,
                                                   TZrSize *used,
                                                   const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasRange) {
        if (!lsp_local_hover_text_append_format(buffer, bufferSize, used, "\n\nNumeric range: ") ||
            !ZrLanguageServer_LspNumericRangeText_AppendRange(
                buffer,
                bufferSize,
                used,
                fact,
                ZR_FALSE)) {
            return ZR_FALSE;
        }
    }

    if (fact->hasUnsignedRange &&
        !lsp_local_hover_text_append_format(buffer,
                                            bufferSize,
                                            used,
                                            "\n\nUnsigned range: %llu..%llu",
                                            (unsigned long long)fact->minUnsignedValue,
                                            (unsigned long long)fact->maxUnsignedValue)) {
        return ZR_FALSE;
    }

    if (fact->mayOverflow &&
        !lsp_local_hover_text_append_format(buffer, bufferSize, used, "\n\nNumeric warning: may overflow")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool lsp_local_hover_text_append_logical(TZrChar *buffer,
                                                   TZrSize bufferSize,
                                                   TZrSize *used,
                                                   const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasKnownValue &&
        !lsp_local_hover_text_append_format(buffer,
                                            bufferSize,
                                            used,
                                            "\n\nLogical value: %s",
                                            fact->knownValue ? "true" : "false")) {
        return ZR_FALSE;
    }

    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT &&
        !lsp_local_hover_text_append_format(buffer,
                                            bufferSize,
                                            used,
                                            "\n\nLogical flow: short-circuits right operand")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool lsp_local_hover_text_append_reachability(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticReachabilityFact *fact) {
    const TZrChar *causeText = ZR_NULL;

    if (fact == ZR_NULL || fact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE) {
        return ZR_TRUE;
    }

    switch (fact->cause) {
        case ZR_SEMANTIC_REACHABILITY_AFTER_RETURN:
            causeText = "after return";
            break;
        case ZR_SEMANTIC_REACHABILITY_AFTER_THROW:
            causeText = "after throw";
            break;
        case ZR_SEMANTIC_REACHABILITY_AFTER_BREAK:
            causeText = "after break";
            break;
        case ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE:
            causeText = "after continue";
            break;
        case ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE:
            causeText = "because the condition is false";
            break;
        case ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH:
            causeText = "because a constant branch excludes it";
            break;
        case ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT:
            causeText = "because short-circuit logic skips it";
            break;
        case ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH:
            causeText = "after exhaustive branch";
            break;
        case ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP:
            causeText = "after non-fallthrough loop";
            break;
        case ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN:
        default:
            causeText = ZR_NULL;
            break;
    }

    if (causeText != ZR_NULL) {
        return lsp_local_hover_text_append_format(buffer,
                                                  bufferSize,
                                                  used,
                                                  "\n\nReachability: unreachable %s",
                                                  causeText);
    }

    return lsp_local_hover_text_append_format(buffer, bufferSize, used, "\n\nReachability: unreachable");
}

static const TZrChar *lsp_local_hover_text_reference_kind(EZrSemanticReferenceKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_REFERENCE_DECLARATION:
            return "declaration";
        case ZR_SEMANTIC_REFERENCE_READ:
            return "read";
        case ZR_SEMANTIC_REFERENCE_WRITE:
            return "write";
        case ZR_SEMANTIC_REFERENCE_CALL:
            return "call";
        case ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS:
            return "member access";
        case ZR_SEMANTIC_REFERENCE_MEMBER_WRITE:
            return "member write";
        case ZR_SEMANTIC_REFERENCE_UNKNOWN:
        default:
            return "unknown";
    }
}

static TZrBool lsp_local_hover_text_append_reference(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticReferenceFact *fact) {
    const TZrChar *symbolName;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!lsp_local_hover_text_append_format(buffer,
                                            bufferSize,
                                            used,
                                            "\n\nReference: %s",
                                            lsp_local_hover_text_reference_kind(fact->kind))) {
        return ZR_FALSE;
    }

    symbolName = lsp_local_hover_text_string_text(fact->name);
    if (symbolName != ZR_NULL &&
        symbolName[0] != '\0' &&
        !lsp_local_hover_text_append_format(buffer, bufferSize, used, "\n\nSymbol: %s", symbolName)) {
        return ZR_FALSE;
    }

    if (fact->isResolved &&
        fact->declarationRange.start.offset != fact->declarationRange.end.offset &&
        !lsp_local_hover_text_append_format(buffer,
                                            bufferSize,
                                            used,
                                            "\n\nDeclared at: %d:%d",
                                            (int)fact->declarationRange.start.line,
                                            (int)fact->declarationRange.start.column)) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool lsp_local_hover_text_append_ownership(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticOwnershipFact *fact) {
    const TZrChar *message;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    message = lsp_local_hover_text_string_text(fact->diagnosticMessage);
    if (fact->isViolation && message != ZR_NULL && message[0] != '\0') {
        return lsp_local_hover_text_append_format(buffer,
                                                  bufferSize,
                                                  used,
                                                  "\n\nOwnership: violation - %s",
                                                  message);
    }

    if (fact->isViolation) {
        return lsp_local_hover_text_append_format(buffer, bufferSize, used, "\n\nOwnership: violation");
    }

    return ZR_TRUE;
}

static TZrBool lsp_local_hover_text_append_expression_payload(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrSemanticExpressionFact *fact) {
    const TZrChar *callName;
    const TZrChar *memberName;

    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasCallInfo) {
        callName = lsp_local_hover_text_string_text(fact->callTargetName);
        if (!lsp_local_hover_text_append_format(
                buffer,
                bufferSize,
                used,
                "\n\nCall: %s args=%llu",
                callName != ZR_NULL && callName[0] != '\0' ? callName : "unknown",
                (unsigned long long)fact->argumentCount)) {
            return ZR_FALSE;
        }
    }

    if (fact->hasMemberInfo) {
        memberName = lsp_local_hover_text_string_text(fact->memberName);
        if (!lsp_local_hover_text_append_format(
                buffer,
                bufferSize,
                used,
                "\n\nMember: %s",
                memberName != ZR_NULL && memberName[0] != '\0'
                    ? memberName
                    : (fact->memberIsComputed ? "computed" : "unknown"))) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspLocalSemanticHoverText_AppendFacts(
    TZrChar *buffer,
    TZrSize bufferSize,
    TZrSize *used,
    const SZrLspLocalSemanticQueryResult *query) {
    if (query == ZR_NULL) {
        return ZR_TRUE;
    }

    return ZrLanguageServer_LspLocalSemanticExpressionText_AppendHover(buffer,
                                                                       bufferSize,
                                                                       used,
                                                                       query->expressionFact) &&
           lsp_local_hover_text_append_numeric(buffer, bufferSize, used, query->numericFact) &&
           lsp_local_hover_text_append_logical(buffer, bufferSize, used, query->logicalFact) &&
           lsp_local_hover_text_append_reachability(buffer, bufferSize, used, query->reachabilityFact) &&
           lsp_local_hover_text_append_reference(buffer, bufferSize, used, query->referenceFact) &&
           lsp_local_hover_text_append_ownership(buffer, bufferSize, used, query->ownershipFact) &&
           lsp_local_hover_text_append_expression_payload(buffer, bufferSize, used, query->expressionFact);
}

SZrString *ZrLanguageServer_LspLocalSemanticHoverText_BuildFactMarkdown(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query) {
    TZrChar markdown[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize used = 0;

    if (state == ZR_NULL ||
        query == ZR_NULL ||
        query->status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
        return ZR_NULL;
    }

    markdown[0] = '\0';
    if (!ZrLanguageServer_LspLocalSemanticHoverText_AppendFacts(markdown,
                                                                sizeof(markdown),
                                                                &used,
                                                                query)) {
        return ZR_NULL;
    }

    while (used > 0 && (markdown[0] == '\n' || markdown[0] == '\r')) {
        memmove(markdown, markdown + 1, used);
        used--;
    }
    if (used == 0) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, markdown, used);
}

SZrString *ZrLanguageServer_LspLocalSemanticHoverText_AppendMarkdownSection(
    SZrState *state,
    SZrString *base,
    SZrString *appendix) {
    const TZrChar *baseText;
    const TZrChar *appendixText;
    TZrSize baseLength;
    TZrSize appendixLength;
    TZrChar buffer[ZR_LSP_MARKDOWN_BUFFER_SIZE];
    TZrSize used = 0;

    if (state == ZR_NULL || base == ZR_NULL || appendix == ZR_NULL) {
        return base;
    }

    baseText = lsp_local_hover_text_string_text(base);
    appendixText = lsp_local_hover_text_string_text(appendix);
    baseLength = ZrCore_String_GetByteLength(base);
    appendixLength = ZrCore_String_GetByteLength(appendix);
    if (baseText == ZR_NULL || appendixText == ZR_NULL || appendixLength == 0) {
        return base;
    }

    if (strstr(baseText, appendixText) != ZR_NULL) {
        return base;
    }

    if (baseLength + appendixLength + 3 >= sizeof(buffer)) {
        return base;
    }

    memcpy(buffer + used, baseText, baseLength);
    used += baseLength;
    memcpy(buffer + used, "\n\n", 2);
    used += 2;
    memcpy(buffer + used, appendixText, appendixLength);
    used += appendixLength;
    buffer[used] = '\0';
    return ZrCore_String_Create(state, buffer, used);
}
