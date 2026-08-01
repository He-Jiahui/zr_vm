#ifndef ZR_TEST_AOT_C_METHOD_INFO_RETURN_PROJECTION_CASES_H
#define ZR_TEST_AOT_C_METHOD_INFO_RETURN_PROJECTION_CASES_H

#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h"
#include "zr_vm_common/zr_instruction_conf.h"

#if defined(ZR_PLATFORM_UNIX)
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.h"
#include "../../zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_function_table.h"
#endif

static void test_aot_exec_ir_callable_return_accessor_isolates_raw_metadata(void) {
    SZrFunction rawFunction;
    SZrAotExecIrFunction functionIr;
    const SZrFunctionTypedTypeRef *selectedType;

    memset(&rawFunction, 0, sizeof(rawFunction));
    memset(&functionIr, 0, sizeof(functionIr));
    functionIr.function = &rawFunction;

    rawFunction.hasCallableReturnType = ZR_FALSE;
    init_signature_type_ref(
            &rawFunction.callableReturnType, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    functionIr.callableReturnTypeKnown = ZR_TRUE;
    init_signature_type_ref(
            &functionIr.callableReturnType, ZR_VALUE_TYPE_BOOL, ZR_STATIC_C_TYPE_BOOL);
    selectedType = backend_aot_exec_ir_callable_return_type(&functionIr);
    TEST_ASSERT_EQUAL_PTR(&functionIr.callableReturnType, selectedType);
    TEST_ASSERT_EQUAL_UINT16(ZR_VALUE_TYPE_BOOL, selectedType->baseType);

    rawFunction.hasCallableReturnType = ZR_TRUE;
    functionIr.callableReturnTypeKnown = ZR_FALSE;
    TEST_ASSERT_NULL(backend_aot_exec_ir_callable_return_type(&functionIr));

    functionIr.callableReturnTypeKnown = (TZrBool)2u;
    TEST_ASSERT_NULL(backend_aot_exec_ir_callable_return_type(&functionIr));
}

#if defined(ZR_PLATFORM_UNIX)
static char *read_return_projection_stream_owned(FILE *file) {
    long textLength;
    char *text;

    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fflush(file));
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    textLength = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, textLength);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));
    text = (char *)malloc((size_t)textLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_size_t((size_t)textLength,
                             fread(text, 1u, (size_t)textLength, file));
    text[textLength] = '\0';
    return text;
}

static const SZrAotExecIrFunction *find_return_projection_function_ir(
        const SZrAotExecIrModule *module,
        const SZrFunction *function) {
    if (module == ZR_NULL || function == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < module->functionCount; index++) {
        if (module->functions[index].function == function) {
            return &module->functions[index];
        }
    }
    return ZR_NULL;
}

static void test_aot_exec_ir_projects_callable_return_borrowed_snapshot(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotExecIrModule module;
    SZrAotFunctionTable functionTable;
    const SZrAotExecIrFunction *functionIr;
    SZrString *typeName;
    FILE *methodMetadataFile;
    char *methodMetadataText;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    typeName = ZrCore_String_CreateFromNative(state, "ProjectedI64");
    TEST_ASSERT_NOT_NULL(typeName);
    function->hasCallableReturnType = ZR_TRUE;
    init_signature_type_ref(
            &function->callableReturnType, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    function->callableReturnType.typeName = typeName;

    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, function, &module));
    functionIr = find_return_projection_function_ir(&module, function);
    TEST_ASSERT_NOT_NULL(functionIr);
    TEST_ASSERT_EQUAL(ZR_TRUE, functionIr->callableReturnTypeKnown);
    TEST_ASSERT_EQUAL_UINT16(ZR_VALUE_TYPE_INT64, functionIr->callableReturnType.baseType);
    TEST_ASSERT_EQUAL_UINT16(ZR_STATIC_C_TYPE_I64, functionIr->callableReturnType.staticCType);
    TEST_ASSERT_EQUAL_PTR(typeName, functionIr->callableReturnType.typeName);

    memset(&functionTable, 0, sizeof(functionTable));
    TEST_ASSERT_TRUE(backend_aot_build_function_table(state, function, &functionTable));
    init_signature_type_ref(
            &function->callableReturnType, ZR_VALUE_TYPE_BOOL, ZR_STATIC_C_TYPE_BOOL);
    methodMetadataFile = tmpfile();
    TEST_ASSERT_NOT_NULL(methodMetadataFile);
    TEST_ASSERT_GREATER_THAN_UINT64(
            0u,
            backend_aot_write_c_method_infos(
                    methodMetadataFile,
                    state,
                    &functionTable,
                    &module,
                    (TZrUInt8)ZR_AOT_REFLECTION_METADATA_NONE));
    methodMetadataText = read_return_projection_stream_owned(methodMetadataFile);
    assert_signature_type_row(
            methodMetadataText, 0u, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    free(methodMetadataText);
    fclose(methodMetadataFile);
    backend_aot_release_function_table(state, &functionTable);
    backend_aot_exec_ir_release_module(state, &module);

    function->hasCallableReturnType = ZR_FALSE;
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, function, &module));
    functionIr = find_return_projection_function_ir(&module, function);
    TEST_ASSERT_NOT_NULL(functionIr);
    TEST_ASSERT_EQUAL(ZR_FALSE, functionIr->callableReturnTypeKnown);
    TEST_ASSERT_NULL(backend_aot_exec_ir_callable_return_type(functionIr));
    backend_aot_exec_ir_release_module(state, &module);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}
#endif

static void test_aot_c_method_info_uses_projected_callable_return_type(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    const char *signatureTypes;
    const char *signatureDescriptor;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->hasCallableReturnType = ZR_TRUE;
    init_signature_type_ref(
            &function->callableReturnType, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_method_info_signature",
            "projected_callable_return",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));

    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_method_info_projected_callable_return";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    signatureTypes = strstr(
            generatedCText, "static const SZrAotSignatureType zr_aot_signature_0_types[] = {");
    signatureDescriptor = strstr(
            generatedCText, "static const SZrAotSignature zr_aot_signature_0 = {");
    TEST_ASSERT_NOT_NULL(signatureTypes);
    TEST_ASSERT_NOT_NULL(signatureDescriptor);
    assert_signature_type_row(
            signatureTypes, 0u, ZR_VALUE_TYPE_INT64, ZR_STATIC_C_TYPE_I64);
    assert_text_contains(signatureDescriptor,
                         "    .returnType = &zr_aot_signature_0_types[0],");
    assert_text_contains(signatureDescriptor, "    .hasReturnValue = (TZrUInt8)1u,");

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_unknown_callable_return_uses_scalar_inference(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state,
                              "var left: uint = 7;\n"
                              "var right: uint = 5;\n"
                              "var result: uint = left + right;\n"
                              "return result;\n",
                              "unknown_callable_return_inference.zr");
    TEST_ASSERT_NOT_NULL(function);
    function->hasCallableReturnType = ZR_FALSE;
    init_signature_type_ref(
            &function->callableReturnType, ZR_VALUE_TYPE_BOOL, ZR_STATIC_C_TYPE_BOOL);
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_method_info_signature",
            "unknown_callable_return_inference",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_unknown_callable_return_inference";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_signature_scalar_return(generatedCText,
                                   ZR_VALUE_TYPE_UINT64,
                                   ZR_STATIC_C_TYPE_U64,
                                   "ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u");

    free(generatedCText);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_c_code_stripping_rejects_unreachable_noncanonical_callable_return_flag(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *unreachable;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    FILE *generatedFile;

    TEST_ASSERT_NOT_NULL(state);
    function = ZrCore_Function_New(state);
    TEST_ASSERT_NOT_NULL(function);
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0].value = 0u;
    function->instructionsList[0].instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION);
    function->instructionsLength = 1u;
    function->stackSize = 1u;
    function->lineInSourceStart = 1u;
    function->lineInSourceEnd = 1u;
    function->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction) * 2u,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->childFunctionList);
    memset(function->childFunctionList, 0, sizeof(SZrFunction) * 2u);
    function->childFunctionLength = 2u;
    function->childFunctionList[0].ownerFunction = function;
    function->childFunctionList[0].stackSize = 1u;
    function->childFunctionList[0].lineInSourceStart = 10u;
    function->childFunctionList[0].lineInSourceEnd = 10u;
    unreachable = &function->childFunctionList[1];
    unreachable->ownerFunction = function;
    unreachable->stackSize = 1u;
    unreachable->lineInSourceStart = 20u;
    unreachable->lineInSourceEnd = 20u;

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_method_info_signature",
            "noncanonical_unreachable_callable_return",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(generatedCPath));
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_noncanonical_unreachable_callable_return";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_text_contains(generatedCText, "/* code_stripping.enabled = 1 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsBefore = 3 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsAfter = 2 */");
    assert_text_contains(generatedCText, "/* code_stripping.functionsRemoved = 1 */");
    free(generatedCText);
    (void)remove(generatedCPath);
    unreachable->hasCallableReturnType = (TZrBool)2u;
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedFile = fopen(generatedCPath, "rb");
    if (generatedFile != ZR_NULL) {
        fclose(generatedFile);
    }
    TEST_ASSERT_NULL(generatedFile);

    ZrTests_Runtime_State_Destroy(state);
}

#endif
