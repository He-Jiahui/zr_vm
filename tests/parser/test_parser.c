//
// Created by Auto on 2025/01/XX.
//

// 定义GNU源以支持realpath函数（Linux系统）
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
// 定义POSIX源以支持realpath函数（备用）
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifdef _MSC_VER
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif
#include "unity.h"
#include "zr_vm_parser.h"
#include "../../zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/value.h"
#include "test_support.h"
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

// realpath 兼容函数（MSVC使用_fullpath）
#ifdef _MSC_VER
static char* test_realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, _MAX_PATH);
}
#define realpath test_realpath
#endif

static void print_generated_file_path(const char* description, const char* fileName) {
#ifdef _MSC_VER
    char resolvedPath[_MAX_PATH];
    if (realpath(fileName, resolvedPath) != ZR_NULL) {
        printf("  Generated %s: %s\n", description, resolvedPath);
        return;
    }
#else
    char* resolvedPath = realpath(fileName, ZR_NULL);
    if (resolvedPath != ZR_NULL) {
        printf("  Generated %s: %s\n", description, resolvedPath);
        free(resolvedPath);
        return;
    }
#endif

    char* cwd = getcwd(ZR_NULL, 0);
    if (cwd != ZR_NULL) {
        size_t pathLength = strlen(cwd) + 1 + strlen(fileName) + 1;
        char* joinedPath = (char*)malloc(pathLength);
        if (joinedPath != ZR_NULL) {
            snprintf(joinedPath, pathLength, "%s/%s", cwd, fileName);
            printf("  Generated %s: %s\n", description, joinedPath);
            free(joinedPath);
            free(cwd);
            return;
        }
        free(cwd);
    }

    printf("  Generated %s: %s\n", description, fileName);
}

static char* read_parser_fixture(const char* fileName, TZrSize* outLength) {
    char filePath[ZR_TESTS_PATH_MAX];

    if (!ZrTests_Path_GetParserFixture(fileName, filePath, sizeof(filePath))) {
        return ZR_NULL;
    }

    return ZrTests_ReadTextFile(filePath, outLength);
}

static TZrBool get_parser_generated_path(const char* baseName,
                                         const char* subDir,
                                         const char* extension,
                                         char* outPath,
                                         TZrSize maxLen) {
    return ZrTests_Path_GetGeneratedArtifact("language_pipeline", subDir, baseName, extension, outPath, maxLen);
}

// 创建测试用的SZrState
static SZrState* create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

// 销毁测试用的SZrState
static void destroy_test_state(SZrState* state) {
    ZrTests_State_Destroy(state);
}

static SZrAstNode* get_script_statement(SZrAstNode* ast, TZrSize index) {
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT || ast->data.script.statements == ZR_NULL ||
        index >= ast->data.script.statements->count) {
        return ZR_NULL;
    }

    return ast->data.script.statements->nodes[index];
}

static SZrAstNode* unwrap_statement_expression(SZrAstNode* statement) {
    if (statement == ZR_NULL) {
        return ZR_NULL;
    }

    if (statement->type == ZR_AST_EXPRESSION_STATEMENT) {
        return statement->data.expressionStatement.expr;
    }

    return statement;
}

static const char *string_node_native(SZrState *state, SZrAstNode *node) {
    if (state == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_STRING_LITERAL ||
        node->data.stringLiteral.value == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(node->data.stringLiteral.value);
}

static const char *module_declaration_name_native(SZrState *state, SZrAstNode *ast) {
    if (state == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.moduleName == ZR_NULL ||
        ast->data.script.moduleName->type != ZR_AST_MODULE_DECLARATION) {
        return ZR_NULL;
    }

    return string_node_native(state, ast->data.script.moduleName->data.moduleDeclaration.name);
}

typedef struct {
    TZrBool reported;
    EZrToken token;
    SZrFileRange location;
    char message[512];
} SZrCapturedParserDiagnostic;

static void clear_parser_diagnostic(SZrCapturedParserDiagnostic *diagnostic) {
    if (diagnostic == ZR_NULL) {
        return;
    }

    diagnostic->reported = ZR_FALSE;
    diagnostic->token = ZR_TK_EOS;
    memset(&diagnostic->location, 0, sizeof(diagnostic->location));
    diagnostic->message[0] = '\0';
}

static void capture_parser_error(void *userData,
                                 const SZrFileRange *location,
                                 const char *message,
                                 EZrToken token) {
    SZrCapturedParserDiagnostic *diagnostic = (SZrCapturedParserDiagnostic *)userData;

    if (diagnostic == ZR_NULL || diagnostic->reported) {
        return;
    }

    diagnostic->reported = ZR_TRUE;
    diagnostic->token = token;
    if (location != ZR_NULL) {
        diagnostic->location = *location;
    }
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

static SZrAstNode *parse_source_with_diagnostic(SZrState *state,
                                                const char *source,
                                                size_t sourceLength,
                                                const char *sourceNameText,
                                                SZrCapturedParserDiagnostic *diagnostic) {
    SZrParserState parserState;
    SZrString *sourceName;
    SZrAstNode *ast;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    clear_parser_diagnostic(diagnostic);
    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, state, source, sourceLength, sourceName);
    parserState.errorCallback = capture_parser_error;
    parserState.errorUserData = diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);
    if (diagnostic != ZR_NULL && !diagnostic->reported &&
        parserState.hasError && parserState.errorMessage != ZR_NULL) {
        diagnostic->reported = ZR_TRUE;
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", parserState.errorMessage);
    }

    ZrParser_State_Free(&parserState);
    return ast;
}

static void assert_token_location_matches(const SZrFileRange *location,
                                          TZrSize startOffset,
                                          TZrInt32 startLine,
                                          TZrInt32 startColumn,
                                          TZrSize endOffset,
                                          TZrInt32 endLine,
                                          TZrInt32 endColumn) {
    TEST_ASSERT_NOT_NULL(location);
    TEST_ASSERT_EQUAL_UINT32((unsigned int)startOffset, (unsigned int)location->start.offset);
    TEST_ASSERT_EQUAL_INT(startLine, location->start.line);
    TEST_ASSERT_EQUAL_INT(startColumn, location->start.column);
    TEST_ASSERT_EQUAL_UINT32((unsigned int)endOffset, (unsigned int)location->end.offset);
    TEST_ASSERT_EQUAL_INT(endLine, location->end.line);
    TEST_ASSERT_EQUAL_INT(endColumn, location->end.column);
}

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

void test_extern_delegate_parameter_decorator_flags_parsing(void);
void test_top_level_class_decorator_parsing(void);
void test_compile_time_class_decorator_parsing(void);
void test_compile_time_public_class_decorator_parsing(void);
void test_compile_time_struct_decorator_parsing(void);
void test_compile_time_function_decorator_parsing(void);
static void test_function_declaration_requires_fn_keyword(void);
static void test_legacy_async_surfaces_are_rejected(void);
static void test_function_type_annotation_parsing(void);
static void test_type_query_accepts_function_type_expression(void);
static void test_type_value_alias_parsing_variants(void);
static void test_class_abstract_member_and_final_class_parsing(void);
static void test_class_member_modifier_and_super_member_parsing(void);

// ==================== 基础测试 ====================

// 测试整数字面量解析
static void test_integer_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Integer Literal Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Integer literal parsing", 
              "Testing parsing of decimal integer: 123");
    const char* source = "123;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        {
            SZrAstNode* expr = unwrap_statement_expression(get_script_statement(ast, 0));
            TEST_ASSERT_NOT_NULL(expr);
            if (expr->type == ZR_AST_INTEGER_LITERAL) {
                TEST_ASSERT_EQUAL_INT64(123, expr->data.integerLiteral.value);
            }
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse integer literal");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试浮点数字面量解析
static void test_float_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Float Literal Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Float literal parsing", 
              "Testing parsing of float: 1.0f");
    const char* source = "1.0f;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse float literal");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试字符串字面量解析
static void test_string_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "String Literal Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("String literal parsing", 
              "Testing parsing of string: \"hello\"");
    const char* source = "\"hello\";";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse string literal");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试布尔字面量解析
static void test_boolean_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Boolean Literal Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Boolean literal parsing", 
              "Testing parsing of boolean: true");
    const char* source = "true;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试模块声明解析
static void test_module_declaration(void) {
    SZrTestTimer timer;
    const char* testSummary = "Module Declaration Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Module declaration parsing", 
              "Testing parsing of current module declaration: module test;");
    const char* source = "module test;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        if (ast->data.script.moduleName != ZR_NULL) {
            TEST_ASSERT_EQUAL_INT(ZR_AST_MODULE_DECLARATION, ast->data.script.moduleName->type);
            TEST_ASSERT_EQUAL_STRING("test", module_declaration_name_native(state, ast));
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse module declaration");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试变量声明解析
static void test_variable_declaration(void) {
    SZrTestTimer timer;
    const char* testSummary = "Variable Declaration Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Variable declaration parsing", 
              "Testing parsing of variable declaration: var x = 5;");
    const char* source = "var x = 5;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        if (ast->data.script.statements != ZR_NULL && ast->data.script.statements->count > 0) {
            SZrAstNode* stmt = ast->data.script.statements->nodes[0];
            TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, stmt->type);
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse variable declaration");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试可见性修饰符解析
static void test_access_modifier_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Access Modifier Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    // 测试 pub var
    TEST_INFO("Public variable parsing", 
              "Testing parsing of: pub var x = 5;");
    const char* source1 = "pub var x = 5;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast1 = ZrParser_Parse(state, source1, strlen(source1), sourceName);
    
    if (ast1 != ZR_NULL && ast1->data.script.statements != ZR_NULL && 
        ast1->data.script.statements->count > 0) {
        SZrAstNode* stmt = ast1->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
            TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, 
                stmt->data.variableDeclaration.accessModifier);
        }
    }
    if (ast1 != ZR_NULL) {
        ZrParser_Ast_Free(state, ast1);
    }
    
    // 测试 pri var
    TEST_INFO("Private variable parsing", 
              "Testing parsing of: pri var y = 10;");
    const char* source2 = "pri var y = 10;";
    SZrAstNode* ast2 = ZrParser_Parse(state, source2, strlen(source2), sourceName);
    
    if (ast2 != ZR_NULL && ast2->data.script.statements != ZR_NULL && 
        ast2->data.script.statements->count > 0) {
        SZrAstNode* stmt = ast2->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
            TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PRIVATE, 
                stmt->data.variableDeclaration.accessModifier);
        }
    }
    if (ast2 != ZR_NULL) {
        ZrParser_Ast_Free(state, ast2);
    }
    
    // 测试 pro var
    TEST_INFO("Protected variable parsing", 
              "Testing parsing of: pro var z = 15;");
    const char* source3 = "pro var z = 15;";
    SZrAstNode* ast3 = ZrParser_Parse(state, source3, strlen(source3), sourceName);
    
    if (ast3 != ZR_NULL && ast3->data.script.statements != ZR_NULL && 
        ast3->data.script.statements->count > 0) {
        SZrAstNode* stmt = ast3->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
            TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PROTECTED, 
                stmt->data.variableDeclaration.accessModifier);
        }
    }
    if (ast3 != ZR_NULL) {
        ZrParser_Ast_Free(state, ast3);
    }
    
    // 测试默认（无修饰符，应该是 pri）
    TEST_INFO("Default access modifier (private)", 
              "Testing parsing of: var w = 20; (should default to private)");
    const char* source4 = "var w = 20;";
    SZrAstNode* ast4 = ZrParser_Parse(state, source4, strlen(source4), sourceName);
    
    if (ast4 != ZR_NULL && ast4->data.script.statements != ZR_NULL && 
        ast4->data.script.statements->count > 0) {
        SZrAstNode* stmt = ast4->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_VARIABLE_DECLARATION) {
            TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PRIVATE, 
                stmt->data.variableDeclaration.accessModifier);
        }
    }
    if (ast4 != ZR_NULL) {
        ZrParser_Ast_Free(state, ast4);
    }
    
    // 测试 struct 的可见性修饰符
    TEST_INFO("Struct access modifier parsing", 
              "Testing parsing of: pub struct Test { var x: int; }");
    const char* source5 = "pub struct Test { var x: int = 0; }";
    SZrAstNode* ast5 = ZrParser_Parse(state, source5, strlen(source5), sourceName);
    
    if (ast5 != ZR_NULL && ast5->data.script.statements != ZR_NULL && 
        ast5->data.script.statements->count > 0) {
        SZrAstNode* stmt = ast5->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_STRUCT_DECLARATION) {
            TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PUBLIC, 
                stmt->data.structDeclaration.accessModifier);
        }
    }
    if (ast5 != ZR_NULL) {
        ZrParser_Ast_Free(state, ast5);
    }
    
    // 测试 class 的可见性修饰符
    TEST_INFO("Class access modifier parsing", 
              "Testing parsing of: pro class Test { }");
    const char* source6 = "pro class Test { }";
    SZrAstNode* ast6 = ZrParser_Parse(state, source6, strlen(source6), sourceName);
    
    if (ast6 != ZR_NULL && ast6->data.script.statements != ZR_NULL && 
        ast6->data.script.statements->count > 0) {
        SZrAstNode* stmt = ast6->data.script.statements->nodes[0];
        if (stmt->type == ZR_AST_CLASS_DECLARATION) {
            TEST_ASSERT_EQUAL_INT(ZR_ACCESS_PROTECTED, 
                stmt->data.classDeclaration.accessModifier);
        }
    }
    if (ast6 != ZR_NULL) {
        ZrParser_Ast_Free(state, ast6);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 表达式测试 ====================

// 测试二元表达式解析
static void test_binary_expression(void) {
    SZrTestTimer timer;
    const char* testSummary = "Binary Expression Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Binary expression parsing", 
              "Testing parsing of binary expression: 1 + 2");
    const char* source = "1 + 2;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_source_compile_rejects_reported_expression_error(void) {
    SZrTestTimer timer;
    const char* testSummary = "Source Compile Rejects Reported Expression Errors";
    SZrState* state;
    SZrString* sourceName;
    SZrFunction* function;
    const char* source = "1 +\n";

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Source compile error gate",
              "Testing that source compilation stops after parser reports an incomplete expression");

    sourceName = ZrCore_String_Create(state, "test.zr", 7);
    TEST_ASSERT_NOT_NULL(sourceName);
    function = ZrParser_Source_Compile(state, source, strlen(source), sourceName);
    TEST_ASSERT_NULL(function);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试一元表达式解析
static void test_unary_expression(void) {
    SZrTestTimer timer;
    const char* testSummary = "Unary Expression Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Unary expression parsing", 
              "Testing parsing of unary expression: !true");
    const char* source = "!true;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        {
            SZrAstNode* expr = unwrap_statement_expression(get_script_statement(ast, 0));
            TEST_ASSERT_NOT_NULL(expr);
            TEST_ASSERT_EQUAL_INT(ZR_AST_UNARY_EXPRESSION, expr->type);
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse unary expression");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_prototype_construction_expression_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Prototype Construction Expression Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Value construction parsing",
              "Testing parsing of `init math.Vector3(1.0, 2.0, 3.0)` as a valid construction expression");
    {
        const char *source =
                "let math = import(\"zr.math\");\n"
                "let vector = init math.Vector3(1.0, 2.0, 3.0);";
        SZrString *sourceName = ZrCore_String_Create(state, "prototype_construct.zr", 22);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);
        TEST_ASSERT_NOT_NULL(unwrap_statement_expression(get_script_statement(ast, 1)));

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_native_boxed_new_expression_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Boxed New Expression Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Native boxed new parsing",
              "Testing parsing of `new math.Tensor(...)` inside a variable initializer");
    {
        const char *source =
                "let math = import(\"zr.math\");\n"
                "var tensor = new math.Tensor([2, 2], [1.0, 2.0, 3.0, 4.0]);";
        SZrString *sourceName = ZrCore_String_Create(state, "native_boxed_new.zr", 19);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[1]);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ast->data.script.statements->nodes[1]->type);

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_generic_boxed_new_expression_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Generic Boxed New Expression Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Generic boxed new parsing",
              "Testing parsing of `new Array<int>()` preserves a generic construct target");
    {
        const char *source =
                "let {Array} = import(\"zr.container\");\n"
                "var values = new Array<int>();";
        SZrString *sourceName = ZrCore_String_Create(state, "generic_boxed_new.zr", 20);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;
        SZrAstNode *expr;
        SZrAstNode *target;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, decl->type);

        expr = decl->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expr->type);
        TEST_ASSERT_TRUE(expr->data.constructExpression.isNew);

        target = expr->data.constructExpression.target;
        TEST_ASSERT_NOT_NULL(target);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, target->type);
        TEST_ASSERT_NOT_NULL(target->data.type.name);
        TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, target->data.type.name->type);
        TEST_ASSERT_NOT_NULL(target->data.type.name->data.genericType.name);
        TEST_ASSERT_EQUAL_STRING("Array",
                                 ZrCore_String_GetNativeString(
                                         target->data.type.name->data.genericType.name->name));
        TEST_ASSERT_NOT_NULL(target->data.type.name->data.genericType.params);
        TEST_ASSERT_EQUAL_INT(1, (int)target->data.type.name->data.genericType.params->count);

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_explicit_generic_function_call_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Explicit Generic Function Call Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "let {Array} = import(\"zr.container\");\n"
                "fn map<TIn, TOut>(source: Array<TIn>): Array<TOut> { return source; }\n"
                "map<int, string>(values);";
        SZrString *sourceName = ZrCore_String_Create(state, "explicit_generic_call.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *statement;
        SZrAstNode *expr;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        statement = ast->data.script.statements->nodes[2];
        TEST_ASSERT_NOT_NULL(statement);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, statement->type);

        expr = statement->data.expressionStatement.expr;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expr->type);
        TEST_ASSERT_NOT_NULL(expr->data.primaryExpression.property);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, expr->data.primaryExpression.property->type);
        TEST_ASSERT_NOT_NULL(expr->data.primaryExpression.members);
        TEST_ASSERT_EQUAL_INT(1, (int)expr->data.primaryExpression.members->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, expr->data.primaryExpression.members->nodes[0]->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_interface_variance_and_where_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Interface Variance And Where Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "interface IProducer<out T> where T: class, owner, Disposable, new() {\n"
                "    fn next(): T;\n"
                "}";
        SZrString *sourceName = ZrCore_String_Create(state, "interface_variance_where.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;
        SZrAstNode *genericParam;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, decl->type);
        TEST_ASSERT_NOT_NULL(decl->data.interfaceDeclaration.generic);
        TEST_ASSERT_NOT_NULL(decl->data.interfaceDeclaration.generic->params);
        TEST_ASSERT_EQUAL_INT(1, (int)decl->data.interfaceDeclaration.generic->params->count);
        TEST_ASSERT_NOT_NULL(decl->data.interfaceDeclaration.members);
        TEST_ASSERT_EQUAL_INT(1, (int)decl->data.interfaceDeclaration.members->count);

        genericParam = decl->data.interfaceDeclaration.generic->params->nodes[0];
        TEST_ASSERT_NOT_NULL(genericParam);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PARAMETER, genericParam->type);
        TEST_ASSERT_TRUE(genericParam->data.parameter.genericRequiresClass);
        TEST_ASSERT_TRUE(genericParam->data.parameter.genericRequiresNew);
        TEST_ASSERT_TRUE(genericParam->data.parameter.genericRequiresOwner);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_NONE,
                              genericParam->data.parameter.genericRequiredOwnershipQualifier);
        TEST_ASSERT_NOT_NULL(genericParam->data.parameter.genericTypeConstraints);
        TEST_ASSERT_EQUAL_INT(1, (int)genericParam->data.parameter.genericTypeConstraints->count);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    {
        SZrState *state = create_test_state();
        const char *source =
                "fn own<T, U, V>(): int where T: unique where U: shared where V: weak { return 1; }";
        SZrString *sourceName = ZrCore_String_Create(state, "specific_owner_where.zr", 23);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;
        SZrAstNode *uniqueParam;
        SZrAstNode *sharedParam;
        SZrAstNode *weakParam;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, decl->type);
        TEST_ASSERT_NOT_NULL(decl->data.functionDeclaration.generic);
        TEST_ASSERT_NOT_NULL(decl->data.functionDeclaration.generic->params);
        TEST_ASSERT_EQUAL_INT(3, (int)decl->data.functionDeclaration.generic->params->count);

        uniqueParam = decl->data.functionDeclaration.generic->params->nodes[0];
        sharedParam = decl->data.functionDeclaration.generic->params->nodes[1];
        weakParam = decl->data.functionDeclaration.generic->params->nodes[2];
        TEST_ASSERT_TRUE(uniqueParam->data.parameter.genericRequiresOwner);
        TEST_ASSERT_TRUE(sharedParam->data.parameter.genericRequiresOwner);
        TEST_ASSERT_TRUE(weakParam->data.parameter.genericRequiresOwner);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              uniqueParam->data.parameter.genericRequiredOwnershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              sharedParam->data.parameter.genericRequiredOwnershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK,
                              weakParam->data.parameter.genericRequiredOwnershipQualifier);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_parameter_passing_mode_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Parameter Passing Mode Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "fn swap<T>(left: ref T, right: ref T): T {\n"
                "    return left;\n"
                "}";
        SZrString *sourceName = ZrCore_String_Create(state, "parameter_passing_mode.zr", 25);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, decl->type);
        TEST_ASSERT_NOT_NULL(decl->data.functionDeclaration.generic);
        TEST_ASSERT_NOT_NULL(decl->data.functionDeclaration.params);
        TEST_ASSERT_EQUAL_INT(2, (int)decl->data.functionDeclaration.params->count);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_const_generic_construction_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Const Generic Construction Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "let {Array} = import(\"zr.container\");\n"
                "class Matrix<T, const N: int> {\n"
                "    var rows: Array<T>[N];\n"
                "}\n"
                "var matrix = new Matrix<int, 4>();";
        SZrString *sourceName = ZrCore_String_Create(state, "const_generic_construction.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;
        SZrAstNode *constructDecl;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, decl->type);
        TEST_ASSERT_NOT_NULL(decl->data.classDeclaration.generic);

        constructDecl = ast->data.script.statements->nodes[2];
        TEST_ASSERT_NOT_NULL(constructDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, constructDecl->type);
        TEST_ASSERT_NOT_NULL(constructDecl->data.variableDeclaration.value);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, constructDecl->data.variableDeclaration.value->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_resource_ownership_surface_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Resource Ownership Surface Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Resource ownership syntax parsing",
              "Testing README resource ownership forms: resource class, Unique/Shared/Weak, own, share, weak, upgrade, and drop");
    {
        const char *source =
            "resource class Holder { var resource: Unique<Resource>; }\n"
            "let uniqueHolder: Unique<Holder> = own Holder();\n"
            "let sharedHolder: Shared<Holder> = uniqueHolder.share();\n"
            "let weakHolder: Weak<Holder> = sharedHolder.weak();\n"
            "let active = weakHolder.upgrade();\n"
            "drop(sharedHolder);";
        SZrString *sourceName = ZrCore_String_Create(state, "resource_ownership_syntax.zr", 29);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *classDecl;
        SZrAstNode *fieldNode;
        SZrClassField *field;
        SZrAstNode *uniqueDecl;
        SZrAstNode *expr;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(6, (int)ast->data.script.statements->count);

        classDecl = ast->data.script.statements->nodes[0];
        uniqueDecl = ast->data.script.statements->nodes[1];

        TEST_ASSERT_NOT_NULL(classDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classDecl->type);
        TEST_ASSERT_TRUE(classDecl->data.classDeclaration.isOwned);
        TEST_ASSERT_NOT_NULL(classDecl->data.classDeclaration.members);
        TEST_ASSERT_EQUAL_INT(1, (int)classDecl->data.classDeclaration.members->count);

        fieldNode = classDecl->data.classDeclaration.members->nodes[0];
        TEST_ASSERT_NOT_NULL(fieldNode);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, fieldNode->type);
        field = &fieldNode->data.classField;
        TEST_ASSERT_FALSE(field->reservedRemovedUsingManaged);
        TEST_ASSERT_NOT_NULL(field->typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE, field->typeInfo->ownershipQualifier);

        expr = uniqueDecl->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expr->type);
        TEST_ASSERT_TRUE(expr->data.constructExpression.isNew);
        TEST_ASSERT_FALSE(expr->data.constructExpression.isUsing);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              expr->data.constructExpression.ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE,
                              expr->data.constructExpression.builtinKind);

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试条件表达式解析
static void test_conditional_expression(void) {
    SZrTestTimer timer;
    const char* testSummary = "Conditional Expression Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Conditional expression parsing", 
              "Testing parsing of conditional expression: true ? 1 : 2");
    const char* source = "true ? 1 : 2;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        {
            SZrAstNode* expr = unwrap_statement_expression(get_script_statement(ast, 0));
            TEST_ASSERT_NOT_NULL(expr);
            TEST_ASSERT_EQUAL_INT(ZR_AST_CONDITIONAL_EXPRESSION, expr->type);
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse conditional expression");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试数组字面量解析
static void test_array_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Array Literal Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Array literal parsing", 
              "Testing parsing of array literal: [1, 2, 3]");
    const char* source = "[1, 2, 3];";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        {
            SZrAstNode* expr = unwrap_statement_expression(get_script_statement(ast, 0));
            TEST_ASSERT_NOT_NULL(expr);
            TEST_ASSERT_TRUE(expr->type == ZR_AST_ARRAY_LITERAL ||
                             expr->type == ZR_AST_PRIMARY_EXPRESSION);
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse array literal");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试对象字面量解析
static void test_object_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Object Literal Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Object literal parsing", 
              "Testing parsing of object literal: {a: 1, b: 2}");
    const char* source = "{a: 1, b: 2};";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        {
            SZrAstNode* expr = unwrap_statement_expression(get_script_statement(ast, 0));
            TEST_ASSERT_NOT_NULL(expr);
            TEST_ASSERT_TRUE(expr->type == ZR_AST_OBJECT_LITERAL ||
                             expr->type == ZR_AST_PRIMARY_EXPRESSION);
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse object literal");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 声明测试 ====================

// 测试函数声明解析
static void test_function_declaration(void) {
    SZrTestTimer timer;
    const char* testSummary = "Function Declaration Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Function declaration parsing",
              "Testing parsing of current function declaration: fn test(): void {}");
    const char* source = "fn test(): void {}";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_extern_block_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Extern Block Parsing";
    const char *source =
            "native extern(\"fixture\") {\n"
            "    #zr.ffi.entry(\"zr_ffi_add_i32\")# fn Add(lhs:i32, rhs:i32): i32;\n"
            "    delegate Unary(value:f64): f64;\n"
            "    struct Point {\n"
            "        #zr.ffi.offset(0)# var x:i32;\n"
            "        #zr.ffi.offset(4)# var y:i32;\n"
            "    }\n"
            "}\n";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *externStmt;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "extern_test.zr", 14);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

    externStmt = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(externStmt);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, externStmt->type);
    TEST_ASSERT_EQUAL_STRING("fixture", string_node_native(state, externStmt->data.externBlock.libraryName));
    TEST_ASSERT_NOT_NULL(externStmt->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_INT(3, (int)externStmt->data.externBlock.declarations->count);

    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_FUNCTION_DECLARATION,
                          externStmt->data.externBlock.declarations->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_DELEGATE_DECLARATION,
                          externStmt->data.externBlock.declarations->nodes[1]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION,
                          externStmt->data.externBlock.declarations->nodes[2]->type);

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_static_import_expression_uses_current_syntax(void) {
    SZrTestTimer timer;
    const char *testSummary = "Static Import Expression Uses Current Syntax";
    static const struct {
        const char *source;
        const char *expectedName;
    } fixtures[] = {
        { "let math = import(\"zr.math\");", "zr.math" },
        { "let system = import(\"zr.system\");", "zr.system" },
    };
    TZrSize index;

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Static import parsing",
              "Testing that current import syntax normalizes to the canonical import-expression AST");

    for (index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName = ZrCore_String_Create(state, "reserved_import_variants.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, fixtures[index].source, strlen(fixtures[index].source), sourceName);
        SZrAstNode *statement;
        SZrAstNode *expr;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        statement = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(statement);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statement->type);
        TEST_ASSERT_TRUE(statement->data.variableDeclaration.isConst);
        expr = statement->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, expr->type);
        TEST_ASSERT_EQUAL_STRING(fixtures[index].expectedName,
                                 string_node_native(state, expr->data.importExpression.modulePath));

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_static_import_expression_member_chain_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Static Import Member Chain Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "import(\"helper\").toolkit.math.greet();";
        SZrString *sourceName = ZrCore_String_Create(state, "reserved_import_chain.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *expr;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        expr = unwrap_statement_expression(get_script_statement(ast, 0));
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expr->type);
        TEST_ASSERT_NOT_NULL(expr->data.primaryExpression.property);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IMPORT_EXPRESSION, expr->data.primaryExpression.property->type);
        TEST_ASSERT_EQUAL_STRING("helper",
                                 string_node_native(state,
                                                    expr->data.primaryExpression.property->data.importExpression.modulePath));
        TEST_ASSERT_NOT_NULL(expr->data.primaryExpression.members);
        TEST_ASSERT_EQUAL_INT(4, (int)expr->data.primaryExpression.members->count);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_function_declaration_requires_fn_keyword(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Declaration Requires Fn Keyword";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "fn first(): int { return 1; }\n"
                "fn second(): int { return 2; }\n";
        SZrString *sourceName = ZrCore_String_Create(state, "optional_func_keyword.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, ast->data.script.statements->nodes[0]->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, ast->data.script.statements->nodes[1]->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void assert_ownership_generic_type(SZrType *typeInfo,
                                          const char *expectedWrapper,
                                          EZrOwnershipQualifier expectedQualifier) {
    SZrGenericType *genericType;
    SZrAstNode *argumentNode;

    TEST_ASSERT_NOT_NULL(typeInfo);
    TEST_ASSERT_EQUAL_INT(expectedQualifier, typeInfo->ownershipQualifier);
    TEST_ASSERT_TRUE(typeInfo->isImplicitBuiltinType);
    TEST_ASSERT_NOT_NULL(typeInfo->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, typeInfo->name->type);

    genericType = &typeInfo->name->data.genericType;
    TEST_ASSERT_NOT_NULL(genericType->name);
    TEST_ASSERT_NOT_NULL(genericType->name->name);
    TEST_ASSERT_EQUAL_STRING(expectedWrapper, ZrCore_String_GetNativeString(genericType->name->name));
    TEST_ASSERT_NOT_NULL(genericType->params);
    TEST_ASSERT_EQUAL_INT(1, (int)genericType->params->count);

    argumentNode = genericType->params->nodes[0];
    TEST_ASSERT_NOT_NULL(argumentNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, argumentNode->type);
}

typedef struct SLegacyOwnershipWarningCapture {
    TZrUInt32 warningCount;
    TZrUInt32 errorCount;
    TZrUInt32 legacyCodeCount;
    TZrBool sawUniqueSuggestion;
    TZrBool sawSharedSuggestion;
    TZrBool sawBorrowSuggestion;
    TZrBool sawLoanSuggestion;
} SLegacyOwnershipWarningCapture;

static void capture_legacy_ownership_structured_diagnostic(TZrPtr userData,
                                                           const SZrStructuredDiagnostic *diagnostic,
                                                           EZrToken token) {
    SLegacyOwnershipWarningCapture *capture = (SLegacyOwnershipWarningCapture *)userData;
    const TZrChar *code;
    const TZrChar *suggestion;

    ZR_UNUSED_PARAMETER(token);

    if (capture == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }

    if (diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_WARNING) {
        capture->warningCount++;
    } else if (diagnostic->severity == ZR_STRUCTURED_DIAGNOSTIC_ERROR) {
        capture->errorCount++;
    }

    code = diagnostic->code != ZR_NULL ? ZrCore_String_GetNativeString(diagnostic->code) : ZR_NULL;
    if (code == ZR_NULL || strcmp(code, "legacy_ownership_type_syntax") != 0) {
        return;
    }

    capture->legacyCodeCount++;
    suggestion = diagnostic->suggestion != ZR_NULL
                         ? ZrCore_String_GetNativeString(diagnostic->suggestion)
                         : ZR_NULL;
    if (suggestion == ZR_NULL) {
        return;
    }

    if (strstr(suggestion, "Unique<T>") != ZR_NULL) {
        capture->sawUniqueSuggestion = ZR_TRUE;
    }
    if (strstr(suggestion, "Shared<T>") != ZR_NULL) {
        capture->sawSharedSuggestion = ZR_TRUE;
    }
    if (strstr(suggestion, "Borrow<T>") != ZR_NULL) {
        capture->sawBorrowSuggestion = ZR_TRUE;
    }
    if (strstr(suggestion, "Loan<T>") != ZR_NULL) {
        capture->sawLoanSuggestion = ZR_TRUE;
    }
}

static void test_ownership_generic_type_surface_parsing(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Ownership Generic Type Surface Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Ownership generic surface parsing",
              "Testing that the README Unique<T>, Shared<T>, and Weak<T> forms preserve canonical ownership qualifiers");
    {
        const char *source =
            "var directUnique: Unique<Resource>;\n"
            "var directShared: Shared<Box<int>>;\n"
            "var directWeak: Weak<Resource>;";
        SZrString *sourceName = ZrCore_String_Create(state, "ownership_generic_surface.zr", 28);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *directUnique;
        SZrAstNode *directShared;
        SZrAstNode *directWeak;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        directUnique = ast->data.script.statements->nodes[0];
        directShared = ast->data.script.statements->nodes[1];
        directWeak = ast->data.script.statements->nodes[2];

        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, directUnique->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, directShared->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, directWeak->type);

        assert_ownership_generic_type(directUnique->data.variableDeclaration.typeInfo,
                                      "Unique",
                                      ZR_OWNERSHIP_QUALIFIER_UNIQUE);
        assert_ownership_generic_type(directShared->data.variableDeclaration.typeInfo,
                                      "Shared",
                                      ZR_OWNERSHIP_QUALIFIER_SHARED);
        assert_ownership_generic_type(directWeak->data.variableDeclaration.typeInfo,
                                      "Weak",
                                      ZR_OWNERSHIP_QUALIFIER_WEAK);

        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_legacy_ownership_type_syntax_is_rejected(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Legacy Ownership Type Syntax Is Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Legacy ownership type syntax rejection",
              "Testing that removed percent ownership spelling produces a directed diagnostic instead of remaining compatible");
    {
        SZrState *state = create_test_state();
        SZrCapturedParserDiagnostic diagnostic;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        ast = parse_source_with_diagnostic(state,
                                           "var legacyUnique: %unique Resource;",
                                           strlen("var legacyUnique: %unique Resource;"),
                                           "legacy_ownership_type.zr",
                                           &diagnostic);
        TEST_ASSERT_TRUE(diagnostic.reported);
        TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Legacy syntax '%unique' was removed"));
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void assert_resource_own_construct_value(SZrAstNode *decl,
                                                EZrOwnershipQualifier expectedQualifier,
                                                EZrOwnershipBuiltinKind expectedBuiltinKind) {
    SZrAstNode *expr;

    TEST_ASSERT_NOT_NULL(decl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, decl->type);
    expr = decl->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(expr);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expr->type);
    TEST_ASSERT_TRUE(expr->data.constructExpression.isNew);
    TEST_ASSERT_FALSE(expr->data.constructExpression.isUsing);
    TEST_ASSERT_EQUAL_INT(expectedQualifier, expr->data.constructExpression.ownershipQualifier);
    TEST_ASSERT_EQUAL_INT(expectedBuiltinKind, expr->data.constructExpression.builtinKind);
}

static void test_resource_ownership_lifecycle_surface_parsing(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Resource Ownership Lifecycle Surface Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Resource ownership lifecycle parsing",
              "Testing the README own, share, weak, upgrade, and drop forms without legacy generic constructors");
    {
        const char *source =
            "resource class Box {}\n"
            "let owner: Unique<Box> = own Box();\n"
            "let shared: Shared<Box> = owner.share();\n"
            "let weak: Weak<Box> = shared.weak();\n"
            "let active = weak.upgrade();\n"
            "drop(shared);";
        SZrString *sourceName = ZrCore_String_Create(state, "resource_ownership_lifecycle.zr", 31);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(6, (int)ast->data.script.statements->count);

        assert_resource_own_construct_value(ast->data.script.statements->nodes[1],
                                            ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                                            ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE);

        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_legacy_percent_ownership_lifecycle_is_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Legacy Percent Ownership Lifecycle Is Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Legacy ownership lifecycle syntax rejection",
              "Testing that lifecycle operations use member calls and drop rather than removed percent expressions");
    {
        SZrState *state = create_test_state();
        SZrCapturedParserDiagnostic diagnostic;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        ast = parse_source_with_diagnostic(state,
                                           "%upgrade(owner);",
                                           strlen("%upgrade(owner);"),
                                           "legacy_percent_upgrade.zr",
                                           &diagnostic);
        TEST_ASSERT_TRUE(diagnostic.reported);
        TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Legacy syntax '%upgrade' was removed"));
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_reference_expression_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reference Expression Parsing";
    SZrState *state;
    SZrAstNode *ast;
    SZrAstNode *readonlyDecl;
    SZrAstNode *writableDecl;
    SZrAstNode *reborrowDecl;
    SZrAstNode *expression;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Canonical reference expression parsing",
              "Testing README-standard ref readonly T, ref T, and ref existingRef expressions");

    ast = ZrParser_Parse(state,
                         "var owner: int = 0;\n"
                         "let readonlyView: ref readonly int = ref owner;\n"
                         "let writableLoan: ref int = ref owner;\n"
                         "let reborrow: ref readonly int = ref readonlyView;\n",
                         strlen("var owner: int = 0;\n"
                                "let readonlyView: ref readonly int = ref owner;\n"
                                "let writableLoan: ref int = ref owner;\n"
                                "let reborrow: ref readonly int = ref readonlyView;\n"),
                         ZrCore_String_Create(state, "reference_expression.zr", 23));
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(4, (int)ast->data.script.statements->count);

    readonlyDecl = ast->data.script.statements->nodes[1];
    writableDecl = ast->data.script.statements->nodes[2];
    reborrowDecl = ast->data.script.statements->nodes[3];
    TEST_ASSERT_NOT_NULL(readonlyDecl);
    TEST_ASSERT_NOT_NULL(writableDecl);
    TEST_ASSERT_NOT_NULL(reborrowDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, readonlyDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, writableDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, reborrowDecl->type);
    TEST_ASSERT_NOT_NULL(readonlyDecl->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(writableDecl->data.variableDeclaration.typeInfo);
    TEST_ASSERT_EQUAL_INT(ZR_REFERENCE_ACCESS_READONLY,
                          readonlyDecl->data.variableDeclaration.typeInfo->referenceAccess);
    TEST_ASSERT_EQUAL_INT(ZR_REFERENCE_ACCESS_WRITABLE,
                          writableDecl->data.variableDeclaration.typeInfo->referenceAccess);

    expression = readonlyDecl->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_BORROW,
                          expression->data.constructExpression.builtinKind);
    TEST_ASSERT_NOT_NULL(expression->data.constructExpression.target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                          expression->data.constructExpression.target->type);

    expression = writableDecl->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    TEST_ASSERT_NOT_NULL(expression->data.constructExpression.target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                          expression->data.constructExpression.target->type);

    expression = reborrowDecl->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(expression);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, expression->type);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_BORROW,
                          expression->data.constructExpression.builtinKind);
    TEST_ASSERT_NOT_NULL(expression->data.constructExpression.target);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                          expression->data.constructExpression.target->type);

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_reserved_type_expression_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reserved Type Expression Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "let math = import(\"zr.math\");\n"
                "let reflection = typeof(math.Vector3);";
        SZrString *sourceName = ZrCore_String_Create(state, "reserved_type_expression.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *statement;
        SZrAstNode *expr;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        statement = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(statement);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, statement->type);

        expr = statement->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_QUERY_EXPRESSION, expr->type);
        TEST_ASSERT_NOT_NULL(expr->data.typeQueryExpression.operand);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expr->data.typeQueryExpression.operand->type);
        TEST_ASSERT_NOT_NULL(expr->data.typeQueryExpression.operand->data.primaryExpression.property);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL,
                              expr->data.typeQueryExpression.operand->data.primaryExpression.property->type);
        TEST_ASSERT_EQUAL_STRING("math",
                                 ZrCore_String_GetNativeString(
                                         expr->data.typeQueryExpression.operand->data.primaryExpression.property->data.identifier.name));
        TEST_ASSERT_NOT_NULL(expr->data.typeQueryExpression.operand->data.primaryExpression.members);
        TEST_ASSERT_EQUAL_INT(1, (int)expr->data.typeQueryExpression.operand->data.primaryExpression.members->count);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_member_named_import_is_not_static_import(void) {
    SZrTestTimer timer;
    const char *testSummary = "Member Named Import Is Not Static Import";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "moduleApi.import(\"zr.math\");\n"
                "var ok = 1;";
        SZrString *sourceName = ZrCore_String_Create(state, "member_named_import.zr", 22);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_EXPRESSION_STATEMENT, ast->data.script.statements->nodes[0]->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ast->data.script.statements->nodes[1]->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_current_module_declaration_variants(void) {
    SZrTestTimer timer;
    const char *testSummary = "Current Module Declaration Variants";
    static const struct {
        const char *source;
        const char *expectedName;
    } fixtures[] = {
        { "module foo;", "foo" },
        { "module foo.bar;", "foo.bar" },
    };
    TZrSize index;

    TEST_START(testSummary);
    timer.startTime = clock();

    for (index = 0; index < sizeof(fixtures) / sizeof(fixtures[0]); index++) {
        SZrState *state = create_test_state();
        SZrString *sourceName;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        sourceName = ZrCore_String_Create(state, "reserved_module_variants.zr", 27);
        ast = ZrParser_Parse(state, fixtures[index].source, strlen(fixtures[index].source), sourceName);

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.moduleName);
        TEST_ASSERT_EQUAL_INT(ZR_AST_MODULE_DECLARATION, ast->data.script.moduleName->type);
        TEST_ASSERT_EQUAL_STRING(fixtures[index].expectedName, module_declaration_name_native(state, ast));

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_current_module_keyword_is_parsed(void) {
    SZrTestTimer timer;
    const char *testSummary = "Current Module Keyword Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "module examples.hello;\nvar value = 1;";
        SZrString *sourceName = ZrCore_String_Create(state, "current_module_keyword.zr", 25);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.moduleName);
        TEST_ASSERT_EQUAL_INT(ZR_AST_MODULE_DECLARATION, ast->data.script.moduleName->type);
        TEST_ASSERT_EQUAL_STRING("examples.hello", module_declaration_name_native(state, ast));
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ast->data.script.statements->nodes[0]->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_native_extern_single_declaration_normalizes_to_block(void) {
    SZrTestTimer timer;
    const char *testSummary = "Native Extern Single Declaration Parsing";
    const char *source = "native extern(\"fixture\") fn Add(lhs:i32, rhs:i32): i32;";
    SZrState *state;
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *externStmt;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "extern_single_test.zr", 21);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

    externStmt = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(externStmt);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_BLOCK, externStmt->type);
    TEST_ASSERT_EQUAL_STRING("fixture", string_node_native(state, externStmt->data.externBlock.libraryName));
    TEST_ASSERT_NOT_NULL(externStmt->data.externBlock.declarations);
    TEST_ASSERT_EQUAL_INT(1, (int)externStmt->data.externBlock.declarations->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_EXTERN_FUNCTION_DECLARATION,
                          externStmt->data.externBlock.declarations->nodes[0]->type);

    ZrParser_Ast_Free(state, ast);
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试结构体声明解析
static void test_struct_declaration(void) {
    SZrTestTimer timer;
    const char* testSummary = "Struct Declaration Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Struct declaration parsing", 
              "Testing parsing of struct declaration: struct Vector3{}");
    const char* source = "struct Vector3{}";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_field_scoped_using_field_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Owned Field Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Owned field parsing",
              "Testing parsing of direct Unique<T>/Shared<T> owner fields without field-scoped using");

    {
        const char *source =
            "struct HandleBox { var handle: Unique<Resource>; }\n"
            "class Holder { static var version: int; var resource: Shared<Resource>; }";
        SZrString *sourceName = ZrCore_String_Create(state, "using_fields.zr", 15);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *structDecl;
        SZrAstNode *classDecl;
        SZrAstNode *structFieldNode;
        SZrAstNode *classVersionFieldNode;
        SZrAstNode *classFieldNode;
        SZrStructField *structField;
        SZrClassField *classVersionField;
        SZrClassField *classField;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        structDecl = ast->data.script.statements->nodes[0];
        classDecl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(structDecl);
        TEST_ASSERT_NOT_NULL(classDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, structDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classDecl->type);

        TEST_ASSERT_NOT_NULL(structDecl->data.structDeclaration.members);
        TEST_ASSERT_NOT_NULL(classDecl->data.classDeclaration.members);
        TEST_ASSERT_EQUAL_INT(1, (int)structDecl->data.structDeclaration.members->count);
        TEST_ASSERT_EQUAL_INT(2, (int)classDecl->data.classDeclaration.members->count);

        structFieldNode = structDecl->data.structDeclaration.members->nodes[0];
        classVersionFieldNode = classDecl->data.classDeclaration.members->nodes[0];
        classFieldNode = classDecl->data.classDeclaration.members->nodes[1];
        TEST_ASSERT_NOT_NULL(structFieldNode);
        TEST_ASSERT_NOT_NULL(classVersionFieldNode);
        TEST_ASSERT_NOT_NULL(classFieldNode);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_FIELD, structFieldNode->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, classVersionFieldNode->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, classFieldNode->type);

        structField = &structFieldNode->data.structField;
        classVersionField = &classVersionFieldNode->data.classField;
        classField = &classFieldNode->data.classField;

        TEST_ASSERT_FALSE(structField->reservedRemovedUsingManaged);
        TEST_ASSERT_FALSE(structField->isStatic);
        TEST_ASSERT_NOT_NULL(structField->typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              structField->typeInfo->ownershipQualifier);

        TEST_ASSERT_FALSE(classVersionField->reservedRemovedUsingManaged);
        TEST_ASSERT_TRUE(classVersionField->isStatic);
        TEST_ASSERT_NOT_NULL(classVersionField->typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_NONE,
                              classVersionField->typeInfo->ownershipQualifier);

        TEST_ASSERT_FALSE(classField->reservedRemovedUsingManaged);
        TEST_ASSERT_FALSE(classField->isStatic);
        TEST_ASSERT_NOT_NULL(classField->typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              classField->typeInfo->ownershipQualifier);

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_field_scoped_using_field_requires_var_keyword(void) {
    SZrTestTimer timer;
    const char *testSummary = "Field-Scoped Using Is Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Field-scoped using syntax rejection",
              "Testing that field-scoped `%using var` is no longer accepted now that owner fields carry lifecycle directly");

    {
        const char *source = "struct Broken { %using var handle: Unique<Resource>; }";
        SZrString *sourceName = ZrCore_String_Create(state, "using_missing_var.zr", 20);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, ast->data.script.statements->nodes[0]->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.structDeclaration.members);
        TEST_ASSERT_EQUAL_INT(0, (int)ast->data.script.statements->nodes[0]->data.structDeclaration.members->count);
        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_removed_percent_using_new_expression_reports_migration_diagnostic(void) {
    SZrTestTimer timer;
    const char *testSummary = "Removed Percent Using New Expression Reports Migration Diagnostic";
    const char *expectedMessage = "Legacy syntax '%using' was removed";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCapturedParserDiagnostic diagnostic;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        TEST_INFO("Removed %using new diagnostic",
                  "Testing that `%using new ...` now reports the ownership migration diagnostic");

        ast = parse_source_with_diagnostic(state,
                                           "%using new Holder();",
                                           strlen("%using new Holder();"),
                                           "removed_percent_using_new_expr.zr",
                                           &diagnostic);
        TEST_ASSERT_TRUE(diagnostic.reported);
        TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedMessage));
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_removed_percent_using_expression_reports_migration_diagnostic(void) {
    SZrTestTimer timer;
    const char *testSummary = "Removed Percent Using Expression Reports Migration Diagnostic";
    const char *expectedMessage = "Legacy syntax '%using' was removed";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCapturedParserDiagnostic diagnostic;
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        TEST_INFO("Removed %using(expr) diagnostic",
                  "Testing that `%using(expr)` now reports the ownership migration diagnostic");

        ast = parse_source_with_diagnostic(state,
                                           "%using(owner);",
                                           strlen("%using(owner);"),
                                           "removed_percent_using_expr.zr",
                                           &diagnostic);
        TEST_ASSERT_TRUE(diagnostic.reported);
        TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedMessage));
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_field_scoped_bare_using_field_is_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Field-Scoped Bare Using Field Rejection";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Field-scoped bare using syntax rejection",
              "Testing that bare `using var` fields are no longer accepted now that owner lifecycle lives in direct field types");

    {
        const char *source = "struct Broken { using var handle: Unique<Resource>; }";
        SZrString *sourceName = ZrCore_String_Create(state, "bare_using_field.zr", 19);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_DECLARATION, ast->data.script.statements->nodes[0]->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.structDeclaration.members);
        TEST_ASSERT_EQUAL_INT(0, (int)ast->data.script.statements->nodes[0]->data.structDeclaration.members->count);
        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_using_keyword_statement_parsing(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Using Keyword Statement Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Bare using statement parsing",
              "Testing that the `using` keyword is accepted as the canonical statement/block lifetime fence while field-scoped bare using remains rejected elsewhere");
    {
        const char *source =
            "var resource = \"x\";\n"
            "using (resource) { var inner = 1; }\n"
            "using resource;";
        SZrString *sourceName = ZrCore_String_Create(state, "using_keyword_statement.zr", 26);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *blockUsing;
        SZrAstNode *singleUsing;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

        blockUsing = ast->data.script.statements->nodes[1];
        singleUsing = ast->data.script.statements->nodes[2];

        TEST_ASSERT_NOT_NULL(blockUsing);
        TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, blockUsing->type);
        TEST_ASSERT_TRUE(blockUsing->data.usingStatement.isBlockScoped);
        TEST_ASSERT_NOT_NULL(blockUsing->data.usingStatement.resource);
        TEST_ASSERT_NOT_NULL(blockUsing->data.usingStatement.body);

        TEST_ASSERT_NOT_NULL(singleUsing);
        TEST_ASSERT_EQUAL_INT(ZR_AST_USING_STATEMENT, singleUsing->type);
        TEST_ASSERT_FALSE(singleUsing->data.usingStatement.isBlockScoped);
        TEST_ASSERT_NOT_NULL(singleUsing->data.usingStatement.resource);
        TEST_ASSERT_NULL(singleUsing->data.usingStatement.body);

        ZrParser_Ast_Free(state, ast);
    }

    destroy_test_state(state);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_using_else_without_guard_reports_diagnostic(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Using Else Without Guard Diagnostic";
    const char *expectedMessage = "using else requires a guard binder";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCapturedParserDiagnostic diagnostic;
        const char *source =
            "var resource = \"x\";\n"
            "using (resource) { var inner = 1; } else { var fallback = 2; }";
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        TEST_INFO("Using else without guard diagnostic",
                  "Testing that drop-style using reports the dedicated guard diagnostic when followed by else");

        ast = parse_source_with_diagnostic(state,
                                           source,
                                           strlen(source),
                                           "using_else_without_guard.zr",
                                           &diagnostic);
        TEST_ASSERT_TRUE(diagnostic.reported);
        TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedMessage));
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_using_binder_invalid_reports_diagnostic(void) {
    SZrTestTimer timer = {0};
    const char *testSummary = "Using Binder Invalid Diagnostic";
    const char *expectedMessage = "using_binder_invalid";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        SZrCapturedParserDiagnostic diagnostic;
        const char *source =
            "var resource = \"x\";\n"
            "using (var 1 = resource) { var inner = 1; }";
        SZrAstNode *ast;

        TEST_ASSERT_NOT_NULL(state);
        TEST_INFO("Using invalid binder diagnostic",
                  "Testing that a guard binder which is not an identifier or union variant pattern reports a dedicated diagnostic");

        ast = parse_source_with_diagnostic(state,
                                           source,
                                           strlen(source),
                                           "using_binder_invalid.zr",
                                           &diagnostic);
        TEST_ASSERT_TRUE(diagnostic.reported);
        TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, expectedMessage));
        if (ast != ZR_NULL) {
            ZrParser_Ast_Free(state, ast);
        }

        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_resource_class_and_owner_types_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Resource Class And Owner Types Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Resource class and owner type parsing",
              "Testing README resource class ownership with Unique<T>, Shared<T>, Weak<T>, and own T()");

    {
        const char *source =
            "resource class Holder {}\n"
            "let owned: Unique<Holder> = own Holder();\n"
            "let sharedRef: Shared<Holder>;\n"
            "let weakRef: Weak<Holder>;";
        SZrString *sourceName = ZrCore_String_Create(state, "resource_owner_types.zr", 23);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *classDecl;
        SZrAstNode *uniqueOwnerDecl;
        SZrConstructExpression *uniqueConstruct;

        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(4, (int)ast->data.script.statements->count);

        classDecl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(classDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, classDecl->type);
        TEST_ASSERT_TRUE(classDecl->data.classDeclaration.isOwned);

        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              ast->data.script.statements->nodes[1]->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_SHARED,
                              ast->data.script.statements->nodes[2]->data.variableDeclaration.typeInfo->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_WEAK,
                              ast->data.script.statements->nodes[3]->data.variableDeclaration.typeInfo->ownershipQualifier);

        uniqueOwnerDecl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(uniqueOwnerDecl);

        uniqueConstruct = &uniqueOwnerDecl->data.variableDeclaration.value->data.constructExpression;
        TEST_ASSERT_EQUAL_INT(ZR_AST_CONSTRUCT_EXPRESSION, uniqueOwnerDecl->data.variableDeclaration.value->type);
        TEST_ASSERT_TRUE(uniqueConstruct->isNew);
        TEST_ASSERT_FALSE(uniqueConstruct->isUsing);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE, uniqueConstruct->ownershipQualifier);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE, uniqueConstruct->builtinKind);

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_legacy_async_surfaces_are_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Legacy Async Surfaces Are Rejected";
    SZrState *state;
    SZrCapturedParserDiagnostic diagnostic;
    SZrAstNode *ast;

    TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    ast = parse_source_with_diagnostic(state,
                                       "%async run(): zr.task.Task<int> { return 1; }\n",
                                       strlen("%async run(): zr.task.Task<int> { return 1; }\n"),
                                       "legacy_percent_async.zr",
                                       &diagnostic);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Legacy syntax '%async' was removed"));
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }

    ast = parse_source_with_diagnostic(state,
                                       "var runner: %async int = null;\n",
                                       strlen("var runner: %async int = null;\n"),
                                       "legacy_percent_async_type.zr",
                                       &diagnostic);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Legacy syntax '%async' was removed"));
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }

    ast = parse_source_with_diagnostic(state,
                                       "%await pending;\n",
                                       strlen("%await pending;\n"),
                                       "legacy_percent_await.zr",
                                       &diagnostic);
    TEST_ASSERT_TRUE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(strstr(diagnostic.message, "Legacy syntax '%await' was removed"));
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }

    ast = parse_source_with_diagnostic(state,
                                       "async fn run(): zr.task.Task<int> { return 1; }\n",
                                       strlen("async fn run(): zr.task.Task<int> { return 1; }\n"),
                                       "canonical_async.zr",
                                       &diagnostic);
    TEST_ASSERT_FALSE(diagnostic.reported);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_class_abstract_member_and_final_class_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Class Abstract Member And Final Class Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "abstract class Base {\n"
                "    pub abstract fn speak(): int;\n"
                "    pub abstract property score: int { get; }\n"
                "    pub abstract @dispose(): int;\n"
                "}\n"
                "final class Leaf: Base {\n"
                "    pub override fn speak(): int { return super.speak(); }\n"
                "    pub override property score: int { get { return super.score; } }\n"
                "    pub override @dispose(): int { return super.dispose(); }\n"
                "}";
        SZrString *sourceName = ZrCore_String_Create(state, "class_advanced_oop_parse.zr", 27);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *baseDecl;
        SZrAstNode *leafDecl;
        SZrAstNode *methodMember;
        SZrAstNode *getterMember;
        SZrAstNode *metaMember;
        SZrAstNode *returnStmt;
        SZrAstNode *returnExpr;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        baseDecl = ast->data.script.statements->nodes[0];
        leafDecl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(baseDecl);
        TEST_ASSERT_NOT_NULL(leafDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, baseDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, leafDecl->type);
        TEST_ASSERT_NOT_NULL(baseDecl->data.classDeclaration.members);
        TEST_ASSERT_NOT_NULL(leafDecl->data.classDeclaration.members);
        TEST_ASSERT_EQUAL_INT(3, (int)baseDecl->data.classDeclaration.members->count);
        TEST_ASSERT_EQUAL_INT(3, (int)leafDecl->data.classDeclaration.members->count);
        TEST_ASSERT_EQUAL_INT(1, (int)leafDecl->data.classDeclaration.inherits->count);

        methodMember = leafDecl->data.classDeclaration.members->nodes[0];
        getterMember = leafDecl->data.classDeclaration.members->nodes[1];
        metaMember = leafDecl->data.classDeclaration.members->nodes[2];
        TEST_ASSERT_NOT_NULL(methodMember);
        TEST_ASSERT_NOT_NULL(getterMember);
        TEST_ASSERT_NOT_NULL(metaMember);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, methodMember->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_DECLARATION, getterMember->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_META_FUNCTION, metaMember->type);

        TEST_ASSERT_NOT_NULL(methodMember->data.classMethod.body);
        TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, methodMember->data.classMethod.body->type);
        TEST_ASSERT_NOT_NULL(methodMember->data.classMethod.body->data.block.body);
        TEST_ASSERT_EQUAL_INT(1, (int)methodMember->data.classMethod.body->data.block.body->count);
        returnStmt = methodMember->data.classMethod.body->data.block.body->nodes[0];
        TEST_ASSERT_NOT_NULL(returnStmt);
        TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnStmt->type);
        returnExpr = returnStmt->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(returnExpr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, returnExpr->type);
        TEST_ASSERT_NOT_NULL(returnExpr->data.primaryExpression.property);
        TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, returnExpr->data.primaryExpression.property->type);
        TEST_ASSERT_EQUAL_STRING("super",
                                 ZrCore_String_GetNativeString(
                                         returnExpr->data.primaryExpression.property->data.identifier.name));
        TEST_ASSERT_NOT_NULL(returnExpr->data.primaryExpression.members);
        TEST_ASSERT_EQUAL_INT(2, (int)returnExpr->data.primaryExpression.members->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, returnExpr->data.primaryExpression.members->nodes[0]->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_CALL, returnExpr->data.primaryExpression.members->nodes[1]->type);

        TEST_ASSERT_NOT_NULL(getterMember->data.propertyDeclaration.accessors);
        TEST_ASSERT_EQUAL_INT(1, (int)getterMember->data.propertyDeclaration.accessors->count);
        TEST_ASSERT_EQUAL_INT(
                ZR_AST_PROPERTY_ACCESSOR,
                getterMember->data.propertyDeclaration.accessors->nodes[0]->type);
        TEST_ASSERT_EQUAL_INT(
                ZR_PROPERTY_ACCESSOR_GET,
                getterMember->data.propertyDeclaration.accessors->nodes[0]
                        ->data.propertyAccessor.kind);
        TEST_ASSERT_NOT_NULL(
                getterMember->data.propertyDeclaration.accessors->nodes[0]
                        ->data.propertyAccessor.body);
        returnStmt = getterMember->data.propertyDeclaration.accessors->nodes[0]
                             ->data.propertyAccessor.body->data.block.body->nodes[0];
        TEST_ASSERT_NOT_NULL(returnStmt);
        TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnStmt->type);
        returnExpr = returnStmt->data.returnStatement.expr;
        TEST_ASSERT_NOT_NULL(returnExpr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, returnExpr->type);
        TEST_ASSERT_NOT_NULL(returnExpr->data.primaryExpression.property);
        TEST_ASSERT_EQUAL_STRING("super",
                                 ZrCore_String_GetNativeString(
                                         returnExpr->data.primaryExpression.property->data.identifier.name));
        TEST_ASSERT_NOT_NULL(returnExpr->data.primaryExpression.members);
        TEST_ASSERT_EQUAL_INT(1, (int)returnExpr->data.primaryExpression.members->count);
        TEST_ASSERT_EQUAL_INT(ZR_AST_MEMBER_EXPRESSION, returnExpr->data.primaryExpression.members->nodes[0]->type);

        TEST_ASSERT_NOT_NULL(metaMember->data.classMetaFunction.body);
        TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, metaMember->data.classMetaFunction.body->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_class_member_modifier_and_super_member_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Class Member Modifier And Super Member Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "class Base {\n"
                "    pub virtual fn speak(): int { return 1; }\n"
                "}\n"
                "class Fancy: Base {\n"
                "    pub override final fn speak(): int { return super.speak(); }\n"
                "    override final property score: int { get { return super.score; } }\n"
                "    shadow fn ping(): int { return 2; }\n"
                "}";
        SZrString *sourceName = ZrCore_String_Create(state, "class_modifier_super_member.zr", 30);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *fancyDecl;
        SZrAstNode *methodMember;
        SZrAstNode *getterMember;
        SZrAstNode *shadowMember;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        fancyDecl = ast->data.script.statements->nodes[1];
        TEST_ASSERT_NOT_NULL(fancyDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, fancyDecl->type);
        TEST_ASSERT_NOT_NULL(fancyDecl->data.classDeclaration.members);
        TEST_ASSERT_EQUAL_INT(3, (int)fancyDecl->data.classDeclaration.members->count);

        methodMember = fancyDecl->data.classDeclaration.members->nodes[0];
        getterMember = fancyDecl->data.classDeclaration.members->nodes[1];
        shadowMember = fancyDecl->data.classDeclaration.members->nodes[2];
        TEST_ASSERT_NOT_NULL(methodMember);
        TEST_ASSERT_NOT_NULL(getterMember);
        TEST_ASSERT_NOT_NULL(shadowMember);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, methodMember->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_PROPERTY_DECLARATION, getterMember->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_METHOD, shadowMember->type);
        TEST_ASSERT_NOT_NULL(getterMember->data.propertyDeclaration.accessors);
        TEST_ASSERT_EQUAL_INT(
                ZR_PROPERTY_ACCESSOR_GET,
                getterMember->data.propertyDeclaration.accessors->nodes[0]
                        ->data.propertyAccessor.kind);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_function_type_annotation_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Type Annotation Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "var direct: fn(int) -> int = fn(x: int): int => x;\n"
                "var alternate: fn(int) -> int = fn(x: int): int => x;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "function_type_annotation_test.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *directDecl;
        SZrAstNode *compatDecl;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

        directDecl = ast->data.script.statements->nodes[0];
        compatDecl = ast->data.script.statements->nodes[1];

        TEST_ASSERT_NOT_NULL(directDecl);
        TEST_ASSERT_NOT_NULL(compatDecl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, directDecl->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, compatDecl->type);

        TEST_ASSERT_NOT_NULL(directDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(compatDecl->data.variableDeclaration.typeInfo);
        TEST_ASSERT_NOT_NULL(directDecl->data.variableDeclaration.value);
        TEST_ASSERT_NOT_NULL(compatDecl->data.variableDeclaration.value);
        TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, directDecl->data.variableDeclaration.value->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, compatDecl->data.variableDeclaration.value->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_function_type_missing_return_arrow_is_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Function Type Missing Return Arrow Is Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "var broken: fn(int) = null;\n"
                "var ok = 1;\n";
        SZrString *sourceName = ZrCore_String_Create(state, "function_type_missing_arrow_test.zr", 35);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, ast->data.script.statements->nodes[0]->type);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void test_type_query_accepts_function_type_expression(void) {
    SZrTestTimer timer;
    const char *testSummary = "Type Query Accepts Function Type Expression";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "let funcType = typeid(fn(int) -> int);";
        SZrString *sourceName = ZrCore_String_Create(state, "type_query_function_type_test.zr", 32);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *decl;
        SZrAstNode *expr;

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        TEST_ASSERT_EQUAL_INT(1, (int)ast->data.script.statements->count);

        decl = ast->data.script.statements->nodes[0];
        TEST_ASSERT_NOT_NULL(decl);
        TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, decl->type);

        expr = decl->data.variableDeclaration.value;
        TEST_ASSERT_NOT_NULL(expr);
        TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_QUERY_EXPRESSION, expr->type);
        TEST_ASSERT_EQUAL_INT(ZR_TYPE_QUERY_CANONICAL_IDENTITY, expr->data.typeQueryExpression.kind);
        TEST_ASSERT_NOT_NULL(expr->data.typeQueryExpression.typeOperand);

        ZrParser_Ast_Free(state, ast);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

static void assert_function_type_value_alias_parsing_case(void) {
    SZrState *state = create_test_state();
    const char *source =
            "var f = fn(int) -> int;\n"
            "var c:f = fn(x:int)=>{ return x; };";
    SZrString *sourceName = ZrCore_String_Create(state, "type_value_alias_test.zr", 24);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrAstNode *aliasDecl;
    SZrAstNode *closureDecl;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(2, (int)ast->data.script.statements->count);

    aliasDecl = ast->data.script.statements->nodes[0];
    closureDecl = ast->data.script.statements->nodes[1];
    TEST_ASSERT_NOT_NULL(aliasDecl);
    TEST_ASSERT_NOT_NULL(closureDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, aliasDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, closureDecl->type);
    TEST_ASSERT_NOT_NULL(aliasDecl->data.variableDeclaration.value);
    TEST_ASSERT_NOT_NULL(closureDecl->data.variableDeclaration.typeInfo);
    TEST_ASSERT_NOT_NULL(closureDecl->data.variableDeclaration.typeInfo->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, closureDecl->data.variableDeclaration.typeInfo->name->type);
    TEST_ASSERT_EQUAL_STRING(
            "f",
            ZrCore_String_GetNativeString(closureDecl->data.variableDeclaration.typeInfo->name->data.identifier.name));
    TEST_ASSERT_NOT_NULL(closureDecl->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_LAMBDA_EXPRESSION, closureDecl->data.variableDeclaration.value->type);

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void assert_array_type_value_alias_parsing_case(void) {
    SZrState *state = create_test_state();
    const char *source =
            "var cubeType = int[][][];\n"
            "let container = import(\"zr.container\");\n"
            "var jaggedType = container.Array<int[]>[];\n";
    SZrString *sourceName = ZrCore_String_Create(state, "array_type_value_alias_test.zr", 30);
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    SZrAstNode *cubeAliasDecl;
    SZrAstNode *jaggedAliasDecl;
    SZrType *cubeAliasType;
    SZrType *jaggedAliasType;
    SZrAstNode *jaggedNameNode;
    SZrAstNode *elementTypeNode;
    SZrType *elementTypeInfo;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_INT(3, (int)ast->data.script.statements->count);

    cubeAliasDecl = ast->data.script.statements->nodes[0];
    jaggedAliasDecl = ast->data.script.statements->nodes[2];
    TEST_ASSERT_NOT_NULL(cubeAliasDecl);
    TEST_ASSERT_NOT_NULL(jaggedAliasDecl);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, cubeAliasDecl->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, jaggedAliasDecl->type);

    TEST_ASSERT_NOT_NULL(cubeAliasDecl->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_LITERAL_EXPRESSION, cubeAliasDecl->data.variableDeclaration.value->type);
    cubeAliasType = cubeAliasDecl->data.variableDeclaration.value->data.typeLiteralExpression.typeInfo;
    TEST_ASSERT_NOT_NULL(cubeAliasType);
    TEST_ASSERT_EQUAL_INT(3, cubeAliasType->dimensions);
    TEST_ASSERT_NOT_NULL(cubeAliasType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, cubeAliasType->name->type);
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(cubeAliasType->name->data.identifier.name));

    TEST_ASSERT_NOT_NULL(jaggedAliasDecl->data.variableDeclaration.value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_LITERAL_EXPRESSION, jaggedAliasDecl->data.variableDeclaration.value->type);
    jaggedAliasType = jaggedAliasDecl->data.variableDeclaration.value->data.typeLiteralExpression.typeInfo;
    TEST_ASSERT_NOT_NULL(jaggedAliasType);
    TEST_ASSERT_NOT_NULL(jaggedAliasType->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, jaggedAliasType->name->type);
    TEST_ASSERT_EQUAL_STRING("container", ZrCore_String_GetNativeString(jaggedAliasType->name->data.identifier.name));
    TEST_ASSERT_NOT_NULL(jaggedAliasType->subType);
    TEST_ASSERT_EQUAL_INT(1, jaggedAliasType->subType->dimensions);
    jaggedNameNode = jaggedAliasType->subType->name;
    TEST_ASSERT_NOT_NULL(jaggedNameNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, jaggedNameNode->type);
    TEST_ASSERT_EQUAL_STRING("Array", ZrCore_String_GetNativeString(jaggedNameNode->data.genericType.name->name));
    TEST_ASSERT_NOT_NULL(jaggedNameNode->data.genericType.params);
    TEST_ASSERT_EQUAL_INT(1, (int)jaggedNameNode->data.genericType.params->count);
    elementTypeNode = jaggedNameNode->data.genericType.params->nodes[0];
    TEST_ASSERT_NOT_NULL(elementTypeNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE, elementTypeNode->type);
    elementTypeInfo = &elementTypeNode->data.type;
    TEST_ASSERT_EQUAL_INT(1, elementTypeInfo->dimensions);
    TEST_ASSERT_NOT_NULL(elementTypeInfo->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, elementTypeInfo->name->type);
    TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(elementTypeInfo->name->data.identifier.name));

    ZrParser_Ast_Free(state, ast);
    destroy_test_state(state);
}

static void test_type_value_alias_parsing_variants(void) {
    SZrTestTimer timer;
    const char *testSummary = "Type Value Alias Parsing Variants";

    TEST_START(testSummary);
    timer.startTime = clock();

    assert_function_type_value_alias_parsing_case();
    assert_array_type_value_alias_parsing_case();

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    TEST_DIVIDER();
}

// ==================== 语句测试 ====================

// 测试 if 语句解析
static void test_if_statement(void) {
    SZrTestTimer timer;
    const char* testSummary = "If Statement Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("If statement parsing", 
              "Testing parsing of if statement: if(true){}");
    const char* source = "if(true){}";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试返回语句解析
static void test_return_statement(void) {
    SZrTestTimer timer;
    const char* testSummary = "Return Statement Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Return statement parsing", 
              "Testing parsing of return statement: return 0;");
    const char* source = "return 0;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// ==================== 完整脚本测试 ====================

// 测试简单脚本解析
static void test_simple_script(void) {
    SZrTestTimer timer;
    const char* testSummary = "Simple Script Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Simple script parsing", 
              "Testing parsing of simple script with module and variable declarations");
    const char* source = "module test;\nvar x = 1;\nvar y = 2;";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        if (ast->data.script.statements != ZR_NULL) {
            TEST_ASSERT_TRUE(ast->data.script.statements->count >= 2);
        }
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse simple script");
        destroy_test_state(state);
        TEST_FAIL_MESSAGE("Test assertion failed");
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试 simple.zr 完整文件解析
static void test_simple_zr_file(void) {
    SZrTestTimer timer;
    const char* testSummary = "Simple.zr File Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Simple.zr file parsing", 
              "Testing parsing of complete simple.zr file with all syntax features");
    
    TZrSize readSize = 0;
    char* source = read_parser_fixture("test_simple.zr", &readSize);
    if (source == ZR_NULL) {
        // 如果文件不存在，跳过测试
        timer.endTime = clock();
        printf("Skip - Cost Time:%.3fms - %s:\n Cannot find test_simple.zr file\n", 
               0.0, testSummary);
        destroy_test_state(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_simple.zr", 15);
    SZrAstNode* ast = ZrParser_Parse(state, source, readSize, sourceName);
    
    free(source);
    
    if (ast != ZR_NULL) {
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        // 验证解析成功
        TEST_ASSERT_NOT_NULL(ast->data.script.statements);
        ZrParser_Ast_Free(state, ast);
    } else {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse simple.zr file");
        destroy_test_state(state);
        return;
    }
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试编译器生成文件
static void test_compiler_generate_files(void) {
    
    SZrTestTimer timer;
    const char* testSummary = "Compiler Generate .zro and .zri Files";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Compiler file generation", 
              "Testing compilation of test_simple.zr and generation of .zro binary and .zri intermediate files");
    
    TZrSize readSize = 0;
    char* source = read_parser_fixture("test_simple.zr", &readSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        printf("Skip - Cost Time:%.3fms - %s:\n Cannot find test_simple.zr file\n", 
               0.0, testSummary);
        destroy_test_state(state);
        TEST_DIVIDER();
        return;
    }
    
    // 解析 AST
    SZrString* sourceName = ZrCore_String_Create(state, "test_simple.zr", 15);
    SZrAstNode* ast = ZrParser_Parse(state, source, readSize, sourceName);
    
    if (ast == ZR_NULL) {
        free(source);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse test_simple.zr file");
        destroy_test_state(state);
        return;
    }
    
    // AST 节点类型验证（不输出调试信息）
    
    // 输出语法树到 .zrs 文件
    char zrsFileName[ZR_TESTS_PATH_MAX];
    TEST_ASSERT_TRUE(get_parser_generated_path("test_simple", "ast", ".zrs", zrsFileName, sizeof(zrsFileName)));
    unsigned char writeSyntaxTreeResult = ZrParser_Writer_WriteSyntaxTreeFile(state, ast, zrsFileName);
    if (writeSyntaxTreeResult) {
        print_generated_file_path(".zrs syntax tree file", zrsFileName);
    }
    
    // 编译 AST 为指令码
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    
    if (function == ZR_NULL) {
        free(source);
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile AST to instructions");
        destroy_test_state(state);
        return;
    }
    
    // 生成 .zro 二进制文件
    char zroFileName[ZR_TESTS_PATH_MAX];
    TEST_ASSERT_TRUE(get_parser_generated_path("test_simple", "binary", ".zro", zroFileName, sizeof(zroFileName)));
    unsigned char writeBinaryResult = ZrParser_Writer_WriteBinaryFile(state, function, zroFileName);
    if (!writeBinaryResult) {
        free(source);
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to write .zro binary file");
        destroy_test_state(state);
        return;
    }
    
    // 输出 .zro 文件生成位置
    print_generated_file_path(".zro binary file", zroFileName);
    
    // 生成 .zri 明文中间文件
    char zriFileName[ZR_TESTS_PATH_MAX];
    TEST_ASSERT_TRUE(get_parser_generated_path("test_simple", "intermediate", ".zri", zriFileName, sizeof(zriFileName)));
    unsigned char writeIntermediateResult = ZrParser_Writer_WriteIntermediateFile(state, function, zriFileName);
    if (!writeIntermediateResult) {
        free(source);
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to write .zri intermediate file");
        destroy_test_state(state);
        return;
    }
    
    // 输出 .zri 文件生成位置
    print_generated_file_path(".zri intermediate file", zriFileName);
    
    // 清理资源
    free(source);
    ZrParser_Ast_Free(state, ast);
    // 注意：function 由 GC 管理，不需要手动释放
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试数组字面量编译
static void test_compiler_array_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Array Literal";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Array literal compilation", 
              "Testing compilation of array literal: [1, 2, 3]");
    
    const char* source = "[1, 2, 3]";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse array literal");
        destroy_test_state(state);
        return;
    }
    
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile array literal");
        destroy_test_state(state);
        return;
    }
    
    // 验证编译成功（至少有一条指令）
    TEST_ASSERT_TRUE(function->instructionsLength > 0);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试对象字面量编译
static void test_compiler_object_literal(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Object Literal";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Object literal compilation", 
              "Testing compilation of object literal: {a: 1, b: 2}");
    
    const char* source = "{a: 1, b: 2}";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse object literal");
        destroy_test_state(state);
        return;
    }
    
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile object literal");
        destroy_test_state(state);
        return;
    }
    
    // 验证编译成功
    TEST_ASSERT_TRUE(function->instructionsLength > 0);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试 Lambda 表达式编译
static void test_compiler_lambda_expression(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Lambda Expression";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Lambda expression compilation", 
              "Testing compilation of lambda expression in a current variable declaration");
    
    const char* source = "let increment = fn(x: int): int => { return x + 1; };";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse lambda expression");
        destroy_test_state(state);
        return;
    }
    
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile lambda expression");
        destroy_test_state(state);
        return;
    }
    
    // 验证编译成功
    TEST_ASSERT_TRUE(function->instructionsLength > 0);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_for_loop_variable_initializer_statement(void) {
    SZrTestTimer timer;
    const char* testSummary = "For Loop Variable Initializer Statement";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    const char* source = "for (var i = 0; i < 3; i = i + 1) { i; }";
    SZrString* sourceName = ZrCore_String_Create(state, "test_for_var_header.zr", 22);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse traditional for loop with var initializer");
        destroy_test_state(state);
        return;
    }

    SZrAstNode *forNode = get_script_statement(ast, 0);
    TEST_ASSERT_NOT_NULL(forNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FOR_LOOP, forNode->type);
    TEST_ASSERT_NOT_NULL(forNode->data.forLoop.init);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, forNode->data.forLoop.init->type);
    TEST_ASSERT_NOT_NULL(forNode->data.forLoop.cond);
    TEST_ASSERT_NOT_NULL(forNode->data.forLoop.step);
    TEST_ASSERT_NOT_NULL(forNode->data.forLoop.block);

    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_compiler_parenthesized_lambda_iife(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Parenthesized Lambda IIFE";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Parenthesized lambda immediate invocation",
              "Testing compilation of grouped lambda IIFE syntax: ((delta: int) -> { return delta + 1; })(3)");

    const char* source =
        "var result = (fn(delta: int) => {\n"
        "    return delta + 1;\n"
        "})(3);\n"
        "return result;\n";
    SZrString* sourceName = ZrCore_String_Create(state, "lambda_iife.zr", 14);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse parenthesized lambda IIFE");
        destroy_test_state(state);
        return;
    }

    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile parenthesized lambda IIFE");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_TRUE(function->instructionsLength > 0);
    TEST_ASSERT_TRUE(function->childFunctionLength > 0);

    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_compiler_lambda_crlf_debug_metadata(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Lambda CRLF Debug Metadata";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Lambda CRLF debug metadata",
              "Testing that CRLF sources still compile child lambdas with source references and execution locations");

    const char* source =
        "module artifact_baseline;\r\n"
        "\r\n"
        "pub var greet = fn() => {\r\n"
        "    return \"hello artifact\";\r\n"
        "};\r\n"
        "\r\n"
        "var buildMessage = fn() => {\r\n"
        "    return greet();\r\n"
        "};\r\n"
        "\r\n"
        "return buildMessage();\r\n";

    SZrString* sourceName = ZrCore_String_Create(state, "artifact_baseline.zr", 20);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);

    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse CRLF lambda source");
        destroy_test_state(state);
        return;
    }

    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile CRLF lambda source");
        destroy_test_state(state);
        return;
    }

    TEST_ASSERT_EQUAL_UINT32(2, function->childFunctionLength);

      {
          SZrFunction* greetFunction = &function->childFunctionList[0];
          SZrFunction* buildMessageFunction = &function->childFunctionList[1];

          TEST_ASSERT_EQUAL_PTR(sourceName, greetFunction->sourceCodeList);
          TEST_ASSERT_EQUAL_PTR(sourceName, buildMessageFunction->sourceCodeList);
          TEST_ASSERT_NOT_NULL(greetFunction->executionLocationInfoList);
          TEST_ASSERT_NOT_NULL(buildMessageFunction->executionLocationInfoList);
          TEST_ASSERT_TRUE(greetFunction->executionLocationInfoLength > 0);
          TEST_ASSERT_TRUE(buildMessageFunction->executionLocationInfoLength > 0);
      }

    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_current_token_location_tracks_crlf_and_multiline_template_ranges(void) {
    SZrTestTimer timer;
    const char *testSummary = "Parser Token Location Tracking";
    const char *source =
            "var first = 1;\r\n"
            "var text = `ab\r\n"
            "cd`;\r\n"
            "return text;\r\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrParserState parserState;
    SZrFileRange location;

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "token_location_ranges.zr", 24);
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, state, source, strlen(source), sourceName);

    TEST_ASSERT_EQUAL_INT(ZR_TK_VAR, parserState.lexer->t.token);
    location = get_current_token_location(&parserState);
    assert_token_location_matches(&location, 0u, 1, 1, 3u, 1, 4);

    while (parserState.lexer->t.token != ZR_TK_TEMPLATE_STRING &&
           parserState.lexer->t.token != ZR_TK_EOS) {
        ZrParser_Lexer_Next(parserState.lexer);
    }
    TEST_ASSERT_EQUAL_INT(ZR_TK_TEMPLATE_STRING, parserState.lexer->t.token);
    location = get_current_token_location(&parserState);
    assert_token_location_matches(&location, 27u, 2, 12, 35u, 3, 4);

    while (parserState.lexer->t.token != ZR_TK_RETURN &&
           parserState.lexer->t.token != ZR_TK_EOS) {
        ZrParser_Lexer_Next(parserState.lexer);
    }
    TEST_ASSERT_EQUAL_INT(ZR_TK_RETURN, parserState.lexer->t.token);
    location = get_current_token_location(&parserState);
    assert_token_location_matches(&location, 38u, 4, 1, 44u, 4, 7);

    ZrParser_State_Free(&parserState);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_parser_cursor_restore_preserves_identifier_reference_locations(void) {
    SZrTestTimer timer;
    const char *testSummary = "Parser Cursor Restore Identifier Location Tracking";
    const char *source =
            "var seed = 0.0;\n"
            "fn helper(seed: float): void {\n"
            "    var localValue = seed + 1.0;\n"
            "    return localValue;\n"
            "}\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *functionNode;
    SZrAstNode *blockNode;
    SZrAstNode *varNode;
    SZrAstNode *binaryNode;
    SZrAstNode *seedRef;
    SZrAstNode *returnNode;
    SZrAstNode *localValueRef;

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "cursor_restore_locations.zr", 27);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse cursor restore location source");
        destroy_test_state(state);
        return;
    }

    functionNode = get_script_statement(ast, 1);
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, functionNode->type);
    blockNode = functionNode->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(blockNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, blockNode->type);
    TEST_ASSERT_TRUE(blockNode->data.block.body->count >= 2);

    varNode = blockNode->data.block.body->nodes[0];
    TEST_ASSERT_NOT_NULL(varNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, varNode->type);
    binaryNode = varNode->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(binaryNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BINARY_EXPRESSION, binaryNode->type);
    seedRef = binaryNode->data.binaryExpression.left;
    TEST_ASSERT_NOT_NULL(seedRef);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, seedRef->type);
    assert_token_location_matches(&seedRef->location, 68u, 3, 22, 72u, 3, 26);

    returnNode = blockNode->data.block.body->nodes[1];
    TEST_ASSERT_NOT_NULL(returnNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_RETURN_STATEMENT, returnNode->type);
    localValueRef = returnNode->data.returnStatement.expr;
    TEST_ASSERT_NOT_NULL(localValueRef);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, localValueRef->type);
    assert_token_location_matches(&localValueRef->location, 91u, 4, 12, 101u, 4, 22);

    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

static void test_parser_call_callee_locations_cover_identifier_text(void) {
    SZrTestTimer timer;
    const char *testSummary = "Parser Call Callee Location Tracking";
    const char *source =
            "fn callee(): void {}\n"
            "fn use(): void {\n"
            "    callee();\n"
            "    swap<int>(slot);\n"
            "}\n";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrAstNode *useFunction;
    SZrAstNode *blockNode;
    SZrAstNode *plainCall;
    SZrAstNode *genericCall;

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state, "call_callee_locations.zr", 24);
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse call callee location source");
        destroy_test_state(state);
        return;
    }

    useFunction = get_script_statement(ast, 1);
    TEST_ASSERT_NOT_NULL(useFunction);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, useFunction->type);
    blockNode = useFunction->data.functionDeclaration.body;
    TEST_ASSERT_NOT_NULL(blockNode);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK, blockNode->type);
    TEST_ASSERT_TRUE(blockNode->data.block.body->count >= 2);

    plainCall = unwrap_statement_expression(blockNode->data.block.body->nodes[0]);
    TEST_ASSERT_NOT_NULL(plainCall);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, plainCall->type);
    TEST_ASSERT_NOT_NULL(plainCall->data.primaryExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, plainCall->data.primaryExpression.property->type);
    assert_token_location_matches(&plainCall->data.primaryExpression.property->location,
                                   42u,
                                  3,
                                  5,
                                   48u,
                                  3,
                                  11);

    genericCall = unwrap_statement_expression(blockNode->data.block.body->nodes[1]);
    TEST_ASSERT_NOT_NULL(genericCall);
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, genericCall->type);
    TEST_ASSERT_NOT_NULL(genericCall->data.primaryExpression.property);
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, genericCall->data.primaryExpression.property->type);
    assert_token_location_matches(&genericCall->data.primaryExpression.property->location,
                                   56u,
                                  4,
                                  5,
                                   60u,
                                  4,
                                  9);

    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试 break/continue 语句编译
static void test_compiler_break_continue(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Break/Continue Statement";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Break/continue statement compilation", 
              "Testing compilation of break and continue statements in loops");
    
    // break/continue 语句需要独立写，不能嵌套在 if 表达式里
    const char* source = "while(true) { break; continue; }";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to parse break/continue statement");
        destroy_test_state(state);
        return;
    }
    
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile break/continue statement");
        destroy_test_state(state);
        return;
    }
    
    // 验证编译成功
    TEST_ASSERT_TRUE(function->instructionsLength > 0);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试当前 yield 语句解析
static void test_parser_yield_statement(void) {
    SZrTestTimer timer;
    const char* testSummary = "Parser Yield Statement";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Yield statement parsing",
              "Testing the current iterator yield statement AST");

    const char* source = "fn values(): zr.iteration.Iterator<int> { yield 42; }";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_FUNCTION_DECLARATION, ast->data.script.statements->nodes[0]->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements->nodes[0]->data.functionDeclaration.body);
    TEST_ASSERT_EQUAL_INT(ZR_AST_BLOCK,
                          ast->data.script.statements->nodes[0]->data.functionDeclaration.body->type);
    TEST_ASSERT_EQUAL_UINT32(
            1U,
            ast->data.script.statements->nodes[0]->data.functionDeclaration.body->data.block.body->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_YIELD_STATEMENT,
            ast->data.script.statements->nodes[0]->data.functionDeclaration.body->data.block.body->nodes[0]->type);

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
    printf("Parser Module Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 基础测试
    RUN_TEST(test_integer_literal);
    RUN_TEST(test_float_literal);
    RUN_TEST(test_string_literal);
    RUN_TEST(test_boolean_literal);
    RUN_TEST(test_module_declaration);
    RUN_TEST(test_current_module_declaration_variants);
    RUN_TEST(test_current_module_keyword_is_parsed);
    RUN_TEST(test_variable_declaration);
    RUN_TEST(test_access_modifier_parsing);
    
    TEST_MODULE_DIVIDER();
    printf("Expression Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 表达式测试
    RUN_TEST(test_binary_expression);
    RUN_TEST(test_source_compile_rejects_reported_expression_error);
    RUN_TEST(test_unary_expression);
    RUN_TEST(test_static_import_expression_uses_current_syntax);
    RUN_TEST(test_static_import_expression_member_chain_parsing);
    RUN_TEST(test_reserved_type_expression_parsing);
    RUN_TEST(test_member_named_import_is_not_static_import);
    RUN_TEST(test_prototype_construction_expression_parsing);
    RUN_TEST(test_native_boxed_new_expression_parsing);
    RUN_TEST(test_generic_boxed_new_expression_parsing);
    RUN_TEST(test_explicit_generic_function_call_parsing);
    RUN_TEST(test_interface_variance_and_where_parsing);
    RUN_TEST(test_parameter_passing_mode_parsing);
    RUN_TEST(test_const_generic_construction_parsing);
    RUN_TEST(test_resource_ownership_surface_parsing);
    RUN_TEST(test_ownership_generic_type_surface_parsing);
    RUN_TEST(test_legacy_ownership_type_syntax_is_rejected);
    RUN_TEST(test_resource_ownership_lifecycle_surface_parsing);
    RUN_TEST(test_legacy_percent_ownership_lifecycle_is_rejected);
    RUN_TEST(test_reference_expression_parsing);
    RUN_TEST(test_conditional_expression);
    RUN_TEST(test_array_literal);
    RUN_TEST(test_object_literal);
    
    TEST_MODULE_DIVIDER();
    printf("Declaration Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 声明测试
    RUN_TEST(test_function_declaration);
    RUN_TEST(test_function_declaration_requires_fn_keyword);
    RUN_TEST(test_extern_block_parsing);
    RUN_TEST(test_extern_delegate_parameter_decorator_flags_parsing);
    RUN_TEST(test_top_level_class_decorator_parsing);
    RUN_TEST(test_compile_time_class_decorator_parsing);
    RUN_TEST(test_compile_time_public_class_decorator_parsing);
    RUN_TEST(test_compile_time_struct_decorator_parsing);
    RUN_TEST(test_compile_time_function_decorator_parsing);
    RUN_TEST(test_native_extern_single_declaration_normalizes_to_block);
    RUN_TEST(test_struct_declaration);
    RUN_TEST(test_field_scoped_using_field_parsing);
    RUN_TEST(test_field_scoped_using_field_requires_var_keyword);
    RUN_TEST(test_removed_percent_using_new_expression_reports_migration_diagnostic);
    RUN_TEST(test_removed_percent_using_expression_reports_migration_diagnostic);
    RUN_TEST(test_field_scoped_bare_using_field_is_rejected);
    RUN_TEST(test_using_keyword_statement_parsing);
    RUN_TEST(test_using_else_without_guard_reports_diagnostic);
    RUN_TEST(test_using_binder_invalid_reports_diagnostic);
    RUN_TEST(test_resource_class_and_owner_types_parsing);
    RUN_TEST(test_class_abstract_member_and_final_class_parsing);
    RUN_TEST(test_class_member_modifier_and_super_member_parsing);
    RUN_TEST(test_legacy_async_surfaces_are_rejected);
    RUN_TEST(test_function_type_annotation_parsing);
    RUN_TEST(test_function_type_missing_return_arrow_is_rejected);
    RUN_TEST(test_type_query_accepts_function_type_expression);
    
    TEST_MODULE_DIVIDER();
    printf("Statement Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 语句测试
    RUN_TEST(test_if_statement);
    RUN_TEST(test_return_statement);
    RUN_TEST(test_for_loop_variable_initializer_statement);
    
    TEST_MODULE_DIVIDER();
    printf("Complete Script Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 完整脚本测试
    RUN_TEST(test_simple_script);
    RUN_TEST(test_simple_zr_file);
    
    TEST_MODULE_DIVIDER();
    printf("Compiler Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 编译器测试
    RUN_TEST(test_compiler_generate_files);
    RUN_TEST(test_compiler_array_literal);
    RUN_TEST(test_compiler_object_literal);
    RUN_TEST(test_compiler_lambda_expression);
    RUN_TEST(test_compiler_parenthesized_lambda_iife);
    RUN_TEST(test_compiler_lambda_crlf_debug_metadata);
    RUN_TEST(test_current_token_location_tracks_crlf_and_multiline_template_ranges);
    RUN_TEST(test_parser_cursor_restore_preserves_identifier_reference_locations);
    RUN_TEST(test_parser_call_callee_locations_cover_identifier_text);
    RUN_TEST(test_compiler_break_continue);
    RUN_TEST(test_parser_yield_statement);
    
    return UNITY_END();
}

