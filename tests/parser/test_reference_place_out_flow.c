#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/place.h"

#include "../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h"

static SZrState *g_state;

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

static SZrAstNode *parse_source(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "reference_place_out_flow.zr",
            strlen("reference_place_out_flow.zr"));
    return ZrParser_Parse(g_state, source, strlen(source), sourceName);
}

static SZrAstNode *script_statement(SZrAstNode *script, TZrSize index) {
    TEST_ASSERT_NOT_NULL(script);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, script->type);
    TEST_ASSERT_NOT_NULL(script->data.script.statements);
    TEST_ASSERT_GREATER_THAN_UINT32((TZrUInt32)index,
                                    (TZrUInt32)script->data.script.statements->count);
    return script->data.script.statements->nodes[index];
}

static void release_compiler_function(SZrCompilerState *cs) {
    if (cs->topLevelFunction != ZR_NULL && cs->topLevelFunction != cs->currentFunction) {
        ZrCore_Function_Free(g_state, cs->topLevelFunction);
        cs->topLevelFunction = ZR_NULL;
    }
    if (cs->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, cs->currentFunction);
        cs->currentFunction = ZR_NULL;
    }
}

static void assert_script_compile(const TZrChar *source,
                                  TZrBool expectedSuccess,
                                  const TZrChar *expectedMessage) {
    SZrCompilerState cs;
    SZrAstNode *script = parse_source(source);

    TEST_ASSERT_NOT_NULL(script);
    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(cs.currentFunction);

    compile_script(&cs, script);
    if (expectedSuccess) {
        TEST_ASSERT_FALSE(cs.hasError);
    } else {
        TEST_ASSERT_TRUE(cs.hasError);
        TEST_ASSERT_NOT_NULL(cs.errorMessage);
        TEST_ASSERT_NOT_NULL_MESSAGE(
                strstr(cs.errorMessage, expectedMessage), cs.errorMessage);
    }

    release_compiler_function(&cs);
    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, script);
}

static void assert_out_validation(const TZrChar *source,
                                  TZrBool expectedSuccess,
                                  const TZrChar *expectedMessage) {
    SZrCompilerState cs;
    SZrAstNode *script = parse_source(source);
    SZrAstNode *declaration;
    SZrFunctionDeclaration *function;
    TZrBool result;

    TEST_ASSERT_NOT_NULL(script);
    declaration = script_statement(
            script,
            script->data.script.statements->count - 1u);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, declaration->type);
    function = &declaration->data.functionDeclaration;

    memset(&cs, 0, sizeof(cs));
    ZrParser_CompilerState_Init(&cs, g_state);
    cs.suppressErrorOutput = ZR_TRUE;
    cs.scriptAst = script;
    result = compiler_validate_out_parameter_definite_assignment(
            &cs,
            function->params,
            function->body,
            declaration->location);

    TEST_ASSERT_EQUAL_INT(expectedSuccess, result);
    TEST_ASSERT_EQUAL_INT(!expectedSuccess, cs.hasError);
    if (!expectedSuccess) {
        TEST_ASSERT_NOT_NULL(cs.errorMessage);
        TEST_ASSERT_NOT_NULL_MESSAGE(
                strstr(cs.errorMessage, expectedMessage), cs.errorMessage);
    }

    ZrParser_CompilerState_Free(&cs);
    ZrParser_Ast_Free(g_state, script);
}

static void test_place_classifier_accepts_local_field_and_index_but_rejects_rvalues(void) {
    const TZrChar *source =
            "local;\n"
            "record.field;\n"
            "values[index];\n"
            "factory().field;\n"
            "local + 1;\n";
    const EZrParserPlaceExpressionKind expected[] = {
            ZR_PARSER_PLACE_EXPRESSION_LOCAL,
            ZR_PARSER_PLACE_EXPRESSION_FIELD,
            ZR_PARSER_PLACE_EXPRESSION_INDEX,
            ZR_PARSER_PLACE_EXPRESSION_INVALID,
            ZR_PARSER_PLACE_EXPRESSION_INVALID,
    };
    SZrAstNode *script = parse_source(source);

    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(expected); index++) {
        SZrAstNode *statement = script_statement(script, index);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);
        TEST_ASSERT_EQUAL_INT(
                expected[index],
                ZrParser_PlaceExpression_Classify(statement->data.expressionStatement.expr));
    }

    ZrParser_Ast_Free(g_state, script);
}

static void test_call_contract_requires_exact_markers_and_writable_places(void) {
    assert_script_compile(
            "fn fill(value: out int): void { value = 1; }\n"
            "var target: int = 0;\n"
            "fill(target);\n",
            ZR_FALSE,
            "requires the 'out' argument marker");
    assert_script_compile(
            "fn fill(value: out int): void { value = 1; }\n"
            "var target: int = 0;\n"
            "fill(ref target);\n",
            ZR_FALSE,
            "requires the 'out' argument marker");
    assert_script_compile(
            "fn consume(value: int): void {}\n"
            "var target: int = 0;\n"
            "consume(out target);\n",
            ZR_FALSE,
            "do not accept an argument marker");
    assert_script_compile(
            "fn fill(value: out int): void { value = 1; }\n"
            "fill(out 1);\n",
            ZR_FALSE,
            "writable Place");
    assert_script_compile(
            "struct Cell { pub var value: int = 0; }\n"
            "fn fill(value: out int): void { value = 1; }\n"
            "fn assign(result: out int, input: int): void { result = input; }\n"
            "fn touch(value: ref int): void {}\n"
            "fn forward(value: ref int): void { touch(ref value); }\n"
            "var target: int = 0;\n"
            "var cell: Cell = $Cell();\n"
            "var items = [0, 1];\n"
            "fill(out target);\n"
            "fill(out cell.value);\n"
            "fill(out items[0]);\n"
            "assign(input: 7, result: out target);\n"
            "touch(ref target);\n",
            ZR_TRUE,
            ZR_NULL);
}

static void test_out_assignment_joins_conditional_paths(void) {
    assert_out_validation(
            "fn fill(value: out int, flag: bool): void {\n"
            "  if (flag) { value = 1; } else { value = 2; }\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int, flag: bool): void {\n"
            "  if (flag) { value = 1; }\n"
            "}\n",
            ZR_FALSE,
            "must be assigned on every normal return path");
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  if (initialize(out value)) {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int, flag: bool): void {\n"
            "  flag && initialize(out value);\n"
            "}\n",
            ZR_FALSE,
            "must be assigned on every normal return path");
}

static void test_out_assignment_tracks_fields_and_cross_call_transfer(void) {
    assert_out_validation(
            "struct Pair {\n"
            "  pub var left: int;\n"
            "  pub var right: int;\n"
            "}\n"
            "fn fill(pair: out Pair): void { pair.left = 1; }\n",
            ZR_FALSE,
            "field 'right'");
    assert_out_validation(
            "struct Pair {\n"
            "  pub var left: int;\n"
            "  pub var right: int;\n"
            "}\n"
            "fn fill(pair: out Pair): void { pair.left = 1; pair.right = 2; }\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void { initialize(out value); }\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  var old = value;\n"
            "  value = 1;\n"
            "}\n",
            ZR_FALSE,
            "cannot be read before it is initialized");
}

static void test_out_assignment_distinguishes_normal_exception_and_loop_edges(void) {
    assert_out_validation(
            "fn fail(value: out int): void { throw 1; }\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  try { initialize(out value); } catch (error) { value = 2; }\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  try { initialize(out value); } catch (error) {}\n"
            "}\n",
            ZR_FALSE,
            "must be assigned on every normal return path");
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  try { value = 1; mayThrow(); } catch (error) {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  try { throw 1; } finally {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  try { initialize(out value); } finally {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  try {\n"
            "    try { mayThrow(); } finally { value = 1; }\n"
            "  } catch (error) {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int, flag: bool): void {\n"
            "  while (flag) { value = 1; }\n"
            "}\n",
            ZR_FALSE,
            "must be assigned on every normal return path");
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  for (value = 1; false; ) {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  while (initialize(out value) && false) {}\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
    assert_out_validation(
            "fn fill(value: out int, flag: bool): void {\n"
            "  for (; flag; value) {}\n"
            "  value = 1;\n"
            "}\n",
            ZR_FALSE,
            "cannot be read before it is initialized");
    assert_out_validation(
            "fn fill(value: out int): void {\n"
            "  while (true) { value = 1; break; }\n"
            "}\n",
            ZR_TRUE,
            ZR_NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_place_classifier_accepts_local_field_and_index_but_rejects_rvalues);
    RUN_TEST(test_call_contract_requires_exact_markers_and_writable_places);
    RUN_TEST(test_out_assignment_joins_conditional_paths);
    RUN_TEST(test_out_assignment_tracks_fields_and_cross_call_transfer);
    RUN_TEST(test_out_assignment_distinguishes_normal_exception_and_loop_edges);
    return UNITY_END();
}
