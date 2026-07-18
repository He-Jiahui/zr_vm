#include <string.h>
#include <stdlib.h>

#include "unity.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/zrp_metadata.h"

#define TEST_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_TYPE_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define TEST_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u)
#define TEST_OUTER_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 2u)
#define TEST_OUTER_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 3u)
#define TEST_COMPOUND_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 3u)
#define TEST_COMPOUND_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 4u)
#define TEST_TYPE_LAYOUT_ID 42u
#define TEST_OUTER_TYPE_LAYOUT_ID 43u
#define TEST_COMPOUND_TYPE_LAYOUT_ID 44u
#define TEST_UNION_NAME_STRING_OFFSET 23u
#define TEST_SIGNATURE_HASH 0x123456789ABCDEF0ULL
#define TEST_OUTER_SIGNATURE_HASH 0x0FEDCBA987654321ULL
#define TEST_COMPOUND_SIGNATURE_HASH 0x1020304050607080ULL

static const TZrByte TEST_GENERIC_INSTANCE_SIGNATURE[] = {
        ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
        2u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
        (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
};

static const TZrByte TEST_BASE_TYPE_SIGNATURE[] = {
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
};

static const TZrByte TEST_OUTER_GENERIC_INSTANCE_SIGNATURE[] = {
        ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
        2u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
        2u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
        (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_ARRAY,
        1u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
};

static const TZrByte TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE[] = {
        ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
        1u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_TUPLE,
        2u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_OWNERSHIP,
        1u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        17u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_UNION,
        (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
        (TZrByte)TEST_UNION_NAME_STRING_OFFSET, 0u, 0u, 0u,
        1u, 0u, 0u, 0u,
        ZR_METADATA_SIGNATURE_NODE_NULLABLE,
        ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
        (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
};

typedef struct SReflectionDynamicGenericFixture {
    SZrObjectModule module;
    SZrFunction metadataFunction;
    SZrMetadataTokenRecord records[8];
    SZrAotCodeRegistration registration;
    SZrTypeLayout typeLayout;
    SZrTypeLayout outerTypeLayout;
    SZrTypeLayout compoundTypeLayout;
    const SZrTypeLayout *registeredLayouts[TEST_COMPOUND_TYPE_LAYOUT_ID + 1u];
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          sizeof(SZrZrpMetadataTypeDefRow) +
                          (3u * sizeof(SZrZrpMetadataTypeSpecRow)) +
                          (2u * sizeof(SZrZrpMetadataGenericParamRow)) +
                          sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                          sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE) +
                          sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE) +
                          sizeof(TEST_BASE_TYPE_SIGNATURE)];
} SReflectionDynamicGenericFixture;

void setUp(void) {}
void tearDown(void) {}

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0u) {
        if (pointer != ZR_NULL && (TZrPtr)pointer >= (TZrPtr)0x1000) {
            free(pointer);
        }
        return ZR_NULL;
    }
    if (pointer == ZR_NULL) {
        return malloc(newSize);
    }
    if ((TZrPtr)pointer >= (TZrPtr)0x1000) {
        return realloc(pointer, newSize);
    }
    return malloc(newSize);
}

static SZrState *create_reflection_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 12345u, &callbacks);

    if (global == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_GlobalState_InitRegistry(global->mainThreadState, global);
    return global->mainThreadState;
}

static void destroy_reflection_test_state(SZrState *state) {
    if (state != ZR_NULL && state->global != ZR_NULL) {
        ZrCore_GlobalState_Free(state->global);
    }
}

#include "test_reflection_dynamic_generic_instance_assertions.h"

static void set_counted_section(SZrZrpMetadataSection *section,
                                TZrUInt32 *nextOffset,
                                TZrUInt32 count,
                                TZrUInt32 elementSize) {
    section->offset = *nextOffset;
    section->count = count;
    section->elementSize = elementSize;
    section->byteLength = count * elementSize;
    *nextOffset += section->byteLength;
}

static SZrMetadataRuntime *fixture_init(SReflectionDynamicGenericFixture *fixture,
                                        TZrBool registerTypeLayout) {
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeDefRows;
    SZrZrpMetadataTypeSpecRow *typeSpecRows;
    SZrZrpMetadataGenericParamRow *genericParamRows;
    TZrByte signaturePayload[sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                             sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE) +
                             sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE) +
                             sizeof(TEST_BASE_TYPE_SIGNATURE)];
    SZrMetadataRuntime *runtime;
    TZrUInt32 nextOffset;

    memset(fixture, 0, sizeof(*fixture));
    memcpy(signaturePayload, TEST_GENERIC_INSTANCE_SIGNATURE, sizeof(TEST_GENERIC_INSTANCE_SIGNATURE));
    memcpy(signaturePayload + sizeof(TEST_GENERIC_INSTANCE_SIGNATURE),
           TEST_OUTER_GENERIC_INSTANCE_SIGNATURE,
           sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE));
    memcpy(signaturePayload + sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                            sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE),
           TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE,
           sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE));
    memcpy(signaturePayload + sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                            sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE) +
                            sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE),
           TEST_BASE_TYPE_SIGNATURE,
           sizeof(TEST_BASE_TYPE_SIGNATURE));

    fixture->records[0].token = TEST_TYPE_SPEC_TOKEN;
    fixture->records[0].relatedToken = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[0].signatureHash = TEST_SIGNATURE_HASH;
    fixture->records[1].token = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[1].relatedToken = TEST_TYPE_SPEC_TOKEN;
    fixture->records[1].ownerToken = TEST_TYPE_SPEC_TOKEN;
    fixture->records[1].signatureBlobLength = (TZrUInt32)sizeof(TEST_GENERIC_INSTANCE_SIGNATURE);
    fixture->records[1].signatureHash = TEST_SIGNATURE_HASH;
    fixture->records[2].token = TEST_OUTER_TYPE_SPEC_TOKEN;
    fixture->records[2].relatedToken = TEST_OUTER_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[2].signatureHash = TEST_OUTER_SIGNATURE_HASH;
    fixture->records[3].token = TEST_OUTER_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[3].relatedToken = TEST_OUTER_TYPE_SPEC_TOKEN;
    fixture->records[3].ownerToken = TEST_OUTER_TYPE_SPEC_TOKEN;
    fixture->records[3].signatureBlobOffset = (TZrUInt32)sizeof(TEST_GENERIC_INSTANCE_SIGNATURE);
    fixture->records[3].signatureBlobLength = (TZrUInt32)sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE);
    fixture->records[3].signatureHash = TEST_OUTER_SIGNATURE_HASH;
    fixture->records[4].token = TEST_TYPE_DEF_TOKEN;
    fixture->records[4].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    fixture->records[4].signatureHash = 0x2233445566778899ULL;
    fixture->records[5].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    fixture->records[5].relatedToken = TEST_TYPE_DEF_TOKEN;
    fixture->records[5].ownerToken = TEST_TYPE_DEF_TOKEN;
    fixture->records[5].signatureBlobOffset =
            (TZrUInt32)(sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                        sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE) +
                        sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE));
    fixture->records[5].signatureBlobLength = (TZrUInt32)sizeof(TEST_BASE_TYPE_SIGNATURE);
    fixture->records[5].signatureHash = fixture->records[4].signatureHash;
    fixture->records[6].token = TEST_COMPOUND_TYPE_SPEC_TOKEN;
    fixture->records[6].relatedToken = TEST_COMPOUND_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[6].signatureHash = TEST_COMPOUND_SIGNATURE_HASH;
    fixture->records[7].token = TEST_COMPOUND_TYPE_SPEC_SIGNATURE_TOKEN;
    fixture->records[7].relatedToken = TEST_COMPOUND_TYPE_SPEC_TOKEN;
    fixture->records[7].ownerToken = TEST_COMPOUND_TYPE_SPEC_TOKEN;
    fixture->records[7].signatureBlobOffset =
            (TZrUInt32)(sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                        sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE));
    fixture->records[7].signatureBlobLength =
            (TZrUInt32)sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE);
    fixture->records[7].signatureHash = TEST_COMPOUND_SIGNATURE_HASH;
    fixture->metadataFunction.metadataTokenRecords = fixture->records;
    fixture->metadataFunction.metadataTokenRecordLength = 8u;

    if (registerTypeLayout) {
        fixture->typeLayout.cTypeId = TEST_TYPE_LAYOUT_ID;
        fixture->typeLayout.byteSize = 80u;
        fixture->registeredLayouts[TEST_TYPE_LAYOUT_ID] = &fixture->typeLayout;
        fixture->outerTypeLayout.cTypeId = TEST_OUTER_TYPE_LAYOUT_ID;
        fixture->outerTypeLayout.byteSize = 96u;
        fixture->registeredLayouts[TEST_OUTER_TYPE_LAYOUT_ID] = &fixture->outerTypeLayout;
        fixture->compoundTypeLayout.cTypeId = TEST_COMPOUND_TYPE_LAYOUT_ID;
        fixture->compoundTypeLayout.byteSize = 112u;
        fixture->registeredLayouts[TEST_COMPOUND_TYPE_LAYOUT_ID] = &fixture->compoundTypeLayout;
        fixture->registration.typeLayouts = fixture->registeredLayouts;
        fixture->registration.typeLayoutCount = TEST_COMPOUND_TYPE_LAYOUT_ID + 1u;
    }

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.typeSpecs,
                        &nextOffset,
                        3u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_counted_section(&header.genericParams,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow));
    set_counted_section(&header.signatureBlobPool,
                        &nextOffset,
                        (TZrUInt32)sizeof(signaturePayload),
                        1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(
            fixture->metadataBytes, sizeof(fixture->metadataBytes), &header));

    typeDefRows = (SZrZrpMetadataTypeDefRow *)(void *)(fixture->metadataBytes + header.typeDefs.offset);
    typeDefRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeDefRows[0].typeLayoutId = TEST_TYPE_LAYOUT_ID;
    typeDefRows[0].firstGenericParamIndex = 0u;
    typeDefRows[0].genericParamCount = 2u;

    typeSpecRows = (SZrZrpMetadataTypeSpecRow *)(void *)(fixture->metadataBytes + header.typeSpecs.offset);
    typeSpecRows[0].token = TEST_TYPE_SPEC_TOKEN;
    typeSpecRows[0].signatureBlobLength = (TZrUInt32)sizeof(TEST_GENERIC_INSTANCE_SIGNATURE);
    typeSpecRows[0].typeLayoutId = TEST_TYPE_LAYOUT_ID;
    typeSpecRows[0].signatureHash = TEST_SIGNATURE_HASH;
    typeSpecRows[1].token = TEST_OUTER_TYPE_SPEC_TOKEN;
    typeSpecRows[1].signatureBlobOffset = (TZrUInt32)sizeof(TEST_GENERIC_INSTANCE_SIGNATURE);
    typeSpecRows[1].signatureBlobLength = (TZrUInt32)sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE);
    typeSpecRows[1].typeLayoutId = TEST_OUTER_TYPE_LAYOUT_ID;
    typeSpecRows[1].signatureHash = TEST_OUTER_SIGNATURE_HASH;
    typeSpecRows[2].token = TEST_COMPOUND_TYPE_SPEC_TOKEN;
    typeSpecRows[2].signatureBlobOffset =
            (TZrUInt32)(sizeof(TEST_GENERIC_INSTANCE_SIGNATURE) +
                        sizeof(TEST_OUTER_GENERIC_INSTANCE_SIGNATURE));
    typeSpecRows[2].signatureBlobLength =
            (TZrUInt32)sizeof(TEST_COMPOUND_GENERIC_INSTANCE_SIGNATURE);
    typeSpecRows[2].typeLayoutId = TEST_COMPOUND_TYPE_LAYOUT_ID;
    typeSpecRows[2].signatureHash = TEST_COMPOUND_SIGNATURE_HASH;

    genericParamRows = (SZrZrpMetadataGenericParamRow *)(void *)(
            fixture->metadataBytes + header.genericParams.offset);
    genericParamRows[0].ownerToken = TEST_TYPE_DEF_TOKEN;
    genericParamRows[0].nameStringOffset = 11u;
    genericParamRows[0].parameterIndex = 0u;
    genericParamRows[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    genericParamRows[1].nameStringOffset = 13u;
    genericParamRows[1].parameterIndex = 1u;
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(fixture->metadataBytes,
                                                        sizeof(fixture->metadataBytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(
            &fixture->module, &fixture->metadataFunction, &fixture->registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(
            runtime, fixture->metadataBytes, sizeof(fixture->metadataBytes)));
    return runtime;
}

static void test_dynamic_generic_instance_rejects_invalid_input_and_clears_output(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_FALSE);
    SZrReflectionDynamicGenericTypeInstance instance;

    memset(&instance, 0xA5, sizeof(instance));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            ZR_NULL, TEST_TYPE_SPEC_TOKEN, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    memset(&instance, 0xA5, sizeof(instance));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_TYPE_DEF_TOKEN, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_TYPE_SPEC_TOKEN, ZR_NULL));
}

static void test_dynamic_generic_instance_routes_uncollected_typespec_to_interpreter(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_FALSE);
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_TYPE_SPEC_TOKEN, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_SIGNATURE_TOKEN, instance.genericSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_SIGNATURE_HASH, instance.genericSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, instance.genericBaseToken);
    TEST_ASSERT_EQUAL_PTR(&fixture.records[4], instance.genericBaseRecord);
    TEST_ASSERT_EQUAL_UINT32(2u, instance.genericArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(14u, instance.genericArgumentListBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, instance.typeLayoutId);
    TEST_ASSERT_NULL(instance.typeLayout);
}

static void test_dynamic_generic_instance_routes_registered_typespec_to_aot(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrObject *typeObject;
    SZrObject *argumentsArray;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_TYPE_SPEC_TOKEN, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_LAYOUT_ID, instance.typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&fixture.typeLayout, instance.typeLayout);
    TEST_ASSERT_EQUAL_UINT32(80u, instance.typeLayout->byteSize);
    TEST_ASSERT_NOT_NULL(state);
    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &instance);
    TEST_ASSERT_NOT_NULL(typeObject);
    argumentsArray = assert_object_object_field(state, typeObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    assert_object_int_field(state,
                            assert_array_object_entry(state, argumentsArray, 0u),
                            "primitiveValueType",
                            ZR_VALUE_TYPE_INT64);
    assert_object_int_field(state,
                            assert_array_object_entry(state, argumentsArray, 1u),
                            "typeToken",
                            TEST_TYPE_DEF_TOKEN);
    destroy_reflection_test_state(state);
}

static void test_dynamic_generic_type_object_materializes_token_only_nested_generic_and_array(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrObject *typeObject;
    SZrObject *argumentsArray;
    SZrObject *nestedGenericObject;
    SZrObject *arrayObject;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_OUTER_TYPE_SPEC_TOKEN, &instance));
    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &instance);
    TEST_ASSERT_NOT_NULL(typeObject);
    argumentsArray = assert_object_object_field(state, typeObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    nestedGenericObject = assert_array_object_entry(state, argumentsArray, 0u);
    assert_object_string_field(state, nestedGenericObject, "genericArgumentKind", "typeToken");
    assert_object_int_field(state, nestedGenericObject, "typeToken", TEST_TYPE_SPEC_TOKEN);
    arrayObject = assert_array_object_entry(state, argumentsArray, 1u);
    assert_object_string_field(state, arrayObject, "genericArgumentKind", "array");
    assert_object_int_field(state, arrayObject, "arrayRank", 1);
    assert_object_int_field(state,
                            assert_object_object_field(
                                    state, arrayObject, "elementType", ZR_VALUE_TYPE_OBJECT),
                            "typeToken",
                            TEST_TYPE_DEF_TOKEN);
    destroy_reflection_test_state(state);
}

static void test_dynamic_generic_type_object_materializes_token_only_compound_argument(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrObject *typeObject;
    SZrObject *argumentsArray;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveDynamicGenericTypeInstance(
            runtime, TEST_COMPOUND_TYPE_SPEC_TOKEN, &instance));
    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &instance);
    TEST_ASSERT_NOT_NULL(typeObject);
    argumentsArray = assert_object_object_field(state, typeObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    assert_compound_generic_arguments(state, argumentsArray);
    destroy_reflection_test_state(state);
}

static void test_constructed_generic_type_resolves_existing_typespec_and_route(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_INT64,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_PTR(arguments, instance.requestedArguments);
    TEST_ASSERT_EQUAL_UINT32(2u, instance.genericArgumentCount);
    TEST_ASSERT_EQUAL_PTR(&fixture.typeLayout, instance.typeLayout);
}

static void test_constructed_generic_type_routes_uncollected_arguments_to_interpreter(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, instance.genericBaseToken);
    TEST_ASSERT_EQUAL_PTR(&fixture.records[4], instance.genericBaseRecord);
    TEST_ASSERT_EQUAL_PTR(arguments, instance.requestedArguments);
    TEST_ASSERT_EQUAL_UINT32(2u, instance.genericArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, instance.typeLayoutId);
    TEST_ASSERT_NULL(instance.typeLayout);
}

static void test_constructed_generic_type_rejects_invalid_request(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_FALSE);
    const SZrReflectionGenericTypeArgument invalidArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE,
    };
    const SZrReflectionGenericTypeArgument primitiveArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
            .primitiveValueType = ZR_VALUE_TYPE_INT64,
    };
    const SZrReflectionGenericTypeArgument unknownPrimitiveArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
            .primitiveValueType = ZR_VALUE_TYPE_UNKNOWN,
    };
    const SZrReflectionGenericTypeArgument invalidTupleArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE,
            .childCount = 1u,
    };
    const SZrReflectionGenericTypeArgument invalidOwnershipArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP,
            .ownershipQualifier = ZR_REFLECTION_OWNERSHIP_QUALIFIER_NONE,
            .elementType = &primitiveArgument,
    };
    const SZrReflectionGenericTypeArgument invalidNullableArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE,
    };
    const SZrReflectionGenericTypeArgument invalidUnionArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION,
            .unionValueType = ZR_VALUE_TYPE_OBJECT,
            .unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET,
            .childCount = 1u,
    };
    SZrReflectionGenericTypeArgument recursiveArrayArgument = {0};
    SZrReflectionDynamicGenericTypeInstance instance;

    recursiveArrayArgument.kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY;
    recursiveArrayArgument.arrayRank = 1u;
    recursiveArrayArgument.elementType = &recursiveArrayArgument;

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_SPEC_TOKEN, &primitiveArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &invalidArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &unknownPrimitiveArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &recursiveArrayArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &invalidTupleArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &invalidOwnershipArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &invalidNullableArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &invalidUnionArgument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_NONE, instance.route);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, ZR_NULL, 1u, &instance));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &primitiveArgument, 0u, &instance));
}

static void test_constructed_generic_type_matches_nested_typespec_argument(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument elementArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
            .typeToken = TEST_TYPE_DEF_TOKEN,
    };
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_SPEC_TOKEN,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY,
                    .arrayRank = 1u,
                    .elementType = &elementArgument,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(TEST_OUTER_TYPE_SPEC_TOKEN, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_OUTER_SIGNATURE_HASH, instance.genericSignatureHash);
    TEST_ASSERT_EQUAL_PTR(arguments, instance.requestedArguments);
    TEST_ASSERT_EQUAL_UINT32(TEST_OUTER_TYPE_LAYOUT_ID, instance.typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&fixture.outerTypeLayout, instance.typeLayout);
}

static void test_constructed_generic_type_does_not_shallow_match_nested_typespec(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument elementArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
            .typeToken = TEST_TYPE_DEF_TOKEN,
    };
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_OUTER_TYPE_SPEC_TOKEN,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY,
                    .arrayRank = 1u,
                    .elementType = &elementArgument,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_PTR(arguments, instance.requestedArguments);
    TEST_ASSERT_NULL(instance.typeLayout);
}

static void test_constructed_generic_type_does_not_match_different_array_rank(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument elementArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
            .typeToken = TEST_TYPE_DEF_TOKEN,
    };
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_SPEC_TOKEN,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY,
                    .arrayRank = 2u,
                    .elementType = &elementArgument,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_PTR(arguments, instance.requestedArguments);
}

static void test_constructed_generic_type_matches_tuple_ownership_union_and_nullable_argument(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument baseArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
            .typeToken = TEST_TYPE_DEF_TOKEN,
    };
    const SZrReflectionGenericTypeArgument primitiveArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
            .primitiveValueType = ZR_VALUE_TYPE_INT64,
    };
    const SZrReflectionGenericTypeArgument unionChildren[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE,
                    .elementType = &primitiveArgument,
            },
    };
    const SZrReflectionGenericTypeArgument tupleChildren[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP,
                    .ownershipQualifier = ZR_REFLECTION_OWNERSHIP_QUALIFIER_UNIQUE,
                    .elementType = &baseArgument,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION,
                    .unionValueType = ZR_VALUE_TYPE_OBJECT,
                    .unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET,
                    .childCount = 1u,
                    .childTypes = unionChildren,
            },
    };
    const SZrReflectionGenericTypeArgument argument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE,
            .childCount = 2u,
            .childTypes = tupleChildren,
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));

    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(TEST_COMPOUND_TYPE_SPEC_TOKEN, instance.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT64(TEST_COMPOUND_SIGNATURE_HASH, instance.genericSignatureHash);
    TEST_ASSERT_EQUAL_PTR(&argument, instance.requestedArguments);
    TEST_ASSERT_EQUAL_UINT32(TEST_COMPOUND_TYPE_LAYOUT_ID, instance.typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&fixture.compoundTypeLayout, instance.typeLayout);
}

static void test_constructed_generic_type_rejects_different_compound_argument_shape(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    const SZrReflectionGenericTypeArgument baseArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
            .typeToken = TEST_TYPE_DEF_TOKEN,
    };
    const SZrReflectionGenericTypeArgument int64Argument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
            .primitiveValueType = ZR_VALUE_TYPE_INT64,
    };
    const SZrReflectionGenericTypeArgument boolArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
            .primitiveValueType = ZR_VALUE_TYPE_BOOL,
    };
    SZrReflectionGenericTypeArgument unionChildren[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE,
                    .elementType = &int64Argument,
            },
    };
    SZrReflectionGenericTypeArgument tupleChildren[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP,
                    .ownershipQualifier = ZR_REFLECTION_OWNERSHIP_QUALIFIER_SHARED,
                    .elementType = &baseArgument,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION,
                    .unionValueType = ZR_VALUE_TYPE_OBJECT,
                    .unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET,
                    .childCount = 1u,
                    .childTypes = unionChildren,
            },
    };
    SZrReflectionGenericTypeArgument argument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE,
            .childCount = 2u,
            .childTypes = tupleChildren,
    };
    SZrReflectionDynamicGenericTypeInstance instance;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    tupleChildren[0].ownershipQualifier = ZR_REFLECTION_OWNERSHIP_QUALIFIER_UNIQUE;
    unionChildren[0].elementType = &boolArgument;
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    unionChildren[0].elementType = &int64Argument;
    tupleChildren[1].unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET + 1u;
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    tupleChildren[1].unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET;
    tupleChildren[1].unionValueType = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    tupleChildren[1].unionValueType = ZR_VALUE_TYPE_OBJECT;
    tupleChildren[1].childCount = 0u;
    tupleChildren[1].childTypes = ZR_NULL;
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);

    tupleChildren[1].childCount = 1u;
    tupleChildren[1].childTypes = unionChildren;
    argument.childCount = 1u;
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);
    TEST_ASSERT_EQUAL_UINT32(0u, instance.typeSpecToken);
}

static void test_dynamic_generic_type_object_copies_recursive_aot_request_identity(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    SZrReflectionGenericTypeArgument baseArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
            .typeToken = TEST_TYPE_DEF_TOKEN,
    };
    SZrReflectionGenericTypeArgument primitiveArgument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
            .primitiveValueType = ZR_VALUE_TYPE_INT64,
    };
    SZrReflectionGenericTypeArgument unionChildren[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE,
                    .elementType = &primitiveArgument,
            },
    };
    SZrReflectionGenericTypeArgument tupleChildren[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP,
                    .ownershipQualifier = ZR_REFLECTION_OWNERSHIP_QUALIFIER_UNIQUE,
                    .elementType = &baseArgument,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION,
                    .unionValueType = ZR_VALUE_TYPE_OBJECT,
                    .unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET,
                    .childCount = 1u,
                    .childTypes = unionChildren,
            },
    };
    SZrReflectionGenericTypeArgument argument = {
            .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE,
            .childCount = 2u,
            .childTypes = tupleChildren,
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrObject *typeObject;
    SZrObject *madeTypeObject;
    SZrObject *argumentsArray;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT, instance.route);

    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &instance);
    TEST_ASSERT_NOT_NULL(typeObject);
    madeTypeObject = ZrCore_Reflection_MakeGenericTypeObject(
            state, runtime, TEST_TYPE_DEF_TOKEN, &argument, 1u);
    TEST_ASSERT_NOT_NULL(madeTypeObject);
    assert_object_int_field(state, madeTypeObject, "metadataToken", TEST_COMPOUND_TYPE_SPEC_TOKEN);
    assert_object_int_field(state, madeTypeObject, "typeLayoutId", TEST_COMPOUND_TYPE_LAYOUT_ID);

    tupleChildren[0].ownershipQualifier = ZR_REFLECTION_OWNERSHIP_QUALIFIER_SHARED;
    tupleChildren[1].unionNameStringOffset = TEST_UNION_NAME_STRING_OFFSET + 1u;
    primitiveArgument.primitiveValueType = ZR_VALUE_TYPE_BOOL;

    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, typeObject));
    assert_object_string_field(state, typeObject, "kind", "type");
    assert_object_string_field(state, typeObject, "name", "constructedGeneric");
    assert_object_string_field(state, typeObject, "genericInstanceRoute", "aot");
    assert_object_bool_field(state, typeObject, "isGenericType", ZR_TRUE);
    assert_object_bool_field(state, typeObject, "isConstructedGenericType", ZR_TRUE);
    assert_object_bool_field(state, typeObject, "isAotCollected", ZR_TRUE);
    assert_object_int_field(state, typeObject, "metadataToken", TEST_COMPOUND_TYPE_SPEC_TOKEN);
    assert_object_int_field(state, typeObject, "genericBaseToken", TEST_TYPE_DEF_TOKEN);
    assert_object_int_field(state, typeObject, "genericArgumentCount", 1);
    assert_object_int_field(state, typeObject, "typeLayoutId", TEST_COMPOUND_TYPE_LAYOUT_ID);
    assert_object_native_pointer_field(state, typeObject, "metadataRuntime", runtime);

    argumentsArray = assert_object_object_field(state, typeObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    assert_compound_generic_arguments(state, argumentsArray);

    destroy_reflection_test_state(state);
}

static void test_dynamic_generic_type_object_materializes_interpreter_deopt_route(void) {
    SReflectionDynamicGenericFixture fixture;
    SZrMetadataRuntime *runtime = fixture_init(&fixture, ZR_TRUE);
    SZrState *state = create_reflection_test_state();
    const SZrReflectionGenericTypeArgument arguments[] = {
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE,
                    .primitiveValueType = ZR_VALUE_TYPE_BOOL,
            },
            {
                    .kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN,
                    .typeToken = TEST_TYPE_DEF_TOKEN,
            },
    };
    SZrReflectionDynamicGenericTypeInstance instance;
    SZrObject *typeObject;
    SZrObject *madeTypeObject;
    SZrObject *argumentsArray;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveConstructedGenericType(
            runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u, &instance));
    TEST_ASSERT_EQUAL_UINT32(ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_INTERPRETER_DEOPT, instance.route);

    typeObject = ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &instance);
    TEST_ASSERT_NOT_NULL(typeObject);
    madeTypeObject = ZrCore_Reflection_MakeGenericTypeObject(
            state, runtime, TEST_TYPE_DEF_TOKEN, arguments, 2u);
    TEST_ASSERT_NOT_NULL(madeTypeObject);
    assert_object_string_field(state, madeTypeObject, "genericInstanceRoute", "interpreter-deopt");
    assert_object_string_field(state, typeObject, "genericInstanceRoute", "interpreter-deopt");
    assert_object_bool_field(state, typeObject, "isAotCollected", ZR_FALSE);
    assert_object_int_field(state, typeObject, "metadataToken", 0);
    assert_object_int_field(state, typeObject, "typeLayoutId", -1);
    argumentsArray = assert_object_object_field(state, typeObject, "genericArguments", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    assert_object_int_field(state,
                            assert_array_object_entry(state, argumentsArray, 0u),
                            "primitiveValueType",
                            ZR_VALUE_TYPE_BOOL);
    assert_object_int_field(state,
                            assert_array_object_entry(state, argumentsArray, 1u),
                            "typeToken",
                            TEST_TYPE_DEF_TOKEN);

    TEST_ASSERT_NULL(ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(ZR_NULL, runtime, &instance));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, ZR_NULL, &instance));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, ZR_NULL));
    destroy_reflection_test_state(state);
}

#include "test_reflection_dynamic_generic_method_context.h"
#include "test_reflection_dynamic_generic_instance_interpreter.h"
#include "test_reflection_dynamic_generic_cross_module.h"

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dynamic_generic_instance_rejects_invalid_input_and_clears_output);
    RUN_TEST(test_dynamic_generic_instance_routes_uncollected_typespec_to_interpreter);
    RUN_TEST(test_dynamic_generic_instance_routes_registered_typespec_to_aot);
    RUN_TEST(test_dynamic_generic_type_object_materializes_token_only_nested_generic_and_array);
    RUN_TEST(test_dynamic_generic_type_object_materializes_token_only_compound_argument);
    RUN_TEST(test_constructed_generic_type_resolves_existing_typespec_and_route);
    RUN_TEST(test_constructed_generic_type_routes_uncollected_arguments_to_interpreter);
    RUN_TEST(test_constructed_generic_type_rejects_invalid_request);
    RUN_TEST(test_constructed_generic_type_matches_nested_typespec_argument);
    RUN_TEST(test_constructed_generic_type_does_not_shallow_match_nested_typespec);
    RUN_TEST(test_constructed_generic_type_does_not_match_different_array_rank);
    RUN_TEST(test_constructed_generic_type_matches_tuple_ownership_union_and_nullable_argument);
    RUN_TEST(test_constructed_generic_type_rejects_different_compound_argument_shape);
    RUN_TEST(test_dynamic_generic_type_object_copies_recursive_aot_request_identity);
    RUN_TEST(test_dynamic_generic_type_object_materializes_interpreter_deopt_route);
    RUN_TEST(test_interpreter_generic_instance_materializes_reference_object_context);
    RUN_TEST(test_interpreter_generic_instance_resolves_generic_parameter_type_object);
    RUN_TEST(test_interpreter_generic_call_info_context_survives_full_gc);
    RUN_TEST(test_interpreter_generic_instance_executes_resolved_vm_method_with_context);
    RUN_TEST(test_interpreter_generic_value_instance_preserves_copy_and_execution_semantics);
    RUN_TEST(test_constructed_generic_method_resolves_existing_method_spec);
    RUN_TEST(test_constructed_generic_method_resolves_module_metadata_method_spec);
    RUN_TEST(test_constructed_generic_method_rejects_mismatch_and_clears_output);
    RUN_TEST(test_constructed_generic_method_object_links_definition_and_arguments);
    RUN_TEST(test_make_generic_method_object_resolves_and_materializes);
    RUN_TEST(test_generic_method_definition_object_materializes_parameters);
    RUN_TEST(test_method_spec_generic_context_materializes_metadata_arguments);
    RUN_TEST(test_method_spec_generic_call_info_context_survives_full_gc);
    RUN_TEST(test_method_spec_executes_resolved_vm_function_with_context);
    RUN_TEST(test_dynamic_generic_instance_resolves_bound_provider_typespec_identity);
    return UNITY_END();
}
