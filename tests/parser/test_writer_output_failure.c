/* Linux 的 /dev/full 可以按写模式打开，但每次 write 都以 ENOSPC 失败。用它让 .zro/.zri/.zrs
 * writer 走到真实的刷新失败，不依赖磁盘配额或注入钩子。本文件只在 Linux 注册。 */
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

#define FULL_DEVICE "/dev/full"
/* 填充长度扫过一整个 stdio 缓冲区（glibc 对 /dev/full 取 4096），让输出末尾落在缓冲区的每个位置。 */
#define PADDING_LIMIT 4096u

static SZrState *state;
static SZrFunction *function;
static SZrAstNode *ast;
static char padding[PADDING_LIMIT + 1u];
static char sourceText[PADDING_LIMIT + 64u];

void setUp(void) {
    state = ZR_NULL;
    function = ZR_NULL;
    ast = ZR_NULL;
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
}

void tearDown(void) {
    if (state != ZR_NULL) {
        if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
        if (function != ZR_NULL) ZrCore_Function_Free(state, function);
    }
    ZrTests_Runtime_State_Destroy(state);
}

/* 先确认本机的 /dev/full 确实只在刷新时报错：缓冲写入成功、错误标志为空，fclose 才返回 EOF。 */
static void require_full_device(void) {
    static const char payload[] = "small buffered payload";
    FILE *file = fopen(FULL_DEVICE, "wb");
    size_t written;
    int errorBeforeClose;
    int closeResult;

    if (file == ZR_NULL) {
        TEST_IGNORE_MESSAGE(FULL_DEVICE " cannot be opened for writing in this environment");
    }
    written = fwrite(payload, 1u, sizeof(payload) - 1u, file);
    errorBeforeClose = ferror(file);
    closeResult = fclose(file);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload) - 1u, written);
    TEST_ASSERT_EQUAL_INT(0, errorBeforeClose);
    TEST_ASSERT_EQUAL_INT(EOF, closeResult);
}

static const char *padded_text(const char *prefix, size_t length, const char *suffix) {
    TEST_ASSERT_TRUE(length <= PADDING_LIMIT);
    memset(padding, 'a', length);
    padding[length] = '\0';
    TEST_ASSERT_TRUE(snprintf(sourceText, sizeof(sourceText), "%s%s%s", prefix, padding, suffix) > 0);
    return sourceText;
}

static void compile_source(const char *source) {
    function = ZrParser_Source_Compile(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "writer_output_failure.zr"));
    TEST_ASSERT_NOT_NULL(function);
}

static void parse_source(const char *source) {
    ast = ZrParser_Parse(state, source, strlen(source),
            ZrCore_String_CreateFromNative(state, "writer_output_failure.zr"));
    TEST_ASSERT_NOT_NULL(ast);
}

static void generated_path(const char *extension, char *path, size_t pathSize) {
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("writer_output_failure", "", "control", extension,
                                                       path, pathSize));
}

static void expect_failure(const char *writer, size_t length, TZrBool written) {
    char message[128];

    if (written) {
        snprintf(message, sizeof(message), "%s reported success on " FULL_DEVICE " with %lu padding bytes",
                 writer, (unsigned long)length);
        TEST_FAIL_MESSAGE(message);
    }
}

/* 小输出装得进 glibc 的一个缓冲区，唯一会失败的调用就是 fclose。普通路径上的对照写入排除了参数本身被拒绝。 */
static void test_binary_writer_reports_close_failure(void) {
    char path[ZR_TESTS_PATH_MAX];

    require_full_device();
    compile_source("return 42;");
    generated_path(".zro", path, sizeof(path));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(state, function, path));
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteBinaryFile(state, function, FULL_DEVICE));
}

static void test_intermediate_writer_reports_close_failure(void) {
    char path[ZR_TESTS_PATH_MAX];

    require_full_device();
    compile_source("return 42;");
    generated_path(".zri", path, sizeof(path));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, function, path));
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteIntermediateFile(state, function, FULL_DEVICE));
}

static void test_syntax_tree_writer_reports_close_failure(void) {
    char path[ZR_TESTS_PATH_MAX];

    require_full_device();
    parse_source("var text = \"a\";");
    generated_path(".zrs", path, sizeof(path));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteSyntaxTreeFile(state, ast, path));
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteSyntaxTreeFile(state, ast, FULL_DEVICE));
}

/* 输出越过缓冲区后，失败发生在中途的刷新上：stdio 丢掉那一块并置错误标志。若最后一次写入正好触发
 * 这次刷新，关闭时已无待写数据，fclose 返回 0。逐字节改变输出长度，要求每个对齐下都报告失败。 */
static void test_binary_writer_reports_flush_failure_at_every_alignment(void) {
    SZrBinaryWriterOptions options = {padding, "writer-output-failure"};

    require_full_device();
    compile_source("return 42;");
    for (size_t length = 1u; length <= PADDING_LIMIT; ++length) {
        memset(padding, 'm', length);
        padding[length] = '\0';
        expect_failure("binary writer", length,
                       ZrParser_Writer_WriteBinaryFileWithOptions(state, function, FULL_DEVICE, &options));
    }
}

static void test_intermediate_writer_reports_flush_failure_at_every_alignment(void) {
    require_full_device();
    for (size_t length = 0u; length <= PADDING_LIMIT; ++length) {
        TZrBool written;

        compile_source(padded_text("return \"", length, "\";"));
        written = ZrParser_Writer_WriteIntermediateFile(state, function, FULL_DEVICE);
        ZrCore_Function_Free(state, function);
        function = ZR_NULL;
        expect_failure("intermediate writer", length, written);
    }
}

static void test_syntax_tree_writer_reports_flush_failure_at_every_alignment(void) {
    require_full_device();
    for (size_t length = 0u; length <= PADDING_LIMIT; ++length) {
        TZrBool written;

        parse_source(padded_text("var text = \"", length, "\";"));
        written = ZrParser_Writer_WriteSyntaxTreeFile(state, ast, FULL_DEVICE);
        ZrParser_Ast_Free(state, ast);
        ast = ZR_NULL;
        expect_failure("syntax tree writer", length, written);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binary_writer_reports_close_failure);
    RUN_TEST(test_intermediate_writer_reports_close_failure);
    RUN_TEST(test_syntax_tree_writer_reports_close_failure);
    RUN_TEST(test_binary_writer_reports_flush_failure_at_every_alignment);
    RUN_TEST(test_intermediate_writer_reports_flush_failure_at_every_alignment);
    RUN_TEST(test_syntax_tree_writer_reports_flush_failure_at_every_alignment);
    return UNITY_END();
}
