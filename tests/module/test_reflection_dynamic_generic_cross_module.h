#ifndef ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_CROSS_MODULE_H
#define ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_CROSS_MODULE_H

#define TEST_PROVIDER_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 9u)
#define TEST_PROVIDER_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 29u)
#define TEST_PROVIDER_MODULE_SIGNATURE_HASH 0x8877665544332211ULL
#define TEST_PROVIDER_FIRST_ARGUMENT_VALUE_TYPE_OFFSET 15u

static void expose_cross_module_type_spec_signature(
        SReflectionDynamicGenericFixture *fixture,
        SZrMetadataRuntime *runtime) {
    SZrZrpMetadataSectionView view;

    TEST_ASSERT_NOT_NULL(fixture);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetZrpSectionView(
            runtime, ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL, &view));
    TEST_ASSERT_NOT_NULL(view.data);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
            (TZrUInt32)sizeof(TEST_GENERIC_INSTANCE_SIGNATURE), (TZrUInt32)view.byteLength);
    fixture->metadataFunction.signatureBlobHeap = (TZrByte *)(TZrPtr)view.data;
    fixture->metadataFunction.signatureBlobHeapLength = (TZrUInt32)view.byteLength;
    fixture->records[0].signatureBlobOffset = 0u;
    fixture->records[0].signatureBlobLength =
            (TZrUInt32)sizeof(TEST_GENERIC_INSTANCE_SIGNATURE);
}

static void remap_cross_module_provider_type_spec(
        SReflectionDynamicGenericFixture *fixture,
        SZrMetadataRuntime *runtime) {
    SZrZrpMetadataSectionView view;
    SZrZrpMetadataTypeSpecRow *rows;

    TEST_ASSERT_NOT_NULL(fixture);
    TEST_ASSERT_NOT_NULL(runtime);
    fixture->records[0].token = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    fixture->records[0].relatedToken = TEST_PROVIDER_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[1].token = TEST_PROVIDER_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[1].relatedToken = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    fixture->records[1].ownerToken = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    fixture->metadataFunction.moduleSignatureHash = TEST_PROVIDER_MODULE_SIGNATURE_HASH;
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetZrpSectionView(
            runtime, ZR_ZRP_METADATA_SECTION_TYPE_SPECS, &view));
    TEST_ASSERT_NOT_NULL(view.data);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1u, view.count);
    rows = (SZrZrpMetadataTypeSpecRow *)(void *)view.data;
    rows[0].token = TEST_PROVIDER_TYPE_SPEC_TOKEN;
}

static void test_dynamic_generic_instance_resolves_bound_provider_typespec_identity(void) {
    SReflectionDynamicGenericFixture requesterFixture;
    SReflectionDynamicGenericFixture unboundRequesterFixture;
    SReflectionDynamicGenericFixture providerFixture;
    SReflectionDynamicGenericFixture wrongProviderFixture;
    SZrMetadataRuntime *requesterRuntime = fixture_init(&requesterFixture, ZR_FALSE);
    SZrMetadataRuntime *unboundRequesterRuntime = fixture_init(&unboundRequesterFixture, ZR_FALSE);
    SZrMetadataRuntime *providerRuntime = fixture_init(&providerFixture, ZR_TRUE);
    SZrMetadataRuntime *wrongProviderRuntime = fixture_init(&wrongProviderFixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrMetadataTokenBinding *binding;
    SZrMetadataTokenBinding *mutableBinding;
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrObject *typeObject;
    SZrZrpMetadataSectionView providerSignatureView;
    TZrByte *providerSignatureBlob;
    TZrByte originalArgumentValueType;

    TEST_ASSERT_NOT_NULL(state);
    expose_cross_module_type_spec_signature(&requesterFixture, requesterRuntime);
    expose_cross_module_type_spec_signature(&providerFixture, providerRuntime);
    remap_cross_module_provider_type_spec(&providerFixture, providerRuntime);
    TEST_ASSERT_TRUE(ZrCore_Function_BindMatchingTypeSpecMetadata(
            state,
            &requesterFixture.metadataFunction,
            &providerFixture.metadataFunction));
    binding = ZrCore_Function_FindModuleMetadataBinding(
            &requesterFixture.metadataFunction, TEST_TYPE_SPEC_TOKEN);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_EQUAL_UINT32(TEST_PROVIDER_TYPE_SPEC_TOKEN, binding->resolvedMetadataToken);
    TEST_ASSERT_EQUAL_UINT32(
            TEST_PROVIDER_TYPE_SPEC_SIGNATURE_TOKEN, binding->resolvedSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(
            TEST_PROVIDER_MODULE_SIGNATURE_HASH, binding->resolvedModuleSignatureHash);
    mutableBinding = (SZrMetadataTokenBinding *)(TZrPtr)binding;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
            requesterRuntime,
            TEST_TYPE_SPEC_TOKEN,
            providerRuntime,
            &instance));
    TEST_ASSERT_EQUAL_UINT32(TEST_PROVIDER_TYPE_SPEC_TOKEN, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(
            TEST_PROVIDER_TYPE_SPEC_SIGNATURE_TOKEN, instance.genericSignatureToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, instance.genericBaseToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT, instance.route);
    TEST_ASSERT_EQUAL_PTR(&providerFixture.typeLayout, instance.typeLayout);
    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(
            state, providerRuntime, &instance);
    TEST_ASSERT_NOT_NULL(typeObject);
    assert_object_int_field(state, typeObject, "metadataToken", TEST_PROVIDER_TYPE_SPEC_TOKEN);
    assert_object_native_pointer_field(state, typeObject, "metadataRuntime", providerRuntime);

    instance.typeSpecToken = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
            unboundRequesterRuntime,
            TEST_TYPE_SPEC_TOKEN,
            providerRuntime,
            &instance));
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
    instance.typeSpecToken = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
            requesterRuntime,
            TEST_TYPE_SPEC_TOKEN,
            wrongProviderRuntime,
            &instance));
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    mutableBinding->expectedMetadataToken = TEST_OUTER_TYPE_SPEC_TOKEN;
    instance.typeSpecToken = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
            requesterRuntime,
            TEST_TYPE_SPEC_TOKEN,
            providerRuntime,
            &instance));
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
    mutableBinding->expectedMetadataToken = TEST_TYPE_SPEC_TOKEN;

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetZrpSectionView(
            providerRuntime, ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL, &providerSignatureView));
    TEST_ASSERT_GREATER_THAN_UINT32(
            TEST_PROVIDER_FIRST_ARGUMENT_VALUE_TYPE_OFFSET, (TZrUInt32)providerSignatureView.byteLength);
    providerSignatureBlob = (TZrByte *)(TZrPtr)providerSignatureView.data;
    originalArgumentValueType = providerSignatureBlob[TEST_PROVIDER_FIRST_ARGUMENT_VALUE_TYPE_OFFSET];
    providerSignatureBlob[TEST_PROVIDER_FIRST_ARGUMENT_VALUE_TYPE_OFFSET] =
            (TZrByte)ZR_VALUE_TYPE_UINT64;
    instance.typeSpecToken = TEST_PROVIDER_TYPE_SPEC_TOKEN;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveBoundGenericTypeInstanceFromProvider(
            requesterRuntime,
            TEST_TYPE_SPEC_TOKEN,
            providerRuntime,
            &instance));
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
    providerSignatureBlob[TEST_PROVIDER_FIRST_ARGUMENT_VALUE_TYPE_OFFSET] = originalArgumentValueType;
    destroy_reflection_test_state(state);
}

#endif
