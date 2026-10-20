#include "debug_internal.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/type_inference.h"

/* 多类事实共用一段有界诊断文本；空间不足时保留已写入的事实，不截断旧内容。 */
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
            return "Unique";
        case ZR_OWNERSHIP_QUALIFIER_SHARED:
            return "Shared";
        case ZR_OWNERSHIP_QUALIFIER_WEAK:
            return "Weak";
        case ZR_OWNERSHIP_QUALIFIER_BORROWED:
            return "ref readonly";
        case ZR_OWNERSHIP_QUALIFIER_LOANED:
            return "ref";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return "plain";
    }
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
    /* 按字节长度走访，使包含 NUL 和控制字符的编译期常量仍可用于调试摘要。 */
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

    /* AST 与源码位置可能指向同一事实；仅去重相邻的同一事实对象。声明本身不作读取摘要。 */
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

/* 单次 AST 遍历的事实去重游标；context 和 buffer 均由调用方持有。 */
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
    /* 根字面量保留常量说明；嵌套常量由所属表达式的语义覆盖，避免淹没摘要。 */
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

    /* 写入先于普通读取报告，保证赋值目标的引用类别不会被位置级读取事实遮盖。 */
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

    /* 位置索引补足没有直接挂在 AST 节点上的引用和不可达事实。 */
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

static void zr_debug_append_reference_summaries(
        ZrDebugAgent *agent,
        TZrUInt32 frameId,
        const SZrSemanticContext *context,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrSize index;

    if (agent == ZR_NULL || context == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }
    for (index = 0u; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts,
                        index);
        TZrChar summary[ZR_DEBUG_TEXT_CAPACITY];
        TZrSize length;

        /* 仅对已解析的读取补充暂停帧来源；同名来源摘要只输出一次。 */
        if (fact == ZR_NULL || !fact->isResolved || fact->kind != ZR_SEMANTIC_REFERENCE_READ ||
            fact->name == ZR_NULL ||
            !zr_debug_identifier_reference_summary(agent,
                                                   frameId,
                                                   zr_debug_semantic_string_text(fact->name),
                                                   summary,
                                                   sizeof(summary)) ||
            summary[0] == '\0' || strstr(buffer, summary) != ZR_NULL) {
            continue;
        }
        length = strlen(buffer);
        if (length > 0u && length + 2u < bufferSize) {
            snprintf(buffer + length, bufferSize - length, ", %s", summary);
        } else if (length == 0u) {
            snprintf(buffer, bufferSize, "%s", summary);
        }
        buffer[bufferSize - 1u] = '\0';
    }
}

static void zr_debug_append_expression_facts(ZrDebugAgent *agent,
                                             TZrUInt32 frameId,
                                             const TZrChar *expression,
                                             TZrChar *semanticBuffer,
                                             TZrSize semanticBufferSize,
                                             TZrChar *referenceBuffer,
                                             TZrSize referenceBufferSize) {
    SZrState *state;
    SZrParserState parserState;
    SZrCompilerState compilerState;
    SZrInferredType inferredType;
    SZrString *sourceName;
    SZrAstNode *expr = ZR_NULL;
    TZrBool parserStateInitialized = ZR_FALSE;
    TZrBool compilerStateInitialized = ZR_FALSE;
    TZrBool inferredTypeInitialized = ZR_FALSE;

    if (agent == ZR_NULL ||
        agent->state == ZR_NULL ||
        expression == ZR_NULL ||
        ((semanticBuffer == ZR_NULL || semanticBufferSize == 0u) &&
         (referenceBuffer == ZR_NULL || referenceBufferSize == 0u))) {
        return;
    }
    state = agent->state;

    /* 重新解析求值表达式，仅运行类型推断以读取语义事实；不再次执行表达式。 */
    sourceName = ZrCore_String_CreateFromNative(state, "<debug:evaluate>");
    ZrParser_State_Init(&parserState, state, expression, strlen(expression), sourceName);
    parserStateInitialized = ZR_TRUE;
    parserState.suppressErrorOutput = ZR_TRUE;
    expr = ZrParser_ParseExpressionWithState(&parserState);
    if (parserState.hasError || expr == ZR_NULL) {
        goto cleanup;
    }

    memset(&compilerState, 0, sizeof(compilerState));
    ZrParser_CompilerState_Init(&compilerState, state);
    compilerStateInitialized = ZR_TRUE;
    compilerState.currentAst = expr;
    compilerState.scriptAst = expr;
    compilerState.suppressErrorOutput = ZR_TRUE;
    if (!zr_debug_semantic_register_summary_bindings(
                agent, frameId, &compilerState, expr)) {
        goto cleanup;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    inferredTypeInitialized = ZR_TRUE;
    if (ZrParser_ExpressionType_Infer(&compilerState, expr, &inferredType)) {
        if (semanticBuffer != ZR_NULL && semanticBufferSize > 0u) {
            zr_debug_semantic_append_flow_facts(
                    state,
                    compilerState.semanticContext,
                    expr,
                    semanticBuffer,
                    semanticBufferSize);
        }
        zr_debug_append_reference_summaries(agent,
                                            frameId,
                                            compilerState.semanticContext,
                                            referenceBuffer,
                                            referenceBufferSize);
    }

cleanup:
    /* 推断事实依附 compilerState/AST，必须在这两者释放前完成文本化。 */
    if (inferredTypeInitialized) {
        ZrParser_InferredType_Free(state, &inferredType);
    }
    if (compilerStateInitialized) {
        ZrParser_CompilerState_Free(&compilerState);
    }
    if (expr != ZR_NULL) {
        ZrParser_Ast_Free(state, expr);
    }
    if (parserStateInitialized) {
        ZrParser_State_Free(&parserState);
    }
}

/** @brief 为求值结果追加当前表达式的推断事实，保留调用方已有的值摘要。 */
void zr_debug_append_expression_semantic_facts(ZrDebugAgent *agent,
                                               TZrUInt32 frameId,
                                               const TZrChar *expression,
                                               TZrChar *buffer,
                                               TZrSize bufferSize) {
    zr_debug_append_expression_facts(
            agent, frameId, expression, buffer, bufferSize, ZR_NULL, 0u);
}

/** @brief 将已解析标识符映射为暂停帧内的来源摘要。 */
void zr_debug_append_expression_reference_summary(ZrDebugAgent *agent,
                                                  TZrUInt32 frameId,
                                                  const TZrChar *expression,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    zr_debug_append_expression_facts(
            agent, frameId, expression, ZR_NULL, 0u, buffer, bufferSize);
}

/**
 * @fn static void zr_debug_semantic_append_fragment(TZrChar *buffer, TZrSize bufferSize, const TZrChar *fragment)
 *
 * ## Summary
 * 将各类事实片段追加到调用方已有的有界诊断文本，保留较早的值和事实。
 *
 * ## Parameters
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 * @param[in] fragment 本次追加的 NUL 终止只读片段；调用后不保留其地址。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * buffer 已在其容量内以 NUL 终止；空间不足可截断新片段，连同分隔符或后续转义都不保证完整。空片段和无输出容量时不追加。
 *
 * ## References
 * - zr_debug_semantic_append_expression_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:230）
 * - zr_debug_semantic_append_numeric_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:311）
 * - zr_debug_semantic_append_reference_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:352）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_expression_kind_text(EZrSemanticExpressionFactKind kind)
 *
 * ## Summary
 * 为表达式事实提供稳定的显示词，使摘要不依赖枚举数值。
 *
 * ## Parameters
 * @param[in] kind 待显示的事实或引用种类；未知枚举值采用保守显示词。
 *
 * ## Returns
 * @return 返回静态显示字符串，无需释放；未知枚举使用本映射的保守回退值。
 *
 * ## Constraints and lifecycle
 * 纯枚举映射，不读取或修改语义环境；未知值显示 unknown。
 *
 * ## References
 * - zr_debug_semantic_append_expression_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:230）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_exactness_text(EZrSemanticFactExactness exactness)
 *
 * ## Summary
 * 把推断精确度转换为摘要中的显示词。
 *
 * ## Parameters
 * @param[in] exactness 已有推断事实的精确度枚举，映射不增强其语义。
 *
 * ## Returns
 * @return 返回静态显示字符串，无需释放；未知枚举使用本映射的保守回退值。
 *
 * ## Constraints and lifecycle
 * 纯枚举映射；未知值使用 unknown，不补造精确事实。
 *
 * ## References
 * - zr_debug_semantic_append_expression_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:230）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_reference_kind_text(EZrSemanticReferenceKind kind)
 *
 * ## Summary
 * 把引用行为转换为事实摘要中的显示词。
 *
 * ## Parameters
 * @param[in] kind 待显示的事实或引用种类；未知枚举值采用保守显示词。
 *
 * ## Returns
 * @return 返回静态显示字符串，无需释放；未知枚举使用本映射的保守回退值。
 *
 * ## Constraints and lifecycle
 * 映射本身不解析引用身份；未知值使用 unknown。
 *
 * ## References
 * - zr_debug_semantic_append_reference_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:352）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_ownership_kind_text(EZrSemanticOwnershipFactKind kind)
 *
 * ## Summary
 * 把已有所有权事实的种类转换为显示词。
 *
 * ## Parameters
 * @param[in] kind 待显示的事实或引用种类；未知枚举值采用保守显示词。
 *
 * ## Returns
 * @return 返回静态显示字符串，无需释放；未知枚举使用本映射的保守回退值。
 *
 * ## Constraints and lifecycle
 * 不执行移动、借用或释放；未知值使用 unknown。
 *
 * ## References
 * - zr_debug_semantic_append_ownership_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:387）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_ownership_qualifier_text(EZrOwnershipQualifier qualifier)
 *
 * ## Summary
 * 把已有所有权限定义转换为用户可读文字。
 *
 * ## Parameters
 * @param[in] qualifier 已有所有权限定符，纯显示不授权借用或移动。
 *
 * ## Returns
 * @return 返回静态显示字符串，无需释放；未知枚举使用本映射的保守回退值。
 *
 * ## Constraints and lifecycle
 * 没有或未知限定符时回退 plain；显示文字不授予运行时权限。
 *
 * ## References
 * - zr_debug_semantic_append_ownership_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:387）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_string_text(SZrString *value)
 *
 * ## Summary
 * 借用 VM 字符串的短串或长串文本，供事实格式化读取。
 *
 * ## Parameters
 * @param[in] value 借用的 VM 字符串；NULL 对应静态空文本。本函数只选择文本地址，调用方另按 NUL 或真实字节长度读取。
 *
 * ## Returns
 * @return 返回 VM 字符串内的借用文本；NULL 输入返回静态空串。VM 文本有效期不超过源字符串可安全读取的窗口，无需人工释放。
 *
 * ## Constraints and lifecycle
 * 返回指针不转移所有权；非空字符串须在读取期间由 VM 生命周期保护。NULL 使用静态空串，不能据此推断字节长度。
 *
 * ## References
 * - zr_debug_semantic_append_escaped_string_constant（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:185）
 * - zr_debug_semantic_append_expression_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:230）
 */

/**
 * @fn static void zr_debug_semantic_append_fragment_text(TZrChar *fragment, TZrSize fragmentSize, TZrSize *used, const TZrChar *text)
 *
 * ## Summary
 * 向局部常量片段写入一段文本，并维护有限容量的尾游标。
 *
 * ## Parameters
 * @param[in,out] fragment 本次构造的可写局部片段，容量和 used 由调用方配套提供。
 * @param[in] fragmentSize fragment 的实际字节容量，必须与其存储一致。
 * @param[in,out] used 调用方初始化的局部片段尾索引；写入后可饱和到最后可终止字节。
 * @param[in] text 本次追加的 NUL 终止只读文本，调用结束后不保留其地址。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * used 表示当前尾部；缓冲区与游标由调用者初始化。普通截断将 used 饱和到最后可终止位置，末字节保留 NUL；不保证片段语法完整。
 *
 * ## References
 * - zr_debug_semantic_append_escaped_string_constant（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:185）
 * - zr_debug_semantic_append_fragment_byte（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:167）
 */

/**
 * @fn static void zr_debug_semantic_append_fragment_byte(TZrChar *fragment, TZrSize fragmentSize, TZrSize *used, unsigned char byte)
 *
 * ## Summary
 * 把普通字节或控制字节的十六进制表示交给局部片段追加器。
 *
 * ## Parameters
 * @param[in,out] fragment 本次构造的可写局部片段，容量和 used 由调用方配套提供。
 * @param[in] fragmentSize fragment 的实际字节容量，必须与其存储一致。
 * @param[in,out] used 调用方初始化的局部片段尾索引；写入后可饱和到最后可终止字节。
 * @param[in] byte 待表示的一个原始字节；不是 Unicode 码点。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 0x20 以下和 0x7f 使用十六进制转义；其余字节原样交付。UTF-8 多字节不在此解码，容量退让沿用片段追加器。
 *
 * ## References
 * - zr_debug_semantic_append_escaped_string_constant（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:185）
 * - zr_debug_semantic_append_fragment_text（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:142）
 */

/**
 * @fn static void zr_debug_semantic_append_escaped_string_constant(SZrString *value, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 按 VM 字符串的真实字节长度构造常量摘要，保留内嵌 NUL 和控制字节的诊断意义。
 *
 * ## Parameters
 * @param[in] value 借用的 VM 字符串；NULL 表示空文本，非空内容按真实字节长度读取。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 双引号、反斜杠和常见控制符显式转义；先写入固定局部片段再追加到输出，两层容量均可截断，不能承诺闭合引号或完整常量。
 *
 * ## References
 * - zr_debug_semantic_append_expression_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:230）
 * - zr_debug_semantic_string_text（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:132）
 */

/**
 * @fn static void zr_debug_semantic_append_expression_fact(SZrState *state, const SZrSemanticExpressionFact *fact, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 把已有表达式事实的种类、精确度、类型、调用、成员和常量信息转为文本。
 *
 * ## Parameters
 * @param[in,out] state 借用的 VM 线程上下文；格式化或临时类型操作经它访问 VM 数据，调用后所有权仍属宿主。
 * @param[in] fact 借用本次推断事实；其容器和 GC 数据在读取期间有效，NULL 时不输出该事实。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 仅消费本次推断的事实，不再次执行表达式；输出容量不足时允许部分摘要，事实及 GC 字符串须在格式化期间有效。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 * - zr_debug_semantic_append_fragment（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:10）
 */

/**
 * @fn static void zr_debug_semantic_append_numeric_fact(const SZrSemanticNumericFact *fact, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 将浮点、有符号和无符号范围及 mayOverflow 风险标志追加到摘要。
 *
 * ## Parameters
 * @param[in] fact 借用本次推断事实；其容器和 GC 数据在读取期间有效，NULL 时不输出该事实。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 只按已有 hasRange 等标志显示；mayOverflow 是风险信息，不是证明已发生运行时溢出。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 * - zr_debug_semantic_append_fragment（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:10）
 */

/**
 * @fn static void zr_debug_semantic_append_reference_fact(const SZrSemanticReferenceFact *fact, const SZrSemanticReferenceFact **lastFact, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 输出引用事实，同时跳过声明以及本类别连续重复的同一事实指针。
 *
 * ## Parameters
 * @param[in] fact 借用本次推断事实；其容器和 GC 数据在读取期间有效，NULL 时不输出该事实。
 * @param[in,out] lastFact 本类别上次候选事实的可写借用游标；用于相邻指针比较，不取得事实所有权。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * lastFact 只用于相邻指针去重，不是全局事实集合；输出已满仍可能更新游标。事实由当前 semanticContext 借出，不能跨其释放保存。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 * - zr_debug_semantic_reference_kind_text（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:74）
 */

/**
 * @fn static void zr_debug_semantic_append_ownership_fact(const SZrSemanticOwnershipFact *fact, const SZrSemanticOwnershipFact **lastFact, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 输出所有权事实或其违规诊断，并避免本类别连续重复的同一指针。
 *
 * ## Parameters
 * @param[in] fact 借用本次推断事实；其容器和 GC 数据在读取期间有效，NULL 时不输出该事实。
 * @param[in,out] lastFact 本类别上次候选事实的可写借用游标；用于相邻指针比较，不取得事实所有权。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 只报告已有推断信息，不进行资源转移或所有权修复；游标不是全局去重，文本可按容量截断。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 * - zr_debug_semantic_ownership_kind_text（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:94）
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_reachability_cause_text(EZrSemanticReachabilityCause cause)
 *
 * ## Summary
 * 把不可达原因转换为诊断文字。
 *
 * ## Parameters
 * @param[in] cause 已有不可达事实的原因枚举，映射不重算控制流。
 *
 * ## Returns
 * @return 返回静态显示字符串，无需释放；未知枚举使用本映射的保守回退值。
 *
 * ## Constraints and lifecycle
 * 仅作值映射，不重算控制流；未知原因保守显示。
 *
 * ## References
 * - zr_debug_semantic_append_reachability_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:467）
 */

/**
 * @fn static void zr_debug_semantic_append_logical_fact(const SZrSemanticLogicalFact *fact, const SZrSemanticLogicalFact **lastFact, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 追加已知布尔值和短路事实，维持逻辑类别的相邻去重游标。
 *
 * ## Parameters
 * @param[in] fact 借用本次推断事实；其容器和 GC 数据在读取期间有效，NULL 时不输出该事实。
 * @param[in,out] lastFact 本类别上次候选事实的可写借用游标；用于相邻指针比较，不取得事实所有权。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 不求值或改写条件；相同指针的相邻查询才合并，未知布尔值不被补造为常量。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 * - zr_debug_semantic_append_fragment（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:10）
 */

/**
 * @fn static void zr_debug_semantic_append_reachability_fact(const SZrSemanticReachabilityFact *fact, const SZrSemanticReachabilityFact **lastFact, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 仅为推断已标为不可达的节点追加原因，并维持可达性类别的游标。
 *
 * ## Parameters
 * @param[in] fact 借用本次推断事实；其容器和 GC 数据在读取期间有效，NULL 时不输出该事实。
 * @param[in,out] lastFact 本类别上次候选事实的可写借用游标；用于相邻指针比较，不取得事实所有权。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 可达节点不添加片段；指针去重与输出容量限制不构成新的可达性分析。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 * - zr_debug_semantic_reachability_cause_text（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:417）
 */

/**
 * @fn static void zr_debug_semantic_walk_list(SZrAstNodeArray *nodes, ZrDebugSemanticFactWalk *walk)
 *
 * ## Summary
 * 遍历参与摘要的 AST 子列表，复用一次事实遍历的借用状态。
 *
 * ## Parameters
 * @param[in] nodes 借用 AST 子节点数组；本次只读遍历，不释放或重排数组。
 * @param[in,out] walk 借用本次遍历状态；更新分类型游标并向其缓冲区追加文本，不取得其它字段资源。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 每个列表子项从 depth=1 进入；该值用于根与嵌套字面量取舍，不是完整 AST 绝对深度。空列表或无遍历状态时返回。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 */

/**
 * @fn static TZrBool zr_debug_semantic_should_suppress_nested_literal_fact(SZrAstNode *node, TZrUInt32 depth)
 *
 * ## Summary
 * 判断嵌套字面量是否应从摘要中省略，防止其淹没复合表达式事实。
 *
 * ## Parameters
 * @param[in] node 借用当前 AST 节点；NULL 可表示没有子表达式，不转移所有权。
 * @param[in] depth 根为零、嵌套为非零的摘要取舍参数；不是完整 AST 的持久深度索引。
 *
 * ## Returns
 * @return 当该节点是非根的已列出字面量时返回真；否则返回假，仅控制显示取舍。
 *
 * ## Constraints and lifecycle
 * 只对已列出的字面量 AST 种类且 depth>0 返回真；根字面量保留，此谓词不删除 AST 或事实。
 *
 * ## References
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 */

/**
 * @fn static void zr_debug_semantic_walk_node_at_depth(SZrAstNode *node, ZrDebugSemanticFactWalk *walk, TZrUInt32 depth)
 *
 * ## Summary
 * 按已支持的 AST 形态回放推断事实，并协调节点查询与位置补查。
 *
 * ## Parameters
 * @param[in] node 借用当前 AST 节点；NULL 可表示没有子表达式，不转移所有权。
 * @param[in,out] walk 借用本次遍历状态；更新分类型游标并向其缓冲区追加文本，不取得其它字段资源。
 * @param[in] depth 根为零、嵌套为非零的摘要取舍参数；不是完整 AST 的持久深度索引。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * reference_order：先写入及成员写入，再读取；position_fallback：再用当前节点位置补查事实。各类别只相邻去重；嵌套字面量有意抑制，switch 未列出的 AST 形态不承诺遍历。context、AST、输出在整次遍历期间有效。
 *
 * ## References
 * - zr_debug_semantic_walk_list（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:509）
 * - zr_debug_semantic_walk_node（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:749）
 * - zr_debug_semantic_append_reference_fact（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:352）
 */

/**
 * @fn static void zr_debug_semantic_walk_node(SZrAstNode *node, ZrDebugSemanticFactWalk *walk)
 *
 * ## Summary
 * 以根深度零启动事实遍历，使顶层字面量仍可显示。
 *
 * ## Parameters
 * @param[in] node 借用当前 AST 节点；NULL 可表示没有子表达式，不转移所有权。
 * @param[in,out] walk 借用本次遍历状态；更新分类型游标并向其缓冲区追加文本，不取得其它字段资源。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 是根入口的薄转发；不取得 AST 或遍历状态所有权，容量和生命周期沿用深度遍历。
 *
 * ## References
 * - zr_debug_semantic_append_flow_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:753）
 * - zr_debug_semantic_walk_node_at_depth（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:542）
 */

/**
 * @fn static void zr_debug_semantic_append_flow_facts(SZrState *state, const SZrSemanticContext *context, SZrAstNode *expr, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 为一次 AST 事实摘要初始化各类别游标，再遍历根表达式。
 *
 * ## Parameters
 * @param[in,out] state 借用的 VM 线程上下文；格式化或临时类型操作经它访问 VM 数据，调用后所有权仍属宿主。
 * @param[in] context 借用当前推断语义上下文；整个查询和文本化期间不得释放。
 * @param[in] expr 借用已解析的根 AST，不转移所有权。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 状态仅在栈上存在；借用的 state、semanticContext、AST 和输出必须在文本化结束前有效，游标不会持有或延长它们的生命。
 *
 * ## References
 * - zr_debug_append_expression_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:812）
 * - zr_debug_semantic_walk_node（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:749）
 */

/**
 * @fn static void zr_debug_append_reference_summaries( ZrDebugAgent *agent, TZrUInt32 frameId, const SZrSemanticContext *context, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 把已解析 read 事实映射到当前帧的来源文字，补充语义摘要无法独立表示的运行时来源。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in] context 借用当前推断语义上下文；整个查询和文本化期间不得释放。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 只处理有名称且已解析的 read；已有输出通过 strstr 子串匹配跳过重复摘要，不是精确的名称集合。帧必须由调用方保持稳定，输出可截断；不取得暂停值或语义事实所有权。
 *
 * ## References
 * - zr_debug_append_expression_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:812）
 * - ZrDebug_EvaluateWithCapabilities（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:301）
 */

/**
 * @fn static void zr_debug_append_expression_facts(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, TZrChar *semanticBuffer, TZrSize semanticBufferSize, TZrChar *referenceBuffer, TZrSize referenceBufferSize)
 *
 * ## Summary
 * 重新解析表达式并进行类型推断，将事实和来源在临时编译上下文释放前文本化。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in] expression 本次处理的 NUL 终止表达式文本，重新解析期间保持有效。
 * @param[in,out] semanticBuffer 可选的语义摘要缓冲区；已有文本保留，使用时须在容量内 NUL 终止。
 * @param[in] semanticBufferSize semanticBuffer 的字节容量；NULL 或零容量禁用该目的地。
 * @param[in,out] referenceBuffer 可选的来源摘要缓冲区；已有文本保留，使用时须在容量内 NUL 终止。
 * @param[in] referenceBufferSize referenceBuffer 的字节容量；NULL 或零容量禁用该目的地。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 至少一个目的缓冲区有效；不清空已有文本，本函数不调用表达式执行入口。解析或推断的普通失败可留下原有或部分摘要；不以推断成功替代完整 canonical 证明。cleanup 按已初始化标志释放类型、编译状态、AST、解析状态；没有本地异常恢复点，不保证非局部 Throw 下执行普通清理。
 *
 * ## References
 * - zr_debug_append_expression_semantic_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:894）
 * - zr_debug_append_expression_reference_summary（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:904）
 * - zr_debug_semantic_register_summary_bindings（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_bindings.c:728）
 */

/**
 * @fn void zr_debug_append_expression_semantic_facts(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 为已格式化的求值结果追加本表达式的推断事实；也供诊断测试独立调用。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in] expression 本次处理的 NUL 终止表达式文本，重新解析期间保持有效。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 借用 agent、表达式和现有 NUL 终止输出，重新解析及推断但不在本函数直接执行表达式；普通失败不提供成功标志或全量事实保证，调用方须在暂停或等价稳定阶段使用帧元数据。
 *
 * ## References
 * - zr_debug_append_expression_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:812）
 * - ZrDebug_EvaluateWithCapabilities（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:301）
 */

/**
 * @fn void zr_debug_append_expression_reference_summary(ZrDebugAgent *agent, TZrUInt32 frameId, const TZrChar *expression, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 为求值结果追加已解析标识符在当前帧中的来源摘要。
 *
 * ## Parameters
 * @param[in,out] agent 借用调试代理及所连 VM 线程；代理、入口产物及帧元数据在调用期间保持有效稳定，内部可使用线程分配资源。
 * @param[in] frameId 调试帧编号；绑定查询中 0 和 1 都映射 Core 层级 0，其余编号减一，须对应当前稳定调用窗口。
 * @param[in] expression 本次处理的 NUL 终止表达式文本，重新解析期间保持有效。
 * @param[in,out] buffer 调用方已有 NUL 终止文本的可写缓冲区；NULL 或零容量时不要求追加。
 * @param[in] bufferSize 与 buffer 对应的真实字节容量；有效文本须在该范围内终止。
 *
 * ## Returns
 * 无返回值；通过缓冲区文本或遍历游标交付，普通条件不满足时可不追加，不提供完整成功标志。
 *
 * ## Constraints and lifecycle
 * 与语义摘要分开重建临时推断上下文；返回前仅保留文本，不保留事实或暂停值指针。来源可缺席或被容量截断，不能将空摘要视为表达式无引用的证明。
 *
 * ## References
 * - zr_debug_append_expression_facts（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:812）
 * - zr_debug_append_reference_summaries（zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_semantic_facts.c:772）
 * - ZrDebug_EvaluateWithCapabilities（zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h:301）
 */

/**
 * @struct ZrDebugSemanticFactWalk
 *
 * ## Summary
 * 为一次 AST 事实回放保存借用上下文、分类型相邻去重指针和输出目的地。
 *
 * ## Fields
 * - state：借用 VM 线程，只在本次格式化期间使用。
 * - context：借用推断事实容器；compilerState 释放前完成所有读取。
 * - lastExpressionFact：上次表达式事实指针，只与连续候选比较。
 * - lastNumericFact：上次数值事实指针，独立于其它类别。
 * - lastReferenceFact：上次引用事实指针，不是按名称或全局去重集合。
 * - lastLogicalFact：上次逻辑事实指针，借用 context 内存。
 * - lastReachabilityFact：上次可达性事实指针，可达节点不额外显示。
 * - lastOwnershipFact：上次所有权事实指针，不持有值的所有权。
 * - buffer：已有 NUL 终止的可写输出，不取得内存所有权。
 * - bufferSize：buffer 的真实字节容量，追加文本可截断。
 *
 * ## Invariants and lifecycle
 * 由 append_flow_facts 的栈上初始化置零各游标；AST、context、GC 字符串和输出由调用方保持有效。该结构不加锁、不管理暂停、不保活指针，也不承诺全局事实去重。
 *
 * ## References
 * - zr_debug_semantic_append_flow_facts：初始化并限定栈上生命周期。
 * - zr_debug_semantic_walk_node_at_depth：按类别读取和更新游标。
 */
