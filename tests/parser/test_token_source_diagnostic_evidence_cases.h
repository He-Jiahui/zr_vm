#ifndef ZR_TEST_TOKEN_SOURCE_DIAGNOSTIC_EVIDENCE_CASES_H
#define ZR_TEST_TOKEN_SOURCE_DIAGNOSTIC_EVIDENCE_CASES_H

#include "parser/parser_ownership.h"
#include "parser/parser_range_evidence.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/raw_object.h"
#include "zr_vm_parser/semantic.h"

enum { ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS = 16 };

typedef struct SZrDiagnosticEvidenceFixture {
    SZrSemanticContext *context;
    SZrState *foreignState;
    SZrSemanticDiagnosticFact inputs[2];
    SZrGcRootHandle roots[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    SZrString *strings[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize rootCount;
} SZrDiagnosticEvidenceFixture;

static SZrDiagnosticEvidenceFixture g_diagnosticEvidence;
static const char diagnostic_evidence_message[] =
        "Diagnostic evidence must preserve the complete native token interval and its physical source fragments";
static const char diagnostic_evidence_label[] =
        "Caller supplied label must survive automatic source evidence publication unchanged";

static void diagnostic_evidence_release_inputs(void) {
    TZrSize index;

    for (index = 0U; index < 2U; ++index) {
        ZrParser_StructuredDiagnostic_Free(g_state, &g_diagnosticEvidence.inputs[index].diagnostic);
    }
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        ZrCore_GcRootHandle_Release(g_state, &g_diagnosticEvidence.roots[index]);
    }
    memset(g_diagnosticEvidence.inputs, 0, sizeof(g_diagnosticEvidence.inputs));
    memset(g_diagnosticEvidence.roots, 0, sizeof(g_diagnosticEvidence.roots));
    memset(g_diagnosticEvidence.strings, 0, sizeof(g_diagnosticEvidence.strings));
    g_diagnosticEvidence.rootCount = 0U;
}

static void token_source_diagnostic_evidence_cleanup(void) {
    if (g_diagnosticEvidence.context != ZR_NULL) {
        ZrParser_SemanticContext_Free(g_diagnosticEvidence.context);
        g_diagnosticEvidence.context = ZR_NULL;
    }
    diagnostic_evidence_release_inputs();
    if (g_diagnosticEvidence.foreignState != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_diagnosticEvidence.foreignState);
        g_diagnosticEvidence.foreignState = ZR_NULL;
    }
    memset(&g_diagnosticEvidence, 0, sizeof(g_diagnosticEvidence));
}

static void diagnostic_evidence_hold_string(SZrString *string) {
    TZrSize slot = g_diagnosticEvidence.rootCount;

    TEST_ASSERT_NOT_NULL_MESSAGE(string, "Fixture prerequisite: diagnostic input text allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(slot < ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS,
            "Fixture prerequisite: diagnostic text root inventory overflowed");
    g_diagnosticEvidence.strings[slot] = string;
    g_diagnosticEvidence.rootCount++;
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_GcRootHandle_Create(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(string), &g_diagnosticEvidence.roots[slot]),
            "Fixture prerequisite: diagnostic input text rooting failed");
}

static SZrString *diagnostic_evidence_string(const char *text) {
    SZrString *string = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)text);

    diagnostic_evidence_hold_string(string);
    return string;
}

static void diagnostic_evidence_context(void) {
    g_diagnosticEvidence.context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_diagnosticEvidence.context,
            "Fixture prerequisite: diagnostic semantic context allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(g_diagnosticEvidence.context->diagnosticFacts.isValid &&
            g_diagnosticEvidence.context->diagnosticFacts.head != ZR_NULL,
            "Fixture prerequisite: diagnostic fact storage is unavailable");
}

static void diagnostic_evidence_input(TZrSize slot, SZrAstNode *node,
        SZrFileRange location, const char *code, const SZrFileRange *callerLocation) {
    SZrSemanticDiagnosticFact *input = &g_diagnosticEvidence.inputs[slot];

    memset(input, 0, sizeof(*input));
    input->node = node;
    ZrParser_StructuredDiagnostic_Init(&input->diagnostic);
    input->diagnostic.location = location;
    input->diagnostic.code = diagnostic_evidence_string(code);
    input->diagnostic.message = diagnostic_evidence_string(diagnostic_evidence_message);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_StructuredDiagnostic_SetNoFixReason(
            &input->diagnostic, ZR_DIAGNOSTIC_NO_FIX_REASON_NOT_APPLICABLE),
            "Fixture prerequisite: diagnostic no-fix contract could not be prepared");
    if (callerLocation != ZR_NULL) {
        SZrStructuredDiagnosticRelatedInformation related = {0};

        related.location = *callerLocation;
        related.message = diagnostic_evidence_string(diagnostic_evidence_label);
        TEST_ASSERT_TRUE_MESSAGE(ZrCore_Array_TryInit(g_state, &input->diagnostic.relatedInformation,
                sizeof(related), 1U),
                "Fixture prerequisite: caller diagnostic label allocation failed");
        ZrCore_Array_Push(g_state, &input->diagnostic.relatedInformation, &related);
    }
}

static TZrBool diagnostic_evidence_same_position(SZrFilePosition left, SZrFilePosition right) {
    return left.offset == right.offset && left.line == right.line && left.column == right.column;
}

static TZrBool diagnostic_evidence_same_physical(SZrFileRange left, SZrFileRange right) {
    return left.originKey != 0U && left.originKey == right.originKey &&
            diagnostic_evidence_same_position(left.start, right.start) &&
            diagnostic_evidence_same_position(left.end, right.end);
}

static TZrBool diagnostic_evidence_same_range(SZrFileRange left, SZrFileRange right) {
    return diagnostic_evidence_same_physical(left, right) && left.source == right.source &&
            left.ownerKey == right.ownerKey && left.evidenceIndex == right.evidenceIndex &&
            left.flags == right.flags;
}

static TZrBool diagnostic_evidence_text_is(SZrString *string, const char *expected) {
    return string != ZR_NULL && ZrCore_String_GetNativeString(string) != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(string), expected) == 0;
}

static SZrAstOwnership *diagnostic_evidence_require_owner(
        SZrAstNode *node, TZrUInt32 first, TZrUInt32 last, SZrFileRange *atoms) {
    SZrAstOwnership *owner;
    SZrFileRange interval;
    TZrUInt32 index;

    TEST_ASSERT_NOT_NULL_MESSAGE(node, "Fixture prerequisite: diagnostic AST node is missing");
    owner = node->ownership;
    TEST_ASSERT_NOT_NULL_MESSAGE(owner, "Fixture prerequisite: diagnostic AST ownership is missing");
    TEST_ASSERT_TRUE_MESSAGE(g_compositeRange.ast->ownership == owner &&
            g_compositeRange.parser.lexer->ownership == owner &&
            node->location.ownerKey == ZrParser_Ownership_Key(owner) &&
            node->location.originKey != 0U && node->location.evidenceIndex != 0U &&
            node->location.flags == ZR_FILE_RANGE_FLAG_COMPOSITE &&
            ZrParser_RangeEvidence_Validate(owner, node->location),
            "Fixture prerequisite: diagnostic range does not have complete composite ownership");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_TokenLocation(
            owner, node->location.evidenceIndex, first, last, &interval) &&
            diagnostic_evidence_same_range(interval, node->location),
            "Fixture prerequisite: parsed diagnostic interval has unexpected native ordinals");
    for (index = first; index <= last; ++index) {
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Location(owner, index, &atoms[index - first]) &&
                ZrParser_RangeEvidence_Validate(owner, atoms[index - first]) &&
                atoms[index - first].flags == 0U,
                "Fixture prerequisite: diagnostic native singleton evidence is unavailable");
    }
    TEST_ASSERT_TRUE_MESSAGE(diagnostic_evidence_same_physical(node->location, atoms[0]),
            "Fixture prerequisite: parsed composite range lost its first real anchor");
    return owner;
}

/* Existing labels are a preserved prefix; automatic labels cover only missing
 * physical fragments, in native order, without repeating the primary anchor. */
static TZrBool diagnostic_evidence_matches(const SZrStructuredDiagnostic *diagnostic,
        SZrFileRange original, const SZrFileRange *atoms, TZrSize count,
        const SZrFileRange *callerLocation) {
    TZrSize prefix = callerLocation != ZR_NULL ? 1U : 0U;
    TZrSize expectedCount = prefix;
    TZrSize relatedIndex = prefix;
    TZrSize index;

    for (index = 1U; index < count; ++index) {
        TZrSize previous;
        TZrBool duplicate = callerLocation != ZR_NULL &&
                diagnostic_evidence_same_physical(atoms[index], *callerLocation);

        for (previous = 0U; previous < index; ++previous) {
            duplicate = duplicate || diagnostic_evidence_same_physical(atoms[index], atoms[previous]);
        }
        if (!duplicate) {
            expectedCount++;
        }
    }
    if (diagnostic == ZR_NULL || count == 0U ||
            !diagnostic_evidence_same_range(diagnostic->location, original) ||
            !diagnostic_evidence_text_is(diagnostic->message, diagnostic_evidence_message) ||
            !diagnostic->relatedInformation.isValid ||
            diagnostic->relatedInformation.length != expectedCount) {
        return ZR_FALSE;
    }
    if (callerLocation != ZR_NULL) {
        const SZrStructuredDiagnosticRelatedInformation *related =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&diagnostic->relatedInformation, 0U);
        if (!diagnostic_evidence_same_range(related->location, *callerLocation) ||
                !diagnostic_evidence_text_is(related->message, diagnostic_evidence_label)) {
            return ZR_FALSE;
        }
    }
    for (index = 1U; index < count; ++index) {
        const SZrStructuredDiagnosticRelatedInformation *related;
        TZrSize previous;
        TZrBool duplicate = callerLocation != ZR_NULL &&
                diagnostic_evidence_same_physical(atoms[index], *callerLocation);

        for (previous = 0U; previous < index; ++previous) {
            duplicate = duplicate || diagnostic_evidence_same_physical(atoms[index], atoms[previous]);
        }
        if (duplicate) {
            continue;
        }
        related =
                (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                        (SZrArray *)&diagnostic->relatedInformation, relatedIndex++);
        if (!diagnostic_evidence_same_physical(related->location, atoms[index]) ||
                related->location.source == ZR_NULL || atoms[index].source == ZR_NULL ||
                !ZrCore_String_Equal(related->location.source, atoms[index].source) ||
                related->message == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static SZrAstNode *diagnostic_evidence_binary(TZrSize variant, SZrFileRange atoms[3]) {
    TZrTokenSourceId operands;
    TZrTokenSourceId operators;
    const char *operandName = variant == 1U ? "same.zr" : "operands.zr";
    const char *operatorName = variant == 1U ? "same.zr" : "operator.zr";
    TZrSize left = variant == 2U ? 3U : 0U;
    TZrSize plus = variant == 2U ? 4U : 0U;
    TZrSize right = variant == 2U ? 0U : 2U;
    TZrSize semi = variant == 2U ? 1U : 3U;
    SZrAstNode *binary;

    composite_range_begin();
    composite_range_create_builder();
    if (variant == 2U) {
        operandName = "reordered.zr";
        operatorName = operandName;
        operands = composite_range_add_source(operandName, "2; 1+9;");
        operators = operands;
    } else {
        operands = composite_range_add_source(operandName, "1;2;");
        operators = composite_range_add_source(operatorName, "+");
        TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(operands, operators,
                "Fixture prerequisite: diagnostic physical source IDs must be distinct");
    }
    composite_range_append_integer(operands, left, 1, 0U);
    composite_range_append_punct(operators, plus, ZR_TK_PLUS, '+', 1U);
    composite_range_append_integer(operands, right, 2, 2U);
    composite_range_append_punct(operands, semi, ZR_TK_SEMICOLON, ';', 3U);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_compositeRange.stream),
            "Fixture prerequisite: diagnostic binary stream sealing failed");
    composite_range_require_binary_input(operands, operators, operandName, operatorName,
            left, plus, right, semi);
    binary = composite_range_require_binary(composite_range_parse(1U));
    composite_range_require_binary_input(operands, operators, operandName, operatorName,
            left, plus, right, semi);
    TEST_ASSERT_TRUE_MESSAGE(composite_range_location_is(binary->data.binaryExpression.left,
            operandName, left, left + 1U) &&
            composite_range_location_is(binary->data.binaryExpression.right, operandName, right, right + 1U),
            "Fixture prerequisite: diagnostic binary leaf anchors changed");
    diagnostic_evidence_require_owner(binary, 1U, 3U, atoms);
    if (variant != 2U) {
        TEST_ASSERT_TRUE_MESSAGE(atoms[0].originKey == atoms[2].originKey &&
                atoms[0].originKey != atoms[1].originKey,
                "Fixture prerequisite: diagnostic A/B/A physical origin identity changed");
    } else {
        TEST_ASSERT_TRUE_MESSAGE(atoms[0].originKey == atoms[1].originKey &&
                atoms[0].originKey == atoms[2].originKey,
                "Fixture prerequisite: diagnostic reverse splice must use one physical source");
    }
    return binary;
}

static void diagnostic_evidence_release_parser_inputs(void) {
    ZrParser_State_Free(&g_compositeRange.parser);
    g_compositeRange.parserInitialized = ZR_FALSE;
    ZrCore_Gc_RootLeaseRelease(g_state->global, &g_compositeRange.entryNameRoot);
    ZrParser_TokenStream_Release(g_compositeRange.stream);
    g_compositeRange.stream = ZR_NULL;
    ZrParser_TokenStream_Release(g_compositeRange.right);
    g_compositeRange.right = ZR_NULL;
    ZrParser_TokenStream_Release(g_compositeRange.left);
    g_compositeRange.left = ZR_NULL;
    ZrParser_SyntaxContextTable_Release(g_compositeRange.table);
    g_compositeRange.table = ZR_NULL;
    diagnostic_evidence_release_inputs();
}

static void test_composite_diagnostic_preserves_all_physical_fragments(void) {
    TZrBool results[3] = {ZR_FALSE, ZR_FALSE, ZR_FALSE};
    TZrSize variant;

    for (variant = 0U; variant < 3U; ++variant) {
        SZrFileRange atoms[3];
        SZrAstNode *binary = diagnostic_evidence_binary(variant, atoms);
        SZrFileRange original = binary->location;
        TZrBool appended;

        diagnostic_evidence_context();
        diagnostic_evidence_input(0U, binary, original, "native_composite_diagnostic_evidence", &atoms[1]);
        appended = ZrParser_SemanticFacts_AppendDiagnostic(
                g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]);
        if (appended && g_diagnosticEvidence.context->diagnosticFacts.length == 1U) {
            const SZrSemanticDiagnosticFact *published =
                    (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                            &g_diagnosticEvidence.context->diagnosticFacts, 0U);
            results[variant] = published->node == binary &&
                    diagnostic_evidence_text_is(published->diagnostic.code, "native_composite_diagnostic_evidence") &&
                    diagnostic_evidence_matches(&published->diagnostic, original, atoms, 3U, &atoms[1]);
            if (results[variant]) {
                SZrGarbageCollectorStatsSnapshot before = {0};
                SZrGarbageCollectorStatsSnapshot after = {0};

                /* Do not collect legacy copied text until evidence publication
                 * succeeds: the current RED must remain a behavioral failure. */
                diagnostic_evidence_release_parser_inputs();
                ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &before);
                ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
                ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
                ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &after);
                TEST_ASSERT_TRUE_MESSAGE(after.fullCollectionCount > before.fullCollectionCount,
                        "Fixture prerequisite: diagnostic lifetime full GC did not complete");
                published = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                        &g_diagnosticEvidence.context->diagnosticFacts, 0U);
                results[variant] = published->node == binary &&
                        diagnostic_evidence_text_is(published->diagnostic.code, "native_composite_diagnostic_evidence") &&
                        diagnostic_evidence_matches(&published->diagnostic, original, atoms, 3U, &atoms[1]);
            }
        }
        printf("Diagnostic physical fragments variant=%u appended=%u facts=%llu complete=%u\n",
                (unsigned)variant, (unsigned)appended,
                (unsigned long long)g_diagnosticEvidence.context->diagnosticFacts.length,
                (unsigned)results[variant]);
        token_source_diagnostic_evidence_cleanup();
        token_source_composite_range_cleanup();
    }
    if (!results[0] || !results[1] || !results[2]) {
        TEST_FAIL_MESSAGE("Composite diagnostics must retain the first real anchor and every unique physical fragment");
    }
}

static void diagnostic_evidence_legacy_control(void) {
    SZrFileRange unknown = {0};
    const SZrSemanticDiagnosticFact *published;

    diagnostic_evidence_context();
    diagnostic_evidence_input(0U, ZR_NULL, unknown, "legacy_unknown_diagnostic_evidence", ZR_NULL);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]) &&
            ZrParser_SemanticFacts_AppendDiagnostic(
                    g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]),
            "Fixture prerequisite: legacy node-NULL diagnostic publication failed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, g_diagnosticEvidence.context->diagnosticFacts.length,
            "Fixture prerequisite: legacy unknown-range diagnostic deduplication changed");
    published = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
            &g_diagnosticEvidence.context->diagnosticFacts, 0U);
    TEST_ASSERT_TRUE_MESSAGE(published->node == ZR_NULL && published->diagnostic.location.source == ZR_NULL &&
            published->diagnostic.location.originKey == 0U && published->diagnostic.location.ownerKey == 0U &&
            published->diagnostic.location.evidenceIndex == 0U &&
            diagnostic_evidence_text_is(published->diagnostic.code, "legacy_unknown_diagnostic_evidence") &&
            diagnostic_evidence_text_is(published->diagnostic.message, diagnostic_evidence_message),
            "Fixture prerequisite: legacy node-NULL diagnostic payload changed");
    token_source_diagnostic_evidence_cleanup();
}

static SZrAstNode *diagnostic_evidence_left_associative(SZrAstNode **outInner, SZrFileRange atoms[5]) {
    TZrTokenSourceId operands;
    TZrTokenSourceId operators;
    SZrAstNodeArray *statements;
    SZrAstNode *outer;
    SZrAstNode *inner;
    TZrSize index;

    composite_range_begin();
    composite_range_create_builder();
    operands = composite_range_add_source("interval_operands.zr", "1 2 3;");
    operators = composite_range_add_source("interval_operators.zr", "++");
    TEST_ASSERT_NOT_EQUAL_UINT32_MESSAGE(operands, operators,
            "Fixture prerequisite: diagnostic interval source IDs must be distinct");
    composite_range_append_integer(operands, 0U, 1, 0U);
    composite_range_append_punct(operators, 0U, ZR_TK_PLUS, '+', 1U);
    composite_range_append_integer(operands, 2U, 2, 2U);
    composite_range_append_punct(operators, 1U, ZR_TK_PLUS, '+', 3U);
    composite_range_append_integer(operands, 4U, 3, 4U);
    composite_range_append_punct(operands, 5U, ZR_TK_SEMICOLON, ';', 5U);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Seal(g_compositeRange.stream),
            "Fixture prerequisite: diagnostic interval stream sealing failed");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(6U, ZrParser_TokenStream_Count(g_compositeRange.stream),
            "Fixture prerequisite: diagnostic interval root token count changed");
    for (index = 0U; index < 6U; ++index) {
        TZrBool isOperator = index == 1U || index == 3U;
        TZrBool isSemicolon = index == 5U;
        TZrSize offset = isOperator ? (index - 1U) / 2U : index;
        const char *raw = isOperator ? "+" : isSemicolon ? ";" : index == 0U ? "1" : index == 2U ? "2" : "3";

        composite_range_require_leaf(ZrParser_TokenStream_At(g_compositeRange.stream, index),
                isOperator ? operators : operands,
                isOperator ? "interval_operators.zr" : "interval_operands.zr", offset, offset + 1U,
                isOperator || isSemicolon ? ZR_TOKEN_TREE_PUNCT : ZR_TOKEN_TREE_LITERAL,
                isOperator ? ZR_TK_PLUS : isSemicolon ? ZR_TK_SEMICOLON : ZR_TK_INTEGER,
                raw, isOperator || isSemicolon ? 0 : (TZrInt64)(index / 2U + 1U));
    }
    statements = composite_range_parse(1U);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_EXPRESSION_STATEMENT, statements->nodes[0]->type,
            "Fixture prerequisite: diagnostic interval statement is not an expression");
    outer = statements->nodes[0]->data.expressionStatement.expr;
    TEST_ASSERT_NOT_NULL_MESSAGE(outer, "Fixture prerequisite: diagnostic outer binary is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_BINARY_EXPRESSION, outer->type,
            "Fixture prerequisite: diagnostic outer expression is not a binary");
    inner = outer->data.binaryExpression.left;
    TEST_ASSERT_NOT_NULL_MESSAGE(inner, "Fixture prerequisite: diagnostic inner binary is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_BINARY_EXPRESSION, inner->type,
            "Fixture prerequisite: diagnostic addition is not left associative");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("+", inner->data.binaryExpression.op.op,
            "Fixture prerequisite: diagnostic inner operator changed");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("+", outer->data.binaryExpression.op.op,
            "Fixture prerequisite: diagnostic outer operator changed");
    TEST_ASSERT_NOT_NULL_MESSAGE(inner->data.binaryExpression.left,
            "Fixture prerequisite: diagnostic first operand is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(inner->data.binaryExpression.right,
            "Fixture prerequisite: diagnostic second operand is missing");
    TEST_ASSERT_NOT_NULL_MESSAGE(outer->data.binaryExpression.right,
            "Fixture prerequisite: diagnostic third operand is missing");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, inner->data.binaryExpression.left->type,
            "Fixture prerequisite: diagnostic first operand is not an integer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, inner->data.binaryExpression.right->type,
            "Fixture prerequisite: diagnostic second operand is not an integer");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AST_INTEGER_LITERAL, outer->data.binaryExpression.right->type,
            "Fixture prerequisite: diagnostic third operand is not an integer");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(1, inner->data.binaryExpression.left->data.integerLiteral.value,
            "Fixture prerequisite: diagnostic first operand value changed");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(2, inner->data.binaryExpression.right->data.integerLiteral.value,
            "Fixture prerequisite: diagnostic second operand value changed");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(3, outer->data.binaryExpression.right->data.integerLiteral.value,
            "Fixture prerequisite: diagnostic third operand value changed");
    diagnostic_evidence_require_owner(outer, 1U, 5U, atoms);
    {
        SZrFileRange innerAtoms[3];
        diagnostic_evidence_require_owner(inner, 1U, 3U, innerAtoms);
    }
    TEST_ASSERT_TRUE_MESSAGE(inner->ownership == outer->ownership &&
            diagnostic_evidence_same_physical(inner->location, outer->location) &&
            inner->location.evidenceIndex != outer->location.evidenceIndex,
            "Fixture prerequisite: diagnostic inner and outer intervals must share only their anchor");
    *outInner = inner;
    return outer;
}

static void test_diagnostic_dedup_uses_native_interval_identity(void) {
    SZrAstNode *inner = ZR_NULL;
    SZrAstNode *outer;
    SZrFileRange atoms[5];
    SZrFileRange equivalent;
    TZrUInt32 equivalentIndex = 0U;
    TZrBool innerAppended, outerAppended, aliasAppended;
    TZrBool passed = ZR_FALSE;

    diagnostic_evidence_legacy_control();
    outer = diagnostic_evidence_left_associative(&inner, atoms);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Interval(
            outer->ownership, 1U, 3U, &equivalentIndex) &&
            equivalentIndex != inner->location.evidenceIndex &&
            ZrParser_RangeEvidence_Location(outer->ownership, equivalentIndex, &equivalent) &&
            ZrParser_RangeEvidence_Validate(outer->ownership, equivalent) &&
            diagnostic_evidence_same_physical(equivalent, inner->location),
            "Fixture prerequisite: equivalent diagnostic interval handle could not be created");
    diagnostic_evidence_context();
    diagnostic_evidence_input(0U, outer, inner->location, "native_interval_diagnostic_evidence", ZR_NULL);
    innerAppended = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]);
    g_diagnosticEvidence.inputs[0].diagnostic.location = outer->location;
    outerAppended = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]);
    g_diagnosticEvidence.inputs[0].diagnostic.location = equivalent;
    aliasAppended = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]);
    if (innerAppended && outerAppended && aliasAppended &&
            g_diagnosticEvidence.context->diagnosticFacts.length == 2U) {
        const SZrSemanticDiagnosticFact *first = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                &g_diagnosticEvidence.context->diagnosticFacts, 0U);
        const SZrSemanticDiagnosticFact *second = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                &g_diagnosticEvidence.context->diagnosticFacts, 1U);
        passed = first->node == outer && second->node == outer &&
                diagnostic_evidence_text_is(first->diagnostic.code, "native_interval_diagnostic_evidence") &&
                diagnostic_evidence_text_is(second->diagnostic.code, "native_interval_diagnostic_evidence") &&
                diagnostic_evidence_matches(&first->diagnostic, inner->location, atoms, 3U, ZR_NULL) &&
                diagnostic_evidence_matches(&second->diagnostic, outer->location, atoms, 5U, ZR_NULL);
    }
    printf("Diagnostic intervals inner=%u outer=%u alias=%u facts=%llu complete=%u\n",
            (unsigned)innerAppended, (unsigned)outerAppended, (unsigned)aliasAppended,
            (unsigned long long)g_diagnosticEvidence.context->diagnosticFacts.length, (unsigned)passed);
    token_source_diagnostic_evidence_cleanup();
    token_source_composite_range_cleanup();
    if (!passed) {
        TEST_FAIL_MESSAGE("Diagnostic deduplication must distinguish native intervals and ignore equivalent handles");
    }
}

static TZrBool diagnostic_evidence_budget_case(EZrParserComptimeBudgetResource resource) {
    SZrFileRange atoms[3];
    SZrAstNode *binary = diagnostic_evidence_binary(0U, atoms);
    SZrParserComptimeBudget *budget = ZrParser_Ownership_Budget(binary->ownership);
    SZrParserComptimeBudgetLimits limits;
    SZrParserComptimeBudgetUsage usage;
    SZrGarbageCollectorStatsSnapshot before = {0};
    SZrGarbageCollectorStatsSnapshot after = {0};
    SZrArray oldHeader;
    SZrSemanticDiagnosticFact oldFact;
    SZrStructuredDiagnosticRelatedInformation oldRelated;
    TZrSize roots;
    TZrSize pinRefs[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize temporaryRefs[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize index;
    TZrUInt64 cap;
    TZrBool appended, rejected, category, factsUnchanged, rootsUnchanged, usageUnchanged, pinsUnchanged;
    TZrBool retried, retryComplete = ZR_FALSE;

    TEST_ASSERT_NOT_NULL_MESSAGE(budget, "Fixture prerequisite: diagnostic owner budget is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(budget->usage.heapBytes > 0U && budget->usage.fuel > 0U &&
            budget->exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE,
            "Fixture prerequisite: diagnostic owner budget did not complete preparation");
    diagnostic_evidence_context();
    diagnostic_evidence_input(0U, binary->data.binaryExpression.left, atoms[0],
            "native_diagnostic_budget_seed", &atoms[0]);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]) &&
            g_diagnosticEvidence.context->diagnosticFacts.length == 1U,
            "Fixture prerequisite: ordinary diagnostic publication control failed");
    {
        const SZrSemanticDiagnosticFact *seed = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                &g_diagnosticEvidence.context->diagnosticFacts, 0U);
        const SZrStructuredDiagnosticRelatedInformation *related;

        TEST_ASSERT_TRUE_MESSAGE(seed->node == binary->data.binaryExpression.left &&
                diagnostic_evidence_same_range(seed->diagnostic.location, atoms[0]) &&
                diagnostic_evidence_text_is(seed->diagnostic.code, "native_diagnostic_budget_seed") &&
                seed->diagnostic.relatedInformation.isValid && seed->diagnostic.relatedInformation.length == 1U,
                "Fixture prerequisite: ordinary diagnostic publication payload changed");
        related = (const SZrStructuredDiagnosticRelatedInformation *)ZrCore_Array_Get(
                (SZrArray *)&seed->diagnostic.relatedInformation, 0U);
        TEST_ASSERT_TRUE_MESSAGE(diagnostic_evidence_same_range(related->location, atoms[0]) &&
                diagnostic_evidence_text_is(related->message, diagnostic_evidence_label),
                "Fixture prerequisite: ordinary diagnostic caller label changed");
        diagnostic_evidence_hold_string(seed->diagnostic.code);
        diagnostic_evidence_hold_string(seed->diagnostic.message);
        diagnostic_evidence_hold_string(related->message);
    }
    diagnostic_evidence_input(1U, binary, binary->location, "native_diagnostic_budget_candidate", &atoms[1]);
    limits = budget->limits;
    usage = budget->usage;
    cap = resource == ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES ? usage.heapBytes : usage.fuel;
    memcpy(&oldHeader, &g_diagnosticEvidence.context->diagnosticFacts, sizeof(oldHeader));
    memcpy(&oldFact, oldHeader.head, sizeof(oldFact));
    memcpy(&oldRelated, oldFact.diagnostic.relatedInformation.head, sizeof(oldRelated));
    roots = ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &before);
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        SZrRawObject *raw = ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index]);
        pinRefs[index] = raw->garbageCollectMark.nativeCallPinRefCount;
        temporaryRefs[index] = raw->garbageCollectMark.temporaryRootRefCount;
    }
    if (resource == ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES) {
        budget->limits.heapBytes = cap;
    } else {
        budget->limits.fuel = cap;
    }
    appended = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[1]);
    rejected = !appended;
    category = budget->exceededResource == resource && budget->exceededLimit == cap &&
            budget->requestedUsage > cap;
    factsUnchanged = memcmp(&oldHeader, &g_diagnosticEvidence.context->diagnosticFacts, sizeof(oldHeader)) == 0 &&
            memcmp(oldHeader.head, &oldFact, sizeof(oldFact)) == 0 &&
            memcmp(oldFact.diagnostic.relatedInformation.head, &oldRelated, sizeof(oldRelated)) == 0 &&
            diagnostic_evidence_text_is(oldFact.diagnostic.code, "native_diagnostic_budget_seed") &&
            diagnostic_evidence_text_is(oldFact.diagnostic.message, diagnostic_evidence_message) &&
            diagnostic_evidence_text_is(oldRelated.message, diagnostic_evidence_label);
    usageUnchanged = memcmp(&usage, &budget->usage, sizeof(usage)) == 0;
    rootsUnchanged = roots == ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &after);
    rootsUnchanged = rootsUnchanged && before.ignoredObjectCount == after.ignoredObjectCount;
    pinsUnchanged = ZR_TRUE;
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        SZrRawObject *raw = ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index]);
        if (pinRefs[index] != raw->garbageCollectMark.nativeCallPinRefCount ||
                temporaryRefs[index] != raw->garbageCollectMark.temporaryRootRefCount) {
            pinsUnchanged = ZR_FALSE;
        }
    }
    /* Restore only limits, not usage or failure metadata, before retrying. */
    budget->limits = limits;
    retried = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[1]);
    if (retried && g_diagnosticEvidence.context->diagnosticFacts.length == 2U) {
        const SZrSemanticDiagnosticFact *published = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
                &g_diagnosticEvidence.context->diagnosticFacts, 1U);
        retryComplete = published->node == binary &&
                diagnostic_evidence_text_is(published->diagnostic.code, "native_diagnostic_budget_candidate") &&
                diagnostic_evidence_matches(&published->diagnostic, binary->location, atoms, 3U, &atoms[1]);
    }
    printf("Diagnostic budget resource=%u rejected=%u category=%u facts=%u roots=%u pins=%u usage=%u retry=%u\n",
            (unsigned)resource, (unsigned)rejected, (unsigned)category, (unsigned)factsUnchanged,
            (unsigned)rootsUnchanged, (unsigned)pinsUnchanged, (unsigned)usageUnchanged, (unsigned)retryComplete);
    token_source_diagnostic_evidence_cleanup();
    token_source_composite_range_cleanup();
    return rejected && category && factsUnchanged && rootsUnchanged && pinsUnchanged && usageUnchanged && retryComplete;
}

static void test_diagnostic_evidence_budget_failure_is_atomic(void) {
    TZrBool heap = diagnostic_evidence_budget_case(ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES);
    TZrBool fuel = diagnostic_evidence_budget_case(ZR_PARSER_COMPTIME_BUDGET_FUEL);

    if (!heap || !fuel) {
        TEST_FAIL_MESSAGE("Diagnostic evidence budget failure must leave publication and ownership state unchanged and permit retry");
    }
}

static void test_diagnostic_evidence_rejects_foreign_context_domain(void) {
    SZrFileRange atoms[3];
    SZrAstNode *binary = diagnostic_evidence_binary(0U, atoms);
    SZrAstOwnership *owner = binary->ownership;
    SZrParserComptimeBudget *budget = ZrParser_Ownership_Budget(owner);
    SZrParserComptimeBudgetUsage usage;
    SZrGcDomainIdentity domainA;
    SZrGcDomainIdentity domainB;
    SZrGarbageCollectorStatsSnapshot beforeA = {0};
    SZrGarbageCollectorStatsSnapshot beforeB = {0};
    SZrGarbageCollectorStatsSnapshot afterA = {0};
    SZrGarbageCollectorStatsSnapshot afterB = {0};
    SZrSemanticDiagnosticFact original;
    SZrFileRange originalLocation;
    SZrStructuredDiagnosticRelatedInformation originalRelated;
    SZrArray originalFacts;
    TZrSize pinRefs[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize temporaryRefs[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize rootsA;
    TZrSize rootsB;
    TZrSize afterRootsA;
    TZrSize afterRootsB;
    TZrSize index;
    TZrBool appended;
    TZrBool factsUnchanged;
    TZrBool usageUnchanged;
    TZrBool rootsUnchangedA;
    TZrBool rootsUnchangedB;
    TZrBool pinsUnchanged = ZR_TRUE;
    TZrBool inputUnchanged;
    TZrBool passed;

    TEST_ASSERT_NOT_NULL_MESSAGE(g_state->global,
            "Fixture prerequisite: diagnostic source runtime global is unavailable");
    TEST_ASSERT_NOT_NULL_MESSAGE(budget,
            "Fixture prerequisite: foreign-context diagnostic owner budget is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(budget->usage.heapBytes > 0U && budget->usage.fuel > 0U &&
            budget->exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE,
            "Fixture prerequisite: foreign-context diagnostic owner preparation failed");
    g_diagnosticEvidence.foreignState = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_diagnosticEvidence.foreignState,
            "Fixture prerequisite: independent diagnostic runtime allocation failed");
    TEST_ASSERT_NOT_NULL_MESSAGE(g_diagnosticEvidence.foreignState->global,
            "Fixture prerequisite: independent diagnostic runtime global is unavailable");
    domainA = ZrCore_GcDomain_GetIdentity(g_state);
    domainB = ZrCore_GcDomain_GetIdentity(g_diagnosticEvidence.foreignState);
    TEST_ASSERT_TRUE_MESSAGE(g_state != g_diagnosticEvidence.foreignState &&
            g_state->global != g_diagnosticEvidence.foreignState->global &&
            domainA.id != 0U && domainA.generation != 0U &&
            domainB.id != 0U && domainB.generation != 0U &&
            ZrCore_GcDomain_IdentityIsCurrent(g_state, domainA) &&
            ZrCore_GcDomain_IdentityIsCurrent(g_diagnosticEvidence.foreignState, domainB) &&
            !ZrCore_GcDomain_IdentityEquals(domainA, domainB),
            "Fixture prerequisite: diagnostic runtimes must have distinct live GC domains");
    g_diagnosticEvidence.context = ZrParser_SemanticContext_New(g_diagnosticEvidence.foreignState);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_diagnosticEvidence.context,
            "Fixture prerequisite: foreign diagnostic context allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(g_diagnosticEvidence.context->state == g_diagnosticEvidence.foreignState &&
            g_diagnosticEvidence.context->diagnosticFacts.isValid &&
            g_diagnosticEvidence.context->diagnosticFacts.head != ZR_NULL &&
            g_diagnosticEvidence.context->diagnosticFacts.elementSize == sizeof(SZrSemanticDiagnosticFact) &&
            g_diagnosticEvidence.context->diagnosticFacts.capacity > 0U &&
            g_diagnosticEvidence.context->diagnosticFacts.length == 0U,
            "Fixture prerequisite: foreign diagnostic context must have empty valid fact storage");
    diagnostic_evidence_input(0U, binary, binary->location, "native_foreign_context_diagnostic_evidence", &atoms[1]);
    diagnostic_evidence_hold_string(g_diagnosticEvidence.inputs[0].diagnostic.location.source);
    diagnostic_evidence_hold_string(atoms[1].source);
    TEST_ASSERT_TRUE_MESSAGE(g_diagnosticEvidence.inputs[0].diagnostic.relatedInformation.isValid &&
            g_diagnosticEvidence.inputs[0].diagnostic.relatedInformation.length == 1U,
            "Fixture prerequisite: foreign-context diagnostic caller label is unavailable");
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        SZrRawObject *raw = ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index]);
        TEST_ASSERT_TRUE_MESSAGE(ZrCore_GcDomain_ObjectBelongsToState(g_state, raw) &&
                !ZrCore_GcDomain_ObjectBelongsToState(g_diagnosticEvidence.foreignState, raw),
                "Fixture prerequisite: diagnostic inputs must belong only to the AST runtime domain");
        pinRefs[index] = raw->garbageCollectMark.nativeCallPinRefCount;
        temporaryRefs[index] = raw->garbageCollectMark.temporaryRootRefCount;
    }
    usage = budget->usage;
    memcpy(&originalFacts, &g_diagnosticEvidence.context->diagnosticFacts, sizeof(originalFacts));
    memcpy(&original, &g_diagnosticEvidence.inputs[0], sizeof(original));
    memcpy(&originalLocation, &g_diagnosticEvidence.inputs[0].diagnostic.location, sizeof(originalLocation));
    memcpy(&originalRelated, original.diagnostic.relatedInformation.head, sizeof(originalRelated));
    rootsA = ZrCore_GcDomain_GetRootCount(g_state);
    rootsB = ZrCore_GcDomain_GetRootCount(g_diagnosticEvidence.foreignState);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &beforeA);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_diagnosticEvidence.foreignState->global, &beforeB);

    appended = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]);
    factsUnchanged = memcmp(&originalFacts,
            &g_diagnosticEvidence.context->diagnosticFacts, sizeof(originalFacts)) == 0;
    usageUnchanged = memcmp(&usage, &budget->usage, sizeof(usage)) == 0;
    afterRootsA = ZrCore_GcDomain_GetRootCount(g_state);
    afterRootsB = ZrCore_GcDomain_GetRootCount(g_diagnosticEvidence.foreignState);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &afterA);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_diagnosticEvidence.foreignState->global, &afterB);
    rootsUnchangedA = rootsA == afterRootsA && beforeA.ignoredObjectCount == afterA.ignoredObjectCount;
    rootsUnchangedB = rootsB == afterRootsB && beforeB.ignoredObjectCount == afterB.ignoredObjectCount;
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        SZrRawObject *raw = ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index]);
        if (pinRefs[index] != raw->garbageCollectMark.nativeCallPinRefCount ||
                temporaryRefs[index] != raw->garbageCollectMark.temporaryRootRefCount) {
            pinsUnchanged = ZR_FALSE;
        }
    }
    inputUnchanged = memcmp(&original, &g_diagnosticEvidence.inputs[0], sizeof(original)) == 0 &&
            memcmp(&originalLocation, &g_diagnosticEvidence.inputs[0].diagnostic.location,
                    sizeof(originalLocation)) == 0;
    if (inputUnchanged) {
        inputUnchanged = memcmp(&originalRelated,
                g_diagnosticEvidence.inputs[0].diagnostic.relatedInformation.head, sizeof(originalRelated)) == 0;
    }
    passed = !appended && factsUnchanged && usageUnchanged && rootsUnchangedA && rootsUnchangedB &&
            pinsUnchanged && inputUnchanged;
    printf("Diagnostic foreign context rejected=%u facts=%u usage=%u rootsA=%u rootsB=%u pins=%u input=%u\n",
            (unsigned)!appended, (unsigned)factsUnchanged, (unsigned)usageUnchanged,
            (unsigned)rootsUnchangedA, (unsigned)rootsUnchangedB, (unsigned)pinsUnchanged,
            (unsigned)inputUnchanged);
    token_source_diagnostic_evidence_cleanup();
    token_source_composite_range_cleanup();
    if (!passed) {
        TEST_FAIL_MESSAGE("Diagnostic evidence must reject a foreign context domain without changing publication or ownership");
    }
}

static void test_diagnostic_evidence_duplicate_success_charges_fuel(void) {
    SZrFileRange atoms[3];
    SZrAstNode *binary = diagnostic_evidence_binary(0U, atoms);
    SZrParserComptimeBudget *budget = ZrParser_Ownership_Budget(binary->ownership);
    SZrParserComptimeBudgetUsage usage;
    SZrParserComptimeBudgetUsage afterUsage;
    SZrGarbageCollectorStatsSnapshot before = {0};
    SZrGarbageCollectorStatsSnapshot after = {0};
    SZrArray originalHeader;
    SZrSemanticDiagnosticFact originalFact;
    SZrStructuredDiagnosticRelatedInformation originalRelated[2];
    SZrSemanticDiagnosticFact originalInput;
    SZrStructuredDiagnosticRelatedInformation originalInputRelated;
    const SZrSemanticDiagnosticFact *published;
    TZrSize pinRefs[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize temporaryRefs[ZR_TEST_DIAGNOSTIC_EVIDENCE_ROOTS];
    TZrSize roots;
    TZrSize index;
    TZrBool appended;
    TZrBool factsUnchanged;
    TZrBool rootsUnchanged;
    TZrBool pinsUnchanged = ZR_TRUE;
    TZrBool inputUnchanged;
    TZrBool otherUsageUnchanged;
    TZrBool fuelCharged;
    TZrBool passed;

    TEST_ASSERT_NOT_NULL_MESSAGE(budget,
            "Fixture prerequisite: duplicate diagnostic owner budget is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(budget->usage.heapBytes > 0U && budget->usage.fuel > 0U &&
            budget->exceededResource == ZR_PARSER_COMPTIME_BUDGET_NONE,
            "Fixture prerequisite: duplicate diagnostic owner preparation failed");
    diagnostic_evidence_context();
    diagnostic_evidence_input(0U, binary, binary->location,
            "native_duplicate_diagnostic_fuel", &atoms[1]);
    diagnostic_evidence_hold_string(atoms[0].source);
    diagnostic_evidence_hold_string(atoms[1].source);
    TEST_ASSERT_TRUE_MESSAGE(g_diagnosticEvidence.context->state == g_state &&
            g_diagnosticEvidence.inputs[0].diagnostic.relatedInformation.isValid &&
            g_diagnosticEvidence.inputs[0].diagnostic.relatedInformation.length == 1U,
            "Fixture prerequisite: duplicate diagnostic input must have one same-domain caller label");
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        TEST_ASSERT_TRUE_MESSAGE(ZrCore_GcDomain_ObjectBelongsToState(g_state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index])),
                "Fixture prerequisite: duplicate diagnostic input text belongs to another runtime");
    }
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]) &&
            g_diagnosticEvidence.context->diagnosticFacts.length == 1U,
            "Fixture prerequisite: initial complete diagnostic publication failed");
    published = (const SZrSemanticDiagnosticFact *)ZrCore_Array_Get(
            &g_diagnosticEvidence.context->diagnosticFacts, 0U);
    TEST_ASSERT_TRUE_MESSAGE(published != ZR_NULL && published->node == binary &&
            diagnostic_evidence_text_is(published->diagnostic.code, "native_duplicate_diagnostic_fuel") &&
            diagnostic_evidence_matches(&published->diagnostic, binary->location, atoms, 3U, &atoms[1]) &&
            published->diagnostic.relatedInformation.length == 2U,
            "Fixture prerequisite: initial diagnostic must preserve its complete physical evidence");

    usage = budget->usage;
    memcpy(&originalHeader, &g_diagnosticEvidence.context->diagnosticFacts, sizeof(originalHeader));
    memcpy(&originalFact, originalHeader.head, sizeof(originalFact));
    memcpy(originalRelated, originalFact.diagnostic.relatedInformation.head, sizeof(originalRelated));
    memcpy(&originalInput, &g_diagnosticEvidence.inputs[0], sizeof(originalInput));
    memcpy(&originalInputRelated, originalInput.diagnostic.relatedInformation.head,
            sizeof(originalInputRelated));
    roots = ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &before);
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        SZrRawObject *raw = ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index]);
        pinRefs[index] = raw->garbageCollectMark.nativeCallPinRefCount;
        temporaryRefs[index] = raw->garbageCollectMark.temporaryRootRefCount;
    }

    appended = ZrParser_SemanticFacts_AppendDiagnostic(
            g_diagnosticEvidence.context, &g_diagnosticEvidence.inputs[0]);
    factsUnchanged = memcmp(&originalHeader,
            &g_diagnosticEvidence.context->diagnosticFacts, sizeof(originalHeader)) == 0;
    if (factsUnchanged) {
        factsUnchanged = memcmp(originalHeader.head, &originalFact, sizeof(originalFact)) == 0 &&
                memcmp(originalFact.diagnostic.relatedInformation.head,
                        originalRelated, sizeof(originalRelated)) == 0;
    }
    afterUsage = budget->usage;
    fuelCharged = afterUsage.fuel > usage.fuel;
    afterUsage.fuel = usage.fuel;
    otherUsageUnchanged = memcmp(&usage, &afterUsage, sizeof(usage)) == 0;
    rootsUnchanged = roots == ZrCore_GcDomain_GetRootCount(g_state);
    ZrCore_GarbageCollector_GetStatsSnapshot(g_state->global, &after);
    rootsUnchanged = rootsUnchanged && before.ignoredObjectCount == after.ignoredObjectCount;
    for (index = 0U; index < g_diagnosticEvidence.rootCount; ++index) {
        SZrRawObject *raw = ZR_CAST_RAW_OBJECT_AS_SUPER(g_diagnosticEvidence.strings[index]);
        if (pinRefs[index] != raw->garbageCollectMark.nativeCallPinRefCount ||
                temporaryRefs[index] != raw->garbageCollectMark.temporaryRootRefCount) {
            pinsUnchanged = ZR_FALSE;
        }
    }
    inputUnchanged = memcmp(&originalInput, &g_diagnosticEvidence.inputs[0], sizeof(originalInput)) == 0;
    if (inputUnchanged) {
        inputUnchanged = memcmp(&originalInputRelated,
                g_diagnosticEvidence.inputs[0].diagnostic.relatedInformation.head,
                sizeof(originalInputRelated)) == 0;
    }
    passed = appended && factsUnchanged && rootsUnchanged && pinsUnchanged && inputUnchanged &&
            otherUsageUnchanged && fuelCharged;
    printf("Diagnostic duplicate appended=%u facts=%u roots=%u pins=%u input=%u otherUsage=%u fuel=%u before=%llu after=%llu\n",
            (unsigned)appended, (unsigned)factsUnchanged, (unsigned)rootsUnchanged,
            (unsigned)pinsUnchanged, (unsigned)inputUnchanged, (unsigned)otherUsageUnchanged,
            (unsigned)fuelCharged, (unsigned long long)usage.fuel,
            (unsigned long long)budget->usage.fuel);
    token_source_diagnostic_evidence_cleanup();
    token_source_composite_range_cleanup();
    if (!passed) {
        TEST_FAIL_MESSAGE("Successful diagnostic deduplication must charge fuel without changing publication or ownership");
    }
}

#endif
