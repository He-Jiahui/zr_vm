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

static void test_aot_c_reference_generic_call_typed_rejects_receiver_parameter_layout(void) {
    char *generatedCText = write_parameter_layout_case(
            "aot_c_reference_generic_call_typed_receiver_parameter",
            "aot_c_generic_call_typed_receiver_parameter.zr",
            parameter_layout_receiver_generic_source(),
            ZR_NULL,
            ZR_FALSE);

    TEST_ASSERT_NOT_NULL(strstr(
            generatedCText, "/* zr_aot_generic_call_typed_shared_method_slot */"));
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

#endif
