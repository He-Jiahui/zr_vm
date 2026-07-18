#ifndef ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_METHOD_MAKE_H
#define ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_METHOD_MAKE_H

static void test_make_generic_method_object_resolves_and_materializes(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_UINT64,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN,
            },
    };
    SZrObject *methodObject;

    TEST_ASSERT_NOT_NULL(state);
    methodObject = ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments));
    TEST_ASSERT_NOT_NULL(methodObject);
    assert_object_string_field(state, methodObject, "kind", "constructedGenericMethod");
    assert_object_string_field(state, methodObject, "name", "Map");
    assert_object_int_field(
            state, methodObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    assert_object_int_field(
            state, methodObject, "genericMethodToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);

    arguments[1].typeToken = TEST_TYPE_DEF_TOKEN;
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    arguments[1].typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN;
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_SPEC_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            ZR_NULL,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            ZR_NULL,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments)));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodObject(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            ZR_NULL,
            ZR_ARRAY_COUNT(arguments)));

    destroy_reflection_test_state(state);
}

static void test_make_generic_method_from_objects_decodes_reflection_arguments(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrMetadataRuntime differentRuntime;
    SZrState *state = create_reflection_test_state();
    SZrObject *definitionObject;
    SZrObject *contextObject;
    SZrObject *argumentsArray;
    SZrObject *firstArgument;
    SZrObject *constructedObject;

    TEST_ASSERT_NOT_NULL(state);
    differentRuntime = *runtime;
    definitionObject = ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN);
    TEST_ASSERT_NOT_NULL(definitionObject);
    TEST_ASSERT_NOT_NULL(contextObject);
    argumentsArray = assert_object_object_field(
            state, contextObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    firstArgument = assert_array_object_entry(state, argumentsArray, 0u);

    constructedObject = ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, argumentsArray);
    TEST_ASSERT_NOT_NULL(constructedObject);
    assert_object_string_field(
            state, constructedObject, "kind", "constructedGenericMethod");
    assert_object_string_field(state, constructedObject, "name", "Map");
    assert_object_int_field(
            state, constructedObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    assert_object_int_field(
            state,
            assert_object_object_field(
                    state,
                    constructedObject,
                    "genericMethodDefinition",
                    ZR_VALUE_TYPE_OBJECT),
            "metadataToken",
            TEST_METHOD_CONTEXT_MEMBER_TOKEN);

    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, constructedObject, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, &differentRuntime, definitionObject, argumentsArray));
    set_object_int_field_value(
            state,
            firstArgument,
            "genericArgumentKindValue",
            ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION);
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, argumentsArray));
    set_object_int_field_value(
            state,
            firstArgument,
            "genericArgumentKindValue",
            ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE);
    set_object_int_field_value(
            state, firstArgument, "primitiveValueType", ZR_VALUE_TYPE_UNKNOWN);
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, argumentsArray));
    set_object_int_field_value(
            state, firstArgument, "primitiveValueType", ZR_VALUE_TYPE_UINT64);
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, contextObject));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            ZR_NULL, runtime, definitionObject, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, ZR_NULL, definitionObject, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, ZR_NULL, argumentsArray));
    TEST_ASSERT_NULL(ZrCore_Reflection_MakeGenericMethodFromObjects(
            state, runtime, definitionObject, ZR_NULL));

    destroy_reflection_test_state(state);
}

#endif
