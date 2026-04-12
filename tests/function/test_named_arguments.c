//
// Created by Auto on 2025/01/XX.
//

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "unity.h"
#include "test_support.h"
#include "zr_vm_parser.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

// 测试日志宏（符合测试规范）
#define TEST_START(summary) do { \
    printf("Unit Test - %s\n", summary); \
    fflush(stdout); \
} while(0)

#define TEST_INFO(summary, details) do { \
    printf("Testing %s:\n %s\n", summary, details); \
    fflush(stdout); \
} while(0)

#define TEST_PASS_CUSTOM(timer, summary) do { \
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Pass - Cost Time:%.3fms - %s\n", elapsed, summary); \
    fflush(stdout); \
} while(0)

#define TEST_FAIL_CUSTOM(timer, summary, reason) do { \
    clock_t failureTime = clock(); \
    double elapsed = ((double)(failureTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
    printf("Fail - Cost Time:%.3fms - %s:\n %s\n", elapsed, summary, reason); \
    fflush(stdout); \
} while(0)

#define TEST_DIVIDER() do { \
    printf("----------\n"); \
    fflush(stdout); \
} while(0)

#define TEST_MODULE_DIVIDER() do { \
    printf("==========\n"); \
    fflush(stdout); \
} while(0)

static SZrState* create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

static void destroy_test_state(SZrState* state) {
    ZrTests_State_Destroy(state);
}

static char* read_reference_file(const char* relativePath, size_t* outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
}

static void compile_reference_top_level_statement(SZrCompilerState* compilerState, SZrAstNode* node) {
    TEST_ASSERT_NOT_NULL(compilerState);
    TEST_ASSERT_NOT_NULL(node);

    switch (node->type) {
        case ZR_AST_INTERFACE_DECLARATION:
            ZrParser_Compiler_CompileInterfaceDeclaration(compilerState, node);
            break;
        case ZR_AST_CLASS_DECLARATION:
            ZrParser_Compiler_CompileClassDeclaration(compilerState, node);
            break;
        case ZR_AST_STRUCT_DECLARATION:
            ZrParser_Compiler_CompileStructDeclaration(compilerState, node);
            break;
        default:
            ZrParser_Statement_Compile(compilerState, node);
            break;
    }
}

typedef struct {
    TZrBool reported;
    SZrFileRange location;
    EZrToken token;
    char message[256];
} SZrCapturedParserDiagnostic;

typedef struct {
    TZrBool reported;
    SZrFileRange location;
    char message[256];
} SZrCapturedCompilerDiagnostic;

static void clear_parser_diagnostic(SZrCapturedParserDiagnostic* diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
}

static void clear_compiler_diagnostic(SZrCapturedCompilerDiagnostic* diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    memset(diagnostic, 0, sizeof(*diagnostic));
}

static void capture_parser_error(TZrPtr userData,
                                 const SZrFileRange* location,
                                 const TZrChar* message,
                                 EZrToken token) {
    SZrCapturedParserDiagnostic* diagnostic = (SZrCapturedParserDiagnostic*)userData;

    if (diagnostic == ZR_NULL) {
        return;
    }

    if (!diagnostic->reported) {
        diagnostic->reported = ZR_TRUE;
        diagnostic->token = token;
        if (location != ZR_NULL) {
            diagnostic->location = *location;
        }
        if (message != ZR_NULL) {
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
        }
    }
}

static SZrAstNode* parse_reference_source_with_diagnostic(SZrState* state,
                                                          const char* source,
                                                          size_t sourceLength,
                                                          const char* sourceNameText,
                                                          SZrCapturedParserDiagnostic* diagnostic) {
    SZrParserState parserState;
    SZrString* sourceName;
    SZrAstNode* ast;

    clear_parser_diagnostic(diagnostic);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, state, source, sourceLength, sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    if (diagnostic != ZR_NULL && !diagnostic->reported && parserState.hasError && parserState.errorMessage != ZR_NULL) {
        diagnostic->reported = ZR_TRUE;
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", parserState.errorMessage);
    }
    ZrParser_State_Free(&parserState);
    return ast;
}

static TZrBool find_reference_test_call(SZrAstNode* ast,
                                        SZrFunctionCall** outCall,
                                        SZrAstNodeArray** outParams) {
    SZrAstNode* functionNode;
    SZrAstNode* testNode;
    SZrAstNode* returnNode;
    SZrAstNode* expr;
    SZrAstNode* callNode;

    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (outParams != ZR_NULL) {
        *outParams = ZR_NULL;
    }

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count < 2) {
        return ZR_FALSE;
    }

    functionNode = ast->data.script.statements->nodes[0];
    testNode = ast->data.script.statements->nodes[1];
    if (functionNode == ZR_NULL || functionNode->type != ZR_AST_FUNCTION_DECLARATION ||
        testNode == ZR_NULL || testNode->type != ZR_AST_TEST_DECLARATION ||
        testNode->data.testDeclaration.body == ZR_NULL ||
        testNode->data.testDeclaration.body->type != ZR_AST_BLOCK ||
        testNode->data.testDeclaration.body->data.block.body == ZR_NULL ||
        testNode->data.testDeclaration.body->data.block.body->count == 0) {
        return ZR_FALSE;
    }

    returnNode = testNode->data.testDeclaration.body->data.block.body->nodes[0];
    if (returnNode == ZR_NULL || returnNode->type != ZR_AST_RETURN_STATEMENT || returnNode->data.returnStatement.expr == ZR_NULL) {
        return ZR_FALSE;
    }

    expr = returnNode->data.returnStatement.expr;
    if (expr->type != ZR_AST_PRIMARY_EXPRESSION ||
        expr->data.primaryExpression.members == ZR_NULL ||
        expr->data.primaryExpression.members->count == 0) {
        return ZR_FALSE;
    }

    callNode = expr->data.primaryExpression.members->nodes[0];
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }

    if (outCall != ZR_NULL) {
        *outCall = &callNode->data.functionCall;
    }
    if (outParams != ZR_NULL) {
        *outParams = functionNode->data.functionDeclaration.params;
    }

    return ZR_TRUE;
}

static SZrAstNode* find_reference_test_return_expression(SZrAstNode* ast) {
    SZrAstNode* testNode;
    SZrAstNode* returnNode;

    if (ast == ZR_NULL ||
        ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL ||
        ast->data.script.statements->count == 0) {
        return ZR_NULL;
    }

    testNode = ast->data.script.statements->nodes[ast->data.script.statements->count - 1];
    if (testNode == ZR_NULL ||
        testNode->type != ZR_AST_TEST_DECLARATION ||
        testNode->data.testDeclaration.body == ZR_NULL ||
        testNode->data.testDeclaration.body->type != ZR_AST_BLOCK ||
        testNode->data.testDeclaration.body->data.block.body == ZR_NULL ||
        testNode->data.testDeclaration.body->data.block.body->count == 0) {
        return ZR_NULL;
    }

    returnNode = testNode->data.testDeclaration.body->data.block.body->nodes[0];
    if (returnNode == ZR_NULL ||
        returnNode->type != ZR_AST_RETURN_STATEMENT ||
        returnNode->data.returnStatement.expr == ZR_NULL) {
        return ZR_NULL;
    }

    return returnNode->data.returnStatement.expr;
}

static void match_named_arguments_capture_diagnostic(SZrState* state,
                                                     SZrFunctionCall* call,
                                                     SZrAstNodeArray* paramList,
                                                     SZrCapturedCompilerDiagnostic* diagnostic) {
    SZrCompilerState compilerState;

    clear_compiler_diagnostic(diagnostic);

    ZrParser_CompilerState_Init(&compilerState, state);
    (void)ZrParser_Compiler_MatchNamedArguments(&compilerState, call, paramList);

    if (compilerState.errorMessage != ZR_NULL) {
        diagnostic->reported = ZR_TRUE;
        diagnostic->location = compilerState.errorLocation;
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", compilerState.errorMessage);
    }

    ZrParser_CompilerState_Free(&compilerState);
}

static TZrBool execute_test_function(SZrState* state, SZrFunction* testFunc, TZrInt64 expectedValue, const TZrChar* testName) {
    TZrInt64 actualValue = 0;

    ZR_UNUSED_PARAMETER(testName);

    if (!ZrTests_Function_ExecuteExpectInt64(state, testFunc, &actualValue)) {
        return ZR_FALSE;
    }

    if (expectedValue >= 0) {
        TEST_ASSERT_EQUAL_INT64(expectedValue, actualValue);
    }

    return ZR_TRUE;
}

static void test_reference_named_arguments_fixture_executes(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Reference Named Arguments Fixture Executes";
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrCompileResult compileResult;
    TZrSize sourceLength = 0;
    char* source = ZR_NULL;

    memset(&compileResult, 0, sizeof(compileResult));

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/calls/named_arguments_defaults_pass.zr", &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_named_arguments_defaults_pass.zr", 42);
    ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount > 0);
    TEST_ASSERT_TRUE(execute_test_function(state, compileResult.testFunctions[0], 128, testSummary));

    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_duplicate_named_argument_fixture_is_rejected(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Reference Duplicate Named Argument Fixture Is Rejected";
    SZrState* state = ZR_NULL;
    size_t sourceLength = 0;
    char* source = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrFunctionCall* call = ZR_NULL;
    SZrAstNodeArray* paramList = ZR_NULL;
    SZrCapturedCompilerDiagnostic diagnostic;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/calls_named_default_varargs/duplicate_named_fail.zr", &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    ast = parse_reference_source_with_diagnostic(state,
                                                 source,
                                                 sourceLength,
                                                 "reference_duplicate_named_fail.zr",
                                                 ZR_NULL);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(find_reference_test_call(ast, &call, &paramList));

    match_named_arguments_capture_diagnostic(state, call, paramList, &diagnostic);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Duplicate argument name"));
    TEST_ASSERT_EQUAL_INT(6, diagnostic.location.start.line);

    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_unexpected_named_argument_fixture_is_rejected(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Reference Unexpected Named Argument Fixture Is Rejected";
    SZrState* state = ZR_NULL;
    size_t sourceLength = 0;
    char* source = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrFunctionCall* call = ZR_NULL;
    SZrAstNodeArray* paramList = ZR_NULL;
    SZrCapturedCompilerDiagnostic diagnostic;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/calls_named_default_varargs/unexpected_named_fail.zr", &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    ast = parse_reference_source_with_diagnostic(state,
                                                 source,
                                                 sourceLength,
                                                 "reference_unexpected_named_fail.zr",
                                                 ZR_NULL);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(find_reference_test_call(ast, &call, &paramList));

    match_named_arguments_capture_diagnostic(state, call, paramList, &diagnostic);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Unknown argument name"));
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "d"));
    TEST_ASSERT_EQUAL_INT(6, diagnostic.location.start.line);

    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_positional_after_named_argument_fixture_is_rejected(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Reference Positional After Named Argument Fixture Is Rejected";
    SZrState* state = ZR_NULL;
    size_t sourceLength = 0;
    char* source = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrCapturedParserDiagnostic diagnostic;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/calls_named_default_varargs/positional_after_named_fail.zr",
                                 &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    ast = parse_reference_source_with_diagnostic(state,
                                                 source,
                                                 sourceLength,
                                                 "reference_positional_after_named_fail.zr",
                                                 &diagnostic);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Positional arguments cannot come after named arguments"));
    TEST_ASSERT_EQUAL_INT(6, diagnostic.location.start.line);

    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_reference_overload_ambiguity_fixture_is_rejected(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Reference Overload Ambiguity Fixture Is Rejected";
    SZrState* state = ZR_NULL;
    size_t sourceLength = 0;
    char* source = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrAstNode* expr = ZR_NULL;
    SZrCompilerState compilerState;
    SZrInferredType result;
    SZrArray candidates;
    SZrString* functionName = ZR_NULL;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/calls_named_default_varargs/overload_ambiguity_fail.zr",
                                 &sourceLength);
    TEST_ASSERT_NOT_NULL(source);

    ast = parse_reference_source_with_diagnostic(state,
                                                 source,
                                                 sourceLength,
                                                 "reference_overload_ambiguity_fail.zr",
                                                 ZR_NULL);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 3);

    ZrParser_CompilerState_Init(&compilerState, state);
    compilerState.scriptAst = ast;
    compilerState.currentFunction = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(compilerState.currentFunction);

    compile_reference_top_level_statement(&compilerState, ast->data.script.statements->nodes[0]);
    TEST_ASSERT_FALSE(compilerState.hasError);
    compile_reference_top_level_statement(&compilerState, ast->data.script.statements->nodes[1]);
    TEST_ASSERT_FALSE(compilerState.hasError);

    functionName = ZrCore_String_Create(state, "pick", 4);
    TEST_ASSERT_NOT_NULL(functionName);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_LookupFunctions(state, compilerState.typeEnv, functionName, &candidates));
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)candidates.length);
    ZrCore_Array_Free(state, &candidates);

    expr = find_reference_test_return_expression(ast);
    TEST_ASSERT_NOT_NULL(expr);

    ZrParser_InferredType_Init(state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(&compilerState, expr, &result));
    TEST_ASSERT_TRUE(compilerState.hasError);
    TEST_ASSERT_NOT_NULL(compilerState.errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(compilerState.errorMessage, "Ambiguous overload"));
    TEST_ASSERT_NOT_NULL(strstr(compilerState.errorMessage, "pick"));
    TEST_ASSERT_EQUAL_INT(10, compilerState.errorLocation.start.line);
    ZrParser_InferredType_Free(state, &result);

    ZrCore_Function_Free(state, compilerState.currentFunction);
    compilerState.currentFunction = ZR_NULL;
    ZrParser_CompilerState_Free(&compilerState);
    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

// ==================== 命名参数测试 ====================

// 测试1: 基本命名参数调用
static void test_named_arguments_basic(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Basic";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Basic named arguments", 
              "Testing function call with named arguments: func(c: 3, a: 1, b: 2)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testBasicNamedArgs(a: int, b: int, c: int): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testBasicNamedArgs(c: 3, a: 1, b: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_named_args.zr", 20);
    
    printf("  [DEBUG] Starting parsing...\n");
    fflush(stdout);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    printf("  [DEBUG] Parsing completed successfully.\n");
    fflush(stdout);
    
    // 执行测试函数
    SZrCompileResult compileResult;
    // 初始化编译结果
    compileResult.mainFunction = ZR_NULL;
    compileResult.testFunctions = ZR_NULL;
    compileResult.testFunctionCount = 0;
    
    printf("  [DEBUG] Starting compilation with tests...\n");
    fflush(stdout);
    
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    printf("  [DEBUG] Compilation completed. testFunctionCount: %zu\n", compileResult.testFunctionCount);
    fflush(stdout);
    
    // 查找测试函数（测试函数已复制主函数的 childFunctions，GET_SUB_FUNCTION 可从当前闭包解析顶层函数）
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 6, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }   
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试2: 位置参数和命名参数混合
static void test_named_arguments_mixed(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Mixed Positional and Named";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Mixed positional and named arguments", 
              "Testing: func(1, c: 3, b: 2)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testMixedArgs(a: int, b: int, c: int): int {\n"
        "    return a * 100 + b * 10 + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testMixedArgs(1, c: 3, b: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_mixed_args.zr", 20);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 123, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试3: 默认参数与命名参数结合
static void test_named_arguments_with_defaults(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - With Default Parameters";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Named arguments with default parameters", 
              "Testing: func(5) and func(5, c: 30)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testDefaultArgs(a: int, b: int = 10, c: int = 20): int {\n"
        "    return a + b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    var r1 = testDefaultArgs(5);\n"
        "    var r2 = testDefaultArgs(5, c: 30);\n"
        "    return r1 + r2;\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_default_args.zr", 22);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 80, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试4: 命名参数顺序无关
static void test_named_arguments_order_independent(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Order Independent";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Named arguments order independence", 
              "Testing: func(c: 10, a: 5, b: 3)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "testNamedOrder(a: int, b: int, c: int): int {\n"
        "    return a - b + c;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return testNamedOrder(c: 10, a: 5, b: 3);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_order.zr", 14);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 12, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试5: 复杂函数调用中的命名参数
static void test_named_arguments_complex(void) {
    SZrTestTimer timer;
    const TZrChar* testSummary = "Named Arguments - Complex Function Call";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Complex function call with named arguments", 
              "Testing: complexFunction(w: 4, x: 1, z: 3, y: 2)");
    
    const TZrChar* source = 
        "%module \"test\";\n"
        "complexFunction(x: int, y: int, z: int, w: int): int {\n"
        "    return x * 1000 + y * 100 + z * 10 + w;\n"
        "}\n"
        "%test(\"test\") {\n"
        "    return complexFunction(w: 4, x: 1, z: 3, y: 2);\n"
        "}\n";
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_complex.zr", 16);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse source code");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Parse failed");
    }
    
    SZrCompileResult compileResult;
    if (!ZrParser_Compiler_CompileWithTests(state, ast, &compileResult)) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile with tests");
        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Compile with tests failed");
    }
    
    if (compileResult.testFunctionCount > 0) {
        SZrFunction* testFunc = compileResult.testFunctions[0];
        if (testFunc != ZR_NULL) {
            if (!execute_test_function(state, testFunc, 1234, testSummary)) {
                TEST_FAIL_CUSTOM(timer, testSummary, "Failed to execute test function");
            }
        }
    }
    
    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    TEST_MODULE_DIVIDER();
    printf("Named Arguments Tests\n");
    TEST_MODULE_DIVIDER();
    
    RUN_TEST(test_named_arguments_basic);
    RUN_TEST(test_named_arguments_mixed);
    RUN_TEST(test_named_arguments_with_defaults);
    RUN_TEST(test_named_arguments_order_independent);
    RUN_TEST(test_named_arguments_complex);
    RUN_TEST(test_reference_named_arguments_fixture_executes);
    RUN_TEST(test_reference_duplicate_named_argument_fixture_is_rejected);
    RUN_TEST(test_reference_unexpected_named_argument_fixture_is_rejected);
    RUN_TEST(test_reference_positional_after_named_argument_fixture_is_rejected);
    RUN_TEST(test_reference_overload_ambiguity_fixture_is_rejected);
    
    return UNITY_END();
}

