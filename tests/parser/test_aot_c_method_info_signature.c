#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
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

static void init_signature_type_ref(SZrFunctionTypedTypeRef *typeRef,
                                    EZrValueType baseType,
                                    EZrStaticCType staticCType) {
    TEST_ASSERT_NOT_NULL(typeRef);
    memset(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = baseType;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->staticCType = staticCType;
    typeRef->staticCTypeId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
}

static const char *find_signature_type_row(const char *signatureTypes,
                                           TZrUInt32 rowIndex) {
    const char *row = signatureTypes;

    for (TZrUInt32 index = 0u; index <= rowIndex; index++) {
        row = row != ZR_NULL ? strstr(row, "    {\n") : ZR_NULL;
        if (row == ZR_NULL) {
            return ZR_NULL;
        }
        if (index < rowIndex) {
            row += strlen("    {\n");
        }
    }
    return row;
}

static void assert_signature_type_row(const char *signatureTypes,
                                      TZrUInt32 rowIndex,
                                      EZrValueType expectedBaseType,
                                      EZrStaticCType expectedStaticCType) {
    const char *row = find_signature_type_row(signatureTypes, rowIndex);
    const char *rowEnd;
    const char *baseType;
    const char *staticCType;
    char baseTypeText[96];
    char staticCTypeText[96];

    TEST_ASSERT_NOT_NULL(row);
    rowEnd = strstr(row, "    },\n");
    TEST_ASSERT_NOT_NULL(rowEnd);
    snprintf(baseTypeText,
             sizeof(baseTypeText),
             "        .baseType = (TZrUInt16)%uu,",
             (unsigned)expectedBaseType);
    snprintf(staticCTypeText,
             sizeof(staticCTypeText),
             "        .staticCType = (TZrUInt16)%uu,",
             (unsigned)expectedStaticCType);
    baseType = strstr(row, baseTypeText);
    staticCType = strstr(row, staticCTypeText);
    TEST_ASSERT_NOT_NULL(baseType);
    TEST_ASSERT_NOT_NULL(staticCType);
    TEST_ASSERT_TRUE(baseType < rowEnd);
    TEST_ASSERT_TRUE(staticCType < rowEnd);
}

static void test_aot_c_method_info_aligns_receiver_and_explicit_parameter_types(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *root;
    SZrFunction *method;
    SZrFunctionTypedLocalBinding *bindings;
    SZrFunctionMetadataParameter *metadata;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    char *signatureTypes;
    char *signatureDescriptor;
    char *receiverBaseType;
    char *receiverStaticType;
    char *explicitBaseType;
    char *explicitStaticType;
    char receiverBaseTypeText[96];
    char receiverStaticTypeText[96];
    char explicitBaseTypeText[96];
    char explicitStaticTypeText[96];

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);
    root->stackSize = 1u;
    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction));
    root->childFunctionLength = 1u;

    method = &root->childFunctionList[0];
    method->ownerFunction = root;
    method->parameterCount = 2u;
    method->stackSize = 2u;
    method->functionName = ZrCore_String_CreateFromNative(state, "pass");
    TEST_ASSERT_NOT_NULL(method->functionName);

    bindings = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionTypedLocalBinding) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(bindings);
    memset(bindings, 0, sizeof(SZrFunctionTypedLocalBinding) * 2u);
    bindings[0].name = ZrCore_String_CreateFromNative(state, "this");
    bindings[0].stackSlot = 0u;
    bindings[0].symbolId = 11u;
    bindings[0].typeId = 12u;
    bindings[0].placeId = 13u;
    bindings[0].roleFlags = ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER;
    init_signature_type_ref(
            &bindings[0].type, ZR_VALUE_TYPE_OBJECT, ZR_STATIC_C_TYPE_GC_REF);
    bindings[1].name = ZrCore_String_CreateFromNative(state, "value");
    bindings[1].stackSlot = 1u;
    bindings[1].symbolId = 21u;
    bindings[1].typeId = 22u;
    bindings[1].placeId = 23u;
    init_signature_type_ref(
            &bindings[1].type, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    TEST_ASSERT_NOT_NULL(bindings[0].name);
    TEST_ASSERT_NOT_NULL(bindings[1].name);
    method->typedLocalBindings = bindings;
    method->typedLocalBindingLength = 2u;

    metadata = (SZrFunctionMetadataParameter *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionMetadataParameter),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(metadata);
    memset(metadata, 0, sizeof(SZrFunctionMetadataParameter));
    metadata[0].name = bindings[1].name;
    init_signature_type_ref(
            &metadata[0].type, ZR_VALUE_TYPE_UINT64, ZR_STATIC_C_TYPE_U64);
    method->parameterMetadata = metadata;
    method->parameterMetadataCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_method_info_signature",
            "receiver_parameter_alignment",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_method_info_receiver_parameter_alignment";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, root, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    signatureTypes = strstr(
            generatedCText, "static const SZrAotSignatureType zr_aot_signature_1_types[] = {");
    signatureDescriptor = strstr(
            generatedCText, "static const SZrAotSignature zr_aot_signature_1 = {");
    TEST_ASSERT_NOT_NULL(signatureTypes);
    TEST_ASSERT_NOT_NULL(signatureDescriptor);
    TEST_ASSERT_TRUE(signatureTypes < signatureDescriptor);
    snprintf(receiverBaseTypeText,
             sizeof(receiverBaseTypeText),
             "        .baseType = (TZrUInt16)%uu,",
             (unsigned)ZR_VALUE_TYPE_OBJECT);
    snprintf(receiverStaticTypeText,
             sizeof(receiverStaticTypeText),
             "        .staticCType = (TZrUInt16)%uu,",
             (unsigned)ZR_STATIC_C_TYPE_GC_REF);
    snprintf(explicitBaseTypeText,
             sizeof(explicitBaseTypeText),
             "        .baseType = (TZrUInt16)%uu,",
             (unsigned)ZR_VALUE_TYPE_INT64);
    snprintf(explicitStaticTypeText,
             sizeof(explicitStaticTypeText),
             "        .staticCType = (TZrUInt16)%uu,",
             (unsigned)ZR_STATIC_C_TYPE_I64);
    receiverBaseType = strstr(signatureTypes, receiverBaseTypeText);
    receiverStaticType = receiverBaseType != ZR_NULL
                                 ? strstr(receiverBaseType, receiverStaticTypeText)
                                 : ZR_NULL;
    explicitBaseType = receiverStaticType != ZR_NULL
                               ? strstr(receiverStaticType, explicitBaseTypeText)
                               : ZR_NULL;
    explicitStaticType = explicitBaseType != ZR_NULL
                                 ? strstr(explicitBaseType, explicitStaticTypeText)
                                 : ZR_NULL;
    TEST_ASSERT_NOT_NULL(receiverBaseType);
    TEST_ASSERT_NOT_NULL(receiverStaticType);
    TEST_ASSERT_NOT_NULL(explicitBaseType);
    TEST_ASSERT_NOT_NULL(explicitStaticType);
    TEST_ASSERT_TRUE(explicitStaticType < signatureDescriptor);
    assert_text_contains(signatureDescriptor, "    .parameterCount = 2u,");

    free(generatedCText);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_method_info_leaves_ambiguous_legacy_parameter_types_unknown(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *root;
    SZrFunction *method;
    SZrFunctionMetadataParameter *metadata;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    const char *signatureTypes;
    const char *signatureDescriptor;

    TEST_ASSERT_NOT_NULL(state);
    root = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(root);
    root->stackSize = 1u;
    root->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(root->childFunctionList);
    memset(root->childFunctionList, 0, sizeof(SZrFunction));
    root->childFunctionLength = 1u;

    method = &root->childFunctionList[0];
    method->ownerFunction = root;
    method->parameterCount = 2u;
    method->stackSize = 2u;
    method->functionName = ZrCore_String_CreateFromNative(state, "legacyPass");
    TEST_ASSERT_NOT_NULL(method->functionName);

    metadata = (SZrFunctionMetadataParameter *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunctionMetadataParameter),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(metadata);
    memset(metadata, 0, sizeof(SZrFunctionMetadataParameter));
    init_signature_type_ref(
            &metadata[0].type, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    method->parameterMetadata = metadata;
    method->parameterMetadataCount = 1u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_method_info_signature",
            "ambiguous_legacy_parameter_alignment",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_method_info_ambiguous_legacy_parameter_alignment";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, root, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    signatureTypes = strstr(
            generatedCText, "static const SZrAotSignatureType zr_aot_signature_1_types[] = {");
    signatureDescriptor = strstr(
            generatedCText, "static const SZrAotSignature zr_aot_signature_1 = {");
    TEST_ASSERT_NOT_NULL(signatureTypes);
    TEST_ASSERT_NOT_NULL(signatureDescriptor);
    assert_signature_type_row(
            signatureTypes, 1u, (EZrValueType)0u, (EZrStaticCType)0u);
    assert_signature_type_row(
            signatureTypes, 2u, (EZrValueType)0u, (EZrStaticCType)0u);
    assert_text_contains(signatureDescriptor, "    .parameterCount = 2u,");

    free(generatedCText);
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
    RUN_TEST(test_aot_c_method_info_aligns_receiver_and_explicit_parameter_types);
    RUN_TEST(test_aot_c_method_info_leaves_ambiguous_legacy_parameter_types_unknown);
    return UNITY_END();
}
