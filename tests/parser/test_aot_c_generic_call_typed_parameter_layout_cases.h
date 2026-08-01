#ifndef ZR_VM_TEST_AOT_C_GENERIC_CALL_TYPED_PARAMETER_LAYOUT_CASES_H
#define ZR_VM_TEST_AOT_C_GENERIC_CALL_TYPED_PARAMETER_LAYOUT_CASES_H

static TZrBool parameter_layout_type_ref_is_reference(
        const SZrFunctionTypedTypeRef *typeRef) {
    return (TZrBool)(typeRef != ZR_NULL &&
                     (typeRef->baseType == ZR_VALUE_TYPE_OBJECT ||
                      typeRef->baseType == ZR_VALUE_TYPE_ARRAY));
}

static TZrUInt32 replace_legacy_reference_parameter_types_recursive(
        SZrFunction *function) {
    TZrUInt32 replacementCount = 0u;

    if (function == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 parameterIndex = 0u;
         function->parameterMetadata != ZR_NULL &&
         parameterIndex < function->parameterMetadataCount;
         parameterIndex++) {
        SZrFunctionMetadataParameter *metadata =
                &function->parameterMetadata[parameterIndex];
        TZrBool hasReferenceBinding = ZR_FALSE;

        for (TZrUInt32 bindingIndex = 0u;
             function->typedLocalBindings != ZR_NULL &&
             bindingIndex < function->typedLocalBindingLength;
             bindingIndex++) {
            const SZrFunctionTypedLocalBinding *binding =
                    &function->typedLocalBindings[bindingIndex];

            if (binding->stackSlot == parameterIndex &&
                parameter_layout_type_ref_is_reference(&binding->type)) {
                hasReferenceBinding = ZR_TRUE;
                break;
            }
        }

        if (!hasReferenceBinding ||
            !parameter_layout_type_ref_is_reference(&metadata->type)) {
            continue;
        }

        metadata->type.baseType = ZR_VALUE_TYPE_INT64;
        metadata->type.staticCType = ZR_STATIC_C_TYPE_I64;
        replacementCount++;
    }

    for (TZrUInt32 childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        replacementCount += replace_legacy_reference_parameter_types_recursive(
                &function->childFunctionList[childIndex]);
    }

    return replacementCount;
}

static TZrUInt32 replace_legacy_reference_parameter_types(
        SZrState *state,
        SZrFunction *function) {
    (void)state;
    return replace_legacy_reference_parameter_types_recursive(function);
}

static TZrUInt32 clear_projected_reference_parameter_types_recursive(
        SZrFunction *function) {
    TZrUInt32 clearedCount = 0u;

    if (function == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 bindingIndex = 0u;
         function->typedLocalBindings != ZR_NULL &&
         bindingIndex < function->typedLocalBindingLength;
         bindingIndex++) {
        SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[bindingIndex];

        if (binding->stackSlot >= function->parameterCount ||
            binding->roleFlags != 0u ||
            !parameter_layout_type_ref_is_reference(&binding->type)) {
            continue;
        }

        memset(&binding->type, 0, sizeof(binding->type));
        clearedCount++;
    }

    for (TZrUInt32 childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        clearedCount += clear_projected_reference_parameter_types_recursive(
                &function->childFunctionList[childIndex]);
    }

    return clearedCount;
}

static TZrUInt32 clear_projected_reference_parameter_types(
        SZrState *state,
        SZrFunction *function) {
    (void)state;
    return clear_projected_reference_parameter_types_recursive(function);
}

static TZrUInt32 mark_slot_zero_reference_parameter_as_receiver_recursive(
        SZrFunction *function,
        TZrBool clearProjectedType) {
    if (function == ZR_NULL) {
        return 0u;
    }

    for (TZrUInt32 bindingIndex = 0u;
         function->parameterCount > 0u &&
         function->typedLocalBindings != ZR_NULL &&
         bindingIndex < function->typedLocalBindingLength;
         bindingIndex++) {
        SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[bindingIndex];

        if (binding->stackSlot != 0u ||
            binding->roleFlags != 0u ||
            binding->name == ZR_NULL ||
            !parameter_layout_type_ref_is_reference(&binding->type)) {
            continue;
        }

        binding->roleFlags = ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER;
        if (clearProjectedType) {
            memset(&binding->type, 0, sizeof(binding->type));
        }
        return 1u;
    }

    for (TZrUInt32 childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        TZrUInt32 markedCount =
                mark_slot_zero_reference_parameter_as_receiver_recursive(
                        &function->childFunctionList[childIndex],
                        clearProjectedType);

        if (markedCount != 0u) {
            return markedCount;
        }
    }

    return 0u;
}

static TZrUInt32 project_slot_zero_receiver_parameter(
        SZrState *state,
        SZrFunction *function) {
    (void)state;
    if (replace_legacy_reference_parameter_types_recursive(function) == 0u) {
        return 0u;
    }
    return mark_slot_zero_reference_parameter_as_receiver_recursive(
            function, ZR_FALSE);
}

static TZrUInt32 mark_slot_zero_receiver_parameter(
        SZrState *state,
        SZrFunction *function) {
    (void)state;
    return mark_slot_zero_reference_parameter_as_receiver_recursive(
            function, ZR_FALSE);
}

static TZrUInt32 project_slot_zero_receiver_with_unknown_type(
        SZrState *state,
        SZrFunction *function) {
    (void)state;
    return mark_slot_zero_reference_parameter_as_receiver_recursive(
            function, ZR_TRUE);
}

static TZrUInt32 add_unreachable_nested_function_before_reference_callee(
        SZrState *state,
        SZrFunction *function) {
    SZrFunction *parent;
    TZrUInt32 replacementCount;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->childFunctionList == ZR_NULL ||
        function->childFunctionLength == 0u) {
        return 0u;
    }

    parent = &function->childFunctionList[0];
    if (parent->childFunctionList != ZR_NULL ||
        parent->childFunctionLength != 0u) {
        return 0u;
    }

    replacementCount = replace_legacy_reference_parameter_types_recursive(function);
    if (replacementCount == 0u) {
        return 0u;
    }

    parent->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(parent->childFunctionList);
    memset(parent->childFunctionList, 0, sizeof(SZrFunction));
    parent->childFunctionLength = 1u;
    parent->childFunctionList[0].ownerFunction = parent;
    parent->childFunctionList[0].stackSize = 1u;
    parent->childFunctionList[0].lineInSourceStart = 1u;
    parent->childFunctionList[0].lineInSourceEnd = 1u;
    return replacementCount + 1u;
}

static const char *parameter_layout_reference_generic_source(void) {
    return "struct Stamp {\n"
           "    pub var value: int;\n"
           "    pub @constructor(value: int) { this.value = value; }\n"
           "}\n"
           "class RefA { }\n"
           "fn stamp<T>(value: T): Stamp where T: class {\n"
           "    var local: Stamp = init Stamp(42);\n"
           "    return local;\n"
           "}\n"
           "var input: RefA;\n"
           "var returned: Stamp = stamp<RefA>(input);\n"
           "return returned.value;";
}

static const char *parameter_layout_receiver_generic_source(void) {
    return "struct Stamp {\n"
           "    pub var value: int;\n"
           "    pub @constructor(value: int) { this.value = value; }\n"
           "}\n"
           "class RefA { }\n"
           "class RefB { }\n"
           "class Box<T> where T: class {\n"
           "    pub fn stamp(): Stamp {\n"
           "        var local: Stamp = init Stamp(42);\n"
           "        return local;\n"
           "    }\n"
           "}\n"
           "var first = new Box<RefA>();\n"
           "var second = new Box<RefB>();\n"
           "var left: Stamp = first.stamp();\n"
           "var right: Stamp = second.stamp();\n"
           "return left.value + right.value;";
}

static const char *parameter_layout_receiver_window_source(void) {
    return "struct Stamp {\n"
           "    pub var value: int;\n"
           "    pub @constructor(value: int) { this.value = value; }\n"
           "}\n"
           "class RefA { }\n"
           "fn stamp<T>(receiver: T, other: T, seed: int): Stamp where T: class {\n"
           "    var local: Stamp = init Stamp(seed);\n"
           "    return local;\n"
           "}\n"
           "var first: RefA;\n"
           "var second: RefA;\n"
           "var returned: Stamp = stamp<RefA>(first, second, 73);\n"
           "return returned.value;";
}

static const char *parameter_layout_sparse_callee_source(void) {
    return "struct Stamp {\n"
           "    pub var value: int;\n"
           "    pub @constructor(value: int) { this.value = value; }\n"
           "}\n"
           "class RefA { }\n"
           "fn stamp<T>(value: T): Stamp where T: class {\n"
           "    var local: Stamp = init Stamp(42);\n"
           "    return local;\n"
           "}\n"
           "var input: RefA;\n"
           "var returned: Stamp = stamp<RefA>(input);\n"
           "return returned.value;";
}

static char *write_parameter_layout_case(
        const char *caseName,
        const char *sourceName,
        const char *source,
        TZrUInt32 (*mutateFunction)(SZrState *, SZrFunction *),
        TZrBool enableCodeStripping) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, sourceName);
    TEST_ASSERT_NOT_NULL(function);
    if (mutateFunction != ZR_NULL) {
        TEST_ASSERT_GREATER_THAN_UINT32(0u, mutateFunction(state, function));
    }

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            caseName,
            "aot_c/src",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    memset(&options, 0, sizeof(options));
    options.moduleName = caseName;
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = caseName;
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = enableCodeStripping;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return generatedCText;
}

static void test_aot_c_reference_generic_call_typed_uses_exec_ir_parameter_layout(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_parameter_layout",
            "aot_c_generic_call_typed_parameter_layout.zr",
            parameter_layout_reference_generic_source(),
            replace_legacy_reference_parameter_types,
            ZR_FALSE);

    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_rejects_unknown_exec_ir_parameter_type(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_unknown_parameter",
            "aot_c_generic_call_typed_unknown_parameter.zr",
            parameter_layout_reference_generic_source(),
            clear_projected_reference_parameter_types,
            ZR_FALSE);

    TEST_ASSERT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_uses_slot_zero_receiver_parameter_layout(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_receiver_slot_zero",
            "aot_c_generic_call_typed_receiver_slot_zero.zr",
            parameter_layout_reference_generic_source(),
            project_slot_zero_receiver_parameter,
            ZR_FALSE);

    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_rejects_unknown_receiver_parameter_type(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_unknown_receiver",
            "aot_c_generic_call_typed_unknown_receiver.zr",
            parameter_layout_receiver_window_source(),
            project_slot_zero_receiver_with_unknown_type,
            ZR_FALSE);

    TEST_ASSERT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_keeps_dynamic_receiver_call_outside_typed_route(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_receiver_parameter",
            "aot_c_generic_call_typed_receiver_parameter.zr",
            parameter_layout_receiver_generic_source(),
            ZR_NULL,
            ZR_FALSE);

    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_method_slot */"));
    TEST_ASSERT_NOT_NULL(strstr(generatedCText, " DYN_CALL exec="));
    TEST_ASSERT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_finds_sparse_retained_callee_layout(void) {
    unsigned functionsRemoved = 0u;
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_sparse_callee",
            "aot_c_generic_call_typed_sparse_callee.zr",
            parameter_layout_sparse_callee_source(),
            add_unreachable_nested_function_before_reference_callee,
            ZR_TRUE);
    const char *strippingMarker = strstr(
            generatedCText, "/* code_stripping.functionsRemoved = ");

    TEST_ASSERT_NOT_NULL(strippingMarker);
    TEST_ASSERT_EQUAL_INT(1, sscanf(
            strippingMarker, "/* code_stripping.functionsRemoved = %u */", &functionsRemoved));
    TEST_ASSERT_GREATER_THAN_UINT32(0u, functionsRemoved);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_receiver_window_executes_in_aot(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE(
            "AOT C receiver-aware typed-call runtime validation uses the Unix shared-library path");
#else
    const char *source = parameter_layout_receiver_window_source();
    const char *projectJson =
            "{"
            "\"name\":\"aot-receiver-aware-generic-call-typed\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    TZrInt64 interpreterResult = execute_interpreter_i64(source);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrLibrary_Project *project;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    SZrTypeValue aotResult;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar sharedLibraryPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, source, "main.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            1u, mark_slot_zero_receiver_parameter(state, function));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_receiver_aware_generic_call_typed",
            "project",
            "receiver_aware_generic_call_typed",
            ".zrp",
            projectPath,
            sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_receiver_aware_generic_call_typed",
            "project/src",
            "main",
            ".zr",
            sourcePath,
            sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_receiver_aware_generic_call_typed",
            "project/bin",
            "main",
            ".zro",
            zroPath,
            sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_receiver_aware_generic_call_typed",
            "project/bin/aot_c/src",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_receiver_aware_generic_call_typed",
            "project/bin/aot_c/lib",
            "zrvm_aot_main",
            ".so",
            sharedLibraryPath,
            sizeof(sharedLibraryPath)));

    write_text_file_or_fail(projectPath, projectJson);
    write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(
            state, function, zroPath, &binaryOptions));
    hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(
            zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &aotOptions));

    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    assert_generated_c_reports_embedded_module_bytes(
            generatedCText, embeddedBlobLength);
    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "ZrLibrary_AotRuntime_CallInlineStruct(state,"));
    free(generatedCText);

    compile_generated_c_shared_library_or_fail(
            generatedCPath, sharedLibraryPath);

    project = ZrLibrary_Project_New(
            state, (TZrNativeString)projectJson, (TZrNativeString)projectPath);
    TEST_ASSERT_NOT_NULL(project);
    state->global->userData = project;
    TEST_ASSERT_TRUE(ZrLibrary_AotRuntime_ConfigureGlobal(
            state->global,
            ZR_LIBRARY_PROJECT_EXECUTION_MODE_AOT_C,
            ZR_TRUE));

    ZrCore_Value_ResetAsNull(&aotResult);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrLibrary_AotRuntime_ExecuteEntry(
                    state, ZR_AOT_BACKEND_KIND_C, &aotResult),
            ZrLibrary_AotRuntime_GetLastError(state->global));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(aotResult.type));
    TEST_ASSERT_EQUAL_INT64(
            interpreterResult, aotResult.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIBRARY_EXECUTED_VIA_AOT_C,
            ZrLibrary_AotRuntime_GetExecutedVia(state->global));

    state->global->userData = ZR_NULL;
    ZrLibrary_Project_Free(state, project);
    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

#endif
