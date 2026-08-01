#ifndef ZR_VM_TEST_AOT_PARAMETER_PASSING_FORM_ROUNDTRIP_CASES_H
#define ZR_VM_TEST_AOT_PARAMETER_PASSING_FORM_ROUNDTRIP_CASES_H

static const SZrFunctionTypedLocalBinding *find_typed_parameter_binding(
        const SZrFunction *function,
        const char *name) {
    TZrSize nameLength;

    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    nameLength = strlen(name);
    for (TZrUInt32 index = 0u;
         function->typedLocalBindings != ZR_NULL &&
         index < function->typedLocalBindingLength;
         index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[index];

        if (binding->stackSlot >= function->parameterCount ||
            binding->name == ZR_NULL ||
            ZrCore_String_GetByteLength(binding->name) != nameLength) {
            continue;
        }
        if (memcmp(ZrCore_String_GetNativeString(binding->name),
                   name,
                   nameLength) == 0) {
            return binding;
        }
    }
    return ZR_NULL;
}

static void assert_function_parameter_passing_roles(
        const SZrFunction *function,
        const char *const *names,
        const TZrUInt32 *expectedRoleFlags,
        TZrUInt32 parameterCount) {
    TZrUInt32 parameterBindingCount = 0u;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(parameterCount, function->parameterCount);
    for (TZrUInt32 index = 0u;
         function->typedLocalBindings != ZR_NULL &&
         index < function->typedLocalBindingLength;
         index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &function->typedLocalBindings[index];

        if (binding->stackSlot < function->parameterCount &&
            binding->name != ZR_NULL) {
            parameterBindingCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(parameterCount, parameterBindingCount);

    for (TZrUInt32 index = 0u; index < parameterCount; index++) {
        const SZrFunctionTypedLocalBinding *binding =
                find_typed_parameter_binding(function, names[index]);

        TEST_ASSERT_NOT_NULL(binding);
        TEST_ASSERT_EQUAL_UINT32(index, binding->stackSlot);
        TEST_ASSERT_EQUAL_HEX32(expectedRoleFlags[index], binding->roleFlags);
    }
}

static SZrFunction *passing_form_function_constant_at(
        SZrState *state,
        SZrFunction *function,
        TZrUInt32 constantIndex) {
    SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_NULL;
    }
    constant = &function->constantValueList[constantIndex];
    if (constant->type != ZR_VALUE_TYPE_FUNCTION ||
        constant->value.object == ZR_NULL || constant->isNative) {
        return ZR_NULL;
    }
    return ZR_CAST_FUNCTION(state, constant->value.object);
}

static const SZrFunctionTypedLocalBinding *passing_form_binding_at_slot(
        const SZrFunction *function,
        TZrUInt32 stackSlot) {
    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrUInt32 index = 0u;
         function->typedLocalBindings != ZR_NULL &&
         index < function->typedLocalBindingLength;
         index++) {
        if (function->typedLocalBindings[index].stackSlot == stackSlot) {
            return &function->typedLocalBindings[index];
        }
    }
    return ZR_NULL;
}

static SZrFunction *passing_form_find_instance_member_function(
        SZrState *state,
        SZrFunction *function) {
    const TZrByte *current;
    TZrSize remaining;

    if (state == ZR_NULL || function == ZR_NULL ||
        function->prototypeData == ZR_NULL ||
        function->prototypeDataLength <= sizeof(TZrUInt32)) {
        return ZR_NULL;
    }

    current = function->prototypeData + sizeof(TZrUInt32);
    remaining = function->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 prototypeIndex = 0u;
         prototypeIndex < function->prototypeCount;
         prototypeIndex++) {
        const SZrCompiledPrototypeInfo *prototype;
        const SZrCompiledMemberInfo *members;
        TZrSize prototypeSize;

        if (remaining < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_NULL;
        }
        prototype = (const SZrCompiledPrototypeInfo *)current;
        prototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                        prototype->inheritsCount * sizeof(TZrUInt32) +
                        prototype->decoratorsCount * sizeof(TZrUInt32) +
                        prototype->membersCount * sizeof(SZrCompiledMemberInfo);
        if (remaining < prototypeSize) {
            return ZR_NULL;
        }
        members = (const SZrCompiledMemberInfo *)(
                current + sizeof(SZrCompiledPrototypeInfo) +
                prototype->inheritsCount * sizeof(TZrUInt32) +
                prototype->decoratorsCount * sizeof(TZrUInt32));
        for (TZrUInt32 memberIndex = 0u;
             memberIndex < prototype->membersCount;
             memberIndex++) {
            SZrFunction *memberFunction = passing_form_function_constant_at(
                    state,
                    function,
                    members[memberIndex].functionConstantIndex);
            const SZrFunctionTypedLocalBinding *receiver =
                    passing_form_binding_at_slot(memberFunction, 0u);

            if (memberFunction != ZR_NULL &&
                memberFunction->parameterCount == 3u &&
                receiver != ZR_NULL &&
                (receiver->roleFlags &
                 ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER) != 0u) {
                return memberFunction;
            }
        }

        current += prototypeSize;
        remaining -= prototypeSize;
    }
    return ZR_NULL;
}

static void assert_instance_parameter_passing_roles(
        const SZrFunction *function) {
    const SZrFunctionTypedLocalBinding *receiver =
            passing_form_binding_at_slot(function, 0u);
    const SZrFunctionTypedLocalBinding *value =
            passing_form_binding_at_slot(function, 1u);
    const SZrFunctionTypedLocalBinding *input =
            passing_form_binding_at_slot(function, 2u);

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(3u, function->parameterCount);
    TEST_ASSERT_NOT_NULL(receiver);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_EQUAL_HEX32(
            ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER,
            receiver->roleFlags);
    TEST_ASSERT_EQUAL_HEX32(
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            value->roleFlags);
    TEST_ASSERT_EQUAL_HEX32(
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN,
            input->roleFlags);
}

static void test_parameter_passing_forms_roundtrip_into_exec_ir(void) {
    static const char source[] =
            "fn contract(value: int, input: in int, writable: ref int, "
            "observed: ref readonly int, local: scoped ref int, "
            "localView: scoped ref readonly int, result: out int): int { "
            "result = value; return value; } "
            "class Box { pub var raw: int; "
            "pub fn apply(value: int, input: in int): int { return value; } }";
    static const char *const names[] = {
            "value",
            "input",
            "writable",
            "observed",
            "local",
            "localView",
            "result",
    };
    static const TZrUInt32 expectedRoleFlags[] = {
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_IN,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_REF_READONLY,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_SCOPED_REF_READONLY,
            ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_OUT,
    };
#if defined(ZR_PLATFORM_UNIX)
    static const TZrUInt32 expectedExecIrForms[] = {
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_VALUE,
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_IN,
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF,
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_REF_READONLY,
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF,
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_SCOPED_REF_READONLY,
            ZR_AOT_EXEC_IR_PARAMETER_PASSING_FORM_OUT,
    };
#endif
    const char *binaryPath = "aot_parameter_passing_form_roundtrip.zro";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *sourceFunction;
    SZrFunction *sourceContract;
    SZrFunction *sourceInstanceMethod;
    SZrFunction *runtimeFunction;
    SZrFunction *runtimeContract;
    SZrFunction *runtimeInstanceMethod;
    TZrByte *binaryBytes;
    TZrSize binaryLength = 0u;
    SZrBinaryFixtureReader reader;
    SZrIo *io;
    SZrIoSource *sourceObject;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "aot_parameter_passing_form_roundtrip.zr",
            strlen("aot_parameter_passing_form_roundtrip.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    sourceFunction = ZrParser_Source_Compile(
            state, source, sizeof(source) - 1u, sourceName);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    sourceContract = find_named_function(sourceFunction, "contract");
    TEST_ASSERT_NOT_NULL(sourceContract);
    assert_function_parameter_passing_roles(
            sourceContract, names, expectedRoleFlags, 7u);
    sourceInstanceMethod = passing_form_find_instance_member_function(
            state, sourceFunction);
    assert_instance_parameter_passing_roles(sourceInstanceMethod);

    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFile(
            state, sourceFunction, binaryPath));
    binaryBytes = read_binary_file_owned(binaryPath, &binaryLength);
    TEST_ASSERT_NOT_NULL(binaryBytes);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)binaryLength);

    reader.bytes = binaryBytes;
    reader.length = binaryLength;
    reader.consumed = ZR_FALSE;
    io = ZrCore_Io_New(state->global);
    TEST_ASSERT_NOT_NULL(io);
    ZrCore_Io_Init(
            state,
            io,
            binary_fixture_reader_read,
            binary_fixture_reader_close,
            &reader);
    io->isBinary = ZR_TRUE;
    sourceObject = ZrCore_Io_ReadSourceNew(io);
    TEST_ASSERT_NOT_NULL(sourceObject);
    runtimeFunction = ZrCore_Io_LoadEntryFunctionToRuntime(state, sourceObject);
    TEST_ASSERT_NOT_NULL(runtimeFunction);
    runtimeContract = find_named_function(runtimeFunction, "contract");
    TEST_ASSERT_NOT_NULL(runtimeContract);
    assert_function_parameter_passing_roles(
            runtimeContract, names, expectedRoleFlags, 7u);
    runtimeInstanceMethod = passing_form_find_instance_member_function(
            state, runtimeFunction);
    assert_instance_parameter_passing_roles(runtimeInstanceMethod);

#if defined(ZR_PLATFORM_UNIX)
    {
        SZrAotExecIrFrameLayout frameLayout;
        SZrFunction projectedFunction = *runtimeContract;
        SZrFunctionTypedLocalBinding projectedBindings[7];

        TEST_ASSERT_EQUAL_UINT32(
                7u, runtimeContract->typedLocalBindingLength);
        memcpy(projectedBindings,
               runtimeContract->typedLocalBindings,
               sizeof(projectedBindings));
        for (TZrUInt32 index = 0u; index < 7u; index++) {
            projectedBindings[index].symbolId = 0u;
            projectedBindings[index].typeId = 0u;
            projectedBindings[index].placeId = 0u;
        }
        /* Reference identity/storage ABI remains a later A7.2 slice. */
        projectedFunction.typedLocalBindings = projectedBindings;
        projectedFunction.frameSlotLayouts = ZR_NULL;
        projectedFunction.frameSlotLayoutLength = 0u;
        projectedFunction.frameByteSize = 0u;
        projectedFunction.frameByteAlign = 0u;
        memset(&frameLayout, 0, sizeof(frameLayout));
        TEST_ASSERT_TRUE(backend_aot_exec_ir_build_frame_layout(
                state, &projectedFunction, &frameLayout));
        TEST_ASSERT_EQUAL_UINT32(
                7u, frameLayout.parameterLayoutCount);
        for (TZrUInt32 index = 0u; index < 7u; index++) {
            const SZrAotExecIrParameterLayout *layout =
                    &frameLayout.parameterLayouts[index];

            TEST_ASSERT_EQUAL_UINT32(index, layout->stackSlot);
            TEST_ASSERT_EQUAL_HEX32(0u, layout->roleFlags);
            TEST_ASSERT_EQUAL(ZR_TRUE, layout->passingFormKnown);
            TEST_ASSERT_EQUAL_UINT32(expectedExecIrForms[index],
                                     layout->passingForm);
            TEST_ASSERT_TRUE(
                    backend_aot_exec_ir_parameter_passing_form_is_valid(
                            layout));
        }
        backend_aot_exec_ir_release_frame_layout(state, &frameLayout);
    }
#endif

    ZrCore_Function_Free(state, runtimeFunction);
    ZrCore_Function_Free(state, sourceFunction);
    ZrCore_Io_Free(state->global, io);
    free(binaryBytes);
    TEST_ASSERT_EQUAL_INT(0, remove(binaryPath));
    destroy_test_state(state);
}

static void test_parameter_passing_role_stops_at_parameter_prefix(void) {
    static const char source[] =
            "fn project(value: int): int { "
            "{ var value: int = 2; value; } "
            "return value; }";
    SZrState *state = create_test_state();
    SZrString *sourceName;
    SZrFunction *sourceFunction;
    SZrFunction *projectFunction;
    TZrUInt32 sameNameBindingCount = 0u;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state,
            "aot_parameter_passing_form_shadow.zr",
            strlen("aot_parameter_passing_form_shadow.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    sourceFunction = ZrParser_Source_Compile(
            state, source, sizeof(source) - 1u, sourceName);
    TEST_ASSERT_NOT_NULL(sourceFunction);
    projectFunction = find_named_function(sourceFunction, "project");
    TEST_ASSERT_NOT_NULL(projectFunction);
    TEST_ASSERT_EQUAL_UINT32(1u, projectFunction->parameterCount);

    for (TZrUInt32 index = 0u;
         projectFunction->typedLocalBindings != ZR_NULL &&
         index < projectFunction->typedLocalBindingLength;
         index++) {
        const SZrFunctionTypedLocalBinding *binding =
                &projectFunction->typedLocalBindings[index];

        if (binding->name == ZR_NULL ||
            ZrCore_String_GetByteLength(binding->name) != strlen("value") ||
            memcmp(ZrCore_String_GetNativeString(binding->name),
                   "value",
                   strlen("value")) != 0) {
            continue;
        }

        sameNameBindingCount++;
        if (binding->stackSlot == 0u) {
            TEST_ASSERT_EQUAL_HEX32(
                    ZR_FUNCTION_TYPED_LOCAL_ROLE_PARAMETER_PASSING_VALUE,
                    binding->roleFlags);
        } else {
            TEST_ASSERT_EQUAL_HEX32(
                    ZR_FUNCTION_TYPED_LOCAL_ROLE_NONE,
                    binding->roleFlags);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(2u, sameNameBindingCount);

    ZrCore_Function_Free(state, sourceFunction);
    destroy_test_state(state);
}

#endif
