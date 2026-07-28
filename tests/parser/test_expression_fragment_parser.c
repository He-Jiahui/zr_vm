#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

typedef struct SExpressionFragmentDiagnosticCapture {
    TZrUInt32 structuredErrorCount;
    TZrUInt32 legacyErrorCount;
    char firstLegacyMessage[192];
} SExpressionFragmentDiagnosticCapture;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void capture_structured_error(TZrPtr userData,
                                     const SZrStructuredDiagnostic *diagnostic,
                                     EZrToken token) {
    SExpressionFragmentDiagnosticCapture *capture =
            (SExpressionFragmentDiagnosticCapture *)userData;

    ZR_UNUSED_PARAMETER(token);
    if (capture != ZR_NULL && diagnostic != ZR_NULL &&
        diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        capture->structuredErrorCount++;
    }
}

static void capture_legacy_error(TZrPtr userData,
                                 const SZrFileRange *location,
                                 const TZrChar *message,
                                 EZrToken token) {
    SExpressionFragmentDiagnosticCapture *capture =
            (SExpressionFragmentDiagnosticCapture *)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL) {
        return;
    }

    if (capture->legacyErrorCount == 0u && message != ZR_NULL) {
        snprintf(capture->firstLegacyMessage,
                 sizeof(capture->firstLegacyMessage),
                 "%s",
                 message);
    }
    capture->legacyErrorCount++;
}

static SZrAstNode *parse_fragment(const TZrChar *source,
                                  SExpressionFragmentDiagnosticCapture *capture,
                                  SZrParserState *outParserState) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_NOT_NULL(outParserState);
    memset(capture, 0, sizeof(*capture));

    sourceName = ZrCore_String_CreateFromNative(g_state, "debug_expression.zr");
    TEST_ASSERT_NOT_NULL(sourceName);
    ZrParser_State_Init(outParserState,
                        g_state,
                        source,
                        strlen(source),
                        sourceName);
    outParserState->structuredErrorCallback = capture_structured_error;
    outParserState->errorCallback = capture_legacy_error;
    outParserState->errorUserData = capture;
    outParserState->suppressErrorOutput = ZR_TRUE;

    return ZrParser_ParseExpressionWithState(outParserState);
}

static void test_expression_fragment_parses_the_full_formal_expression(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment(
            "true ? 1 : 2",
            &capture,
            &parserState);

    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONDITIONAL_EXPRESSION, expression->type);
    TEST_ASSERT_EQUAL_UINT32(0u, capture.structuredErrorCount);
    TEST_ASSERT_EQUAL_UINT32(0u, capture.legacyErrorCount);

    ZrParser_Ast_Free(g_state, expression);
    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_reports_structured_parser_errors(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("value +", &capture, &parserState);

    TEST_ASSERT_NULL(expression);
    TEST_ASSERT_TRUE(parserState.hasError);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.structuredErrorCount);

    ZrParser_State_Free(&parserState);
}

static void test_expression_fragment_rejects_trailing_tokens(void) {
    SExpressionFragmentDiagnosticCapture capture;
    SZrParserState parserState;
    SZrAstNode *expression = parse_fragment("one two", &capture, &parserState);

    TEST_ASSERT_NULL(expression);
    TEST_ASSERT_TRUE(parserState.hasError);
    TEST_ASSERT_EQUAL_UINT32(1u, capture.legacyErrorCount);
    TEST_ASSERT_NOT_NULL(strstr(capture.firstLegacyMessage,
                                "Unexpected token after expression"));

    ZrParser_State_Free(&parserState);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_expression_fragment_parses_the_full_formal_expression);
    RUN_TEST(test_expression_fragment_reports_structured_parser_errors);
    RUN_TEST(test_expression_fragment_rejects_trailing_tokens);
    return UNITY_END();
}
