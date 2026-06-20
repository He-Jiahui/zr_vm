#include "debug_internal.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/type_inference.h"

static void zr_debug_semantic_append_fragment(TZrChar *buffer, TZrSize bufferSize, const TZrChar *fragment) {
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0 || fragment == ZR_NULL || fragment[0] == '\0') {
        return;
    }

    length = strlen(buffer);
    if (length == 0) {
        snprintf(buffer, bufferSize, "%s", fragment);
    } else if (length + 2u < bufferSize) {
        snprintf(buffer + length, bufferSize - length, ", %s", fragment);
    }
    buffer[bufferSize - 1u] = '\0';
}

static const TZrChar *zr_debug_semantic_expression_kind_text(EZrSemanticExpressionFactKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_EXPRESSION_FACT_LITERAL:
            return "literal";
        case ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER:
            return "identifier";
        case ZR_SEMANTIC_EXPRESSION_FACT_BINARY:
            return "binary";
        case ZR_SEMANTIC_EXPRESSION_FACT_UNARY:
            return "unary";
        case ZR_SEMANTIC_EXPRESSION_FACT_CALL:
            return "call";
        case ZR_SEMANTIC_EXPRESSION_FACT_MEMBER:
            return "member";
        case ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT:
            return "assignment";
        case ZR_SEMANTIC_EXPRESSION_FACT_CONDITIONAL:
            return "conditional";
        case ZR_SEMANTIC_EXPRESSION_FACT_ARRAY:
            return "array";
        case ZR_SEMANTIC_EXPRESSION_FACT_OBJECT:
            return "object";
        case ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA:
            return "lambda";
        case ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN:
            return "ownership";
        case ZR_SEMANTIC_EXPRESSION_FACT_CONVERSION:
            return "conversion";
        case ZR_SEMANTIC_EXPRESSION_FACT_ERROR:
            return "error";
        case ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

static const TZrChar *zr_debug_semantic_exactness_text(EZrSemanticFactExactness exactness) {
    switch (exactness) {
        case ZR_SEMANTIC_FACT_EXACT:
            return "exact";
        case ZR_SEMANTIC_FACT_APPROXIMATE:
            return "approximate";
        case ZR_SEMANTIC_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

static const TZrChar *zr_debug_semantic_reference_kind_text(EZrSemanticReferenceKind kind) {
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

static const TZrChar *zr_debug_semantic_ownership_kind_text(EZrSemanticOwnershipFactKind kind) {
    switch (kind) {
        case ZR_SEMANTIC_OWNERSHIP_FACT_DECLARATION:
            return "declaration";
        case ZR_SEMANTIC_OWNERSHIP_FACT_BORROW:
            return "borrow";
        case ZR_SEMANTIC_OWNERSHIP_FACT_MOVE:
            return "move";
        case ZR_SEMANTIC_OWNERSHIP_FACT_COPY:
            return "copy";
        case ZR_SEMANTIC_OWNERSHIP_FACT_RELEASE:
            return "release";
        case ZR_SEMANTIC_OWNERSHIP_FACT_ERROR:
            return "error";
        case ZR_SEMANTIC_OWNERSHIP_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

static const TZrChar *zr_debug_semantic_ownership_qualifier_text(EZrOwnershipQualifier qualifier) {
    switch (qualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE:
            return "%unique";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "%shared";
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "%weak";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "%borrowed";
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return "%loaned";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return "plain";
    }
}

static TZrChar *zr_debug_semantic_build_expression_statement(const TZrChar *expression) {
    const TZrChar *begin;
    const TZrChar *end;
    TZrSize expressionLength;
    TZrBool hasSemicolon;
    TZrChar *source;

    if (expression == ZR_NULL) {
        return ZR_NULL;
    }

    begin = expression;
    while (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n') {
        begin++;
    }
    if (*begin == '\0') {
        return ZR_NULL;
    }

    end = begin + strlen(begin);
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }

    hasSemicolon = (TZrBool)(end > begin && end[-1] == ';');
    expressionLength = (TZrSize)(end - begin);
    source = (TZrChar *)malloc(expressionLength + (hasSemicolon ? 1u : 2u));
    if (source == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(source, begin, expressionLength);
    if (!hasSemicolon) {
        source[expressionLength++] = ';';
    }
    source[expressionLength] = '\0';
    return source;
}

static SZrAstNode *zr_debug_semantic_last_expression_statement(SZrAstNode *ast) {
    SZrAstNode *statement;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    statement = ast->data.script.statements->nodes[ast->data.script.statements->count - 1u];
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT) {
        return ZR_NULL;
    }

    return statement->data.expressionStatement.expr;
}

static const TZrChar *zr_debug_semantic_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? ZrCore_String_GetNativeStringShort(value)
                   : ZrCore_String_GetNativeString(value);
}

static void zr_debug_semantic_append_fragment_text(TZrChar *fragment,
                                                   TZrSize fragmentSize,
                                                   TZrSize *used,
                                                   const TZrChar *text) {
    TZrSize remaining;
    int written;

    if (fragment == ZR_NULL || fragmentSize == 0 || used == ZR_NULL || text == ZR_NULL) {
        return;
    }
    if (*used >= fragmentSize) {
        fragment[fragmentSize - 1u] = '\0';
        return;
    }

    remaining = fragmentSize - *used;
    written = snprintf(fragment + *used, remaining, "%s", text);
    if (written < 0 || (TZrSize)written >= remaining) {
        *used = fragmentSize - 1u;
    } else {
        *used += (TZrSize)written;
    }
    fragment[fragmentSize - 1u] = '\0';
}

static void zr_debug_semantic_append_fragment_byte(TZrChar *fragment,
                                                   TZrSize fragmentSize,
                                                   TZrSize *used,
                                                   unsigned char byte) {
    TZrChar text[5];

    if (byte < 0x20u || byte == 0x7fu) {
        snprintf(text, sizeof(text), "\\x%02X", (unsigned int)byte);
        text[sizeof(text) - 1u] = '\0';
        zr_debug_semantic_append_fragment_text(fragment, fragmentSize, used, text);
        return;
    }

    text[0] = (TZrChar)byte;
    text[1] = '\0';
    zr_debug_semantic_append_fragment_text(fragment, fragmentSize, used, text);
}

static void zr_debug_semantic_append_escaped_string_constant(SZrString *value,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *text = zr_debug_semantic_string_text(value);
    TZrSize length = value != ZR_NULL ? ZrCore_String_GetByteLength(value) : 0u;
    TZrSize used = 0;
    TZrSize index;

    fragment[0] = '\0';
    zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "constant \"");
    for (index = 0; text != ZR_NULL && index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        switch (byte) {
            case '"':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\\"");
                break;
            case '\\':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\\\");
                break;
            case '\n':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\n");
                break;
            case '\t':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\t");
                break;
            case '\r':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\r");
                break;
            case '\b':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\b");
                break;
            case '\f':
                zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\\f");
                break;
            default:
                zr_debug_semantic_append_fragment_byte(fragment, sizeof(fragment), &used, byte);
                break;
        }
    }
    zr_debug_semantic_append_fragment_text(fragment, sizeof(fragment), &used, "\"");
    zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
}

static void zr_debug_semantic_append_expression_fact(SZrState *state,
                                                     const SZrSemanticExpressionFact *fact,
                                                     TZrChar *buffer,
                                                     TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];
    TZrChar typeBuffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *name;
    const TZrChar *typeName;

    if (fact == ZR_NULL) {
        return;
    }

    snprintf(fragment,
             sizeof(fragment),
             "expression %s %s",
             zr_debug_semantic_expression_kind_text(fact->kind),
             zr_debug_semantic_exactness_text(fact->exactness));
    fragment[sizeof(fragment) - 1u] = '\0';
    zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);

    typeName = ZrParser_TypeNameString_Get(state, &fact->inferredType, typeBuffer, sizeof(typeBuffer));
    if (typeName != ZR_NULL && typeName[0] != '\0') {
        snprintf(fragment, sizeof(fragment), "type %s", typeName);
        fragment[sizeof(fragment) - 1u] = '\0';
        zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
    }

    if (fact->hasCallInfo) {
        name = zr_debug_semantic_string_text(fact->callTargetName);
        snprintf(fragment,
                 sizeof(fragment),
                 "call %s args=%llu",
                 name[0] != '\0' ? name : "unknown",
                 (unsigned long long)fact->argumentCount);
        fragment[sizeof(fragment) - 1u] = '\0';
        zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
    }

    if (fact->hasMemberInfo) {
        name = zr_debug_semantic_string_text(fact->memberName);
        snprintf(fragment,
                 sizeof(fragment),
                 "member %s",
                 name[0] != '\0' ? name : "computed");
        fragment[sizeof(fragment) - 1u] = '\0';
        zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
    }

    if (!fact->hasConstant) {
        return;
    }

    switch (fact->valueKind) {
        case ZR_SEMANTIC_VALUE_KIND_BOOL:
            snprintf(fragment, sizeof(fragment), "constant %s", fact->constantValue.boolValue ? "true" : "false");
            break;
        case ZR_SEMANTIC_VALUE_KIND_INT64:
            snprintf(fragment, sizeof(fragment), "constant %lld", (long long)fact->constantValue.int64Value);
            break;
        case ZR_SEMANTIC_VALUE_KIND_UINT64:
            snprintf(fragment, sizeof(fragment), "constant %llu", (unsigned long long)fact->constantValue.uint64Value);
            break;
        case ZR_SEMANTIC_VALUE_KIND_DOUBLE:
            snprintf(fragment, sizeof(fragment), "constant %.17g", fact->constantValue.doubleValue);
            break;
        case ZR_SEMANTIC_VALUE_KIND_STRING:
            zr_debug_semantic_append_escaped_string_constant(fact->constantValue.stringValue, buffer, bufferSize);
            return;
        case ZR_SEMANTIC_VALUE_KIND_NULL:
            snprintf(fragment, sizeof(fragment), "constant null");
            break;
        case ZR_SEMANTIC_VALUE_KIND_UNKNOWN:
        default:
            fragment[0] = '\0';
            break;
    }
    fragment[sizeof(fragment) - 1u] = '\0';
    zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
}

static void zr_debug_semantic_append_numeric_fact(const SZrSemanticNumericFact *fact,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];

    if (fact == ZR_NULL) {
        return;
    }

    if (fact->hasRange) {
        if (fact->sourceType == ZR_VALUE_TYPE_DOUBLE ||
            fact->targetType == ZR_VALUE_TYPE_DOUBLE ||
            fact->sourceType == ZR_VALUE_TYPE_FLOAT ||
            fact->targetType == ZR_VALUE_TYPE_FLOAT) {
            snprintf(fragment, sizeof(fragment), "range %.17g..%.17g", fact->minDoubleValue, fact->maxDoubleValue);
        } else {
            snprintf(fragment,
                     sizeof(fragment),
                     "range %lld..%lld",
                     (long long)fact->minValue,
                     (long long)fact->maxValue);
        }
        fragment[sizeof(fragment) - 1u] = '\0';
        zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
    }

    if (fact->hasUnsignedRange) {
        snprintf(fragment,
                 sizeof(fragment),
                 "unsigned range %llu..%llu",
                 (unsigned long long)fact->minUnsignedValue,
                 (unsigned long long)fact->maxUnsignedValue);
        fragment[sizeof(fragment) - 1u] = '\0';
        zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
    }

    if (fact->mayOverflow) {
        zr_debug_semantic_append_fragment(buffer, bufferSize, "may overflow");
    }
}

static void zr_debug_semantic_append_reference_fact(const SZrSemanticReferenceFact *fact,
                                                    const SZrSemanticReferenceFact **lastFact,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];
    const TZrChar *name;

    if (fact == ZR_NULL ||
        fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION ||
        (lastFact != ZR_NULL && *lastFact == fact)) {
        return;
    }

    name = zr_debug_semantic_string_text(fact->name);
    if (name != ZR_NULL && name[0] != '\0') {
        snprintf(fragment,
                 sizeof(fragment),
                 "reference %s %s",
                 zr_debug_semantic_reference_kind_text(fact->kind),
                 name);
    } else {
        snprintf(fragment,
                 sizeof(fragment),
                 "reference %s",
                 zr_debug_semantic_reference_kind_text(fact->kind));
    }
    fragment[sizeof(fragment) - 1u] = '\0';
    zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);

    if (lastFact != ZR_NULL) {
        *lastFact = fact;
    }
}

static void zr_debug_semantic_append_ownership_fact(const SZrSemanticOwnershipFact *fact,
                                                    const SZrSemanticOwnershipFact **lastFact,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];

    if (fact == ZR_NULL || (lastFact != ZR_NULL && *lastFact == fact)) {
        return;
    }

    if (fact->isViolation && fact->diagnosticMessage != ZR_NULL) {
        snprintf(fragment,
                 sizeof(fragment),
                 "ownership violation %s",
                 zr_debug_semantic_string_text(fact->diagnosticMessage));
    } else {
        snprintf(fragment,
                 sizeof(fragment),
                 "ownership %s %s",
                 zr_debug_semantic_ownership_kind_text(fact->kind),
                 zr_debug_semantic_ownership_qualifier_text(fact->qualifier));
    }
    fragment[sizeof(fragment) - 1u] = '\0';
    zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);

    if (lastFact != ZR_NULL) {
        *lastFact = fact;
    }
}

static const TZrChar *zr_debug_semantic_reachability_cause_text(EZrSemanticReachabilityCause cause) {
    switch (cause) {
        case ZR_SEMANTIC_REACHABILITY_AFTER_RETURN:
            return "after return";
        case ZR_SEMANTIC_REACHABILITY_AFTER_THROW:
            return "after throw";
        case ZR_SEMANTIC_REACHABILITY_AFTER_BREAK:
            return "after break";
        case ZR_SEMANTIC_REACHABILITY_AFTER_CONTINUE:
            return "after continue";
        case ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE:
            return "because the condition is false";
        case ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH:
            return "because a constant branch skips evaluation";
        case ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT:
            return "because short-circuit skips evaluation";
        case ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH:
            return "after exhaustive branch";
        case ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP:
            return "after non-fallthrough loop";
        case ZR_SEMANTIC_REACHABILITY_CAUSE_UNKNOWN:
        default:
            return "for an unknown reason";
    }
}

static void zr_debug_semantic_append_logical_fact(const SZrSemanticLogicalFact *fact,
                                                  const SZrSemanticLogicalFact **lastFact,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];

    if (fact == ZR_NULL || (lastFact != ZR_NULL && *lastFact == fact)) {
        return;
    }

    if (fact->hasKnownValue) {
        snprintf(fragment, sizeof(fragment), "logical %s", fact->knownValue ? "true" : "false");
        fragment[sizeof(fragment) - 1u] = '\0';
        zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);
    }
    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT) {
        zr_debug_semantic_append_fragment(buffer, bufferSize, "short-circuits");
    }

    if (lastFact != ZR_NULL) {
        *lastFact = fact;
    }
}

static void zr_debug_semantic_append_reachability_fact(const SZrSemanticReachabilityFact *fact,
                                                       const SZrSemanticReachabilityFact **lastFact,
                                                       TZrChar *buffer,
                                                       TZrSize bufferSize) {
    TZrChar fragment[ZR_DEBUG_TEXT_CAPACITY];

    if (fact == ZR_NULL ||
        fact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE ||
        (lastFact != ZR_NULL && *lastFact == fact)) {
        return;
    }

    snprintf(fragment,
             sizeof(fragment),
             "unreachable %s",
             zr_debug_semantic_reachability_cause_text(fact->cause));
    fragment[sizeof(fragment) - 1u] = '\0';
    zr_debug_semantic_append_fragment(buffer, bufferSize, fragment);

    if (lastFact != ZR_NULL) {
        *lastFact = fact;
    }
}

typedef struct ZrDebugSemanticFactWalk {
    SZrState *state;
    const SZrSemanticContext *context;
    const SZrSemanticExpressionFact *lastExpressionFact;
    const SZrSemanticNumericFact *lastNumericFact;
    const SZrSemanticReferenceFact *lastReferenceFact;
    const SZrSemanticLogicalFact *lastLogicalFact;
    const SZrSemanticReachabilityFact *lastReachabilityFact;
    const SZrSemanticOwnershipFact *lastOwnershipFact;
    TZrChar *buffer;
    TZrSize bufferSize;
} ZrDebugSemanticFactWalk;

static void zr_debug_semantic_walk_node_at_depth(SZrAstNode *node,
                                                 ZrDebugSemanticFactWalk *walk,
                                                 TZrUInt32 depth);

static void zr_debug_semantic_walk_list(SZrAstNodeArray *nodes, ZrDebugSemanticFactWalk *walk) {
    TZrSize index;

    if (nodes == ZR_NULL) {
        return;
    }

    for (index = 0; index < nodes->count; ++index) {
        zr_debug_semantic_walk_node_at_depth(nodes->nodes[index], walk, 1u);
    }
}

static TZrBool zr_debug_semantic_should_suppress_nested_literal_fact(SZrAstNode *node,
                                                                     TZrUInt32 depth) {
    if (node == ZR_NULL || depth == 0u) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_TEMPLATE_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static void zr_debug_semantic_walk_node_at_depth(SZrAstNode *node,
                                                 ZrDebugSemanticFactWalk *walk,
                                                 TZrUInt32 depth) {
    TZrBool suppressLiteralFacts;
    const SZrSemanticReferenceFact *directReadFact;

    if (node == ZR_NULL || walk == ZR_NULL || walk->context == ZR_NULL) {
        return;
    }

    suppressLiteralFacts = zr_debug_semantic_should_suppress_nested_literal_fact(node, depth);

    zr_debug_semantic_append_reference_fact(
            ZrParser_SemanticFacts_FindReferenceByNodeAndKind(walk->context,
                                                              node,
                                                              ZR_SEMANTIC_REFERENCE_MEMBER_WRITE),
            &walk->lastReferenceFact,
            walk->buffer,
            walk->bufferSize);
    zr_debug_semantic_append_reference_fact(
            ZrParser_SemanticFacts_FindReferenceByNodeAndKind(walk->context,
                                                              node,
                                                              ZR_SEMANTIC_REFERENCE_WRITE),
            &walk->lastReferenceFact,
            walk->buffer,
            walk->bufferSize);
    directReadFact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(walk->context,
                                                                       node,
                                                                       ZR_SEMANTIC_REFERENCE_READ);
    zr_debug_semantic_append_reference_fact(directReadFact,
                                            &walk->lastReferenceFact,
                                            walk->buffer,
                                            walk->bufferSize);

    if (!suppressLiteralFacts) {
        {
            const SZrSemanticExpressionFact *fact =
                    ZrParser_SemanticFacts_FindExpressionByNode(walk->context, node);
            if (fact != ZR_NULL && fact != walk->lastExpressionFact) {
                zr_debug_semantic_append_expression_fact(walk->state, fact, walk->buffer, walk->bufferSize);
                walk->lastExpressionFact = fact;
            }
        }
        {
            const SZrSemanticNumericFact *fact =
                    ZrParser_SemanticFacts_FindNumericByNode(walk->context, node);
            if (fact != ZR_NULL && fact != walk->lastNumericFact) {
                zr_debug_semantic_append_numeric_fact(fact, walk->buffer, walk->bufferSize);
                walk->lastNumericFact = fact;
            }
        }
    }

    zr_debug_semantic_append_reference_fact(
            ZrParser_SemanticFacts_FindReferenceAtPosition(walk->context, node->location),
            &walk->lastReferenceFact,
            walk->buffer,
            walk->bufferSize);
    if (node->type == ZR_AST_IDENTIFIER_LITERAL && directReadFact == ZR_NULL) {
        zr_debug_semantic_append_reference_fact(
                ZrParser_SemanticFacts_FindReferenceAtPositionByKind(walk->context,
                                                                     node->location,
                                                                     ZR_SEMANTIC_REFERENCE_READ),
                &walk->lastReferenceFact,
                walk->buffer,
                walk->bufferSize);
    }
    zr_debug_semantic_append_logical_fact(
            ZrParser_SemanticFacts_FindLogicalByNode(walk->context, node),
            &walk->lastLogicalFact,
            walk->buffer,
            walk->bufferSize);
    zr_debug_semantic_append_reachability_fact(
            ZrParser_SemanticFacts_FindReachabilityAtPosition(walk->context, node->location),
            &walk->lastReachabilityFact,
            walk->buffer,
            walk->bufferSize);
    zr_debug_semantic_append_ownership_fact(
            ZrParser_SemanticFacts_FindOwnershipByNode(walk->context, node),
            &walk->lastOwnershipFact,
            walk->buffer,
            walk->bufferSize);

    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.binaryExpression.left, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.binaryExpression.right, walk, depth + 1u);
            break;
        case ZR_AST_LOGICAL_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.logicalExpression.left, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.logicalExpression.right, walk, depth + 1u);
            break;
        case ZR_AST_UNARY_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.unaryExpression.argument, walk, depth + 1u);
            break;
        case ZR_AST_TYPE_CAST_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.typeCastExpression.expression, walk, depth + 1u);
            break;
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.typeQueryExpression.operand, walk, depth + 1u);
            break;
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.assignmentExpression.left, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.assignmentExpression.right, walk, depth + 1u);
            break;
        case ZR_AST_CONDITIONAL_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.conditionalExpression.test, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.conditionalExpression.consequent, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.conditionalExpression.alternate, walk, depth + 1u);
            break;
        case ZR_AST_IF_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.ifExpression.condition, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.ifExpression.thenExpr, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.ifExpression.elseExpr, walk, depth + 1u);
            break;
        case ZR_AST_WHILE_LOOP:
            zr_debug_semantic_walk_node_at_depth(node->data.whileLoop.cond, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.whileLoop.block, walk, depth + 1u);
            break;
        case ZR_AST_FOR_LOOP:
            zr_debug_semantic_walk_node_at_depth(node->data.forLoop.init, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.forLoop.cond, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.forLoop.step, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.forLoop.block, walk, depth + 1u);
            break;
        case ZR_AST_FOREACH_LOOP:
            zr_debug_semantic_walk_node_at_depth(node->data.foreachLoop.pattern, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.foreachLoop.expr, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.foreachLoop.block, walk, depth + 1u);
            break;
        case ZR_AST_SWITCH_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.switchExpression.expr, walk, depth + 1u);
            zr_debug_semantic_walk_list(node->data.switchExpression.cases, walk);
            zr_debug_semantic_walk_node_at_depth(node->data.switchExpression.defaultCase, walk, depth + 1u);
            break;
        case ZR_AST_SWITCH_CASE:
            zr_debug_semantic_walk_node_at_depth(node->data.switchCase.value, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.switchCase.block, walk, depth + 1u);
            break;
        case ZR_AST_SWITCH_DEFAULT:
            zr_debug_semantic_walk_node_at_depth(node->data.switchDefault.block, walk, depth + 1u);
            break;
        case ZR_AST_LAMBDA_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.lambdaExpression.block, walk, depth + 1u);
            break;
        case ZR_AST_FUNCTION_CALL:
            zr_debug_semantic_walk_list(node->data.functionCall.args, walk);
            break;
        case ZR_AST_PRIMARY_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.primaryExpression.property, walk, depth + 1u);
            zr_debug_semantic_walk_list(node->data.primaryExpression.members, walk);
            break;
        case ZR_AST_MEMBER_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.memberExpression.property, walk, depth + 1u);
            break;
        case ZR_AST_ARRAY_LITERAL:
            zr_debug_semantic_walk_list(node->data.arrayLiteral.elements, walk);
            break;
        case ZR_AST_OBJECT_LITERAL:
            zr_debug_semantic_walk_list(node->data.objectLiteral.properties, walk);
            break;
        case ZR_AST_KEY_VALUE_PAIR:
            zr_debug_semantic_walk_node_at_depth(node->data.keyValuePair.key, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.keyValuePair.value, walk, depth + 1u);
            break;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            zr_debug_semantic_walk_node_at_depth(node->data.constructExpression.target, walk, depth + 1u);
            zr_debug_semantic_walk_list(node->data.constructExpression.args, walk);
            break;
        case ZR_AST_BLOCK:
            zr_debug_semantic_walk_list(node->data.block.body, walk);
            break;
        case ZR_AST_EXPRESSION_STATEMENT:
            zr_debug_semantic_walk_node_at_depth(node->data.expressionStatement.expr, walk, depth + 1u);
            break;
        case ZR_AST_RETURN_STATEMENT:
            zr_debug_semantic_walk_node_at_depth(node->data.returnStatement.expr, walk, depth + 1u);
            break;
        case ZR_AST_VARIABLE_DECLARATION:
            zr_debug_semantic_walk_node_at_depth(node->data.variableDeclaration.value, walk, depth + 1u);
            break;
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            zr_debug_semantic_walk_node_at_depth(node->data.breakContinueStatement.expr, walk, depth + 1u);
            break;
        case ZR_AST_THROW_STATEMENT:
            zr_debug_semantic_walk_node_at_depth(node->data.throwStatement.expr, walk, depth + 1u);
            break;
        case ZR_AST_USING_STATEMENT:
            zr_debug_semantic_walk_node_at_depth(node->data.usingStatement.resource, walk, depth + 1u);
            zr_debug_semantic_walk_node_at_depth(node->data.usingStatement.body, walk, depth + 1u);
            break;
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            zr_debug_semantic_walk_node_at_depth(node->data.tryCatchFinallyStatement.block, walk, depth + 1u);
            zr_debug_semantic_walk_list(node->data.tryCatchFinallyStatement.catchClauses, walk);
            zr_debug_semantic_walk_node_at_depth(node->data.tryCatchFinallyStatement.finallyBlock, walk, depth + 1u);
            break;
        case ZR_AST_CATCH_CLAUSE:
            zr_debug_semantic_walk_list(node->data.catchClause.pattern, walk);
            zr_debug_semantic_walk_node_at_depth(node->data.catchClause.block, walk, depth + 1u);
            break;
        default:
            break;
    }
}

static void zr_debug_semantic_walk_node(SZrAstNode *node, ZrDebugSemanticFactWalk *walk) {
    zr_debug_semantic_walk_node_at_depth(node, walk, 0u);
}

static void zr_debug_semantic_append_flow_facts(SZrState *state,
                                                const SZrSemanticContext *context,
                                                SZrAstNode *expr,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    ZrDebugSemanticFactWalk walk;

    if (context == ZR_NULL || expr == ZR_NULL) {
        return;
    }

    memset(&walk, 0, sizeof(walk));
    walk.state = state;
    walk.context = context;
    walk.buffer = buffer;
    walk.bufferSize = bufferSize;
    zr_debug_semantic_walk_node(expr, &walk);
}

void zr_debug_append_expression_semantic_facts(ZrDebugAgent *agent,
                                               TZrUInt32 frameId,
                                               const TZrChar *expression,
                                               TZrChar *buffer,
                                               TZrSize bufferSize) {
    SZrState *state;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrString *sourceName;
    TZrChar *source = ZR_NULL;
    SZrAstNode *ast = ZR_NULL;
    SZrAstNode *expr = ZR_NULL;
    TZrBool parserStateInitialized = ZR_FALSE;
    TZrBool compilerStateInitialized = ZR_FALSE;
    TZrBool inferredTypeInitialized = ZR_FALSE;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        expression == ZR_NULL ||
        buffer == ZR_NULL ||
        bufferSize == 0) {
        return;
    }
    state = agent->state;

    source = zr_debug_semantic_build_expression_statement(expression);
    if (source == ZR_NULL) {
        return;
    }

    sourceName = ZrCore_String_CreateFromNative(state, "<debug:evaluate>");
    ZrParser_State_Init(&parserState, state, source, strlen(source), sourceName);
    parserStateInitialized = ZR_TRUE;
    parserState.suppressErrorOutput = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parserState);
    if (parserState.hasError || ast == ZR_NULL) {
        goto cleanup;
    }

    expr = zr_debug_semantic_last_expression_statement(ast);
    if (expr == ZR_NULL) {
        goto cleanup;
    }

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, state);
    compilerStateInitialized = ZR_TRUE;
    compilerState.currentAst = ast;
    compilerState.scriptAst = ast;
    compilerState.suppressErrorOutput = ZR_TRUE;
    zr_debug_semantic_register_bindings(agent, frameId, &compilerState);

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    inferredTypeInitialized = ZR_TRUE;
    if (ZrParser_ExpressionType_Infer(&compilerState, expr, &inferredType)) {
        zr_debug_semantic_append_flow_facts(state, compilerState.semanticContext, expr, buffer, bufferSize);
    }

cleanup:
    if (inferredTypeInitialized) {
        ZrParser_InferredType_Free(state, &inferredType);
    }
    if (compilerStateInitialized) {
        ZrParser_CompilerState_Free(&compilerState);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    if (parserStateInitialized) {
        ZrParser_State_Free(&parserState);
    }
    free(source);
}
