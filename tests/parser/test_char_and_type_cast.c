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
#include "zr_vm_parser/compiler.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_test_log_macros.h"

// realpath 兼容函数（MSVC使用_fullpath）
#ifdef _MSC_VER
static char* test_realpath(const char* path, char* resolved_path) {
    return _fullpath(resolved_path, path, _MAX_PATH);
}
#define realpath test_realpath
#endif

// 创建测试状态
static SZrState* create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

// 销毁测试状态
static void destroy_test_state(SZrState* state) {
    ZrTests_State_Destroy(state);
}

// 读取测试文件内容
static char* read_test_file(const char* filename, size_t* outSize) {
    char path[ZR_TESTS_PATH_MAX];

    if (!ZrTests_Path_GetParserFixture(filename, path, sizeof(path))) {
        return ZR_NULL;
    }

    return ZrTests_ReadTextFile(path, outSize);
}

static char* read_reference_file(const char* relativePath, size_t* outSize) {
    return ZrTests_Reference_ReadFixture(relativePath, outSize);
}

static char* read_repo_doc_file(const char* relativePath, size_t* outSize) {
    return ZrTests_Reference_ReadDoc(relativePath, outSize);
}

static char* read_tests_repo_file(const char* relativePath, size_t* outSize) {
    char path[ZR_TESTS_PATH_MAX];

    if (relativePath == ZR_NULL) {
        return ZR_NULL;
    }

    if (snprintf(path, sizeof(path), "%s/%s", ZR_VM_TESTS_SOURCE_DIR, relativePath) < 0) {
        return ZR_NULL;
    }

    return ZrTests_ReadTextFile(path, outSize);
}

static size_t count_substring_occurrences(const char* text, const char* needle) {
    return ZrTests_Reference_CountOccurrences(text, needle);
}

typedef struct {
    TZrBool reported;
    char message[256];
} SZrCapturedParserDiagnostic;

static void capture_reference_parser_error(TZrPtr userData,
                                           const SZrFileRange* location,
                                           const TZrChar* message,
                                           EZrToken token) {
    SZrCapturedParserDiagnostic* diagnostic = (SZrCapturedParserDiagnostic*)userData;

    ZR_UNUSED_PARAMETER(location);
    ZR_UNUSED_PARAMETER(token);

    if (diagnostic == ZR_NULL) {
        return;
    }

    diagnostic->reported = ZR_TRUE;
    diagnostic->message[0] = '\0';
    if (message != ZR_NULL) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

static void assert_reference_manifest_shape(const char* manifestText,
                                           const char* featureGroup,
                                           size_t minimumCases) {
    TEST_ASSERT_NOT_NULL(manifestText);
    TEST_ASSERT_NOT_NULL(strstr(manifestText, "\"feature_group\""));
    TEST_ASSERT_NOT_NULL(strstr(manifestText, featureGroup));
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"id\"") >= minimumCases);
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"upstream_language\"") >= minimumCases);
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"zr_decision\"") >= minimumCases);
    TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"expected_outcome\"") >= minimumCases);
}

static void execute_reference_test_fixture_expect_int(const char* relativePath, TZrInt64 expectedValue) {
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrCompileResult compileResult;
    TZrInt64 actualValue = 0;

    memset(&compileResult, 0, sizeof(compileResult));

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file(relativePath, &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)relativePath, (TZrSize)strlen(relativePath));
    TEST_ASSERT_NOT_NULL(sourceName);

    ast = ZrParser_Parse(state, source, fileSize, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_TRUE(ZrParser_Compiler_CompileWithTests(state, ast, &compileResult));
    TEST_ASSERT_TRUE(compileResult.testFunctionCount >= 1);
    TEST_ASSERT_NOT_NULL(compileResult.testFunctions[0]);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, compileResult.testFunctions[0], &actualValue));
    TEST_ASSERT_EQUAL_INT64(expectedValue, actualValue);

    ZrParser_CompileResult_Free(state, &compileResult);
    ZrParser_Ast_Free(state, ast);
    free(source);
    destroy_test_state(state);
}

static void compile_reference_fixture_expect_failure(const char* relativePath) {
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrFunction* function = ZR_NULL;

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file(relativePath, &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)relativePath, (TZrSize)strlen(relativePath));
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, fileSize, sourceName);
    TEST_ASSERT_NULL(function);

    free(source);
    destroy_test_state(state);
}

static void parse_reference_fixture_expect_diagnostic(const char* relativePath,
                                                      const char* expectedMessageFragment) {
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrParserState parserState;
    SZrAstNode* ast = ZR_NULL;
    SZrCapturedParserDiagnostic diagnostic;
    const char* message = ZR_NULL;

    memset(&diagnostic, 0, sizeof(diagnostic));

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file(relativePath, &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)relativePath, (TZrSize)strlen(relativePath));
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, state, source, fileSize, sourceName);
    parserState.errorCallback = capture_reference_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_TRUE(diagnostic.reported || parserState.hasError || ast == ZR_NULL);

    message = diagnostic.reported ? diagnostic.message : parserState.errorMessage;
    TEST_ASSERT_NOT_NULL(message);
    if (expectedMessageFragment != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(strstr(message, expectedMessageFragment));
    }

    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrParser_State_Free(&parserState);
    free(source);
    destroy_test_state(state);
}

// 测试字符字面量解析
void test_char_literals_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Character Literals Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    ZR_TEST_INFO("Character literals parsing", 
              "Testing parsing of character literals with various escape sequences");
    
    size_t fileSize;
    char* source = read_test_file("test_char_literals.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Cannot find test_char_literals.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_char_literals.zr", 22);
    SZrAstNode* ast = ZrParser_Parse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse test_char_literals.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试字符字面量编译
void test_char_literals_compilation(void) {
    SZrTestTimer timer;
    const char* testSummary = "Character Literals Compilation";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    ZR_TEST_INFO("Character literals compilation", 
              "Testing compilation of character literals to VM instructions");
    
    size_t fileSize;
    char* source = read_test_file("test_char_literals.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Cannot find test_char_literals.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_char_literals.zr", 22);
    SZrAstNode* ast = ZrParser_Parse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse test_char_literals.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    // 编译 AST 为指令码
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile AST to instructions");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试基本类型转换解析
void test_type_cast_basic_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Basic Type Cast Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    ZR_TEST_INFO("Basic type cast parsing", 
              "Testing parsing of basic type cast expressions: <int>, <float>, <string>, <bool>");
    
    size_t fileSize;
    char* source = read_test_file("test_type_cast_basic.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Cannot find test_type_cast_basic.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_type_cast_basic.zr", 24);
    SZrAstNode* ast = ZrParser_Parse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse test_type_cast_basic.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试基本类型转换编译
void test_type_cast_basic_compilation(void) {
    SZrTestTimer timer;
    const char* testSummary = "Basic Type Cast Compilation";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    ZR_TEST_INFO("Basic type cast compilation", 
              "Testing compilation of basic type cast expressions to conversion instructions");
    
    size_t fileSize;
    char* source = read_test_file("test_type_cast_basic.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Cannot find test_type_cast_basic.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_type_cast_basic.zr", 24);
    SZrAstNode* ast = ZrParser_Parse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse test_type_cast_basic.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    // 编译 AST 为指令码
    SZrFunction* function = ZrParser_Compiler_Compile(state, ast);
    
    if (function == ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to compile AST to instructions");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试结构体类型转换解析
void test_type_cast_struct_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Struct Type Cast Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    ZR_TEST_INFO("Struct type cast parsing", 
              "Testing parsing of struct type cast expressions: <StructType>");
    
    size_t fileSize;
    char* source = read_test_file("test_type_cast_struct.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Cannot find test_type_cast_struct.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_type_cast_struct.zr", 25);
    SZrAstNode* ast = ZrParser_Parse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse test_type_cast_struct.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

// 测试类类型转换解析
void test_type_cast_class_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Class Type Cast Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    SZrState* state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);
    
    ZR_TEST_INFO("Class type cast parsing", 
              "Testing parsing of class type cast expressions: <ClassName>");
    
    size_t fileSize;
    char* source = read_test_file("test_type_cast_class.zr", &fileSize);
    if (source == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Cannot find test_type_cast_class.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    SZrString* sourceName = ZrCore_String_Create(state, "test_type_cast_class.zr", 24);
    SZrAstNode* ast = ZrParser_Parse(state, source, fileSize, sourceName);
    
    free(source);
    
    if (ast == ZR_NULL) {
        timer.endTime = clock();
        ZR_TEST_FAIL(timer, testSummary, "Failed to parse test_type_cast_class.zr file");
        destroy_test_state(state);
        ZR_TEST_DIVIDER();
        return;
    }
    
    // 验证 AST 结构
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    
    ZrParser_Ast_Free(state, ast);
    
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_relational_less_than_does_not_emit_speculative_type_cast_diagnostic(void) {
    SZrTestTimer timer;
    const char* testSummary = "Relational Less Than Does Not Emit Speculative Type Cast Diagnostic";
    const char* source =
            "var left = 4;\n"
            "var right = 8;\n"
            "var flag = left < right;\n"
            "return flag;";
    const char* sourcePath = "relational_less_than_no_speculative_type_cast_diagnostic.zr";
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrParserState parserState;
    SZrAstNode* ast = ZR_NULL;
    SZrCapturedParserDiagnostic diagnostic;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    memset(&diagnostic, 0, sizeof(diagnostic));

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourcePath, (TZrSize)strlen(sourcePath));
    TEST_ASSERT_NOT_NULL(sourceName);

    ZrParser_State_Init(&parserState, state, source, strlen(source), sourceName);
    parserState.errorCallback = capture_reference_parser_error;
    parserState.errorUserData = &diagnostic;
    parserState.suppressErrorOutput = ZR_TRUE;

    ast = ZrParser_ParseWithState(&parserState);

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_FALSE(parserState.hasError);
    TEST_ASSERT_FALSE(diagnostic.reported);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);

    ZrParser_Ast_Free(state, ast);
    ZrParser_State_Free(&parserState);
    destroy_test_state(state);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_core_semantics_manifest_inventory(void) {
    static const struct {
        const char* manifestPath;
        const char* featureGroup;
    } kManifests[] = {
        {"core_semantics/literals/manifest.json", "literals"},
        {"core_semantics/expressions/manifest.json", "expressions"},
        {"core_semantics/imports/manifest.json", "imports"},
        {"core_semantics/calls/manifest.json", "calls"},
        {"core_semantics/casts-and-const/manifest.json", "casts-and-const"},
        {"core_semantics/diagnostics/manifest.json", "diagnostics"},
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Core Semantics Manifest Inventory";
    size_t totalCases = 0;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t i = 0; i < sizeof(kManifests) / sizeof(kManifests[0]); i++) {
        size_t manifestSize = 0;
        char* manifestText = read_reference_file(kManifests[i].manifestPath, &manifestSize);

        ZR_UNUSED_PARAMETER(manifestSize);
        assert_reference_manifest_shape(manifestText, kManifests[i].featureGroup, 6);
        totalCases += count_substring_occurrences(manifestText, "\"id\"");
        free(manifestText);
    }

    TEST_ASSERT_TRUE(totalCases >= 36);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_full_stack_manifest_inventory(void) {
    static const struct {
        const char* manifestPath;
        const char* domainSlug;
    } kManifests[] = {
        {"core_semantics/lexing_literals_diagnostics/manifest.json", "lexing_literals_diagnostics"},
        {"core_semantics/expressions_precedence_chains/manifest.json", "expressions_precedence_chains"},
        {"core_semantics/calls_named_default_varargs/manifest.json", "calls_named_default_varargs"},
        {"core_semantics/types_casts_const/manifest.json", "types_casts_const"},
        {"core_semantics/object_member_index_construct_target/manifest.json", "object_member_index_construct_target"},
        {"core_semantics/protocols_iteration_comparable/manifest.json", "protocols_iteration_comparable"},
        {"core_semantics/modules_imports_artifacts/manifest.json", "modules_imports_artifacts"},
        {"core_semantics/oop_inheritance_descriptors/manifest.json", "oop_inheritance_descriptors"},
        {"core_semantics/ownership_using_resource_lifecycle/manifest.json", "ownership_using_resource_lifecycle"},
        {"core_semantics/exceptions_gc_native_stress/manifest.json", "exceptions_gc_native_stress"},
    };
    static const char* kRequiredFields[] = {
        "\"id\"",
        "\"domain\"",
        "\"feature_group\"",
        "\"upstream_language\"",
        "\"upstream_file\"",
        "\"upstream_intent\"",
        "\"zr_decision\"",
        "\"zr_contract\"",
        "\"layer_targets\"",
        "\"case_kind\"",
        "\"expected_outcome\"",
        "\"assertions\"",
        "\"stress_class\"",
    };
    static const char* kRequiredCaseKinds[] = {
        "happy",
        "negative",
        "boundary",
        "combination",
        "regression",
        "divergence",
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Full Stack Manifest Inventory";
    size_t totalCases = 0;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t i = 0; i < sizeof(kManifests) / sizeof(kManifests[0]); i++) {
        size_t manifestSize = 0;
        char* manifestText = read_reference_file(kManifests[i].manifestPath, &manifestSize);

        ZR_UNUSED_PARAMETER(manifestSize);
        ZrTests_Reference_AssertManifestShape(manifestText,
                                              kManifests[i].domainSlug,
                                              12,
                                              kRequiredFields,
                                              sizeof(kRequiredFields) / sizeof(kRequiredFields[0]));
        TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"layer_targets\"") >= 12);
        TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"assertions\"") >= 12);
        TEST_ASSERT_TRUE(count_substring_occurrences(manifestText, "\"adopt\"") +
                         count_substring_occurrences(manifestText, "\"adapt\"") +
                         count_substring_occurrences(manifestText, "\"reject\"") >= 12);
        ZrTests_Reference_AssertCaseKindsCovered(manifestText,
                                                 kRequiredCaseKinds,
                                                 sizeof(kRequiredCaseKinds) / sizeof(kRequiredCaseKinds[0]),
                                                 2);

        totalCases += count_substring_occurrences(manifestText, "\"id\"");
        free(manifestText);
    }

    TEST_ASSERT_TRUE(totalCases >= 120);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_full_stack_priority_cases_are_present(void) {
    static const char* kPriorityCaseIds[] = {
        "lexing-unclosed-string",
        "lexing-invalid-hex-escape",
        "lexing-unicode-boundary",
        "lexing-invalid-char-width",
        "lexing-multiline-literal-reject",
        "lexing-location-recovery",
        "calls-named-default-reorder-pass",
        "calls-duplicate-named-fail",
        "calls-unexpected-named-fail",
        "calls-positional-after-named-fail",
        "calls-varargs-arity-boundary",
        "calls-overload-ambiguity-fail",
        "modules-root-member-chain-pass",
        "modules-duplicate-import-identity",
        "modules-cyclic-import-reject",
        "modules-binary-metadata-roundtrip",
        "modules-source-binary-same-logical-path",
        "modules-hidden-internal-import-api-reject",
        "object-member-vs-string-index-split",
        "object-array-map-plain-index-split",
        "object-property-getter-setter-precedence",
        "protocols-foreach-contract-lowering",
        "protocols-iterator-invalidation",
        "protocols-comparable-hashable-consistency",
        "ownership-shared-owner-consume",
        "ownership-shared-new-wrapper",
        "ownership-using-cleanup-on-return",
        "exceptions-cleanup-on-throw",
        "ownership-weak-expiry-after-last-shared-release",
        "exceptions-gc-with-active-owned-native-object",
    };
    static const char* kManifestPaths[] = {
        "core_semantics/lexing_literals_diagnostics/manifest.json",
        "core_semantics/expressions_precedence_chains/manifest.json",
        "core_semantics/calls_named_default_varargs/manifest.json",
        "core_semantics/types_casts_const/manifest.json",
        "core_semantics/object_member_index_construct_target/manifest.json",
        "core_semantics/protocols_iteration_comparable/manifest.json",
        "core_semantics/modules_imports_artifacts/manifest.json",
        "core_semantics/oop_inheritance_descriptors/manifest.json",
        "core_semantics/ownership_using_resource_lifecycle/manifest.json",
        "core_semantics/exceptions_gc_native_stress/manifest.json",
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Full Stack Priority Cases Are Present";
    char* manifestTexts[sizeof(kManifestPaths) / sizeof(kManifestPaths[0])] = {0};

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t manifestIndex = 0; manifestIndex < sizeof(kManifestPaths) / sizeof(kManifestPaths[0]); manifestIndex++) {
        size_t manifestSize = 0;
        manifestTexts[manifestIndex] = read_reference_file(kManifestPaths[manifestIndex], &manifestSize);
        ZR_UNUSED_PARAMETER(manifestSize);
        TEST_ASSERT_NOT_NULL(manifestTexts[manifestIndex]);
    }

    for (size_t caseIndex = 0; caseIndex < sizeof(kPriorityCaseIds) / sizeof(kPriorityCaseIds[0]); caseIndex++) {
        TZrBool found = ZR_FALSE;
        for (size_t manifestIndex = 0; manifestIndex < sizeof(kManifestPaths) / sizeof(kManifestPaths[0]); manifestIndex++) {
            if (strstr(manifestTexts[manifestIndex], kPriorityCaseIds[caseIndex]) != ZR_NULL) {
                found = ZR_TRUE;
                break;
            }
        }
        TEST_ASSERT_TRUE(found);
    }

    for (size_t manifestIndex = 0; manifestIndex < sizeof(kManifestPaths) / sizeof(kManifestPaths[0]); manifestIndex++) {
        free(manifestTexts[manifestIndex]);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_full_stack_master_matrix_document_exists(void) {
    static const char* kRequiredPhrases[] = {
        "ZR 全栈参考语言共性测试矩阵",
        "Lexing/Literals/Diagnostics",
        "Expressions/Precedence/Chains",
        "Calls/Named/Default/Varargs",
        "Types/Casts/Const",
        "Object/Member/Index/ConstructTarget",
        "Protocols/Iteration/Comparable",
        "Modules/Imports/Artifacts",
        "OOP/Inheritance/Descriptors",
        "Ownership/Using/ResourceLifecycle",
        "Exceptions/GC/NativeStress",
        "12 条核心 case",
        "120",
        "tests/fixtures/reference/core_semantics/",
        "source",
        "artifact",
        "runtime",
        "project",
        "smoke",
        "core",
        "stress",
        "interp",
        "binary",
        "zr_vm_aot/",
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Full Stack Master Matrix Document Exists";
    size_t docSize = 0;
    char* docText = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    docText = read_repo_doc_file("reference-alignment/full-stack-test-matrix.md", &docSize);
    ZR_UNUSED_PARAMETER(docSize);
    TEST_ASSERT_NOT_NULL(docText);

    for (size_t i = 0; i < sizeof(kRequiredPhrases) / sizeof(kRequiredPhrases[0]); i++) {
        TEST_ASSERT_TRUE(ZrTests_Reference_TextContainsAll(docText, &kRequiredPhrases[i], 1));
    }

    free(docText);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_execution_order_doc_mentions_current_suites_and_tiers(void) {
    static const char *kRequiredPhrases[] = {
        "core_runtime",
        "language_pipeline",
        "containers",
        "language_server",
        "language_server_stdio_smoke",
        "projects",
        "cli_args",
        "cli_integration",
        "ZR_VM_TEST_TIER",
        "--tier <smoke|core|stress>",
        "smoke/core/stress",
    };
    SZrTestTimer timer;
    const char *testSummary = "Execution Order Doc Mentions Current Suites And Tiers";
    size_t docSize = 0;
    char *docText = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    docText = read_tests_repo_file("TEST_EXECUTION_ORDER.md", &docSize);
    ZR_UNUSED_PARAMETER(docSize);
    TEST_ASSERT_NOT_NULL(docText);
    TEST_ASSERT_TRUE(ZrTests_Reference_TextContainsAll(docText,
                                                       kRequiredPhrases,
                                                       sizeof(kRequiredPhrases) / sizeof(kRequiredPhrases[0])));

    free(docText);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_core_semantics_matrix_document_exists(void) {
    static const char* kRequiredPhrases[] = {
        "ZR 核心语义 capability matrix",
        "字面量与转义",
        "表达式与优先级",
        "%module/%import 与成员链",
        "调用面：位置参数、命名参数、默认值、变参、重载/错误 arity",
        "<Type> 转换、prototype/new 误用、`const` 赋值规则",
        "诊断与错误恢复",
        "已符合 ZR 合同",
        "需要补测试",
        "需要补诊断/实现",
        "明确与外部语言不同并保留差异",
        "parser",
        "semantic",
        "compiler",
        "runtime",
        "project",
        "golden",
        "const_if_missing_branch_fail.zr",
        "const_switch_paths_pass.zr",
        "const_ternary_paths_pass.zr",
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Core Semantics Matrix Document Exists";
    size_t docSize = 0;
    char* docText = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    docText = read_repo_doc_file("reference-alignment/core-semantics-matrix.md", &docSize);
    ZR_UNUSED_PARAMETER(docSize);
    TEST_ASSERT_NOT_NULL(docText);

    for (size_t i = 0; i < sizeof(kRequiredPhrases) / sizeof(kRequiredPhrases[0]); i++) {
        TEST_ASSERT_TRUE(ZrTests_Reference_TextContainsAll(docText, &kRequiredPhrases[i], 1));
    }

    free(docText);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_unclosed_string_fixture_surfaces_invalid_literal_node(void) {
    SZrTestTimer timer;
    const char* testSummary = "Reference Unclosed String Fixture Surfaces Invalid Literal Node";
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrAstNode* declaration = ZR_NULL;
    SZrAstNode* literal = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/literals/unclosed_string_fail.zr", &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_unclosed_string_fail.zr", 34);
    ast = ZrParser_Parse(state, source, fileSize, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);

    literal = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(literal);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, literal->type);
    TEST_ASSERT_NULL(literal->data.stringLiteral.value);

    ZrParser_Ast_Free(state, ast);
    free(source);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_reference_invalid_hex_escape_fixture_surfaces_invalid_literal_node(void) {
    SZrTestTimer timer;
    const char* testSummary = "Reference Invalid Hex Escape Fixture Surfaces Invalid Literal Node";
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrAstNode* declaration = ZR_NULL;
    SZrAstNode* literal = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/lexing_literals_diagnostics/invalid_hex_escape_fail.zr", &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_invalid_hex_escape_fail.zr", 38);
    ast = ZrParser_Parse(state, source, fileSize, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);

    literal = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(literal);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, literal->type);
    TEST_ASSERT_NULL(literal->data.stringLiteral.value);

    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_reference_multiline_literal_fixture_surfaces_invalid_literal_node(void) {
    SZrTestTimer timer;
    const char* testSummary = "Reference Multiline Literal Fixture Surfaces Invalid Literal Node";
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrAstNode* declaration = ZR_NULL;
    SZrAstNode* literal = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/lexing_literals_diagnostics/multiline_literal_reject_fail.zr",
                                 &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_multiline_literal_reject_fail.zr", 44);
    ast = ZrParser_Parse(state, source, fileSize, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);

    literal = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(literal);
    TEST_ASSERT_EQUAL_INT(ZR_AST_STRING_LITERAL, literal->type);
    TEST_ASSERT_NULL(literal->data.stringLiteral.value);

    ZrParser_Ast_Free(state, ast);
    free(source);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_reference_invalid_char_width_fixture_surfaces_invalid_char_literal_node(void) {
    SZrTestTimer timer;
    const char* testSummary = "Reference Invalid Char Width Fixture Surfaces Invalid Char Literal Node";
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrAstNode* ast = ZR_NULL;
    SZrAstNode* declaration = ZR_NULL;
    SZrAstNode* literal = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/lexing_literals_diagnostics/invalid_char_width_fail.zr",
                                 &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_invalid_char_width_fail.zr", 37);
    ast = ZrParser_Parse(state, source, fileSize, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_TRUE(ast->data.script.statements->count >= 1);

    declaration = ast->data.script.statements->nodes[0];
    TEST_ASSERT_NOT_NULL(declaration);
    TEST_ASSERT_EQUAL_INT(ZR_AST_VARIABLE_DECLARATION, declaration->type);

    literal = declaration->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(literal);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CHAR_LITERAL, literal->type);
    TEST_ASSERT_TRUE(literal->data.charLiteral.hasError);

    free(source);
    ZrParser_Ast_Free(state, ast);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_reference_const_reassign_fixture_is_rejected(void) {
    SZrTestTimer timer;
    const char* testSummary = "Reference Const Reassign Fixture Is Rejected";
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrFunction* function = ZR_NULL;

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/casts-and-const/const_reassign_fail.zr", &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, "reference_const_reassign_fail.zr", 33);
    function = ZrParser_Source_Compile(state, source, fileSize, sourceName);
    TEST_ASSERT_NULL(function);

    free(source);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

void test_reference_constructor_const_control_flow_fixture_matrix(void) {
    static const struct {
        const char* relativePath;
        TZrBool expectCompileSuccess;
    } kFixtures[] = {
        {"core_semantics/casts-and-const/const_else_if_paths_pass.zr", ZR_TRUE},
        {"core_semantics/casts-and-const/const_if_missing_branch_fail.zr", ZR_FALSE},
        {"core_semantics/casts-and-const/const_switch_paths_pass.zr", ZR_TRUE},
        {"core_semantics/casts-and-const/const_switch_missing_default_fail.zr", ZR_FALSE},
        {"core_semantics/casts-and-const/const_ternary_paths_pass.zr", ZR_TRUE},
        {"core_semantics/casts-and-const/const_ternary_missing_branch_fail.zr", ZR_FALSE},
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Constructor Const Control Flow Fixture Matrix";

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t index = 0; index < sizeof(kFixtures) / sizeof(kFixtures[0]); index++) {
        size_t fileSize = 0;
        char* source = ZR_NULL;
        SZrState* state = ZR_NULL;
        SZrString* sourceName = ZR_NULL;
        SZrFunction* function = ZR_NULL;

        state = create_test_state();
        TEST_ASSERT_NOT_NULL(state);

        source = read_reference_file(kFixtures[index].relativePath, &fileSize);
        TEST_ASSERT_NOT_NULL(source);

        sourceName = ZrCore_String_Create(state,
                                          (TZrNativeString)kFixtures[index].relativePath,
                                          (TZrSize)strlen(kFixtures[index].relativePath));
        TEST_ASSERT_NOT_NULL(sourceName);

        function = ZrParser_Source_Compile(state, source, fileSize, sourceName);
        if (kFixtures[index].expectCompileSuccess) {
            TEST_ASSERT_NOT_NULL(function);
        } else {
            TEST_ASSERT_NULL(function);
        }

        free(source);
        destroy_test_state(state);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_expressions_fixture_matrix(void) {
    static const struct {
        const char* relativePath;
        TZrInt64 expectedValue;
    } kPassFixtures[] = {
        {"core_semantics/expressions_precedence_chains/arithmetic_precedence_pass.zr", 19},
        {"core_semantics/expressions_precedence_chains/nested_conditional_grouping_pass.zr", 30},
        {"core_semantics/expressions_precedence_chains/member_index_call_chain_pass.zr", 11},
    };
    static const struct {
        const char* relativePath;
        const char* expectedMessageFragment;
    } kFailFixtures[] = {
        {"core_semantics/expressions_precedence_chains/missing_conditional_branch_fail.zr",
         "Expected primary expression"},
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Expressions Fixture Matrix";

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t index = 0; index < sizeof(kPassFixtures) / sizeof(kPassFixtures[0]); index++) {
        execute_reference_test_fixture_expect_int(kPassFixtures[index].relativePath,
                                                  kPassFixtures[index].expectedValue);
    }

    for (size_t index = 0; index < sizeof(kFailFixtures) / sizeof(kFailFixtures[0]); index++) {
        parse_reference_fixture_expect_diagnostic(kFailFixtures[index].relativePath,
                                                 kFailFixtures[index].expectedMessageFragment);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_types_casts_const_fixture_matrix(void) {
    static const struct {
        const char* relativePath;
        TZrInt64 expectedValue;
    } kPassFixtures[] = {
        {"core_semantics/types_casts_const/explicit_narrowing_boundary_pass.zr", 0},
        {"core_semantics/types_casts_const/cast_inside_default_expression_pass.zr", 13},
    };
    static const char* kFailFixtures[] = {
        "core_semantics/types_casts_const/implicit_narrowing_reject_fail.zr",
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Types Casts Const Fixture Matrix";

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t index = 0; index < sizeof(kPassFixtures) / sizeof(kPassFixtures[0]); index++) {
        execute_reference_test_fixture_expect_int(kPassFixtures[index].relativePath,
                                                  kPassFixtures[index].expectedValue);
    }

    for (size_t index = 0; index < sizeof(kFailFixtures) / sizeof(kFailFixtures[0]); index++) {
        compile_reference_fixture_expect_failure(kFailFixtures[index]);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}

void test_reference_construct_target_misuse_fixture_matrix(void) {
    static const char* kFailFixtures[] = {
        "core_semantics/object_member_index_construct_target/prototype_misuse_reject_fail.zr",
        "core_semantics/object_member_index_construct_target/new_misuse_reject_fail.zr",
    };
    SZrTestTimer timer;
    const char* testSummary = "Reference Construct Target Misuse Fixture Matrix";

    ZR_TEST_START(testSummary);
    timer.startTime = clock();

    for (size_t index = 0; index < sizeof(kFailFixtures) / sizeof(kFailFixtures[0]); index++) {
        compile_reference_fixture_expect_failure(kFailFixtures[index]);
    }

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    ZR_TEST_DIVIDER();
}
