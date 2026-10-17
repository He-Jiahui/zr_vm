// 字符与 cast 回归同时读取参考语言 fixture、清单及测试矩阵文档。
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

/* 将 tests 源目录中的相对路径读成拥有型缓冲；调用方负责 free。 */
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

/* TODO: 这里只按字段名子串计数，尚不能证明 JSON 结构及每条 case 的字段配对；需核对清单消费者的解析测试。 */
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

/* 从参考 fixture 执行到整数结果；成功路径按函数、源码、VM state 顺序释放。 */
/**
 * ## Summary
 * 完成参考 fixture 的源码编译、整数执行和期望值比较，将矩阵中的一行交付为当前 Unity 用例内的断言。
 *
 * ## Parameters
 * @param[in] relativePath 参考 fixture 根下的相对路径；在同步读取及源名构造期间借用。
 * @param[in] expectedValue 该 fixture 按本矩阵应交付的整数结果。
 *
 * ## Returns
 * 没有返回值；编译、执行及结果比较由硬断言交付，失败会直接跳回当前 runner。
 *
 * ## Constraints and lifecycle
 * 普通完成路径释放 Function、源码和 State；任何资源取得后的硬断言都可能跳过这些释放，不能依赖空 tearDown 回收。
 *
 * ## References
 * test_reference_expressions_fixture_matrix 与 test_reference_types_casts_const_fixture_matrix 借此验收整数结果；ZrTests_Function_ExecuteExpectInt64 提供执行及整数结果检查。
 */
static void execute_reference_test_fixture_expect_int(const char* relativePath, TZrInt64 expectedValue) {
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrFunction* function = ZR_NULL;
    TZrInt64 actualValue = 0;

    /* BUG: 成功取得 State 或源码后的 Unity 硬断言会跳过本 helper 的正常清理；调用方同属空 tearDown 目标，资源保留至进程退出。需建立失败也可达的释放入口。 */
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file(relativePath, &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)relativePath, (TZrSize)strlen(relativePath));
    TEST_ASSERT_NOT_NULL(sourceName);

    function = ZrParser_Source_Compile(state, source, fileSize, sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, function, &actualValue));
    TEST_ASSERT_EQUAL_INT64(expectedValue, actualValue);

    ZrCore_Function_Free(state, function);
    free(source);
    destroy_test_state(state);
}

/**
 * ## Summary
 * 读取并编译指定负例，以 Function 为空表示本矩阵预期的编译拒绝，供构造目标及隐式窄化回归共同使用。
 *
 * ## Parameters
 * @param[in] relativePath 参考 fixture 根下的相对路径；同步调用期间借用，不向 helper 转移字符串所有权。
 *
 * ## Returns
 * 没有返回值；Function 非 NULL 会触发 Unity 硬断言。只检查编译拒绝，不证明指定诊断或拒绝原因。
 *
 * ## Constraints and lifecycle
 * 正常路径 free 源码并销毁 State；硬断言失败会跳过尾部，相关资源所有权不能由调用方日志宏补偿。
 *
 * ## References
 * test_reference_types_casts_const_fixture_matrix 与 test_reference_construct_target_misuse_fixture_matrix 借此验收编译拒绝；ZrParser_Source_Compile 提供被检查的 Function 结果。
 */
static void compile_reference_fixture_expect_failure(const char* relativePath) {
    size_t fileSize = 0;
    char* source = ZR_NULL;
    SZrState* state = ZR_NULL;
    SZrString* sourceName = ZR_NULL;
    SZrFunction* function = ZR_NULL;

    /* BUG: 成功取得 State 或源码后的 Unity 硬断言会跳过本 helper 的正常清理；调用方同属空 tearDown 目标，资源保留至进程退出。需建立失败也可达的释放入口。 */
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

/* 用局部 parser state 捕获指定诊断；AST 与 parser state 均在成功断言后释放。 */
/**
 * ## Summary
 * 在局部 parser state 捕获负例诊断，要求至少出现错误信号，并按调用方给定片段核对诊断消息。
 *
 * ## Parameters
 * @param[in] relativePath 参考 fixture 的相对路径；同步读取期间借用。
 * @param[in] expectedMessageFragment 为 NULL 时省略片段匹配；非 NULL 时只要求消息包含此片段，调用期间借用。
 *
 * ## Returns
 * 没有返回值；错误信号及适用的消息片段由 Unity 硬断言交付，不能由 AST 是否存在单独判断源码合法性。
 *
 * ## Constraints and lifecycle
 * 诊断回调只借用本次栈上的 diagnostic；正常路径释放 AST、parser state、源码及 State。硬断言可能越过全部尾部清理。
 *
 * ## References
 * test_reference_expressions_fixture_matrix 借此验收负例诊断；capture_reference_parser_error 只借用本次局部诊断存储，ZrParser_State_Free 负责正常结束后的 parser 状态清理。
 */
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

    /* BUG: 成功取得 State 或源码后的 Unity 硬断言会跳过本 helper 的正常清理；调用方同属空 tearDown 目标，资源保留至进程退出。需建立失败也可达的释放入口。 */
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
/**
 * ## Summary
 * 解析字符及转义 fixture，检查脚本根和语句容器存在，提供字符字面量解析的基本存活检查。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。只断言脚本根及语句容器，不核对每个字符值或转义解码结果。
 *
 * ## Constraints and lifecycle
 * PASS/FAIL 前记录结束时刻；State 销毁在该日志之后，耗时不包括这一销毁阶段。 本例拥有 State 与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
void test_char_literals_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Character Literals Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    /* BUG: 取得 State 与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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
    
    SZrString* sourceName = ZrCore_String_CreateFromNative(state, "test_char_literals.zr");
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
/**
 * ## Summary
 * 在字符 fixture 的解析结果上构建 Function，提供从字符语法到编译入口的基本成功检查。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。Function 非 NULL 即接受编译结果；本例没有执行字符值或检查具体转换指令。
 *
 * ## Constraints and lifecycle
 * PASS/FAIL 前记录结束时刻；State 销毁在该日志之后，耗时不包括这一销毁阶段。 输入摘要为稳定的本地字面量；配套 helper 或普通返回分支负责资源，日志宏没有释放职责。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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
    
    SZrString* sourceName = ZrCore_String_CreateFromNative(state, "test_char_literals.zr");
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
/**
 * ## Summary
 * 解析基本类型转换 fixture，检查该语法面能够形成脚本根和语句容器。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。不逐项检查 cast 节点种类、目标类型或执行结果。
 *
 * ## Constraints and lifecycle
 * PASS/FAIL 前记录结束时刻；State 销毁在该日志之后，耗时不包括这一销毁阶段。 本例拥有 State 与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
void test_type_cast_basic_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Basic Type Cast Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    /* BUG: 取得 State 与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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
    
    SZrString* sourceName = ZrCore_String_CreateFromNative(state, "test_type_cast_basic.zr");
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
/**
 * ## Summary
 * 把基本类型转换 fixture 的 AST 送入编译层，检查编译入口能够产生 Function。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。Function 非 NULL 是通过条件；本例不验证转换指令或转换值。
 *
 * ## Constraints and lifecycle
 * PASS/FAIL 前记录结束时刻；State 销毁在该日志之后，耗时不包括这一销毁阶段。 输入摘要为稳定的本地字面量；配套 helper 或普通返回分支负责资源，日志宏没有释放职责。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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
    
    SZrString* sourceName = ZrCore_String_CreateFromNative(state, "test_type_cast_basic.zr");
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
/**
 * ## Summary
 * 解析 struct 转换 fixture，为结构类型转换语法提供脚本构造检查。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。只检查脚本根及语句容器，不证明结构转换的编译或运行语义。
 *
 * ## Constraints and lifecycle
 * PASS/FAIL 前记录结束时刻；State 销毁在该日志之后，耗时不包括这一销毁阶段。 本例拥有 State 与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
void test_type_cast_struct_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Struct Type Cast Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    /* BUG: 取得 State 与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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
    
    SZrString* sourceName = ZrCore_String_CreateFromNative(state, "test_type_cast_struct.zr");
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
/**
 * ## Summary
 * 解析 class 转换 fixture，为类类型转换语法提供脚本构造检查。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。只检查脚本根及语句容器，不证明类转换的编译或运行语义。
 *
 * ## Constraints and lifecycle
 * PASS/FAIL 前记录结束时刻；State 销毁在该日志之后，耗时不包括这一销毁阶段。 本例拥有 State 与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
void test_type_cast_class_parsing(void) {
    SZrTestTimer timer;
    const char* testSummary = "Class Type Cast Parsing";
    
    ZR_TEST_START(testSummary);
    timer.startTime = clock();
    
    /* BUG: 取得 State 与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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
    
    SZrString* sourceName = ZrCore_String_CreateFromNative(state, "test_type_cast_class.zr");
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

/**
 * ## Summary
 * 在关系表达式 left < right 中捕获 parser 诊断，防止尝试解析 cast 时把回退分支诊断泄露给真实关系表达式。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。检查 AST 存在、parser 无错误且回调未报告诊断，不执行布尔比较结果。
 *
 * ## Constraints and lifecycle
 * 正常路径释放 AST、parser state 和 State 后结束计时。 本例拥有 State、parser state 与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

    /* BUG: 取得 State、parser state 与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 核对六组核心语义 manifest 的基础文本标记与总 case 数，防止参考用例清单的入口被移除。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。字段名子串及 id 次数不等于逐 case 的 JSON 结构验证。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 本例拥有 当前迭代的 manifest 文本，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；read_reference_file 交付拥有型清单文本；assert_reference_manifest_shape 限定本例的字段子串与最低 case 数检查。
 */
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
        /* BUG: 取得 当前迭代的 manifest 文本 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 核对十个领域的参考 manifest 标记、case kind 次数和最低 case 总数，保留全栈测试库存的可见入口。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。部分检查按子串计数；这些统计不证明 JSON 可解析、字段属于同一 case 或所有 fixture 已执行。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 本例拥有 当前迭代的 manifest 文本，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；read_reference_file 交付拥有型清单文本；ZrTests_Reference_AssertManifestShape 和 ZrTests_Reference_AssertCaseKindsCovered 提供本例的形状及 case kind 检查。
 */
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
        /* BUG: 取得 当前迭代的 manifest 文本 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 在十份 manifest 文本中寻找优先回归 case id，防止约定的关键测试场景从库存中消失。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。当前只查 id 子串存在，不验证唯一性、所属 JSON 字段或目标 fixture 的运行结果。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 本例拥有 各槽位中已读取的 manifest 文本缓冲，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；read_reference_file 交付拥有型清单文本；本例使用 strstr 查 id 子串，最后逐槽位 free 文本，栈上的指针数组不交给 free。
 */
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
        /* BUG: 取得 各槽位中已读取的 manifest 文本缓冲 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 核对全栈参考矩阵文档及规定术语存在，避免规划入口失联或丢失基本领域与层级说明。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。文本包含检查不校验文档与实际测试覆盖的一致性。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 本例拥有 矩阵文档文本，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；read_repo_doc_file 交付拥有型文档文本；ZrTests_Reference_TextContainsAll 仅检查本例列出的文本标记。
 */
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

    /* BUG: 取得 矩阵文档文本 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 核对执行顺序文档中保留当前 suite 与 tier 入口词，供测试执行约定的基础文档回归使用。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。这些文本标记不证明 CTest 注册、排序或 tier 行为本身正确。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 本例拥有 执行顺序文档文本，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；read_tests_repo_file 交付拥有型执行顺序文档文本；ZrTests_Reference_TextContainsAll 仅检查本例列出的术语。
 */
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

    /* BUG: 取得 执行顺序文档文本 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 核对核心语义能力矩阵中的领域、决策与控制流 case 标记，保持参考测试规划文档的基础形状。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。仅检查所列短语存在；不据此认定各语言能力或案例已经验收。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 本例拥有 核心矩阵文档文本，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；read_repo_doc_file 交付拥有型文档文本；ZrTests_Reference_TextContainsAll 仅检查本例列出的文本标记。
 */
void test_reference_core_semantics_matrix_document_exists(void) {
    static const char* kRequiredPhrases[] = {
        "ZR 核心语义 capability matrix",
        "字面量与转义",
        "表达式与优先级",
        "`module` / `import(...)` 与成员链",
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

    /* BUG: 取得 核心矩阵文档文本 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 解析未闭合字符串负例，要求错误恢复仍提供值为空的字符串字面量节点，供后续层识别无效字面量。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。检查恢复后的节点形状与空值；不把非 NULL AST 当作合法源码接受。
 *
 * ## Constraints and lifecycle
 * 正常路径在源码及适用的 AST 释放后采样并打印 PASS，随后才销毁 State；耗时不包含该 State 销毁。 本例拥有 State、源码缓冲与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

    /* BUG: 取得 State、源码缓冲与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/literals/unclosed_string_fail.zr", &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "reference_unclosed_string_fail.zr");
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

/**
 * ## Summary
 * 解析非法十六进制转义负例，检查错误恢复产出的字符串字面量保留无效值标记。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。检查字符串节点值为空。
 *
 * ## Constraints and lifecycle
 * 正常路径在源码及适用的 AST 释放后采样并打印 PASS，随后才销毁 State；耗时不包含该 State 销毁。 本例拥有 State、源码缓冲与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

    /* BUG: 取得 State、源码缓冲与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/lexing_literals_diagnostics/invalid_hex_escape_fail.zr", &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "reference_invalid_hex_escape_fail.zr");
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

/**
 * ## Summary
 * 解析被拒绝的多行字面量负例，检查 parser 以值为空的字符串字面量表达恢复结果。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。不执行该负例。
 *
 * ## Constraints and lifecycle
 * 正常路径在源码及适用的 AST 释放后采样并打印 PASS，随后才销毁 State；耗时不包含该 State 销毁。 本例拥有 State、源码缓冲与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

    /* BUG: 取得 State、源码缓冲与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/lexing_literals_diagnostics/multiline_literal_reject_fail.zr",
                                 &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "reference_multiline_literal_reject_fail.zr");
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

/**
 * ## Summary
 * 解析非法宽度字符负例，要求恢复后的字符字面量节点携带 hasError，防止无效字符被当作正常值继续处理。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。检查节点种类及错误标志，不检查所有字符编码边界。
 *
 * ## Constraints and lifecycle
 * 正常路径在源码及适用的 AST 释放后采样并打印 PASS，随后才销毁 State；耗时不包含该 State 销毁。 本例拥有 State、源码缓冲与 AST，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

    /* BUG: 取得 State、源码缓冲与 AST 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/lexing_literals_diagnostics/invalid_char_width_fail.zr",
                                 &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "reference_invalid_char_width_fail.zr");
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

/**
 * ## Summary
 * 把 const 重赋值负例送入源码编译入口，要求它拒绝产生可执行 Function。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。只检查编译返回 NULL，未核对失败是否来自指定 const 规则或诊断。
 *
 * ## Constraints and lifecycle
 * 正常路径在源码及适用的 AST 释放后采样并打印 PASS，随后才销毁 State；耗时不包含该 State 销毁。 本例拥有 State 与源码缓冲，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

    /* BUG: 取得 State 与源码缓冲 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    source = read_reference_file("core_semantics/casts-and-const/const_reassign_fail.zr", &fileSize);
    TEST_ASSERT_NOT_NULL(source);

    sourceName = ZrCore_String_CreateFromNative(state, "reference_const_reassign_fail.zr");
    function = ZrParser_Source_Compile(state, source, fileSize, sourceName);
    TEST_ASSERT_NULL(function);

    free(source);

    timer.endTime = clock();
    ZR_TEST_PASS(timer, testSummary);
    destroy_test_state(state);
    ZR_TEST_DIVIDER();
}

/**
 * ## Summary
 * 逐个编译构造器 const 的分支覆盖矩阵，检查给定的完整与缺失赋值路径是否符合各自编译成功预期。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。各行比较 Function 是否为空，不执行构造器，也未核对失败诊断的来源。
 *
 * ## Constraints and lifecycle
 * 结束采样覆盖整个矩阵，并发生在各迭代 State 销毁之后。 本例拥有 当前迭代的 State 与源码缓冲，正常路径手工释放；后续硬断言可能跳过释放，空 tearDown 不负责兜底，见相应 BUG。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；create_test_state/destroy_test_state 管理本例 VM 状态；ZR_TEST_PASS 提供调用方已结束采样的日志。
 */
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

        /* BUG: 取得 当前迭代的 State 与源码缓冲 后，其释放只在正常尾部；后续 Unity 硬断言失败会 longjmp，且本目标 tearDown 为空，分配保留至进程退出。需给失败路径安排明确的所有权回收。 */
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

/**
 * ## Summary
 * 经参考支持 helper 验证三份表达式 fixture 的整数结果，并为缺失条件分支负例核对指定诊断片段。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。执行及诊断的验收由各 helper 的 Unity 断言交付；资源与非局部失败边界见 helper 的 BUG。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 输入摘要为稳定的本地字面量；配套 helper 或普通返回分支负责资源，日志宏没有释放职责。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；execute_reference_test_fixture_expect_int 验收正例整数结果；parse_reference_fixture_expect_diagnostic 验收缺失条件分支的诊断片段。
 */
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
         "Missing expression after ':' in conditional expression"},
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

/**
 * ## Summary
 * 执行显式窄化与默认参数表达式正例，并经编译 helper 检查隐式窄化负例被拒绝。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。正例核对整数结果，负例只要求 Function 为空；清理边界见配套 helper。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 输入摘要为稳定的本地字面量；配套 helper 或普通返回分支负责资源，日志宏没有释放职责。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；execute_reference_test_fixture_expect_int 验收正例整数结果；compile_reference_fixture_expect_failure 验收隐式窄化的编译拒绝。
 */
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

/**
 * ## Summary
 * 经编译 helper 检查 prototype 与 new 构造目标误用负例，保留构造目标拒绝路径的入口回归。
 *
 * ## Returns
 * 本例通过 Unity 断言或显式失败分支交付判定；到达 PASS 只表明已执行至该日志位置。只要求编译没有 Function；不核对具体诊断，因此不能排除其他编译错误使负例满足判据。
 *
 * ## Constraints and lifecycle
 * 结束采样发生在本例文本释放或全部 helper 调用之后，PASS 只输出该片段耗时。 输入摘要为稳定的本地字面量；配套 helper 或普通返回分支负责资源，日志宏没有释放职责。
 *
 * ## References
 * test_literal_surface_main.c 中的 main 以 RUN_TEST 注册本例；compile_reference_fixture_expect_failure 交付本例两个构造目标误用 fixture 的编译拒绝断言。
 */
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
