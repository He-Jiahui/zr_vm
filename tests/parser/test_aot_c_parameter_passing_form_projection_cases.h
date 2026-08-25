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

static const char *passing_form_readonly_aggregate_source(void) {
    return
            "readonly struct Snapshot {\n"
            "  pub var value: int;\n"
            "  pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "fn inspectIn(value: in Snapshot): int { var copied: int = value.value; return copied; }\n"
            "fn identity(value: int): int { return value; }\n"
            "fn inspectRef(value: ref readonly Snapshot): int { var copied: int = value.value; return copied; }\n"
            "fn inspectScoped(value: scoped ref readonly Snapshot): int { var copied: int = value.value; return copied; }\n"
            "fn inspectOffset(prefix: in int, value: in Snapshot): int { return prefix + value.value; }\n"
            "fn inspectUnused(value: in Snapshot): int { var copied: int = value.value; return copied; }\n"
            "var snapshot: Snapshot = init Snapshot(7);\n"
            "var inValue: int = inspectIn(snapshot);\n"
            "var scalarValue: int = identity(snapshot.value);\n"
            "var refValue: int = inspectRef(ref snapshot);\n"
            "var scopedValue: int = inspectScoped(ref snapshot);\n"
            "var offsetValue: int = inspectOffset(identity(1), snapshot);\n"
            "var repeatedOffsetValue: int = inspectOffset(2, snapshot);\n"
            "var temporaryValue: int = inspectIn(init Snapshot(2));\n"
            "return inValue + scalarValue + refValue + scopedValue + offsetValue + repeatedOffsetValue + temporaryValue;\n";
}

static TZrBool passing_form_is_call_with_arguments(
        EZrInstructionCode opcode) {
    return (TZrBool)(
            opcode == ZR_INSTRUCTION_ENUM(FUNCTION_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(KNOWN_VM_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(DYN_CALL) ||
            opcode == ZR_INSTRUCTION_ENUM(META_CALL));
}

static const SZrFunctionFrameSlotLayout *
passing_form_nth_call_argument_layout(
        const SZrFunction *function,
        TZrUInt32 argumentCount,
        TZrUInt32 occurrence,
        TZrUInt32 argumentIndex,
        TZrUInt32 *outStackSlot) {
    TZrUInt32 matchedCount = 0u;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0u; index < function->instructionsLength; index++) {
        const TZrInstruction *instruction = &function->instructionsList[index];
        const EZrInstructionCode opcode =
                (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 candidateArgumentCount;
        TZrUInt32 argumentSlot;

        if (!passing_form_is_call_with_arguments(opcode)) {
            continue;
        }
        candidateArgumentCount = instruction->instruction.operand.operand1[1];
        if (candidateArgumentCount != argumentCount ||
            argumentIndex >= candidateArgumentCount) {
            continue;
        }
        if (matchedCount++ != occurrence) {
            continue;
        }
        argumentSlot =
                (TZrUInt32)instruction->instruction.operand.operand1[0] +
                1u + argumentIndex;
        if (outStackSlot != ZR_NULL) {
            *outStackSlot = argumentSlot;
        }
        return ZrCore_Function_FindFrameSlotLayout(function, argumentSlot);
    }
    return ZR_NULL;
}

static SZrFunction *passing_form_find_runtime_function_named(
        SZrState *state,
        SZrFunction *function,
        const char *name,
        TZrUInt32 depth) {
    if (state == ZR_NULL || function == ZR_NULL || name == ZR_NULL ||
        depth > 32u) {
        return ZR_NULL;
    }
    if (passing_form_function_name_equals(function, name)) {
        return function;
    }
    for (TZrUInt32 childIndex = 0u;
         function->childFunctionList != ZR_NULL &&
         childIndex < function->childFunctionLength;
         childIndex++) {
        SZrFunction *found = passing_form_find_runtime_function_named(
                state,
                &function->childFunctionList[childIndex],
                name,
                depth + 1u);

        if (found != ZR_NULL) {
            return found;
        }
    }
    for (TZrUInt32 constantIndex = 0u;
         function->constantValueList != ZR_NULL &&
         constantIndex < function->constantValueLength;
         constantIndex++) {
        SZrTypeValue *constant = &function->constantValueList[constantIndex];
        SZrFunction *candidate;
        SZrFunction *found;

        if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
            constant->value.object == ZR_NULL || constant->isNative) {
            continue;
        }
        candidate = ZR_CAST_FUNCTION(state, constant->value.object);
        if (candidate == function) {
            continue;
        }
        found = passing_form_find_runtime_function_named(
                state, candidate, name, depth + 1u);
        if (found != ZR_NULL) {
            return found;
        }
    }
    return ZR_NULL;
}

static SZrFunctionFrameSlotLayout *passing_form_readonly_parameter_layout(
        SZrState *state,
        SZrFunction *entryFunction,
        const char *functionName,
        TZrUInt32 stackSlot) {
    SZrFunction *function = passing_form_find_runtime_function_named(
            state, entryFunction, functionName, 0u);

    TEST_ASSERT_NOT_NULL(function);
    return (SZrFunctionFrameSlotLayout *)ZrCore_Function_FindFrameSlotLayout(
            function, stackSlot);
}

static void passing_form_assert_readonly_parameter_borrowed(
        SZrState *state,
        SZrFunction *entryFunction,
        const char *functionName,
        TZrUInt32 stackSlot) {
    SZrAotExecIrFrameLayout frameLayout;
    SZrFunction *function = passing_form_find_runtime_function_named(
            state, entryFunction, functionName, 0u);
    const SZrFunctionFrameSlotLayout *layout =
            passing_form_readonly_parameter_layout(
                    state, entryFunction, functionName, stackSlot);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_TRUE_MESSAGE(layout->isParameter, functionName);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
            layout->slotKind,
            functionName);
    TEST_ASSERT_BITS_HIGH_MESSAGE(
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
                    ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS,
            layout->reserved0,
            functionName);
#if defined(ZR_PLATFORM_UNIX)
    memset(&frameLayout, 0, sizeof(frameLayout));
    TEST_ASSERT_TRUE_MESSAGE(
            backend_aot_exec_ir_build_frame_layout(
                    state, function, &frameLayout),
            functionName);
    backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
#else
    (void)frameLayout;
#endif
}

#if defined(ZR_PLATFORM_UNIX)
static void passing_form_assert_readonly_argument_marker_requires_call_window(
        SZrState *state) {
    SZrCompilerState compilerState;
    SZrCompilerStackSlotTypeHint bareHint;
    SZrCompilerStackSlotTypeHint *storedHint;

    memset(&compilerState, 0, sizeof(compilerState));
    memset(&bareHint, 0, sizeof(bareHint));
    compilerState.state = state;
    ZrCore_Array_Init(
            state,
            &compilerState.stackSlotTypeHints,
            sizeof(SZrCompilerStackSlotTypeHint),
            4u);
    bareHint.stackSlot = 5u;
    ZrCore_Array_Push(
            state, &compilerState.stackSlotTypeHints, &bareHint);
    TEST_ASSERT_TRUE(
            compiler_register_stack_slot_readonly_aggregate_argument(
                    &compilerState, 5u));
    storedHint = (SZrCompilerStackSlotTypeHint *)ZrCore_Array_Get(
            &compilerState.stackSlotTypeHints, 0u);
    TEST_ASSERT_NOT_NULL(storedHint);
    TEST_ASSERT_FALSE(storedHint->isReadonlyAggregateArgument);

    TEST_ASSERT_TRUE(compiler_register_isolated_call_window_slot(
            &compilerState, 10u, 10u, 1u));
    TEST_ASSERT_TRUE(compiler_register_isolated_call_window_slot(
            &compilerState, 11u, 10u, 1u));
    TEST_ASSERT_TRUE(
            compiler_register_stack_slot_readonly_aggregate_argument(
                    &compilerState, 11u));
    storedHint = (SZrCompilerStackSlotTypeHint *)ZrCore_Array_Get(
            &compilerState.stackSlotTypeHints, 2u);
    TEST_ASSERT_NOT_NULL(storedHint);
    TEST_ASSERT_TRUE(storedHint->isReadonlyAggregateArgument);
    ZrCore_Array_Free(state, &compilerState.stackSlotTypeHints);
}
#endif

static void test_aot_readonly_aggregate_parameters_use_borrowed_frame_storage(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
#if defined(ZR_PLATFORM_UNIX)
    SZrAotExecIrModule module;
#endif
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    TZrChar binaryPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedLlvmPath[ZR_TESTS_PATH_MAX];
    TZrInt64 result = 0;
    TZrUInt32 readonlyArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 reusedReadonlyArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 scalarArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 nestedScalarArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 offsetScalarArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 offsetReadonlyArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 repeatedOffsetScalarArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 repeatedOffsetReadonlyArgumentSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 temporaryConstructorReceiverSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 temporaryReadonlyArgumentSlot = ZR_PARSER_SLOT_NONE;
    const SZrFunctionFrameSlotLayout *readonlyArgumentLayout;
    const SZrFunctionFrameSlotLayout *reusedReadonlyArgumentLayout;
    const SZrFunctionFrameSlotLayout *scalarArgumentLayout;
    const SZrFunctionFrameSlotLayout *nestedScalarArgumentLayout;
    const SZrFunctionFrameSlotLayout *offsetScalarArgumentLayout;
    const SZrFunctionFrameSlotLayout *offsetReadonlyArgumentLayout;
    const SZrFunctionFrameSlotLayout *repeatedOffsetScalarArgumentLayout;
    const SZrFunctionFrameSlotLayout *repeatedOffsetReadonlyArgumentLayout;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state,
            passing_form_readonly_aggregate_source(),
            "aot_readonly_aggregate_parameter_storage.zr");
    TEST_ASSERT_NOT_NULL(function);

    passing_form_assert_readonly_parameter_borrowed(
            state, function, "inspectIn", 0u);
    passing_form_assert_readonly_parameter_borrowed(
            state, function, "inspectRef", 0u);
    passing_form_assert_readonly_parameter_borrowed(
            state, function, "inspectScoped", 0u);
    passing_form_assert_readonly_parameter_borrowed(
            state, function, "inspectOffset", 1u);
    readonlyArgumentLayout = passing_form_nth_call_argument_layout(
            function, 1u, 0u, 0u, &readonlyArgumentSlot);
    scalarArgumentLayout = passing_form_nth_call_argument_layout(
            function, 1u, 1u, 0u, &scalarArgumentSlot);
    reusedReadonlyArgumentLayout = passing_form_nth_call_argument_layout(
            function, 1u, 2u, 0u, &reusedReadonlyArgumentSlot);
    nestedScalarArgumentLayout = passing_form_nth_call_argument_layout(
            function, 1u, 4u, 0u, &nestedScalarArgumentSlot);
    offsetScalarArgumentLayout = passing_form_nth_call_argument_layout(
            function, 2u, 1u, 0u, &offsetScalarArgumentSlot);
    offsetReadonlyArgumentLayout = passing_form_nth_call_argument_layout(
            function, 2u, 1u, 1u, &offsetReadonlyArgumentSlot);
    repeatedOffsetScalarArgumentLayout = passing_form_nth_call_argument_layout(
            function, 2u, 2u, 0u, &repeatedOffsetScalarArgumentSlot);
    repeatedOffsetReadonlyArgumentLayout = passing_form_nth_call_argument_layout(
            function, 2u, 2u, 1u, &repeatedOffsetReadonlyArgumentSlot);
    TEST_ASSERT_NOT_NULL(passing_form_nth_call_argument_layout(
            function,
            2u,
            3u,
            0u,
            &temporaryConstructorReceiverSlot));
    TEST_ASSERT_NOT_NULL(passing_form_nth_call_argument_layout(
            function,
            1u,
            5u,
            0u,
            &temporaryReadonlyArgumentSlot));
    TEST_ASSERT_NOT_NULL(readonlyArgumentLayout);
    TEST_ASSERT_NOT_NULL(reusedReadonlyArgumentLayout);
    TEST_ASSERT_NOT_NULL(scalarArgumentLayout);
    TEST_ASSERT_NOT_NULL(nestedScalarArgumentLayout);
    TEST_ASSERT_NOT_NULL(offsetScalarArgumentLayout);
    TEST_ASSERT_NOT_NULL(offsetReadonlyArgumentLayout);
    TEST_ASSERT_NOT_NULL(repeatedOffsetScalarArgumentLayout);
    TEST_ASSERT_NOT_NULL(repeatedOffsetReadonlyArgumentLayout);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
            readonlyArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
            reusedReadonlyArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
            scalarArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
            nestedScalarArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
            offsetScalarArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
            offsetReadonlyArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_VALUE,
            repeatedOffsetScalarArgumentLayout->slotKind);
    TEST_ASSERT_EQUAL_UINT8(
            ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT,
            repeatedOffsetReadonlyArgumentLayout->slotKind);
    TEST_ASSERT_NOT_EQUAL(readonlyArgumentSlot, scalarArgumentSlot);
    TEST_ASSERT_NOT_EQUAL(
            nestedScalarArgumentSlot, offsetReadonlyArgumentSlot);
    TEST_ASSERT_EQUAL_UINT32(
            offsetScalarArgumentSlot, repeatedOffsetScalarArgumentSlot);
    TEST_ASSERT_EQUAL_UINT32(
            offsetReadonlyArgumentSlot,
            repeatedOffsetReadonlyArgumentSlot);
    TEST_ASSERT_NOT_EQUAL(
            temporaryConstructorReceiverSlot,
            temporaryReadonlyArgumentSlot);
    TEST_ASSERT_EQUAL_UINT32(
            readonlyArgumentSlot, reusedReadonlyArgumentSlot);
#if defined(ZR_PLATFORM_UNIX)
    memset(&module, 0, sizeof(module));
    TEST_ASSERT_TRUE_MESSAGE(
            backend_aot_exec_ir_build_module(state, function, &module),
            "readonly aggregate ExecIR module build failed");
    backend_aot_exec_ir_release_module(state, &module);
#endif
    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Runtime_Function_ExecuteExpectInt64(
                    state, function, &result),
            "readonly aggregate interpreter execution failed");
    TEST_ASSERT_EQUAL_INT64(47, result);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_readonly_aggregate_parameter_storage",
            "binary",
            "main",
            ".zro",
            binaryPath,
            sizeof(binaryPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_readonly_aggregate_parameter_storage",
            "aot_c",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_readonly_aggregate_parameter_storage",
            "aot_llvm",
            "main",
            ".ll",
            generatedLlvmPath,
            sizeof(generatedLlvmPath)));
    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "aot_readonly_aggregate_parameter_storage";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(
            state, function, binaryPath, &binaryOptions));
    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "aot_readonly_aggregate_parameter_storage";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    aotOptions.inputHash = "readonly-aggregate-parameter-storage";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &aotOptions));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotLlvmFileWithOptions(
            state, function, generatedLlvmPath, &aotOptions));

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_aot_exec_ir_rejects_readonly_aggregate_parameter_storage_role_mismatch(void) {
#if !defined(ZR_PLATFORM_UNIX)
    TEST_IGNORE_MESSAGE(
            "private ExecIR frame validation symbols are not exported by the Windows parser DLL");
#else
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *callee;
    SZrFunction *offsetCallee;
    SZrFunctionTypedLocalBinding *binding = ZR_NULL;
    SZrFunctionFrameSlotLayout *layout;
    SZrAotExecIrFrameLayout frameLayout;
    TZrUInt32 originalRoleFlags;
    TZrUInt32 originalTypeLayoutId;
    TZrUInt32 originalByteSize;
    TZrUInt32 originalByteAlign;
    TZrUInt8 originalSlotKind;
    TZrUInt8 originalIsParameter;
    TZrUInt16 originalLayoutFlags;
    TZrUInt32 originalFrameSlotLayoutLength;
    TZrBool built;

    TEST_ASSERT_NOT_NULL(state);
    passing_form_assert_readonly_argument_marker_requires_call_window(state);
    function = compile_source(
            state,
            passing_form_readonly_aggregate_source(),
            "aot_readonly_aggregate_parameter_validation.zr");
    TEST_ASSERT_NOT_NULL(function);
    callee = passing_form_find_named_function(function, "inspectIn");
    TEST_ASSERT_NOT_NULL(callee);
    layout = (SZrFunctionFrameSlotLayout *)
            ZrCore_Function_FindFrameSlotLayout(callee, 0u);
    TEST_ASSERT_NOT_NULL(layout);
    for (TZrUInt32 bindingIndex = 0u;
         callee->typedLocalBindings != ZR_NULL &&
         bindingIndex < callee->typedLocalBindingLength;
         bindingIndex++) {
        if (callee->typedLocalBindings[bindingIndex].stackSlot == 0u) {
            binding = &callee->typedLocalBindings[bindingIndex];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(binding);
    originalLayoutFlags = layout->reserved0;
    originalTypeLayoutId = layout->typeLayoutId;
    originalByteSize = layout->byteSize;
    originalByteAlign = layout->byteAlign;
    originalSlotKind = layout->slotKind;
    originalIsParameter = layout->isParameter;
    originalRoleFlags = binding->roleFlags;
    layout->reserved0 &= (TZrUInt16)~(
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS);

    memset(&frameLayout, 0, sizeof(frameLayout));
    built = backend_aot_exec_ir_build_frame_layout(
            state, callee, &frameLayout);
    if (built) {
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
    TEST_ASSERT_FALSE(built);

    layout->reserved0 = 0u;
    layout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout->slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout->byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    memset(&frameLayout, 0, sizeof(frameLayout));
    built = backend_aot_exec_ir_build_frame_layout(
            state, callee, &frameLayout);
    if (built) {
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
    TEST_ASSERT_FALSE(built);

    layout->reserved0 = originalLayoutFlags;
    layout->typeLayoutId = originalTypeLayoutId;
    layout->slotKind = originalSlotKind;
    layout->byteSize = originalByteSize;
    layout->byteAlign = originalByteAlign;
    binding->roleFlags =
            (originalRoleFlags &
             ~ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_MASK) |
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE;
    memset(&frameLayout, 0, sizeof(frameLayout));
    built = backend_aot_exec_ir_build_frame_layout(
            state, callee, &frameLayout);
    if (built) {
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
    TEST_ASSERT_FALSE(built);

    binding->roleFlags = originalRoleFlags;
    offsetCallee = passing_form_find_named_function(
            function, "inspectOffset");
    TEST_ASSERT_NOT_NULL(offsetCallee);
    TEST_ASSERT_GREATER_THAN_UINT32(
            1u, offsetCallee->frameSlotLayoutLength);
    originalFrameSlotLayoutLength = offsetCallee->frameSlotLayoutLength;
    offsetCallee->frameSlotLayoutLength = 1u;
    memset(&frameLayout, 0, sizeof(frameLayout));
    built = backend_aot_exec_ir_build_frame_layout(
            state, offsetCallee, &frameLayout);
    if (built) {
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
    TEST_ASSERT_FALSE(built);
    offsetCallee->frameSlotLayoutLength = originalFrameSlotLayoutLength;

    offsetCallee->frameSlotLayoutLength = 0u;
    memset(&frameLayout, 0, sizeof(frameLayout));
    built = backend_aot_exec_ir_build_frame_layout(
            state, offsetCallee, &frameLayout);
    if (built) {
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
    TEST_ASSERT_FALSE(built);
    offsetCallee->frameSlotLayoutLength = originalFrameSlotLayoutLength;

    TEST_ASSERT_GREATER_THAN_UINT32(1u, callee->frameSlotLayoutLength);
    originalFrameSlotLayoutLength = callee->frameSlotLayoutLength;
    callee->frameSlotLayoutLength = 1u;
    layout->reserved0 = 0u;
    layout->typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    layout->slotKind = (TZrUInt8)ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;
    layout->byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    layout->byteAlign = (TZrUInt32)_Alignof(SZrTypeValue);
    layout->isParameter = 0u;
    memset(&frameLayout, 0, sizeof(frameLayout));
    built = backend_aot_exec_ir_build_frame_layout(
            state, callee, &frameLayout);
    if (built) {
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
    TEST_ASSERT_FALSE(built);
    callee->frameSlotLayoutLength = originalFrameSlotLayoutLength;
    layout->reserved0 = originalLayoutFlags;
    layout->typeLayoutId = originalTypeLayoutId;
    layout->slotKind = originalSlotKind;
    layout->byteSize = originalByteSize;
    layout->byteAlign = originalByteAlign;
    layout->isParameter = originalIsParameter;

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
#endif
}

static void test_aot_code_writers_reject_unreachable_readonly_aggregate_storage_mismatch_before_stripping(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrFunction *function;
    SZrFunction *callee;
    SZrFunctionFrameSlotLayout *layout;
    SZrAotWriterOptions options;
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedLlvmPath[ZR_TESTS_PATH_MAX];
    FILE *unexpectedArtifact;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state,
            passing_form_readonly_aggregate_source(),
            "aot_unreachable_readonly_aggregate_parameter_storage.zr");
    TEST_ASSERT_NOT_NULL(function);
    callee = passing_form_find_runtime_function_named(
            state, function, "inspectUnused", 0u);
    TEST_ASSERT_NOT_NULL(callee);
    layout = (SZrFunctionFrameSlotLayout *)
            ZrCore_Function_FindFrameSlotLayout(callee, 0u);
    TEST_ASSERT_NOT_NULL(layout);
    layout->reserved0 &= (TZrUInt16)~(
            ZR_FUNCTION_FRAME_SLOT_FLAG_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_INDIRECT_ALIAS |
            ZR_FUNCTION_FRAME_SLOT_FLAG_BORROWED_ALIAS);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_unreachable_readonly_aggregate_parameter_storage",
            "aot_c",
            "main",
            ".c",
            generatedCPath,
            sizeof(generatedCPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "aot_unreachable_readonly_aggregate_parameter_storage",
            "aot_llvm",
            "main",
            ".ll",
            generatedLlvmPath,
            sizeof(generatedLlvmPath)));
    memset(&options, 0, sizeof(options));
    options.moduleName =
            "aot_unreachable_readonly_aggregate_parameter_storage";
    options.inputKind = ZR_AOT_INPUT_KIND_SOURCE;
    options.inputHash = "unreachable-readonly-aggregate-storage";
    options.enableCodeStripping = ZR_TRUE;
    options.requireExecutableLowering = ZR_TRUE;

    remove(generatedCPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotCFileWithOptions(
            state, function, generatedCPath, &options));
    unexpectedArtifact = fopen(generatedCPath, "rb");
    if (unexpectedArtifact != ZR_NULL) {
        fclose(unexpectedArtifact);
    }
    TEST_ASSERT_NULL(unexpectedArtifact);

    remove(generatedLlvmPath);
    TEST_ASSERT_FALSE(ZrParser_Writer_WriteAotLlvmFileWithOptions(
            state, function, generatedLlvmPath, &options));
    unexpectedArtifact = fopen(generatedLlvmPath, "rb");
    if (unexpectedArtifact != ZR_NULL) {
        fclose(unexpectedArtifact);
    }
    TEST_ASSERT_NULL(unexpectedArtifact);

    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
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
