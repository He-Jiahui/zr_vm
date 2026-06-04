#include "stdio_inline_value_semantic_text.h"

#include "semantic/lsp_local_semantic_query.h"

#include <stdio.h>
#include <string.h>

static int inline_value_append_text(char *buffer,
                                    size_t bufferSize,
                                    size_t *used,
                                    const char *text) {
    size_t textLength;

    if (buffer == NULL || bufferSize == 0 || used == NULL || text == NULL) {
        return 0;
    }

    textLength = strlen(text);
    if (*used + textLength >= bufferSize) {
        buffer[bufferSize - 1] = '\0';
        return 0;
    }

    memcpy(buffer + *used, text, textLength);
    *used += textLength;
    buffer[*used] = '\0';
    return 1;
}

static int inline_value_append_separator(char *buffer, size_t bufferSize, size_t *used) {
    if (used != NULL && *used > 0) {
        return inline_value_append_text(buffer, bufferSize, used, ", ");
    }

    return 1;
}

static int inline_value_append_numeric_fact(char *buffer,
                                            size_t bufferSize,
                                            size_t *used,
                                            const SZrSemanticNumericFact *fact) {
    char segment[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (fact == ZR_NULL) {
        return 1;
    }

    if (fact->hasRange) {
        if (!inline_value_append_separator(buffer, bufferSize, used)) {
            return 0;
        }
        if (fact->sourceType == ZR_VALUE_TYPE_FLOAT ||
            fact->sourceType == ZR_VALUE_TYPE_DOUBLE ||
            fact->targetType == ZR_VALUE_TYPE_FLOAT ||
            fact->targetType == ZR_VALUE_TYPE_DOUBLE) {
            snprintf(segment,
                     sizeof(segment),
                     "range %.17g..%.17g",
                     fact->minDoubleValue,
                     fact->maxDoubleValue);
        } else {
            snprintf(segment,
                     sizeof(segment),
                     "range %lld..%lld",
                     (long long)fact->minValue,
                     (long long)fact->maxValue);
        }
        if (!inline_value_append_text(buffer, bufferSize, used, segment)) {
            return 0;
        }
    }

    if (fact->hasUnsignedRange) {
        if (!inline_value_append_separator(buffer, bufferSize, used)) {
            return 0;
        }
        snprintf(segment,
                 sizeof(segment),
                 "unsigned %llu..%llu",
                 (unsigned long long)fact->minUnsignedValue,
                 (unsigned long long)fact->maxUnsignedValue);
        if (!inline_value_append_text(buffer, bufferSize, used, segment)) {
            return 0;
        }
    }

    if (fact->mayOverflow) {
        if (!inline_value_append_separator(buffer, bufferSize, used) ||
            !inline_value_append_text(buffer, bufferSize, used, "may overflow")) {
            return 0;
        }
    }

    return 1;
}

static int inline_value_append_logical_fact(char *buffer,
                                            size_t bufferSize,
                                            size_t *used,
                                            const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return 1;
    }

    if (fact->hasKnownValue) {
        if (!inline_value_append_separator(buffer, bufferSize, used) ||
            !inline_value_append_text(buffer,
                                      bufferSize,
                                      used,
                                      fact->knownValue ? "logical true" : "logical false")) {
            return 0;
        }
    }

    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT) {
        if (!inline_value_append_separator(buffer, bufferSize, used) ||
            !inline_value_append_text(buffer, bufferSize, used, "short-circuits")) {
            return 0;
        }
    }

    return 1;
}

static const char *inline_value_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static int inline_value_append_expression_payload(char *buffer,
                                                  size_t bufferSize,
                                                  size_t *used,
                                                  const SZrSemanticExpressionFact *fact) {
    char segment[ZR_LSP_TEXT_BUFFER_LENGTH];
    const char *callName;
    const char *memberName;

    if (fact == ZR_NULL) {
        return 1;
    }

    if (fact->hasCallInfo) {
        callName = inline_value_string_text(fact->callTargetName);
        if (!inline_value_append_separator(buffer, bufferSize, used)) {
            return 0;
        }
        snprintf(segment,
                 sizeof(segment),
                 "call %s args=%llu",
                 callName != ZR_NULL && callName[0] != '\0' ? callName : "unknown",
                 (unsigned long long)fact->argumentCount);
        if (!inline_value_append_text(buffer, bufferSize, used, segment)) {
            return 0;
        }
    }

    if (fact->hasMemberInfo) {
        memberName = inline_value_string_text(fact->memberName);
        if (!inline_value_append_separator(buffer, bufferSize, used)) {
            return 0;
        }
        snprintf(segment,
                 sizeof(segment),
                 "member %s",
                 memberName != ZR_NULL && memberName[0] != '\0'
                     ? memberName
                     : (fact->memberIsComputed ? "computed" : "unknown"));
        if (!inline_value_append_text(buffer, bufferSize, used, segment)) {
            return 0;
        }
    }

    return 1;
}

static const char *inline_value_reference_kind_text(EZrSemanticReferenceKind kind) {
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

static int inline_value_append_reference_fact(char *buffer,
                                              size_t bufferSize,
                                              size_t *used,
                                              const SZrSemanticReferenceFact *fact) {
    char segment[ZR_LSP_TEXT_BUFFER_LENGTH];

    if (fact == ZR_NULL) {
        return 1;
    }

    if (!inline_value_append_separator(buffer, bufferSize, used)) {
        return 0;
    }

    snprintf(segment,
             sizeof(segment),
             "reference %s",
             inline_value_reference_kind_text(fact->kind));
    return inline_value_append_text(buffer, bufferSize, used, segment);
}

static int inline_value_range_is_non_empty(SZrLspRange range) {
    if (range.end.line > range.start.line) {
        return 1;
    }

    return range.end.line == range.start.line &&
           range.end.character > range.start.character;
}

cJSON *ZrStdioInlineValue_CreateSemanticTextForLspRange(SZrStdioServer *server,
                                                        SZrString *uri,
                                                        SZrLspRange range,
                                                        SZrLspPosition queryPosition) {
    SZrLspLocalSemanticQueryResult query;
    cJSON *json;
    char factText[ZR_LSP_TEXT_BUFFER_LENGTH];
    size_t used = 0;

    if (server == ZR_NULL || uri == ZR_NULL || !inline_value_range_is_non_empty(range)) {
        return NULL;
    }

    ZrLanguageServer_LspLocalSemanticQuery_Init(&query);
    if (!ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(server->state,
                                                             server->context,
                                                             uri,
                                                             queryPosition,
                                                             &query) ||
        query.status != ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT) {
        return NULL;
    }

    factText[0] = '\0';
    if (!inline_value_append_numeric_fact(factText, sizeof(factText), &used, query.numericFact) ||
        !inline_value_append_logical_fact(factText, sizeof(factText), &used, query.logicalFact) ||
        !inline_value_append_expression_payload(factText, sizeof(factText), &used, query.expressionFact) ||
        !inline_value_append_reference_fact(factText, sizeof(factText), &used, query.referenceFact) ||
        used == 0) {
        return NULL;
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        return NULL;
    }

    cJSON_AddItemToObject(json, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(json, ZR_LSP_FIELD_TEXT, factText);
    return json;
}
