#ifndef ZR_VM_TEST_AOT_C_GENERIC_CALL_TYPED_DEFAULT_DECLARATION_CASES_H
#define ZR_VM_TEST_AOT_C_GENERIC_CALL_TYPED_DEFAULT_DECLARATION_CASES_H

static const char *parameter_layout_omitted_default_argument_source(void) {
    return "struct Stamp {\n"
           "    pub var value: int;\n"
           "    pub @constructor(value: int) { this.value = value; }\n"
           "}\n"
           "class RefA { }\n"
           "fn stamp<T>(value: T, seed: int = 73): Stamp where T: class {\n"
           "    var local: Stamp = init Stamp(seed);\n"
           "    return local;\n"
           "}\n"
           "var input: RefA;\n"
           "var returned: Stamp = stamp<RefA>(input);\n"
           "return returned.value;";
}

static const char *parameter_layout_explicit_default_value_source(void) {
    return "struct Stamp {\n"
           "    pub var value: int;\n"
           "    pub @constructor(value: int) { this.value = value; }\n"
           "}\n"
           "class RefA { }\n"
           "fn stamp<T>(value: T, seed: int = 73): Stamp where T: class {\n"
           "    var local: Stamp = init Stamp(seed);\n"
           "    return local;\n"
           "}\n"
           "var input: RefA;\n"
           "var returned: Stamp = stamp<RefA>(input, 73);\n"
           "return returned.value;";
}

static SZrFunction *parameter_layout_find_default_callee(
        SZrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->parameterCount == 2u &&
        function->parameterMetadata != ZR_NULL &&
        function->parameterMetadataCount == function->parameterCount &&
        function->parameterMetadata[1].hasDefaultValue) {
        return function;
    }

    for (TZrUInt32 childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        SZrFunction *callee = parameter_layout_find_default_callee(
                &function->childFunctionList[childIndex]);

        if (callee != ZR_NULL) {
            return callee;
        }
    }

    return ZR_NULL;
}

static char *write_default_declaration_case(
        const char *source,
        TZrBool hideDefaultDeclaration,
        TZrBool markReceiver) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *callee;
    SZrAotWriterOptions options;
    TZrUInt32 originalMetadataCount;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state,
            source,
            "aot_c_generic_call_typed_default_declaration.zr");
    TEST_ASSERT_NOT_NULL(function);
    callee = parameter_layout_find_default_callee(function);
    TEST_ASSERT_NOT_NULL(callee);
    originalMetadataCount = callee->parameterMetadataCount;
    if (markReceiver) {
        TEST_ASSERT_EQUAL_UINT32(
                1u, mark_slot_zero_receiver_parameter(state, function));
    }
    if (hideDefaultDeclaration) {
        callee->parameterMetadataCount--;
    }

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            hideDefaultDeclaration
                    ? "aot_c_generic_call_typed_unknown_default_declaration"
                    : markReceiver
                              ? "aot_c_generic_call_typed_receiver_default_declaration"
                              : "aot_c_generic_call_typed_default_declaration",
            "aot_c/src",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot-c-generic-call-typed-default-declaration";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-generic-call-typed-default-declaration";
    options.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    callee->parameterMetadataCount = originalMetadataCount;
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return generatedCText;
}

static void test_aot_c_reference_generic_call_typed_observes_defaultable_callee_at_full_arity(void) {
    char *omittedGeneratedCText;
    char *explicitGeneratedCText;

    TEST_ASSERT_EQUAL_INT64(
            73,
            execute_interpreter_i64(parameter_layout_omitted_default_argument_source()));
    omittedGeneratedCText = write_default_declaration_case(
            parameter_layout_omitted_default_argument_source(), ZR_FALSE, ZR_FALSE);
    explicitGeneratedCText = write_default_declaration_case(
            parameter_layout_explicit_default_value_source(), ZR_FALSE, ZR_FALSE);

    TEST_ASSERT_NOT_NULL(strstr(
            omittedGeneratedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NOT_NULL(strstr(omittedGeneratedCText, "argCount=2"));
    TEST_ASSERT_NOT_NULL(strstr(
            omittedGeneratedCText,
            "/* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */"));
    TEST_ASSERT_NOT_NULL(strstr(
            explicitGeneratedCText,
            "/* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */"));
    free(omittedGeneratedCText);
    free(explicitGeneratedCText);
}

static void test_aot_c_reference_generic_call_typed_keeps_partial_default_metadata_unknown(void) {
    char *generatedCText = write_default_declaration_case(
            parameter_layout_omitted_default_argument_source(), ZR_TRUE, ZR_FALSE);

    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NULL(strstr(
            generatedCText,
            "/* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_keeps_receiver_default_metadata_unknown(void) {
    char *generatedCText = write_default_declaration_case(
            parameter_layout_omitted_default_argument_source(), ZR_FALSE, ZR_TRUE);

    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NULL(strstr(
            generatedCText,
            "/* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */"));
    free(generatedCText);
}

#if defined(ZR_PLATFORM_UNIX)
static void assert_default_declaration_projection(
        TZrBool clearDefaultDeclaration,
        TZrBool hideDefaultDeclaration,
        TZrBool markReceiver,
        TZrBool expectedKnown,
        TZrBool expectedDeclared) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *callee;
    SZrAotExecIrModule module;
    const SZrAotExecIrFunction *calleeIr = ZR_NULL;
    TZrUInt32 originalMetadataCount;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state,
            parameter_layout_omitted_default_argument_source(),
            "aot_c_generic_call_typed_default_projection.zr");
    TEST_ASSERT_NOT_NULL(function);
    callee = parameter_layout_find_default_callee(function);
    TEST_ASSERT_NOT_NULL(callee);
    originalMetadataCount = callee->parameterMetadataCount;
    if (clearDefaultDeclaration) {
        callee->parameterMetadata[1].hasDefaultValue = ZR_FALSE;
    }
    if (hideDefaultDeclaration) {
        callee->parameterMetadataCount--;
    }
    if (markReceiver) {
        TEST_ASSERT_EQUAL_UINT32(
                1u, mark_slot_zero_receiver_parameter(state, function));
    }

    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_build_module(state, function, &module));
    for (TZrUInt32 functionIndex = 0u;
         functionIndex < module.functionCount;
         functionIndex++) {
        if (module.functions[functionIndex].function == callee) {
            calleeIr = &module.functions[functionIndex];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(calleeIr);
    TEST_ASSERT_EQUAL_UINT32(
            callee->parameterCount,
            calleeIr->frameLayout.parameterLayoutCount);
    TEST_ASSERT_EQUAL(expectedKnown,
                      calleeIr->frameLayout.parameterLayouts[1]
                              .defaultDeclarationKnown);
    TEST_ASSERT_EQUAL(expectedDeclared,
                      calleeIr->frameLayout.parameterLayouts[1]
                              .hasDeclaredDefault);

    backend_aot_exec_ir_release_module(state, &module);
    callee->parameterMetadataCount = originalMetadataCount;
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static char *write_default_sidecar_state_case(
        TZrBool defaultDeclarationKnown,
        TZrBool hasDeclaredDefault) {
    FILE *file = tmpfile();
    SZrAotExecIrFrameSlotLayout callerSlots[2];
    SZrAotExecIrParameterLayout calleeParameter;
    SZrAotExecIrFrameLayout callerFrame;
    SZrAotExecIrFunction calleeFunctionIr;
    SZrAotExecIrInstruction instruction;
    long textLength;
    char *text;

    TEST_ASSERT_NOT_NULL(file);
    memset(callerSlots, 0, sizeof(callerSlots));
    memset(&calleeParameter, 0, sizeof(calleeParameter));
    memset(&callerFrame, 0, sizeof(callerFrame));
    memset(&calleeFunctionIr, 0, sizeof(calleeFunctionIr));
    memset(&instruction, 0, sizeof(instruction));

    callerSlots[0].stackSlot = 0u;
    callerSlots[0].byteSize = 8u;
    callerSlots[0].typeLayoutId = 5u;
    callerSlots[0].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerSlots[1].stackSlot = 2u;
    callerSlots[1].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    callerSlots[1].slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    callerFrame.generatedFrameSlotCount = 3u;
    callerFrame.slotLayoutCount = 2u;
    callerFrame.slotLayouts = callerSlots;

    calleeParameter.defaultDeclarationKnown = defaultDeclarationKnown;
    calleeParameter.hasDeclaredDefault = hasDeclaredDefault;
    calleeParameter.type.baseType = ZR_VALUE_TYPE_OBJECT;
    calleeFunctionIr.frameLayout.parameterCount = 1u;
    calleeFunctionIr.frameLayout.parameterLayoutCount = 1u;
    calleeFunctionIr.frameLayout.parameterLayouts = &calleeParameter;
    calleeFunctionIr.directInlineReturnLayoutKnown = ZR_TRUE;
    calleeFunctionIr.directInlineReturnTypeLayoutId = 5u;

    instruction.destinationSlot = 0u;
    instruction.operand0 = 1u;
    instruction.operand1 = 1u;
    TEST_ASSERT_TRUE(backend_aot_try_write_c_value_semir_call_typed_exec(
            file,
            &callerFrame,
            &instruction,
            &calleeFunctionIr,
            0u,
            0u,
            1u,
            ZR_FALSE));

    TEST_ASSERT_EQUAL_INT(0, fflush(file));
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    textLength = ftell(file);
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, textLength);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));
    text = (char *)malloc((size_t)textLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_size_t(
            (size_t)textLength, fread(text, 1u, (size_t)textLength, file));
    text[textLength] = '\0';
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return text;
}
#endif

static void test_aot_c_reference_generic_call_typed_rejects_invalid_default_sidecar_state(void) {
    SZrAotExecIrParameterLayout layout;

    memset(&layout, 0, sizeof(layout));
    TEST_ASSERT_TRUE(
            backend_aot_exec_ir_parameter_default_declaration_is_valid(&layout));
    layout.hasDeclaredDefault = ZR_TRUE;
    TEST_ASSERT_FALSE(
            backend_aot_exec_ir_parameter_default_declaration_is_valid(&layout));
    layout.defaultDeclarationKnown = ZR_TRUE;
    TEST_ASSERT_TRUE(
            backend_aot_exec_ir_parameter_default_declaration_is_valid(&layout));
    layout.hasDeclaredDefault = ZR_FALSE;
    TEST_ASSERT_TRUE(
            backend_aot_exec_ir_parameter_default_declaration_is_valid(&layout));
    layout.hasDeclaredDefault = (TZrBool)2u;
    TEST_ASSERT_FALSE(
            backend_aot_exec_ir_parameter_default_declaration_is_valid(&layout));
    layout.hasDeclaredDefault = ZR_FALSE;
    layout.defaultDeclarationKnown = (TZrBool)2u;
    TEST_ASSERT_FALSE(
            backend_aot_exec_ir_parameter_default_declaration_is_valid(&layout));

#if defined(ZR_PLATFORM_UNIX)
    char *validText = write_default_sidecar_state_case(ZR_TRUE, ZR_TRUE);
    char *invalidText = write_default_sidecar_state_case(ZR_FALSE, ZR_TRUE);

    TEST_ASSERT_NOT_NULL(strstr(
            validText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NOT_NULL(strstr(
            validText,
            "/* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */"));
    TEST_ASSERT_NULL(strstr(
            invalidText, "/* zr_aot_generic_call_typed_shared_callsite */"));
    TEST_ASSERT_NULL(strstr(
            invalidText,
            "/* zr_aot_generic_call_typed_callee_defaultable_parameter_full_arity */"));
    free(validText);
    free(invalidText);

    assert_default_declaration_projection(
            ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_TRUE, ZR_TRUE);
    assert_default_declaration_projection(
            ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
    assert_default_declaration_projection(
            ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_FALSE, ZR_FALSE);
    assert_default_declaration_projection(
            ZR_FALSE, ZR_FALSE, ZR_TRUE, ZR_FALSE, ZR_FALSE);
#endif
}

static TZrUInt32 add_unreachable_noncanonical_default_owner(
        SZrState *state,
        SZrFunction *function) {
    SZrFunction *parent;
    SZrFunction *unreachable;

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

    parent->childFunctionList = (SZrFunction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(SZrFunction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(parent->childFunctionList);
    memset(parent->childFunctionList, 0, sizeof(SZrFunction));
    parent->childFunctionLength = 1u;
    unreachable = &parent->childFunctionList[0];
    unreachable->ownerFunction = parent;
    unreachable->parameterCount = 1u;
    unreachable->stackSize = 1u;
    unreachable->lineInSourceStart = 1u;
    unreachable->lineInSourceEnd = 1u;
    unreachable->parameterMetadata =
            (SZrFunctionMetadataParameter *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrFunctionMetadataParameter),
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(unreachable->parameterMetadata);
    memset(unreachable->parameterMetadata, 0, sizeof(SZrFunctionMetadataParameter));
    unreachable->parameterMetadataCount = 1u;
    unreachable->parameterMetadata[0].hasDefaultValue = (TZrBool)2u;
    return 1u;
}

static void test_aot_c_reference_generic_call_typed_rejects_unreachable_noncanonical_default_flag_before_stripping(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state,
            parameter_layout_omitted_default_argument_source(),
            "aot_c_generic_call_typed_invalid_default_declaration.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            1u, add_unreachable_noncanonical_default_owner(state, function));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_generic_call_typed_invalid_default_declaration",
            "aot_c/src",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    (void)remove(generatedCPath);
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot-c-generic-call-typed-invalid-default-declaration";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot-c-generic-call-typed-invalid-default-declaration";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

#endif
