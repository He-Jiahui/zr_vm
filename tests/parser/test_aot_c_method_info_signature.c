#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

static SZrFunction *compile_source(SZrState *state, const char *source, const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static char *read_text_file_owned_or_fail(const TZrChar *path) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize byteLength = 0u;
    char *text;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &byteLength));
    TEST_ASSERT_NOT_NULL(bytes);

    text = (char *)malloc(byteLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    memcpy(text, bytes, byteLength);
    text[byteLength] = '\0';
    free(bytes);
    return text;
}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
}

static void assert_text_does_not_contain(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NULL_MESSAGE(strstr(text, needle), needle);
}

static void assert_signature_scalar_return(const char *generatedCText,
                                           EZrValueType expectedBaseType,
                                           EZrStaticCType expectedStaticCType,
                                           const char *expectedReturnHelper) {
    char expectedBaseTypeText[96];
    char expectedStaticCTypeText[96];

    snprintf(expectedBaseTypeText,
             sizeof(expectedBaseTypeText),
             "        .baseType = (TZrUInt16)%uu,",
             (unsigned)expectedBaseType);
    snprintf(expectedStaticCTypeText,
             sizeof(expectedStaticCTypeText),
             "        .staticCType = (TZrUInt16)%uu,",
             (unsigned)expectedStaticCType);

    assert_text_contains(generatedCText, "static const SZrAotSignature zr_aot_signature_0 = {");
    assert_text_contains(generatedCText, "    .returnType = &zr_aot_signature_0_types[0],");
    assert_text_contains(generatedCText, "    .hasReturnValue = (TZrUInt8)1u,");
    assert_text_contains(generatedCText, expectedBaseTypeText);
    assert_text_contains(generatedCText, expectedStaticCTypeText);
    assert_text_contains(generatedCText, expectedReturnHelper);
    assert_text_does_not_contain(generatedCText, "ZrLibrary_AotRuntime_Return(state, &frame,");
}

static void assert_script_return_signature(const char *caseName,
                                           const char *source,
                                           EZrValueType expectedBaseType,
                                           EZrStaticCType expectedStaticCType,
                                           const char *expectedReturnHelper) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "method_info_signature.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_method_info_signature",
                                                       caseName,
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));

    memset(&options, 0, sizeof(options));
    options.moduleName = caseName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_signature_scalar_return(generatedCText, expectedBaseType, expectedStaticCType, expectedReturnHelper);

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void assert_script_return_signature_without(const char *caseName,
                                                   const char *source,
                                                   EZrValueType expectedBaseType,
                                                   EZrStaticCType expectedStaticCType,
                                                   const char *expectedReturnHelper,
                                                   const char *forbiddenNeedle,
                                                   const char *secondForbiddenNeedle) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "method_info_signature.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_method_info_signature",
                                                       caseName,
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));

    memset(&options, 0, sizeof(options));
    options.moduleName = caseName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_signature_scalar_return(generatedCText, expectedBaseType, expectedStaticCType, expectedReturnHelper);
    assert_text_does_not_contain(generatedCText, forbiddenNeedle);
    assert_text_does_not_contain(generatedCText, secondForbiddenNeedle);

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_method_info_infers_bool_u64_f64_script_return_signatures(void) {
    assert_script_return_signature("bool",
                                   "var left: int = 7;\n"
                                   "var right: int = 5;\n"
                                   "var result: bool = left > right;\n"
                                   "return result;\n",
                                   ZR_VALUE_TYPE_BOOL,
                                   ZR_STATIC_C_TYPE_BOOL,
                                   "ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b");
    assert_script_return_signature("u64",
                                   "var left: uint = 7;\n"
                                   "var right: uint = 5;\n"
                                   "var result: uint = left + right;\n"
                                   "return result;\n",
                                   ZR_VALUE_TYPE_UINT64,
                                   ZR_STATIC_C_TYPE_U64,
                                   "ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u");
    assert_script_return_signature("f64",
                                   "var left: float = 1.25;\n"
                                   "var right: float = 2.5;\n"
                                   "var result: float = left + right;\n"
                                   "return result;\n",
                                   ZR_VALUE_TYPE_DOUBLE,
                                   ZR_STATIC_C_TYPE_F64,
                                   "ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f");
    assert_script_return_signature("bool_expr",
                                   "var left: int = 7;\n"
                                   "var right: int = 5;\n"
                                   "return left > right;\n",
                                   ZR_VALUE_TYPE_BOOL,
                                   ZR_STATIC_C_TYPE_BOOL,
                                   "ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b");
    assert_script_return_signature("u64_expr",
                                   "var left: uint = 7;\n"
                                   "var right: uint = 5;\n"
                                   "return left + right;\n",
                                   ZR_VALUE_TYPE_UINT64,
                                   ZR_STATIC_C_TYPE_U64,
                                   "ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u");
    assert_script_return_signature("f64_expr",
                                   "var left: float = 1.25;\n"
                                   "var right: float = 2.5;\n"
                                   "return left + right;\n",
                                   ZR_VALUE_TYPE_DOUBLE,
                                   ZR_STATIC_C_TYPE_F64,
                                   "ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f");
    assert_script_return_signature("f64_bool_expr",
                                   "var left: float = 2.5;\n"
                                   "var right: float = 1.25;\n"
                                   "return left > right;\n",
                                   ZR_VALUE_TYPE_BOOL,
                                   ZR_STATIC_C_TYPE_BOOL,
                                   "ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b");
    assert_script_return_signature("f64_neg_expr",
                                   "var value: float = 1.25;\n"
                                   "return -value;\n",
                                   ZR_VALUE_TYPE_DOUBLE,
                                   ZR_STATIC_C_TYPE_F64,
                                   "ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f");
    assert_script_return_signature_without("i64_neg_expr",
                                           "var value: int = 7;\n"
                                           "return -value;\n",
                                           ZR_VALUE_TYPE_INT64,
                                           ZR_STATIC_C_TYPE_I64,
                                           "ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s",
                                           "zr_aot_arith_exec_signed_unary",
                                           "SZrTypeValue *zr_aot_destination");
    assert_script_return_signature_without("i64_bit_not_expr",
                                           "var value: int = 7;\n"
                                           "return ~value;\n",
                                           ZR_VALUE_TYPE_INT64,
                                           ZR_STATIC_C_TYPE_I64,
                                           "ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s",
                                           "zr_aot_bitwise_exec_unary",
                                           "SZrTypeValue *zr_aot_destination");
    assert_script_return_signature_without("i64_bitwise_and_expr",
                                           "var left: int = 58;\n"
                                           "var right: int = 47;\n"
                                           "return left & right;\n",
                                           ZR_VALUE_TYPE_INT64,
                                           ZR_STATIC_C_TYPE_I64,
                                           "ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s",
                                           "zr_aot_bitwise_exec_binary",
                                           "SZrTypeValue *zr_aot_destination");
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_method_info_infers_bool_u64_f64_script_return_signatures);
    return UNITY_END();
}
