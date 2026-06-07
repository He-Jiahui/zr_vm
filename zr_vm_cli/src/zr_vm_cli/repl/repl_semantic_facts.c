#include "repl/repl_semantic_facts.h"

#include "zr_vm_core/log.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_string_conf.h"

void ZrCli_ReplSemanticFacts_WriteNumeric(SZrState *state, const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return;
    }

    if (fact->hasRange) {
        if (fact->sourceType == ZR_VALUE_TYPE_DOUBLE || fact->targetType == ZR_VALUE_TYPE_DOUBLE ||
            fact->sourceType == ZR_VALUE_TYPE_FLOAT || fact->targetType == ZR_VALUE_TYPE_FLOAT) {
            ZrCore_Log_Resultf(state,
                               "Numeric range: %.17g..%.17g\n",
                               fact->minDoubleValue,
                               fact->maxDoubleValue);
        } else {
            ZrCore_Log_Resultf(state,
                               "Numeric range: %lld..%lld\n",
                               (long long)fact->minValue,
                               (long long)fact->maxValue);
        }
    }

    if (fact->hasUnsignedRange) {
        ZrCore_Log_Resultf(state,
                           "Unsigned range: %llu..%llu\n",
                           (unsigned long long)fact->minUnsignedValue,
                           (unsigned long long)fact->maxUnsignedValue);
    }

    if (fact->mayOverflow) {
        ZrCore_Log_Resultf(state, "Numeric warning: may overflow\n");
    }
}

void ZrCli_ReplSemanticFacts_WriteLogical(SZrState *state, const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return;
    }

    if (fact->hasKnownValue) {
        ZrCore_Log_Resultf(state, "Logical value: %s\n", fact->knownValue ? "true" : "false");
    }
    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT) {
        ZrCore_Log_Resultf(state, "Logical flow: short-circuits right operand\n");
    }
}

static const TZrChar *repl_ownership_fact_kind_text(EZrSemanticOwnershipFactKind kind) {
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

static const TZrChar *repl_ownership_qualifier_text(EZrOwnershipQualifier qualifier) {
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

static const TZrChar *repl_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static void repl_write_escaped_string_constant(SZrState *state, SZrString *value) {
    const TZrChar *text = repl_string_text(value);
    TZrSize length = ZrCore_String_GetByteLength(value);
    TZrSize index;
    unsigned char byte;

    ZrCore_Log_Resultf(state, "Constant: \"");
    for (index = 0; text != ZR_NULL && index < length; ++index) {
        byte = (unsigned char)text[index];
        switch (byte) {
            case '"':
                ZrCore_Log_Resultf(state, "\\\"");
                break;
            case '\\':
                ZrCore_Log_Resultf(state, "\\\\");
                break;
            case '\n':
                ZrCore_Log_Resultf(state, "\\n");
                break;
            case '\t':
                ZrCore_Log_Resultf(state, "\\t");
                break;
            case '\r':
                ZrCore_Log_Resultf(state, "\\r");
                break;
            case '\b':
                ZrCore_Log_Resultf(state, "\\b");
                break;
            case '\f':
                ZrCore_Log_Resultf(state, "\\f");
                break;
            default:
                if (byte < 0x20u || byte == 0x7fu) {
                    ZrCore_Log_Resultf(state, "\\x%02X", (unsigned int)byte);
                } else {
                    ZrCore_Log_Resultf(state, "%c", (int)byte);
                }
                break;
        }
    }
    ZrCore_Log_Resultf(state, "\"\n");
}

void ZrCli_ReplSemanticFacts_WriteOwnership(SZrState *state, const SZrSemanticOwnershipFact *fact) {
    if (fact == ZR_NULL) {
        return;
    }

    if (fact->isViolation) {
        ZrCore_Log_Resultf(state,
                           "Ownership violation: %s\n",
                           fact->diagnosticMessage != ZR_NULL
                               ? repl_string_text(fact->diagnosticMessage)
                               : repl_ownership_qualifier_text(fact->qualifier));
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Ownership: %s %s\n",
                       repl_ownership_fact_kind_text(fact->kind),
                       repl_ownership_qualifier_text(fact->qualifier));
}

static const TZrChar *repl_expression_fact_kind_text(EZrSemanticExpressionFactKind kind) {
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
            return "ownership builtin";
        case ZR_SEMANTIC_EXPRESSION_FACT_CONVERSION:
            return "conversion";
        case ZR_SEMANTIC_EXPRESSION_FACT_ERROR:
            return "error";
        case ZR_SEMANTIC_EXPRESSION_FACT_UNKNOWN:
        default:
            return "unknown";
    }
}

static const TZrChar *repl_fact_exactness_text(EZrSemanticFactExactness exactness) {
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

static void repl_write_expression_constant(SZrState *state, const SZrSemanticExpressionFact *fact) {
    if (state == ZR_NULL || fact == ZR_NULL || !fact->hasConstant) {
        return;
    }

    switch (fact->valueKind) {
        case ZR_SEMANTIC_VALUE_KIND_BOOL:
            ZrCore_Log_Resultf(state,
                               "Constant: %s\n",
                               fact->constantValue.boolValue ? "true" : "false");
            break;
        case ZR_SEMANTIC_VALUE_KIND_INT64:
            ZrCore_Log_Resultf(state,
                               "Constant: %lld\n",
                               (long long)fact->constantValue.int64Value);
            break;
        case ZR_SEMANTIC_VALUE_KIND_UINT64:
            ZrCore_Log_Resultf(state,
                               "Constant: %llu\n",
                               (unsigned long long)fact->constantValue.uint64Value);
            break;
        case ZR_SEMANTIC_VALUE_KIND_DOUBLE:
            ZrCore_Log_Resultf(state,
                               "Constant: %.17g\n",
                               fact->constantValue.doubleValue);
            break;
        case ZR_SEMANTIC_VALUE_KIND_STRING:
            repl_write_escaped_string_constant(state, fact->constantValue.stringValue);
            break;
        case ZR_SEMANTIC_VALUE_KIND_NULL:
            ZrCore_Log_Resultf(state, "Constant: null\n");
            break;
        case ZR_SEMANTIC_VALUE_KIND_UNKNOWN:
        default:
            break;
    }
}

void ZrCli_ReplSemanticFacts_WriteExpression(SZrState *state, const SZrSemanticExpressionFact *fact) {
    const TZrChar *callTargetName;
    const TZrChar *memberName;

    if (fact == ZR_NULL) {
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Expression: %s %s\n",
                       repl_expression_fact_kind_text(fact->kind),
                       repl_fact_exactness_text(fact->exactness));
    repl_write_expression_constant(state, fact);

    if (fact->hasCallInfo) {
        callTargetName = repl_string_text(fact->callTargetName);
        ZrCore_Log_Resultf(state,
                           "Call: %s args=%u\n",
                           callTargetName[0] != '\0' ? callTargetName : "unknown",
                           (unsigned)fact->argumentCount);
    }

    if (fact->hasMemberInfo) {
        memberName = repl_string_text(fact->memberName);
        ZrCore_Log_Resultf(state,
                           "Member: %s\n",
                           memberName[0] != '\0' ? memberName : "computed");
    }
}

static const TZrChar *repl_reference_kind_text(EZrSemanticReferenceKind kind) {
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
            return "member";
        case ZR_SEMANTIC_REFERENCE_MEMBER_WRITE:
            return "member write";
        case ZR_SEMANTIC_REFERENCE_UNKNOWN:
        default:
            return "unknown";
    }
}

void ZrCli_ReplSemanticFacts_WriteReferenceAtRange(SZrState *state,
                                                   SZrSemanticContext *semanticContext,
                                                   SZrFileRange range) {
    const SZrSemanticReferenceFact *fact;

    if (state == ZR_NULL ||
        semanticContext == ZR_NULL) {
        return;
    }

    fact = ZrParser_SemanticFacts_FindReferenceAtPosition(semanticContext, range);
    if (fact == ZR_NULL || fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION) {
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Reference: %s %s\n",
                       repl_reference_kind_text(fact->kind),
                       repl_string_text(fact->name));
    if (fact->isResolved &&
        (fact->declarationRange.source != ZR_NULL ||
         fact->declarationRange.start.line != 0 ||
         fact->declarationRange.start.column != 0 ||
         fact->declarationRange.start.offset != 0)) {
        ZrCore_Log_Resultf(state,
                           "Declared at: %d:%d\n",
                           fact->declarationRange.start.line,
                           fact->declarationRange.start.column);
    }
}

static const TZrChar *repl_reachability_cause_text(EZrSemanticReachabilityCause cause) {
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

void ZrCli_ReplSemanticFacts_WriteReachabilityAtRange(SZrState *state,
                                                      SZrSemanticContext *semanticContext,
                                                      SZrFileRange range,
                                                      const SZrSemanticReachabilityFact **lastFact) {
    const SZrSemanticReachabilityFact *fact;
    const SZrSemanticReachabilityFact *last;

    if (state == ZR_NULL ||
        semanticContext == ZR_NULL) {
        return;
    }

    last = lastFact != ZR_NULL ? *lastFact : ZR_NULL;
    fact = ZrParser_SemanticFacts_FindReachabilityAtPosition(semanticContext, range);
    if (fact == ZR_NULL ||
        fact == last ||
        fact->state != ZR_SEMANTIC_REACHABILITY_UNREACHABLE) {
        return;
    }

    ZrCore_Log_Resultf(state,
                       "Reachability: unreachable %s\n",
                       repl_reachability_cause_text(fact->cause));
    if (lastFact != ZR_NULL) {
        *lastFact = fact;
    }
}
