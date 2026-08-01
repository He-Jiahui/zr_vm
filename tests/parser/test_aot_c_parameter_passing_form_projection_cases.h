#ifndef ZR_VM_TEST_AOT_C_PARAMETER_PASSING_FORM_PROJECTION_CASES_H
#define ZR_VM_TEST_AOT_C_PARAMETER_PASSING_FORM_PROJECTION_CASES_H

static TZrBool passing_form_function_name_equals(
        const SZrFunction *function,
        const char *name) {
    TZrSize nameLength;

    if (function == ZR_NULL || function->functionName == ZR_NULL ||
        name == ZR_NULL) {
        return ZR_FALSE;
    }
    nameLength = strlen(name);
    return (TZrBool)(
            ZrCore_String_GetByteLength(function->functionName) == nameLength &&
            memcmp(ZrCore_String_GetNativeString(function->functionName),
                   name,
                   nameLength) == 0);
}

static SZrFunction *passing_form_find_named_function(
        SZrFunction *function,
        const char *name) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    if (passing_form_function_name_equals(function, name)) {
        return function;
    }
    for (childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        SZrFunction *found = passing_form_find_named_function(
                &function->childFunctionList[childIndex], name);

        if (found != ZR_NULL) {
            return found;
        }
    }
    return ZR_NULL;
}

static TZrUInt32 passing_form_mutate_reference_callee(
        SZrFunction *function,
        TZrUInt32 passingFlag) {
    SZrFunction *callee = passing_form_find_named_function(function, "stamp");

    if (callee == ZR_NULL || callee->typedLocalBindings == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 bindingIndex = 0u;
         bindingIndex < callee->typedLocalBindingLength;
         bindingIndex++) {
        SZrFunctionTypedLocalBinding *binding =
                &callee->typedLocalBindings[bindingIndex];

        if (binding->stackSlot >= callee->parameterCount ||
            (binding->roleFlags &
             ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u) {
            continue;
        }
        binding->roleFlags =
                (binding->roleFlags &
                 ~ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK) |
                passingFlag;
        return 1u;
    }
    return 0u;
}

static TZrUInt32 passing_form_clear_reference_callee(
        SZrState *state,
        SZrFunction *function) {
    TZrUInt32 replacementCount;

    (void)state;
    replacementCount =
            replace_legacy_reference_parameter_types_recursive(function);
    if (replacementCount == 0u) {
        return 0u;
    }
    return passing_form_mutate_reference_callee(function, 0u);
}

static TZrUInt32 passing_form_mark_reference_callee_ref_readonly(
        SZrState *state,
        SZrFunction *function) {
    TZrUInt32 replacementCount;

    (void)state;
    replacementCount =
            replace_legacy_reference_parameter_types_recursive(function);
    if (replacementCount == 0u) {
        return 0u;
    }
    return passing_form_mutate_reference_callee(
            function,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF_READONLY);
}

static TZrUInt32 passing_form_clear_parameter_slot(
        SZrFunction *function,
        TZrUInt32 stackSlot) {
    if (function == ZR_NULL) {
        return 0u;
    }
    for (TZrUInt32 bindingIndex = 0u;
         function->typedLocalBindings != ZR_NULL &&
         bindingIndex < function->typedLocalBindingLength;
         bindingIndex++) {
        SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[bindingIndex];

        if (binding->stackSlot != stackSlot ||
            binding->stackSlot >= function->parameterCount ||
            binding->name == ZR_NULL ||
            (binding->roleFlags &
             ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u) {
            continue;
        }
        binding->roleFlags &=
                ~ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK;
        return 1u;
    }
    return 0u;
}

static TZrUInt32 passing_form_add_unreachable_parameter_function(
        SZrState *state,
        SZrFunction *function) {
    SZrFunction *parent;
    SZrFunction *unreachable;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->childFunctionList == ZR_NULL ||
        function->childFunctionLength != 1u) {
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
    unreachable->parameterCount = 2u;
    unreachable->stackSize = 3u;
    unreachable->lineInSourceStart = 2u;
    unreachable->lineInSourceEnd = 2u;
    unreachable->typedLocalBindings =
            (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
                    state->global,
                    sizeof(SZrFunctionTypedLocalBinding) * 3u,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(unreachable->typedLocalBindings);
    memset(unreachable->typedLocalBindings,
           0,
           sizeof(SZrFunctionTypedLocalBinding) * 3u);
    unreachable->typedLocalBindingLength = 3u;
    unreachable->typedLocalBindings[0].name =
            ZrCore_String_CreateFromNative(state, "left");
    unreachable->typedLocalBindings[0].stackSlot = 0u;
    unreachable->typedLocalBindings[0].roleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    unreachable->typedLocalBindings[1].name =
            ZrCore_String_CreateFromNative(state, "right");
    unreachable->typedLocalBindings[1].stackSlot = 1u;
    unreachable->typedLocalBindings[1].roleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    unreachable->typedLocalBindings[2].name =
            ZrCore_String_CreateFromNative(state, "local");
    unreachable->typedLocalBindings[2].stackSlot = 2u;
    TEST_ASSERT_NOT_NULL(unreachable->typedLocalBindings[0].name);
    TEST_ASSERT_NOT_NULL(unreachable->typedLocalBindings[1].name);
    TEST_ASSERT_NOT_NULL(unreachable->typedLocalBindings[2].name);
    return 1u;
}

static void passing_form_assert_aot_write_rejected_without_output(
        SZrState *state,
        SZrFunction *function,
        const TZrChar *generatedCPath,
        const SZrAotWriterOptions *options) {
    FILE *unexpectedArtifact;

    remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, options));
    unexpectedArtifact = fopen(generatedCPath, "rb");
    if (unexpectedArtifact != ZR_NULL) {
        fclose(unexpectedArtifact);
    }
    TEST_ASSERT_NULL(unexpectedArtifact);
}

static void test_aot_exec_ir_parameter_passing_form_sidecar_rejects_noncanonical_states(void) {
    SZrAotExecIrParameterLayout layout;

    memset(&layout, 0, sizeof(layout));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_parameter_passing_form_is_valid(
            &layout));
    TEST_ASSERT_FALSE(backend_aot_exec_ir_parameter_is_value_passing(
            &layout));

    layout.passingFormKnown = ZR_TRUE;
    layout.passingForm =
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE;
    TEST_ASSERT_TRUE(backend_aot_exec_ir_parameter_passing_form_is_valid(
            &layout));
    TEST_ASSERT_TRUE(backend_aot_exec_ir_parameter_is_value_passing(
            &layout));

    layout.passingForm =
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_UNKNOWN;
    TEST_ASSERT_FALSE(backend_aot_exec_ir_parameter_passing_form_is_valid(
            &layout));
    layout.passingFormKnown = ZR_FALSE;
    layout.passingForm =
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT;
    TEST_ASSERT_FALSE(backend_aot_exec_ir_parameter_passing_form_is_valid(
            &layout));
    layout.passingFormKnown = (TZrBool)2;
    layout.passingForm =
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE;
    TEST_ASSERT_FALSE(backend_aot_exec_ir_parameter_passing_form_is_valid(
            &layout));
    layout.passingFormKnown = ZR_TRUE;
    layout.passingForm =
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT + 1u;
    TEST_ASSERT_FALSE(backend_aot_exec_ir_parameter_passing_form_is_valid(
            &layout));
}

static void test_aot_c_reference_generic_call_typed_rejects_unknown_parameter_passing_form(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_unknown_passing_form",
            "aot_c_generic_call_typed_unknown_passing_form.zr",
            parameter_layout_reference_generic_source(),
            passing_form_clear_reference_callee,
            ZR_FALSE);

    TEST_ASSERT_NULL(strstr(
            generatedCText,
            "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static void test_aot_c_reference_generic_call_typed_rejects_non_value_parameter_passing_form(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_ref_readonly_passing_form",
            "aot_c_generic_call_typed_ref_readonly_passing_form.zr",
            parameter_layout_reference_generic_source(),
            passing_form_mark_reference_callee_ref_readonly,
            ZR_FALSE);

    TEST_ASSERT_NULL(strstr(
            generatedCText,
            "/* zr_aot_generic_call_typed_shared_callsite */"));
    free(generatedCText);
}

static TZrBool passing_form_try_write_typed_call(TZrUInt32 passingForm) {
#if !defined(ZR_PLATFORM_UNIX)
    (void)passingForm;
    return ZR_FALSE;
#else
    FILE *file = tmpfile();
    SZrAotExecIrFrameSlotLayout callerSlots[2];
    SZrAotExecIrParameterLayout calleeParameter;
    SZrAotExecIrFrameLayout callerFrame;
    SZrAotExecIrFunction calleeFunctionIr;
    SZrAotExecIrInstruction instruction;
    TZrBool wrote;

    TEST_ASSERT_NOT_NULL(file);
    memset(callerSlots, 0, sizeof(callerSlots));
    memset(&calleeParameter, 0, sizeof(calleeParameter));
    memset(&callerFrame, 0, sizeof(callerFrame));
    memset(&calleeFunctionIr, 0, sizeof(calleeFunctionIr));
    memset(&instruction, 0, sizeof(instruction));

    callerSlots[0].stackSlot = 0u;
    callerSlots[0].byteSize = 8u;
    callerSlots[0].typeLayoutId = 5u;
    callerSlots[0].slotKind =
            (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
    callerSlots[1].stackSlot = 2u;
    callerSlots[1].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    callerSlots[1].slotKind =
            (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    callerFrame.generatedFrameSlotCount = 3u;
    callerFrame.slotLayoutCount = 2u;
    callerFrame.slotLayouts = callerSlots;

    calleeParameter.passingFormKnown =
            (TZrBool)(passingForm !=
                      (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_UNKNOWN);
    calleeParameter.passingForm = passingForm;
    calleeParameter.type.baseType = ZR_VALUE_TYPE_OBJECT;
    calleeFunctionIr.frameLayout.parameterCount = 1u;
    calleeFunctionIr.frameLayout.parameterLayoutCount = 1u;
    calleeFunctionIr.frameLayout.parameterLayouts = &calleeParameter;
    calleeFunctionIr.directInlineReturnLayoutKnown = ZR_TRUE;
    calleeFunctionIr.directInlineReturnTypeLayoutId = 5u;

    instruction.destinationSlot = 0u;
    instruction.operand0 = 1u;
    instruction.operand1 = 1u;
    wrote = backend_aot_try_write_c_value_semir_call_typed_exec(
            file,
            &callerFrame,
            &instruction,
            &calleeFunctionIr,
            0u,
            0u,
            1u,
            ZR_FALSE);
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return wrote;
#endif
}

static void test_aot_c_value_semir_typed_call_accepts_only_value_passing_parameters(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE(
            "AOT C private typed-call passing-form validation uses the Unix parser linkage");
#else
    TEST_ASSERT_TRUE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_UNKNOWN));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_IN));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF_READONLY));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF_READONLY));
    TEST_ASSERT_FALSE(passing_form_try_write_typed_call(
            (TZrUInt32)ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT));
#endif
}

static void test_aot_c_code_stripping_rejects_unreachable_partial_parameter_passing_forms(void) {
    const char *source =
            "fn keep(value: int): int { return value; }\n"
            "return keep(42);";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *removedFunction;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    const char *beforeMarker;
    const char *afterMarker;
    const char *removedMarker;
    unsigned before = 0u;
    unsigned after = 0u;
    unsigned removed = 0u;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, source, "aot_c_partial_passing_form_removed_owner.zr");
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(
            1u, passing_form_add_unreachable_parameter_function(state, function));
    TEST_ASSERT_EQUAL_UINT32(1u, function->childFunctionLength);
    TEST_ASSERT_EQUAL_UINT32(
            1u, function->childFunctionList[0].childFunctionLength);
    removedFunction =
            &function->childFunctionList[0].childFunctionList[0];
    TEST_ASSERT_EQUAL_UINT32(2u, removedFunction->parameterCount);
    TEST_ASSERT_NOT_NULL(removedFunction->typedLocalBindings);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_c_partial_passing_form_removed_owner",
            "aot_c/src",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    memset(&options, 0, sizeof(options));
    options.moduleName = "aot_c_partial_passing_form_removed_owner";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "aot_c_partial_passing_form_removed_owner";
    options.requireExecutableLowering = ZR_TRUE;
    options.enableCodeStripping = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    generatedCText = read_text_file_owned_or_fail(generatedCPath);
    beforeMarker = strstr(
            generatedCText, "/* code_stripping.functionsBefore = ");
    afterMarker = strstr(
            generatedCText, "/* code_stripping.functionsAfter = ");
    removedMarker = strstr(
            generatedCText, "/* code_stripping.functionsRemoved = ");
    TEST_ASSERT_NOT_NULL(beforeMarker);
    TEST_ASSERT_NOT_NULL(afterMarker);
    TEST_ASSERT_NOT_NULL(removedMarker);
    TEST_ASSERT_EQUAL_INT(1, sscanf(
            beforeMarker,
            "/* code_stripping.functionsBefore = %u */",
            &before));
    TEST_ASSERT_EQUAL_INT(1, sscanf(
            afterMarker,
            "/* code_stripping.functionsAfter = %u */",
            &after));
    TEST_ASSERT_EQUAL_INT(1, sscanf(
            removedMarker,
            "/* code_stripping.functionsRemoved = %u */",
            &removed));
    TEST_ASSERT_EQUAL_UINT32(3u, before);
    TEST_ASSERT_EQUAL_UINT32(2u, after);
    TEST_ASSERT_EQUAL_UINT32(1u, removed);
    free(generatedCText);
    TEST_ASSERT_EQUAL_INT(0, remove(generatedCPath));

    TEST_ASSERT_EQUAL_UINT32(
            1u, passing_form_clear_parameter_slot(removedFunction, 1u));
    passing_form_assert_aot_write_rejected_without_output(
            state, function, generatedCPath, &options);

    removedFunction->typedLocalBindings[1].roleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    removedFunction->typedLocalBindings[0].roleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN;
    passing_form_assert_aot_write_rejected_without_output(
            state, function, generatedCPath, &options);

    removedFunction->typedLocalBindings[0].roleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    removedFunction->typedLocalBindings[2].roleFlags =
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    passing_form_assert_aot_write_rejected_without_output(
            state, function, generatedCPath, &options);

    removedFunction->typedLocalBindings[2].roleFlags = 1u << 31;
    passing_form_assert_aot_write_rejected_without_output(
            state, function, generatedCPath, &options);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

#endif
