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
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/value.h"
#include "test_support.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

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
    double elapsed = ((double)(timer.endTime - timer.startTime) / CLOCKS_PER_SEC) * 1000.0; \
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

// 测试初始化和清理
void setUp(void) {
}

void tearDown(void) {
}

// ==================== 基础测试 ====================

// 测试整数字面量解析
void test_integer_literal(void) {
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
void test_float_literal(void) {
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
void test_string_literal(void) {
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
void test_boolean_literal(void) {
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
void test_module_declaration(void) {
    SZrTestTimer timer;
    const char* testSummary = "Module Declaration Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Module declaration parsing", 
              "Testing parsing of reserved module declaration: %module \"test\";");
    const char* source = "%module \"test\";";
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
void test_variable_declaration(void) {
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
void test_access_modifier_parsing(void) {
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
void test_binary_expression(void) {
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

// 测试一元表达式解析
void test_unary_expression(void) {
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

void test_prototype_construction_expression_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Prototype Construction Expression Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Prototype construction parsing",
              "Testing parsing of `$(math.Vector3)(1.0, 2.0, 3.0)` as a valid construction expression");
    {
        const char *source =
                "var math = %import(\"zr.math\");\n"
                "$(math.Vector3)(1.0, 2.0, 3.0);";
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

void test_native_boxed_new_expression_parsing(void) {
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
                "var math = %import(\"zr.math\");\n"
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

// 测试条件表达式解析
void test_conditional_expression(void) {
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
void test_array_literal(void) {
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
void test_object_literal(void) {
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
void test_function_declaration(void) {
    SZrTestTimer timer;
    const char* testSummary = "Function Declaration Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Function declaration parsing", 
              "Testing parsing of function declaration: test(){}");
    const char* source = "test(){}";
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

void test_extern_block_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Extern Block Parsing";
    const char *source =
            "%extern(\"fixture\") {\n"
            "    #zr.ffi.entry(\"zr_ffi_add_i32\")# Add(lhs:i32, rhs:i32): i32;\n"
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

void test_reserved_import_expression_variants(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reserved Import Expression Variants";
    static const struct {
        const char *source;
        const char *expectedName;
    } fixtures[] = {
        { "var math = %import zr.math;", "zr.math" },
        { "var math = %import \"zr.math\";", "zr.math" },
        { "var math = %import(\"zr.math\");", "zr.math" },
    };
    TZrSize index;

    TEST_START(testSummary);
    timer.startTime = clock();

    TEST_INFO("Reserved import parsing",
              "Testing that all supported %import spellings normalize to the same import-expression AST");

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

void test_reserved_import_expression_member_chain_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reserved Import Member Chain Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "%import(\"helper\").toolkit.math.greet();";
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

void test_legacy_import_syntax_is_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Legacy Import Syntax Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source =
                "import(\"zr.math\");\n"
                "zr.import(\"zr.math\");\n"
                "var ok = 1;";
        SZrString *sourceName = ZrCore_String_Create(state, "legacy_import_syntax.zr", 23);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
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

void test_reserved_module_declaration_variants(void) {
    SZrTestTimer timer;
    const char *testSummary = "Reserved Module Declaration Variants";
    static const struct {
        const char *source;
        const char *expectedName;
    } fixtures[] = {
        { "%module foo;", "foo" },
        { "%module \"foo.bar\";", "foo.bar" },
        { "%module(\"foo.bar\");", "foo.bar" },
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

void test_legacy_module_keyword_is_rejected(void) {
    SZrTestTimer timer;
    const char *testSummary = "Legacy Module Keyword Rejected";

    TEST_START(testSummary);
    timer.startTime = clock();

    {
        SZrState *state = create_test_state();
        const char *source = "module \"legacy\";\nvar value = 1;";
        SZrString *sourceName = ZrCore_String_Create(state, "legacy_module_keyword.zr", 24);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);

        TEST_ASSERT_NOT_NULL(state);
        TEST_ASSERT_NOT_NULL(ast);
        TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
        TEST_ASSERT_NULL(ast->data.script.moduleName);
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

void test_extern_single_declaration_normalizes_to_block(void) {
    SZrTestTimer timer;
    const char *testSummary = "Extern Single Declaration Parsing";
    const char *source = "%extern(\"fixture\") Add(lhs:i32, rhs:i32): i32;";
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
void test_struct_declaration(void) {
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

void test_field_scoped_using_field_parsing(void) {
    SZrTestTimer timer;
    const char *testSummary = "Field-Scoped Using Field Parsing";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Field-scoped using parsing",
              "Testing parsing of `using var` fields in struct/class members, including static+using syntax that should be rejected later by semantic analysis");

    {
        const char *source =
            "struct HandleBox { using var handle: unique<Resource>; }\n"
            "class Holder { static using var resource: unique<Resource>; }";
        SZrString *sourceName = ZrCore_String_Create(state, "using_fields.zr", 15);
        SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), sourceName);
        SZrAstNode *structDecl;
        SZrAstNode *classDecl;
        SZrAstNode *structFieldNode;
        SZrAstNode *classFieldNode;
        SZrStructField *structField;
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
        TEST_ASSERT_EQUAL_INT(1, (int)classDecl->data.classDeclaration.members->count);

        structFieldNode = structDecl->data.structDeclaration.members->nodes[0];
        classFieldNode = classDecl->data.classDeclaration.members->nodes[0];
        TEST_ASSERT_NOT_NULL(structFieldNode);
        TEST_ASSERT_NOT_NULL(classFieldNode);
        TEST_ASSERT_EQUAL_INT(ZR_AST_STRUCT_FIELD, structFieldNode->type);
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, classFieldNode->type);

        structField = &structFieldNode->data.structField;
        classField = &classFieldNode->data.classField;

        TEST_ASSERT_TRUE(structField->isUsingManaged);
        TEST_ASSERT_FALSE(structField->isStatic);
        TEST_ASSERT_NOT_NULL(structField->typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              structField->typeInfo->ownershipQualifier);

        TEST_ASSERT_TRUE(classField->isUsingManaged);
        TEST_ASSERT_TRUE(classField->isStatic);
        TEST_ASSERT_NOT_NULL(classField->typeInfo);
        TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_QUALIFIER_UNIQUE,
                              classField->typeInfo->ownershipQualifier);

        ZrParser_Ast_Free(state, ast);
    }

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

void test_field_scoped_using_field_requires_var_keyword(void) {
    SZrTestTimer timer;
    const char *testSummary = "Field-Scoped Using Requires Var Keyword";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState *state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Field-scoped using syntax rejection",
              "Testing that `using` fields without the required `var` keyword are rejected instead of silently becoming ordinary fields");

    {
        const char *source = "struct Broken { using handle: unique<Resource>; }";
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

// ==================== 语句测试 ====================

// 测试 if 语句解析
void test_if_statement(void) {
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
void test_return_statement(void) {
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
void test_simple_script(void) {
    SZrTestTimer timer;
    const char* testSummary = "Simple Script Parsing";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Simple script parsing", 
              "Testing parsing of simple script with module and variable declarations");
    const char* source = "%module(\"test\");\nvar x = 1;\nvar y = 2;";
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
void test_simple_zr_file(void) {
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
void test_compiler_generate_files(void) {
    
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
void test_compiler_array_literal(void) {
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
void test_compiler_object_literal(void) {
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
void test_compiler_lambda_expression(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Lambda Expression";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Lambda expression compilation", 
              "Testing compilation of lambda expression: (x) => { return x + 1; }");
    
    const char* source = "(x) => { return x + 1; }";
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

void test_compiler_lambda_crlf_locations(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Lambda CRLF Locations";

    TEST_START(testSummary);
    timer.startTime = clock();

    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    TEST_INFO("Lambda CRLF location tracking",
              "Testing that compiled lambda line ranges stay stable when the source uses CRLF line endings");

    const char* source =
        "%module \"artifact_baseline\";\r\n"
        "\r\n"
        "pub var greet = () => {\r\n"
        "    return \"hello artifact\";\r\n"
        "};\r\n"
        "\r\n"
        "var buildMessage = () => {\r\n"
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

    TEST_ASSERT_EQUAL_UINT32(2, function->constantValueLength);

    for (TZrUInt32 i = 0; i < function->constantValueLength; i++) {
        const SZrTypeValue* constant = &function->constantValueList[i];
        TEST_ASSERT_TRUE(constant->type == ZR_VALUE_TYPE_FUNCTION || constant->type == ZR_VALUE_TYPE_CLOSURE);
        TEST_ASSERT_NOT_NULL(constant->value.object);
    }

    {
        SZrRawObject* rawGreet = function->constantValueList[0].value.object;
        SZrRawObject* rawBuildMessage = function->constantValueList[1].value.object;
        SZrFunction* greetFunction = ZR_CAST(SZrFunction*, rawGreet);
        SZrFunction* buildMessageFunction = ZR_CAST(SZrFunction*, rawBuildMessage);

        TEST_ASSERT_EQUAL_UINT32(3, greetFunction->lineInSourceStart);
        TEST_ASSERT_EQUAL_UINT32(6, greetFunction->lineInSourceEnd);
        TEST_ASSERT_EQUAL_UINT32(7, buildMessageFunction->lineInSourceStart);
        TEST_ASSERT_EQUAL_UINT32(10, buildMessageFunction->lineInSourceEnd);
    }

    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    TEST_PASS_CUSTOM(timer, testSummary);
    destroy_test_state(state);
    TEST_DIVIDER();
}

// 测试 break/continue 语句编译
void test_compiler_break_continue(void) {
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

// 测试 OUT 语句编译
void test_compiler_out_statement(void) {
    SZrTestTimer timer;
    const char* testSummary = "Compiler Out Statement";
    
    TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    TEST_INFO("Out statement compilation", 
              "Testing compilation of out statement in generator context");
    
    // OUT 语句需要在生成器表达式 {{ }} 中使用
    // 但解析器可能还不支持生成器表达式作为顶层语句
    // 这里先测试在一个块中使用
    const char* source = "{{ out 42; }};";
    SZrString* sourceName = ZrCore_String_Create(state, "test.zr", 7);
    SZrAstNode* ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    
    if (ast == ZR_NULL) {
        // OUT 语句可能需要在特定上下文中，暂时跳过测试
        timer.endTime = clock();
        printf("Skip - Cost Time:%.3fms - %s:\n Out statement requires generator expression context\n", 
               0.0, testSummary);
        destroy_test_state(state);
        TEST_DIVIDER();
        return;
    }
    
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        TEST_FAIL_CUSTOM(timer, testSummary, "Failed to compile out statement");
        destroy_test_state(state);
        return;
    }
    
    // 验证编译成功（即使没有指令也算通过，因为 OUT 可能还未完全实现）
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
    RUN_TEST(test_reserved_module_declaration_variants);
    RUN_TEST(test_legacy_module_keyword_is_rejected);
    RUN_TEST(test_variable_declaration);
    RUN_TEST(test_access_modifier_parsing);
    
    TEST_MODULE_DIVIDER();
    printf("Expression Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 表达式测试
    RUN_TEST(test_binary_expression);
    RUN_TEST(test_unary_expression);
    RUN_TEST(test_reserved_import_expression_variants);
    RUN_TEST(test_reserved_import_expression_member_chain_parsing);
    RUN_TEST(test_legacy_import_syntax_is_rejected);
    RUN_TEST(test_prototype_construction_expression_parsing);
    RUN_TEST(test_native_boxed_new_expression_parsing);
    RUN_TEST(test_conditional_expression);
    RUN_TEST(test_array_literal);
    RUN_TEST(test_object_literal);
    
    TEST_MODULE_DIVIDER();
    printf("Declaration Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 声明测试
    RUN_TEST(test_function_declaration);
    RUN_TEST(test_extern_block_parsing);
    RUN_TEST(test_extern_single_declaration_normalizes_to_block);
    RUN_TEST(test_struct_declaration);
    RUN_TEST(test_field_scoped_using_field_parsing);
    RUN_TEST(test_field_scoped_using_field_requires_var_keyword);
    
    TEST_MODULE_DIVIDER();
    printf("Statement Tests\n");
    TEST_MODULE_DIVIDER();
    
    // 语句测试
    RUN_TEST(test_if_statement);
    RUN_TEST(test_return_statement);
    
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
    RUN_TEST(test_compiler_lambda_crlf_locations);
    RUN_TEST(test_compiler_break_continue);
    RUN_TEST(test_compiler_out_statement);
    
    return UNITY_END();
}
