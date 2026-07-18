#ifndef ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_METHOD_CONTEXT_H
#define ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_METHOD_CONTEXT_H

#define TEST_METHOD_CONTEXT_MEMBER_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_METHOD_CONTEXT_MEMBER_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 21u)
#define TEST_METHOD_CONTEXT_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 22u)
#define TEST_METHOD_CONTEXT_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 5u)
#define TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 23u)
#define TEST_METHOD_CONTEXT_METHOD_NAME_OFFSET 0u
#define TEST_METHOD_CONTEXT_PARAMETER0_NAME_OFFSET 4u
#define TEST_METHOD_CONTEXT_PARAMETER1_NAME_OFFSET 9u

static const TZrByte TEST_METHOD_CONTEXT_STRING_POOL[] = {
        'M', 'a', 'p', 0,
        'T', 'K', 'e', 'y', 0,
        'T', 'V', 'a', 'l', 'u', 'e', 0,
};

static const TZrByte TEST_METHOD_CONTEXT_SPEC_SIGNATURE[] = {
        ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
        ZR_METADATA_SIGNATURE_NODE_MEMBER_REF,
        (TZrByte)(TEST_METHOD_CONTEXT_MEMBER_TOKEN & 0xFFu),
        (TZrByte)((TEST_METHOD_CONTEXT_MEMBER_TOKEN >> 8u) & 0xFFu),
        (TZrByte)((TEST_METHOD_CONTEXT_MEMBER_TOKEN >> 16u) & 0xFFu),
        (TZrByte)((TEST_METHOD_CONTEXT_MEMBER_TOKEN >> 24u) & 0xFFu),
        2u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
        (TZrByte)ZR_VALUE_TYPE_UINT64, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        41u, 0u, 0u, 0u,
};

static const TZrByte TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE[] = {
        ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        41u, 0u, 0u, 0u,
};

typedef struct SMethodSpecGenericContextFixture {
    SZrObjectModule module;
    SZrFunction metadataFunction;
    SZrAotCodeRegistration registration;
    SZrMetadataTokenRecord functionRecords[2];
    SZrMetadataTokenRecord moduleRecords[3];
    TZrByte metadataBytes[
            ZR_ZRP_METADATA_HEADER_SIZE +
            sizeof(SZrZrpMetadataTypeDefRow) +
            sizeof(SZrZrpMetadataMethodDefRow) +
            (2u * sizeof(SZrZrpMetadataGenericParamRow)) +
            sizeof(TEST_METHOD_CONTEXT_STRING_POOL) +
            sizeof(TEST_METHOD_CONTEXT_SPEC_SIGNATURE) +
            sizeof(TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE)];
} SMethodSpecGenericContextFixture;

static SZrMetadataRuntime *method_spec_generic_context_fixture_init(
        SMethodSpecGenericContextFixture *fixture) {
    TZrByte signaturePayload[
            sizeof(TEST_METHOD_CONTEXT_SPEC_SIGNATURE) +
            sizeof(TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE)] = {0};
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefRows;
    SZrZrpMetadataMethodDefRow *methodDefRows;
    SZrZrpMetadataGenericParamRow *genericParamRows;
    TZrUInt32 nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    SZrMetadataRuntime *runtime;

    TEST_ASSERT_NOT_NULL(fixture);
    memset(fixture, 0, sizeof(*fixture));
    fixture->functionRecords[0].token = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    fixture->functionRecords[0].relatedToken = TEST_METHOD_CONTEXT_MEMBER_SIGNATURE_TOKEN;
    fixture->functionRecords[1].token = TEST_METHOD_CONTEXT_SPEC_TOKEN;
    fixture->functionRecords[1].relatedToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    fixture->functionRecords[1].ownerToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    fixture->functionRecords[1].signatureBlobOffset = 0u;
    fixture->functionRecords[1].signatureBlobLength =
            (TZrUInt32)sizeof(TEST_METHOD_CONTEXT_SPEC_SIGNATURE);
    fixture->functionRecords[1].signatureHash = 0x1020304050607080ULL;
    fixture->metadataFunction.metadataTokenRecords = fixture->functionRecords;
    fixture->metadataFunction.metadataTokenRecordLength =
            ZR_ARRAY_COUNT(fixture->functionRecords);

    fixture->moduleRecords[0].token = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN;
    fixture->moduleRecords[0].relatedToken = TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE_TOKEN;
    fixture->moduleRecords[1].token = TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE_TOKEN;
    fixture->moduleRecords[1].relatedToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN;
    fixture->moduleRecords[1].ownerToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN;
    fixture->moduleRecords[1].signatureBlobOffset =
            (TZrUInt32)sizeof(TEST_METHOD_CONTEXT_SPEC_SIGNATURE);
    fixture->moduleRecords[1].signatureBlobLength =
            (TZrUInt32)sizeof(TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE);
    fixture->moduleRecords[2] = fixture->functionRecords[1];
    fixture->metadataFunction.moduleMetadataTokenRecords = fixture->moduleRecords;
    fixture->metadataFunction.moduleMetadataTokenRecordLength =
            ZR_ARRAY_COUNT(fixture->moduleRecords);

    memcpy(signaturePayload,
           TEST_METHOD_CONTEXT_SPEC_SIGNATURE,
           sizeof(TEST_METHOD_CONTEXT_SPEC_SIGNATURE));
    memcpy(signaturePayload + sizeof(TEST_METHOD_CONTEXT_SPEC_SIGNATURE),
           TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE,
           sizeof(TEST_METHOD_CONTEXT_TYPE_REF_SIGNATURE));
    ZrCore_ZrpMetadata_InitHeader(&header);
    set_counted_section(
            &header.typeDefs,
            &nextOffset,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(
            &header.methodDefs,
            &nextOffset,
            1u,
            (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    set_counted_section(
            &header.genericParams,
            &nextOffset,
            2u,
            (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow));
    set_counted_section(
            &header.stringPool,
            &nextOffset,
            (TZrUInt32)sizeof(TEST_METHOD_CONTEXT_STRING_POOL),
            1u);
    set_counted_section(
            &header.signatureBlobPool,
            &nextOffset,
            (TZrUInt32)sizeof(signaturePayload),
            1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(
            fixture->metadataBytes, sizeof(fixture->metadataBytes), &header));
    typeDefRows = (SZrZrpMetadataTypeDefRow *)(void *)(
            fixture->metadataBytes + header.typeDefs.offset);
    methodDefRows = (SZrZrpMetadataMethodDefRow *)(void *)(
            fixture->metadataBytes + header.methodDefs.offset);
    genericParamRows = (SZrZrpMetadataGenericParamRow *)(void *)(
            fixture->metadataBytes + header.genericParams.offset);
    typeDefRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeDefRows[0].firstMethodDefIndex = 0u;
    typeDefRows[0].methodDefCount = 1u;
    methodDefRows[0].token = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    methodDefRows[0].ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    methodDefRows[0].nameStringOffset = TEST_METHOD_CONTEXT_METHOD_NAME_OFFSET;
    methodDefRows[0].functionIndex = 1u;
    methodDefRows[0].firstGenericParamIndex = 0u;
    methodDefRows[0].genericParamCount = 2u;
    genericParamRows[0].ownerToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    genericParamRows[0].nameStringOffset = TEST_METHOD_CONTEXT_PARAMETER0_NAME_OFFSET;
    genericParamRows[0].parameterIndex = 0u;
    genericParamRows[1].ownerToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    genericParamRows[1].nameStringOffset = TEST_METHOD_CONTEXT_PARAMETER1_NAME_OFFSET;
    genericParamRows[1].parameterIndex = 1u;
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(
            fixture->metadataBytes,
            sizeof(fixture->metadataBytes),
            &header,
            ZR_ZRP_METADATA_SECTION_STRING_POOL,
            TEST_METHOD_CONTEXT_STRING_POOL,
            (TZrUInt32)sizeof(TEST_METHOD_CONTEXT_STRING_POOL)));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(
            fixture->metadataBytes,
            sizeof(fixture->metadataBytes),
            &header,
            ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
            signaturePayload,
            (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(
            &fixture->module,
            &fixture->metadataFunction,
            &fixture->registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(
            runtime, fixture->metadataBytes, sizeof(fixture->metadataBytes)));
    return runtime;
}

static void assert_resolved_generic_method_spec_cleared(
        const SZrReflectionResolvedGenericMethodSpec *resolved) {
    TEST_ASSERT_EQUAL_UINT32(0u, resolved->methodSpecToken);
    TEST_ASSERT_NULL(resolved->methodSpecRecord);
    TEST_ASSERT_EQUAL_UINT32(0u, resolved->genericMethodToken);
    TEST_ASSERT_NULL(resolved->genericMethodRecord);
    TEST_ASSERT_EQUAL_UINT64(0u, resolved->genericSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(0u, resolved->genericArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(0u, resolved->genericArgumentListBlobOffset);
    TEST_ASSERT_NULL(resolved->requestedArguments);
}

static void test_constructed_generic_method_resolves_existing_method_spec(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_UINT64,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN,
            },
    };
    SZrReflectionResolvedGenericMethodSpec resolved;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments),
            &resolved));
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_CONTEXT_SPEC_TOKEN, resolved.methodSpecToken);
    TEST_ASSERT_EQUAL_PTR(&fixture.functionRecords[1], resolved.methodSpecRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_CONTEXT_MEMBER_TOKEN, resolved.genericMethodToken);
    TEST_ASSERT_EQUAL_PTR(&fixture.functionRecords[0], resolved.genericMethodRecord);
    TEST_ASSERT_EQUAL_UINT64(0x1020304050607080ULL, resolved.genericSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(2u, resolved.genericArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(10u, resolved.genericArgumentListBlobOffset);
    TEST_ASSERT_EQUAL_PTR(arguments, resolved.requestedArguments);
}

static void test_constructed_generic_method_resolves_module_metadata_method_spec(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_UINT64,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN,
            },
    };
    SZrReflectionResolvedGenericMethodSpec resolved;

    fixture.metadataFunction.metadataTokenRecordLength = 1u;
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments),
            &resolved));
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_CONTEXT_SPEC_TOKEN, resolved.methodSpecToken);
    TEST_ASSERT_EQUAL_PTR(&fixture.moduleRecords[2], resolved.methodSpecRecord);
    TEST_ASSERT_EQUAL_PTR(arguments, resolved.requestedArguments);
}

static void test_constructed_generic_method_rejects_mismatch_and_clears_output(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
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
    SZrReflectionResolvedGenericMethodSpec resolved;

    memset(&resolved, 0xA5, sizeof(resolved));
    arguments[0].primitiveValueType = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    arguments[0].primitiveValueType = ZR_VALUE_TYPE_UINT64;
    arguments[1].typeToken = TEST_TYPE_DEF_TOKEN;
    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    arguments[1].typeToken = TEST_METHOD_CONTEXT_TYPE_REF_TOKEN;
    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 1u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN, arguments, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    fixture.functionRecords[1].ownerToken = 0u;
    fixture.moduleRecords[2].ownerToken = 0u;
    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    arguments[0].kind = (EZrReflectionGenericTypeArgumentKind)0;
    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    fixture.functionRecords[1].ownerToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    fixture.moduleRecords[2].ownerToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;
    arguments[0].kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE;
    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            ZR_NULL, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, ZR_NULL, 2u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);

    memset(&resolved, 0xA5, sizeof(resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 0u, &resolved));
    assert_resolved_generic_method_spec_cleared(&resolved);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN, arguments, 2u, ZR_NULL));
}

static void test_constructed_generic_method_object_links_definition_and_arguments(void) {
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
    SZrReflectionResolvedGenericMethodSpec resolved;
    SZrObject *methodObject;
    SZrObject *definitionObject;
    SZrObject *argumentsArray;
    const SZrTypeValue *signatureHashValue;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericMethod(
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            arguments,
            ZR_ARRAY_COUNT(arguments),
            &resolved));
    methodObject = ZrCore_Reflection_BuildConstructedGenericMethodObject(
            state, runtime, &resolved);
    TEST_ASSERT_NOT_NULL(methodObject);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, methodObject));
    assert_object_string_field(state, methodObject, "kind", "constructedGenericMethod");
    assert_object_string_field(state, methodObject, "name", "Map");
    assert_object_bool_field(state, methodObject, "isGenericMethod", ZR_TRUE);
    assert_object_bool_field(state, methodObject, "isGenericMethodDefinition", ZR_FALSE);
    assert_object_bool_field(state, methodObject, "isConstructedGenericMethod", ZR_TRUE);
    assert_object_int_field(
            state, methodObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    assert_object_int_field(
            state, methodObject, "genericMethodToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    assert_object_int_field(state, methodObject, "genericArgumentCount", 2);
    assert_object_native_pointer_field(state, methodObject, "metadataRuntime", runtime);
    signatureHashValue = get_object_field_value(state, methodObject, "genericSignatureHash");
    TEST_ASSERT_NOT_NULL(signatureHashValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, signatureHashValue->type);
    TEST_ASSERT_EQUAL_UINT64(
            fixture.functionRecords[1].signatureHash,
            signatureHashValue->value.nativeObject.nativeUInt64);

    argumentsArray = assert_object_object_field(
            state, methodObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    assert_object_int_field(
            state,
            assert_array_object_entry(state, argumentsArray, 0u),
            "primitiveValueType",
            ZR_VALUE_TYPE_UINT64);
    assert_object_int_field(
            state,
            assert_array_object_entry(state, argumentsArray, 1u),
            "typeToken",
            TEST_METHOD_CONTEXT_TYPE_REF_TOKEN);

    definitionObject = assert_object_object_field(
            state, methodObject, "genericMethodDefinition", ZR_VALUE_TYPE_OBJECT);
    assert_object_string_field(state, definitionObject, "kind", "genericMethodDefinition");
    assert_object_string_field(state, definitionObject, "name", "Map");
    assert_object_int_field(
            state, definitionObject, "metadataToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);

    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject)));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    argumentsArray = assert_object_object_field(
            state, methodObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    definitionObject = assert_object_object_field(
            state, methodObject, "genericMethodDefinition", ZR_VALUE_TYPE_OBJECT);
    assert_object_string_field(state, definitionObject, "name", "Map");
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject)));

    resolved.genericSignatureHash ^= 1u;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildConstructedGenericMethodObject(
            state, runtime, &resolved));
    resolved.genericSignatureHash ^= 1u;
    arguments[0].primitiveValueType = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildConstructedGenericMethodObject(
            state, runtime, &resolved));
    arguments[0].primitiveValueType = ZR_VALUE_TYPE_UINT64;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildConstructedGenericMethodObject(
            ZR_NULL, runtime, &resolved));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildConstructedGenericMethodObject(
            state, ZR_NULL, &resolved));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildConstructedGenericMethodObject(
            state, runtime, ZR_NULL));

    destroy_reflection_test_state(state);
}

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

static void test_generic_method_definition_object_materializes_parameters(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrObject *methodObject;
    SZrObject *parametersArray;
    SZrObject *parameterObject;
    SZrZrpMetadataMethodDefRow *methodDefRows;
    SZrZrpMetadataGenericParamRow *genericParamRows;

    TEST_ASSERT_NOT_NULL(state);
    methodDefRows = (SZrZrpMetadataMethodDefRow *)(void *)(
            fixture.metadataBytes + runtime->zrpMetadataHeader.methodDefs.offset);
    genericParamRows = (SZrZrpMetadataGenericParamRow *)(void *)(
            fixture.metadataBytes + runtime->zrpMetadataHeader.genericParams.offset);

    methodObject = ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    TEST_ASSERT_NOT_NULL(methodObject);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, methodObject));
    assert_object_string_field(state, methodObject, "kind", "genericMethodDefinition");
    assert_object_string_field(state, methodObject, "name", "Map");
    assert_object_bool_field(state, methodObject, "isGenericMethod", ZR_TRUE);
    assert_object_bool_field(state, methodObject, "isGenericMethodDefinition", ZR_TRUE);
    assert_object_bool_field(state, methodObject, "isConstructedGenericMethod", ZR_FALSE);
    assert_object_int_field(
            state, methodObject, "metadataToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    assert_object_int_field(
            state, methodObject, "genericMethodToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    assert_object_int_field(state, methodObject, "genericArgumentCount", 2);
    assert_object_native_pointer_field(state, methodObject, "metadataRuntime", runtime);
    parametersArray = assert_object_object_field(
            state, methodObject, "genericParameters", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)parametersArray->nodeMap.elementCount);

    parameterObject = assert_array_object_entry(state, parametersArray, 0u);
    assert_object_string_field(state, parameterObject, "kind", "genericMethodParameter");
    assert_object_string_field(state, parameterObject, "name", "TKey");
    assert_object_int_field(state, parameterObject, "genericMethodToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    assert_object_int_field(state, parameterObject, "genericParameterIndex", 0);
    assert_object_int_field(state, parameterObject, "genericParameterMetadataIndex", 0);
    assert_object_int_field(
            state, parameterObject, "nameStringOffset", TEST_METHOD_CONTEXT_PARAMETER0_NAME_OFFSET);
    assert_object_int_field(state, parameterObject, "metadataFlags", 0);
    assert_object_int_field(state, parameterObject, "constraintCount", 0);
    assert_object_native_pointer_field(state, parameterObject, "metadataRuntime", runtime);

    parameterObject = assert_array_object_entry(state, parametersArray, 1u);
    assert_object_string_field(state, parameterObject, "name", "TValue");
    assert_object_int_field(state, parameterObject, "genericParameterIndex", 1);
    assert_object_int_field(state, parameterObject, "genericParameterMetadataIndex", 1);
    assert_object_int_field(
            state, parameterObject, "nameStringOffset", TEST_METHOD_CONTEXT_PARAMETER1_NAME_OFFSET);

    TEST_ASSERT_NULL(ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            ZR_NULL, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN));

    methodDefRows[0].genericParamCount = 0u;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN));
    methodDefRows[0].genericParamCount = 3u;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN));
    methodDefRows[0].genericParamCount = 2u;
    genericParamRows[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildGenericMethodDefinitionObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN));
    genericParamRows[1].ownerToken = TEST_METHOD_CONTEXT_MEMBER_TOKEN;

    destroy_reflection_test_state(state);
}

static void test_method_spec_generic_context_materializes_metadata_arguments(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrObject *contextObject;
    SZrObject *argumentsArray;
    SZrObject *argumentObject;
    const SZrTypeValue *signatureHashValue;

    TEST_ASSERT_NOT_NULL(state);
    contextObject = ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN);
    TEST_ASSERT_NOT_NULL(contextObject);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, contextObject));
    assert_object_string_field(state, contextObject, "kind", "genericMethodContext");
    assert_object_string_field(state, contextObject, "name", "constructedGenericMethod");
    assert_object_int_field(state, contextObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    assert_object_int_field(state, contextObject, "genericMethodToken", TEST_METHOD_CONTEXT_MEMBER_TOKEN);
    assert_object_int_field(state, contextObject, "genericArgumentCount", 2);
    signatureHashValue = get_object_field_value(state, contextObject, "genericSignatureHash");
    TEST_ASSERT_NOT_NULL(signatureHashValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, signatureHashValue->type);
    TEST_ASSERT_EQUAL_UINT64(
            fixture.functionRecords[1].signatureHash,
            signatureHashValue->value.nativeObject.nativeUInt64);
    assert_object_native_pointer_field(state, contextObject, "metadataRuntime", runtime);
    argumentsArray = assert_object_object_field(
            state, contextObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    argumentObject = assert_array_object_entry(state, argumentsArray, 0u);
    assert_object_string_field(state, argumentObject, "genericArgumentKind", "primitive");
    assert_object_int_field(state, argumentObject, "primitiveValueType", ZR_VALUE_TYPE_UINT64);
    argumentObject = assert_array_object_entry(state, argumentsArray, 1u);
    assert_object_string_field(state, argumentObject, "genericArgumentKind", "typeToken");
    assert_object_int_field(state, argumentObject, "typeToken", TEST_METHOD_CONTEXT_TYPE_REF_TOKEN);

    TEST_ASSERT_NULL(ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            state, runtime, TEST_METHOD_CONTEXT_MEMBER_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildMethodSpecGenericContextObject(
            ZR_NULL, runtime, TEST_METHOD_CONTEXT_SPEC_TOKEN));
    destroy_reflection_test_state(state);
}

static void test_method_spec_generic_call_info_context_survives_full_gc(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrCallInfo callInfo = {0};
    SZrCallInfo *previousCallInfo;
    SZrObject *contextObject;
    SZrObject *argumentObject;

    TEST_ASSERT_NOT_NULL(state);
    previousCallInfo = state->callInfoList;
    callInfo.previous = previousCallInfo;
    callInfo.callStatus = ZR_CALL_STATUS_NONE;
    state->callInfoList = &callInfo;
    TEST_ASSERT_TRUE(ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo(
            state, runtime, &callInfo, TEST_METHOD_CONTEXT_SPEC_TOKEN));
    contextObject = ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
            state, &callInfo);
    TEST_ASSERT_NOT_NULL(contextObject);
    assert_object_int_field(state, contextObject, "metadataToken", TEST_METHOD_CONTEXT_SPEC_TOKEN);
    argumentObject =
            ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
                    state,
                    runtime,
                    &callInfo,
                    TEST_METHOD_CONTEXT_MEMBER_TOKEN,
                    0u);
    TEST_ASSERT_NOT_NULL(argumentObject);
    assert_object_int_field(state, argumentObject, "primitiveValueType", ZR_VALUE_TYPE_UINT64);

    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    contextObject = ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
            state, &callInfo);
    TEST_ASSERT_NOT_NULL(contextObject);
    argumentObject =
            ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
                    state,
                    runtime,
                    &callInfo,
                    TEST_METHOD_CONTEXT_MEMBER_TOKEN,
                    1u);
    TEST_ASSERT_NOT_NULL(argumentObject);
    assert_object_int_field(state, argumentObject, "typeToken", TEST_METHOD_CONTEXT_TYPE_REF_TOKEN);
    TEST_ASSERT_NULL(
            ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
                    state,
                    runtime,
                    &callInfo,
                    TEST_TYPE_DEF_TOKEN,
                    0u));
    TEST_ASSERT_FALSE(ZrCore_Reflection_BindInterpreterGenericMethodSpecCallInfo(
            state, runtime, &callInfo, TEST_METHOD_CONTEXT_MEMBER_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
            state, &callInfo));
    state->callInfoList = previousCallInfo;
    destroy_reflection_test_state(state);
}

typedef struct SMethodSpecExecutionCapture {
    SZrMetadataRuntime *runtime;
    SZrFunction *expectedFunction;
    SZrObject *resolvedArgument;
    TZrBool observedMethodContext;
    TZrBool observedNoTypeContext;
    TZrUInt32 observedCount;
} SMethodSpecExecutionCapture;

static TZrDebugSignal test_capture_method_spec_execution_context(
        SZrState *state,
        SZrFunction *function,
        const TZrInstruction *programCounter,
        TZrUInt32 instructionOffset,
        TZrUInt32 sourceLine,
        TZrPtr userData) {
    SMethodSpecExecutionCapture *capture = (SMethodSpecExecutionCapture *)userData;

    ZR_UNUSED_PARAMETER(programCounter);
    ZR_UNUSED_PARAMETER(instructionOffset);
    ZR_UNUSED_PARAMETER(sourceLine);
    if (state == ZR_NULL || capture == ZR_NULL || function != capture->expectedFunction ||
        capture->observedCount != 0u) {
        return ZR_DEBUG_SIGNAL_NONE;
    }
    capture->observedCount = 1u;
    capture->observedMethodContext =
            ZrCore_Reflection_GetInterpreterGenericMethodCallInfoContextObject(
                    state, state->callInfoList) != ZR_NULL;
    capture->observedNoTypeContext =
            ZrCore_Reflection_GetInterpreterGenericCallInfoTypeObject(
                    state, state->callInfoList) == ZR_NULL;
    capture->resolvedArgument =
            ZrCore_Reflection_ResolveInterpreterGenericMethodCallInfoParameterTypeObject(
                    state,
                    capture->runtime,
                    state->callInfoList,
                    TEST_METHOD_CONTEXT_MEMBER_TOKEN,
                    1u);
    state->debugHookSignal = 0u;
    return ZR_DEBUG_SIGNAL_NONE;
}

static SZrFunction *test_create_method_spec_identity_function(SZrState *state) {
    SZrFunction *function = ZrCore_Function_New(state);
    TZrInstruction instruction;

    TEST_ASSERT_NOT_NULL(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.operationCode =
            (TZrUInt16)ZR_INSTRUCTION_ENUM(FUNCTION_RETURN);
    instruction.instruction.operandExtra = 1u;
    instruction.instruction.operand.operand1[0] = 0u;
    function->instructionsList = (TZrInstruction *)ZrCore_Memory_RawMallocWithType(
            state->global,
            sizeof(TZrInstruction),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    TEST_ASSERT_NOT_NULL(function->instructionsList);
    function->instructionsList[0] = instruction;
    function->instructionsLength = 1u;
    function->stackSize = 1u;
    function->parameterCount = 1u;
    function->hasVariableArguments = ZR_FALSE;
    function->closureValueLength = 0u;
    return function;
}

static void test_method_spec_executes_resolved_vm_function_with_context(void) {
    SMethodSpecGenericContextFixture fixture;
    SZrMetadataRuntime *runtime = method_spec_generic_context_fixture_init(&fixture);
    SZrState *state = create_reflection_test_state();
    SZrFunction *function;
    SZrMetadataRuntimeInterpreterMethodBindingView methodBindingView;
    SZrZrpMetadataSectionView methodDefView;
    SZrZrpMetadataMethodDefRow *methodDefRows;
    SZrTypeValue functionConstant;
    SZrTypeValue argument;
    SZrTypeValue result;
    SMethodSpecExecutionCapture capture = {0};

    TEST_ASSERT_NOT_NULL(state);
    function = test_create_method_spec_identity_function(state);
    ZrCore_Value_ResetAsNull(&functionConstant);
    ZrCore_Value_InitAsRawObject(
            state, &functionConstant, ZR_CAST_RAW_OBJECT_AS_SUPER(function));
    functionConstant.type = ZR_VALUE_TYPE_FUNCTION;
    functionConstant.isNative = ZR_FALSE;
    fixture.metadataFunction.constantValueList = &functionConstant;
    fixture.metadataFunction.constantValueLength = 1u;
    fixture.metadataFunction.childFunctionList = function;
    fixture.metadataFunction.childFunctionLength = 1u;
    fixture.metadataFunction.childFunctionGraphIsBorrowed = ZR_TRUE;
    TEST_ASSERT_EQUAL_PTR(
            &fixture.metadataFunction,
            ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                    state, &fixture.metadataFunction, 0u));
    TEST_ASSERT_EQUAL_PTR(
            function,
            ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                    state, &fixture.metadataFunction, 1u));
    TEST_ASSERT_NULL(ZrCore_Function_ResolveGraphFunctionByFlatIndex(
            state, &fixture.metadataFunction, 2u));
    TEST_ASSERT_NULL(ZrCore_Function_ResolveGraphFunctionByFlatIndex(
            state, &fixture.metadataFunction, UINT32_MAX - 1u));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadInterpreterMethodBindingView(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            &methodBindingView));
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_CONTEXT_MEMBER_TOKEN, methodBindingView.methodToken);
    TEST_ASSERT_NOT_NULL(methodBindingView.methodRecord);
    TEST_ASSERT_NOT_NULL(methodBindingView.methodDefRow);
    TEST_ASSERT_EQUAL_UINT32(1u, methodBindingView.functionIndex);
    TEST_ASSERT_EQUAL_PTR(function, methodBindingView.function);
    capture.runtime = runtime;
    capture.expectedFunction = function;
    ZrCore_Debug_SetTraceObserver(state, test_capture_method_spec_execution_context, &capture);
    state->debugHookSignal = ZR_DEBUG_HOOK_MASK_LINE;
    ZrCore_Value_InitAsInt(state, &argument, 109);
    ZrCore_Value_ResetAsNull(&result);
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
            state,
            runtime,
            TEST_METHOD_CONTEXT_SPEC_TOKEN,
            &argument,
            1u,
            &result));
    ZrCore_Debug_SetTraceObserver(state, ZR_NULL, ZR_NULL);
    state->debugHookSignal = 0u;

    TEST_ASSERT_EQUAL_UINT32(1u, capture.observedCount);
    TEST_ASSERT_TRUE(capture.observedMethodContext);
    TEST_ASSERT_TRUE(capture.observedNoTypeContext);
    TEST_ASSERT_NOT_NULL(capture.resolvedArgument);
    assert_object_int_field(
            state, capture.resolvedArgument, "typeToken", TEST_METHOD_CONTEXT_TYPE_REF_TOKEN);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, result.type);
    TEST_ASSERT_EQUAL_INT64(109, result.value.nativeObject.nativeInt64);
    TEST_ASSERT_EQUAL_INT(ZR_THREAD_STATUS_FINE, state->threadStatus);

    ZrCore_Value_InitAsInt(state, &result, 91);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            &argument,
            1u,
            &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    ZrCore_Value_InitAsInt(state, &result, 92);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
            state,
            runtime,
            TEST_METHOD_CONTEXT_SPEC_TOKEN,
            ZR_NULL,
            0u,
            &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetZrpSectionView(
            runtime, ZR_ZRP_METADATA_SECTION_METHOD_DEFS, &methodDefView));
    TEST_ASSERT_EQUAL_UINT32(1u, methodDefView.count);
    methodDefRows = (SZrZrpMetadataMethodDefRow *)(void *)methodDefView.data;
    methodDefRows[0].functionIndex = 2u;
    memset(&methodBindingView, 0xA5, sizeof(methodBindingView));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadInterpreterMethodBindingView(
            state,
            runtime,
            TEST_METHOD_CONTEXT_MEMBER_TOKEN,
            &methodBindingView));
    TEST_ASSERT_EQUAL_UINT32(0u, methodBindingView.methodToken);
    TEST_ASSERT_NULL(methodBindingView.methodRecord);
    TEST_ASSERT_NULL(methodBindingView.methodDefRow);
    TEST_ASSERT_EQUAL_UINT32(0u, methodBindingView.functionIndex);
    TEST_ASSERT_NULL(methodBindingView.function);
    ZrCore_Value_InitAsInt(state, &result, 93);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeInterpreterGenericMethodSpec(
            state,
            runtime,
            TEST_METHOD_CONTEXT_SPEC_TOKEN,
            &argument,
            1u,
            &result));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_NULL(result.type));
    methodDefRows[0].functionIndex = 1u;
    destroy_reflection_test_state(state);
}

#endif
