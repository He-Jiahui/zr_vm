#include <float.h>
#include <string.h>
#include <stdlib.h>

#include "unity.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/zrp_metadata.h"

#define TEST_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_TYPE_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_FIELD_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u)
#define TEST_FIELD_TYPE_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 7u)
#define TEST_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_MEMBER_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u)
#define TEST_FIELD_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u)
#define TEST_FIELD_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 3u)
#define TEST_FIELD_SIGNATURE_BLOB_OFFSET 4u
#define TEST_FIELD_SIGNATURE_BLOB_LENGTH 7u
#define TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET 2u
#define TEST_FIELD_SIGNATURE_FIELD_TYPE_NEXT_BLOB_OFFSET TEST_FIELD_SIGNATURE_BLOB_LENGTH
#define TEST_FIELD_SIGNATURE_POOL_LENGTH (TEST_FIELD_SIGNATURE_BLOB_OFFSET + TEST_FIELD_SIGNATURE_BLOB_LENGTH)
#define TEST_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define TEST_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 4u)
#define TEST_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define TEST_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 5u)
#define TEST_GENERIC_ARG_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 2u)
#define TEST_GENERIC_ARG_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 6u)
#define TEST_GENERIC_CONSTRAINT_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 3u)
#define TEST_GENERIC_CONSTRAINT_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 8u)
#define TEST_METHOD_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u)
#define TEST_FIELD_VALUE_SLOT_OFFSET 32u
#define TEST_FIELD_RAW_I32_OFFSET 40u
#define TEST_FIELD_RAW_MATRIX_OFFSET 48u
#define TEST_FIELD_RAW_MATRIX_MAX_BYTES sizeof(TZrUInt64)
#define TEST_FIELD_INLINE_STRUCT_OFFSET 64u

void setUp(void) {}

void tearDown(void) {}

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize originalSize, TZrSize newSize, TZrInt64 flag) {
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

static const SZrTypeValue *get_object_field_value(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static void assert_object_string_field(SZrState *state,
                                       SZrObject *object,
                                       const TZrChar *fieldName,
                                       const TZrChar *expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    TEST_ASSERT_EQUAL_STRING(expectedValue, ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object)));
}

static void assert_object_int_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *fieldName,
                                    TZrInt64 expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    TEST_ASSERT_EQUAL_INT64(expectedValue, value->value.nativeObject.nativeInt64);
}

static void assert_object_bool_field(SZrState *state,
                                     SZrObject *object,
                                     const TZrChar *fieldName,
                                     TZrBool expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, value->type);
    TEST_ASSERT_EQUAL_INT(expectedValue ? ZR_TRUE : ZR_FALSE, value->value.nativeObject.nativeBool);
}

static void assert_object_null_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, value->type);
}

static void assert_object_native_pointer_field(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               TZrPtr expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NATIVE_POINTER, value->type);
    TEST_ASSERT_EQUAL_PTR(expectedValue, value->value.nativeObject.nativePointer);
}

static SZrObject *assert_object_object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObject *assert_type_literal_field(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            const TZrChar *expectedName) {
    SZrObject *typeObject = assert_object_object_field(state, object, fieldName);

    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, typeObject));
    assert_object_string_field(state, typeObject, "kind", "type");
    assert_object_string_field(state, typeObject, "name", expectedName);
    assert_object_string_field(state, typeObject, "qualifiedName", expectedName);
    return typeObject;
}

static SZrObject *assert_object_array_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);
    SZrObject *arrayObject;

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    arrayObject = ZR_CAST_OBJECT(state, value->value.object);
    TEST_ASSERT_NOT_NULL(arrayObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, arrayObject->internalType);
    return arrayObject;
}

static TZrUInt32 assert_array_length(SZrObject *arrayObject, TZrUInt32 expectedLength) {
    TEST_ASSERT_NOT_NULL(arrayObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, arrayObject->internalType);
    TEST_ASSERT_EQUAL_UINT32(expectedLength, (TZrUInt32)arrayObject->nodeMap.elementCount);
    return (TZrUInt32)arrayObject->nodeMap.elementCount;
}

static SZrObject *assert_array_object_entry(SZrState *state, SZrObject *arrayObject, TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    TEST_ASSERT_NOT_NULL(arrayObject);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, arrayObject->internalType);
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    entryValue = ZrCore_Object_GetValue(state, arrayObject, &key);
    TEST_ASSERT_NOT_NULL(entryValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, entryValue->type);
    TEST_ASSERT_NOT_NULL(entryValue->value.object);
    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

static SZrObject *assert_signature_node_object(SZrState *state,
                                               SZrObject *fieldInfo,
                                               TZrUInt32 expectedNode,
                                               TZrUInt32 expectedBlobOffset,
                                               TZrUInt32 expectedNextBlobOffset,
                                               TZrUInt32 expectedPayload0,
                                               TZrUInt32 expectedPayload1,
                                               TZrUInt32 expectedBaseTypeBlobOffset,
                                               TZrUInt32 expectedChildCount,
                                               TZrUInt32 expectedChildListBlobOffset,
                                               TZrMetadataToken expectedTypeToken,
                                               TZrUInt32 expectedTypeLayoutId,
                                               TZrUInt32 expectedTypeSize,
                                               const TZrChar *expectedTypeName,
                                               TZrBool expectedMatchesLayout) {
    SZrObject *nodeObject = assert_object_object_field(state, fieldInfo, "fieldTypeSignatureNodeObject");

    assert_object_string_field(state, nodeObject, "kind", "signatureTypeNode");
    assert_object_int_field(state, nodeObject, "node", expectedNode);
    assert_object_int_field(state, nodeObject, "blobOffset", expectedBlobOffset);
    assert_object_int_field(state, nodeObject, "nextBlobOffset", expectedNextBlobOffset);
    assert_object_int_field(state, nodeObject, "payload0", expectedPayload0);
    assert_object_int_field(state, nodeObject, "payload1", expectedPayload1);
    assert_object_int_field(state, nodeObject, "baseTypeBlobOffset", expectedBaseTypeBlobOffset);
    assert_object_int_field(state, nodeObject, "childCount", expectedChildCount);
    assert_object_int_field(state, nodeObject, "childListBlobOffset", expectedChildListBlobOffset);
    assert_object_int_field(state, nodeObject, "typeToken", expectedTypeToken);
    assert_object_int_field(state, nodeObject, "typeLayoutId", expectedTypeLayoutId);
    assert_object_int_field(state, nodeObject, "typeSize", expectedTypeSize);
    if (expectedTypeName != ZR_NULL && expectedTypeName[0] != '\0') {
        assert_object_string_field(state, nodeObject, "typeName", expectedTypeName);
    } else {
        assert_object_null_field(state, nodeObject, "typeName");
    }
    assert_object_bool_field(state, nodeObject, "matchesLayout", expectedMatchesLayout);
    return nodeObject;
}

static TZrInt64 test_reflection_aot_entry(struct SZrState *state) {
    (void)state;
    return 0;
}

static TZrUInt32 test_reflection_invoker_call_count;
static struct SZrState *test_reflection_invoker_state;
static FZrAotEntryThunk test_reflection_invoker_target;
static const SZrAotMethodInfo *test_reflection_invoker_method;
static SZrTypeValue *test_reflection_invoker_self;
static SZrTypeValue *test_reflection_invoker_args;
static SZrTypeValue *test_reflection_invoker_out_return;
static EZrValueType test_reflection_invoker_return_type = ZR_VALUE_TYPE_ENUM_MAX;

static void reset_reflection_invoker_capture(void) {
    test_reflection_invoker_call_count = 0u;
    test_reflection_invoker_state = ZR_NULL;
    test_reflection_invoker_target = ZR_NULL;
    test_reflection_invoker_method = ZR_NULL;
    test_reflection_invoker_self = ZR_NULL;
    test_reflection_invoker_args = ZR_NULL;
    test_reflection_invoker_out_return = ZR_NULL;
    test_reflection_invoker_return_type = ZR_VALUE_TYPE_ENUM_MAX;
}

static void test_reflection_aot_invoker(struct SZrState *state,
                                        FZrAotEntryThunk target,
                                        const SZrAotMethodInfo *method,
                                        SZrTypeValue *self,
                                        SZrTypeValue *args,
                                        SZrTypeValue *outReturn) {
    test_reflection_invoker_call_count++;
    test_reflection_invoker_state = state;
    test_reflection_invoker_target = target;
    test_reflection_invoker_method = method;
    test_reflection_invoker_self = self;
    test_reflection_invoker_args = args;
    test_reflection_invoker_out_return = outReturn;
    if (outReturn != ZR_NULL && test_reflection_invoker_return_type != ZR_VALUE_TYPE_ENUM_MAX) {
        outReturn->type = test_reflection_invoker_return_type;
    }
}

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

static SZrMetadataRuntime *attach_runtime_with_type_and_field_metadata(
        SZrObjectModule *module,
        SZrFunction *metadataFunction,
        SZrAotCodeRegistration *registration,
        SZrMetadataTokenRecord *records,
        const SZrTypeLayout **registeredLayouts,
        TZrByte *metadataBytes,
        TZrSize metadataByteLength,
        const TZrByte *fieldSignatureBlob,
        TZrUInt32 fieldSignatureBlobLength,
        const TZrByte *fieldTypeSignatureBlob,
        TZrUInt32 fieldTypeSignatureBlobLength,
        SZrZrpMetadataTypeDefRow **outTypeRows,
        SZrZrpMetadataFieldDefRow **outFieldRows) {
    static const TZrByte namePool[] = {
            'P', 'o', 'i', 'n', 't', '\0',
            'x', '\0',
            'i', 'n', 't', '\0',
    };
    static const TZrByte defaultFieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_BOOL, 0u, 0u, 0u,
    };
    const TZrByte *actualFieldSignatureBlob = fieldSignatureBlob != ZR_NULL && fieldSignatureBlobLength > 0u
                                                      ? fieldSignatureBlob
                                                      : defaultFieldSignatureBlob;
    TZrUInt32 actualFieldSignatureBlobLength = fieldSignatureBlob != ZR_NULL && fieldSignatureBlobLength > 0u
                                                       ? fieldSignatureBlobLength
                                                       : (TZrUInt32)sizeof(defaultFieldSignatureBlob);
    TZrUInt32 signaturePoolLength = TEST_FIELD_SIGNATURE_BLOB_OFFSET + actualFieldSignatureBlobLength;
    TZrUInt32 fieldTypeSignatureBlobOffset = signaturePoolLength;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    SZrMetadataRuntime *runtime;
    TZrBool hasNamePool = ZR_FALSE;
    TZrBool hasSignaturePool = ZR_FALSE;

    if (fieldTypeSignatureBlob != ZR_NULL && fieldTypeSignatureBlobLength > 0u) {
        signaturePoolLength += fieldTypeSignatureBlobLength;
    }

    records[0].token = TEST_TYPE_DEF_TOKEN;
    records[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    records[0].layoutVersion = 7u;
    records[0].layoutHash = 0x1111222233334444ULL;
    records[1].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_TYPE_DEF_TOKEN;
    records[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    records[1].signatureHash = records[0].signatureHash;
    records[2].token = TEST_FIELD_DEF_TOKEN;
    records[2].relatedToken = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    records[2].ownerToken = TEST_TYPE_DEF_TOKEN;
    records[3].token = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    records[3].relatedToken = TEST_FIELD_DEF_TOKEN;
    records[3].ownerToken = TEST_FIELD_DEF_TOKEN;
    records[3].signatureHash = records[2].signatureHash;
    records[4].token = TEST_FIELD_TYPE_DEF_TOKEN;
    records[4].relatedToken = TEST_FIELD_TYPE_DEF_SIGNATURE_TOKEN;
    records[4].layoutVersion = 11u;
    records[4].layoutHash = 0x9999888877776666ULL;
    records[5].token = TEST_FIELD_TYPE_DEF_SIGNATURE_TOKEN;
    records[5].relatedToken = TEST_FIELD_TYPE_DEF_TOKEN;
    records[5].ownerToken = TEST_FIELD_TYPE_DEF_TOKEN;
    records[5].signatureHash = records[4].signatureHash;
    records[6].token = TEST_MEMBER_DEF_TOKEN;
    records[6].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[6].signatureHash = 0x1234432111223344ULL;
    records[7].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[7].relatedToken = TEST_MEMBER_DEF_TOKEN;
    records[7].ownerToken = TEST_MEMBER_DEF_TOKEN;
    records[7].signatureHash = records[6].signatureHash;

    metadataFunction->metadataTokenRecords = records;
    metadataFunction->metadataTokenRecordLength = 8u;
    registration->typeLayouts = registeredLayouts;
    registration->typeLayoutCount = 43u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.fieldDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    if (metadataByteLength >= (TZrSize)nextOffset + sizeof(namePool)) {
        set_counted_section(&header.stringPool, &nextOffset, (TZrUInt32)sizeof(namePool), 1u);
        hasNamePool = ZR_TRUE;
    }
    if (metadataByteLength >= (TZrSize)nextOffset + signaturePoolLength) {
        set_counted_section(&header.signatureBlobPool,
                            &nextOffset,
                            signaturePoolLength,
                            1u);
        hasSignaturePool = ZR_TRUE;
    }
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(metadataBytes, metadataByteLength, &header));
    if (hasNamePool) {
        TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(metadataBytes,
                                                            metadataByteLength,
                                                            &header,
                                                            ZR_ZRP_METADATA_SECTION_STRING_POOL,
                                                            namePool,
                                                            (TZrUInt32)sizeof(namePool)));
    }
    if (hasSignaturePool) {
        TZrByte *signaturePoolBytes = metadataBytes + header.signatureBlobPool.offset;

        records[3].signatureBlobOffset = TEST_FIELD_SIGNATURE_BLOB_OFFSET;
        records[3].signatureBlobLength = actualFieldSignatureBlobLength;
        memset(signaturePoolBytes, 0, signaturePoolLength);
        memcpy(signaturePoolBytes + TEST_FIELD_SIGNATURE_BLOB_OFFSET,
               actualFieldSignatureBlob,
               actualFieldSignatureBlobLength);
        if (fieldTypeSignatureBlob != ZR_NULL && fieldTypeSignatureBlobLength > 0u) {
            records[5].signatureBlobOffset = fieldTypeSignatureBlobOffset;
            records[5].signatureBlobLength = fieldTypeSignatureBlobLength;
            memcpy(signaturePoolBytes + fieldTypeSignatureBlobOffset,
                   fieldTypeSignatureBlob,
                   fieldTypeSignatureBlobLength);
        }
    }

    *outTypeRows = (SZrZrpMetadataTypeDefRow *)(void *)(metadataBytes + header.typeDefs.offset);
    *outFieldRows = (SZrZrpMetadataFieldDefRow *)(void *)(metadataBytes + header.fieldDefs.offset);
    (*outTypeRows)[0].token = TEST_TYPE_DEF_TOKEN;
    (*outTypeRows)[0].nameStringOffset = hasNamePool ? 0u : 0u;
    (*outTypeRows)[0].firstFieldDefIndex = 0u;
    (*outTypeRows)[0].fieldDefCount = 1u;
    (*outTypeRows)[0].typeLayoutId = 7u;
    (*outTypeRows)[1].token = TEST_FIELD_TYPE_DEF_TOKEN;
    (*outTypeRows)[1].nameStringOffset = hasNamePool ? 8u : 0u;
    (*outTypeRows)[1].firstFieldDefIndex = 1u;
    (*outTypeRows)[1].fieldDefCount = 0u;
    (*outTypeRows)[1].typeLayoutId = 42u;
    (*outFieldRows)[0].token = TEST_FIELD_DEF_TOKEN;
    (*outFieldRows)[0].ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    (*outFieldRows)[0].nameStringOffset = hasNamePool ? 6u : 0u;
    (*outFieldRows)[0].signatureBlobOffset = TEST_FIELD_SIGNATURE_BLOB_OFFSET;
    (*outFieldRows)[0].signatureBlobLength = TEST_FIELD_SIGNATURE_BLOB_LENGTH;
    (*outFieldRows)[0].byteOffset = 24u;
    (*outFieldRows)[0].typeLayoutId = 42u;
    (*outFieldRows)[0].flags = 0xA5u;

    runtime = ZrCore_Module_AttachMetadataRuntime(module, metadataFunction, registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, metadataBytes, metadataByteLength));
    return runtime;
}

static void test_reflection_resolve_token_returns_type_field_and_method_entities(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    FZrAotEntryThunk functionPointers[2] = {
            test_reflection_aot_entry,
            test_reflection_aot_entry,
    };
    SZrAotMethodInfo methodInfo0 = {0};
    SZrAotMethodInfo methodInfo1 = {0};
    const SZrAotMethodInfo *methodInfos[2] = {
            &methodInfo0,
            &methodInfo1,
    };
    TZrUInt32 methodTokens[2] = {
            0u,
            TEST_MEMBER_DEF_TOKEN,
    };
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrReflectionResolvedToken resolved;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow)] = {0};

    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = 64u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = 16u;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    methodInfo0.functionIndex = 0u;
    methodInfo0.invoker = test_reflection_aot_invoker;
    methodInfo1.functionIndex = 1u;
    methodInfo1.invoker = test_reflection_aot_invoker;
    registration.functionCount = 2u;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = 2u;
    registration.methodTokens = methodTokens;
    registration.methodTokenCount = 2u;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          ZR_NULL,
                                                          0u,
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveToken(ZR_NULL, TEST_TYPE_DEF_TOKEN, &resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveToken(runtime, 0u, &resolved));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveToken(runtime, TEST_TYPE_DEF_TOKEN, ZR_NULL));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveToken(runtime, TEST_TYPE_DEF_TOKEN, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_RESOLVED_TOKEN_TYPE, resolved.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, resolved.token);
    TEST_ASSERT_EQUAL_PTR(&records[0], resolved.record);
    TEST_ASSERT_EQUAL_PTR(&typeRows[0], resolved.typeDefRow);
    TEST_ASSERT_EQUAL_UINT32(7u, resolved.typeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&ownerLayout, resolved.typeLayout);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveToken(runtime, TEST_FIELD_DEF_TOKEN, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_RESOLVED_TOKEN_FIELD, resolved.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_FIELD_DEF_TOKEN, resolved.token);
    TEST_ASSERT_EQUAL_PTR(&records[2], resolved.record);
    TEST_ASSERT_EQUAL_PTR(&fieldRows[0], resolved.fieldDefRow);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, resolved.ownerTypeToken);
    TEST_ASSERT_EQUAL_PTR(&records[0], resolved.ownerTypeRecord);
    TEST_ASSERT_EQUAL_PTR(&typeRows[0], resolved.ownerTypeDefRow);
    TEST_ASSERT_EQUAL_UINT32(TEST_FIELD_TYPE_DEF_TOKEN, resolved.fieldTypeToken);
    TEST_ASSERT_EQUAL_PTR(&records[4], resolved.fieldTypeRecord);
    TEST_ASSERT_EQUAL_UINT32(24u, resolved.byteOffset);
    TEST_ASSERT_EQUAL_UINT32(42u, resolved.fieldTypeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&fieldLayout, resolved.fieldTypeLayout);
    TEST_ASSERT_EQUAL_PTR(&ownerLayout, resolved.ownerTypeLayout);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveToken(runtime, TEST_MEMBER_DEF_TOKEN, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_RESOLVED_TOKEN_METHOD, resolved.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, resolved.token);
    TEST_ASSERT_EQUAL_PTR(&records[6], resolved.record);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, resolved.methodToken);
    TEST_ASSERT_EQUAL_PTR(&records[6], resolved.methodRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_SIGNATURE_TOKEN, resolved.methodSignatureToken);
    TEST_ASSERT_EQUAL_PTR(&records[7], resolved.methodSignatureRecord);
    TEST_ASSERT_EQUAL_UINT64(records[7].signatureHash, resolved.methodSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(1u, resolved.methodFunctionIndex);
    TEST_ASSERT_EQUAL_PTR(&methodInfo1, resolved.methodInfo);
    TEST_ASSERT_TRUE(resolved.methodFunctionPointer == test_reflection_aot_entry);
    TEST_ASSERT_TRUE(resolved.methodInvoker == test_reflection_aot_invoker);
}

static void test_reflection_builds_field_info_object_from_fielddef_token(void) {
    SZrState *state = create_reflection_test_state();
    SZrObjectModule *module;
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    const SZrTypeValue *fieldTypeValue;
    SZrObject *fieldTypeInfo;
    const SZrTypeValue *fieldTypeSignatureTypeValue;
    SZrObject *fieldTypeSignatureTypeInfo;
    const SZrTypeValue *declaringTypeValue;
    SZrObject *declaringTypeInfo;
    const SZrTypeValue *ownerValue;
    SZrObject *ownerInfo;
    const SZrTypeValue *moduleValue;
    SZrObject *moduleInfo;
    SZrString *moduleName;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);
    module = ZrCore_Module_Create(state);
    TEST_ASSERT_NOT_NULL(module);
    moduleName = ZrCore_String_CreateFromNative(state, "geometry");
    TEST_ASSERT_NOT_NULL(moduleName);
    ZrCore_Module_SetInfo(state, module, moduleName, 0x67656f6d65747279ULL, ZR_NULL);

    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = 64u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = 16u;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          ZR_NULL,
                                                          0u,
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);

    TEST_ASSERT_NULL(ZrCore_Reflection_BuildFieldInfoTokenObject(ZR_NULL, runtime, TEST_FIELD_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildFieldInfoTokenObject(state, ZR_NULL, TEST_FIELD_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_TYPE_DEF_TOKEN));

    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, fieldInfo));
    assert_object_string_field(state, fieldInfo, "kind", "field");
    assert_object_string_field(state, fieldInfo, "name", "x");
    assert_object_string_field(state, fieldInfo, "qualifiedName", "Point.x");
    assert_object_string_field(state, fieldInfo, "typeName", "int");
    assert_object_string_field(state, fieldInfo, "ownerTypeName", "Point");
    assert_object_string_field(state, fieldInfo, "declaringTypeName", "Point");
    assert_object_string_field(state, fieldInfo, "moduleName", "geometry");
    assert_object_int_field(state, fieldInfo, "metadataToken", TEST_FIELD_DEF_TOKEN);
    assert_object_native_pointer_field(state, fieldInfo, "metadataRuntime", runtime);
    assert_object_int_field(state, fieldInfo, "metadataFlags", 0xA5);
    assert_object_int_field(state, fieldInfo, "signatureBlobOffset", 4);
    assert_object_int_field(state, fieldInfo, "signatureBlobLength", 7);
    assert_object_int_field(state, fieldInfo, "signatureRootNode", ZR_METADATA_SIGNATURE_NODE_FIELD_SIG);
    assert_object_int_field(state, fieldInfo, "signatureFlags", 1);
    assert_object_int_field(state, fieldInfo, "fieldTypeBlobOffset", TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureNode", ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureBlobOffset", TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureNextBlobOffset",
                            TEST_FIELD_SIGNATURE_FIELD_TYPE_NEXT_BLOB_OFFSET);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignaturePayload0", ZR_VALUE_TYPE_BOOL);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignaturePayload1", 0);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureValueType", ZR_VALUE_TYPE_BOOL);
    assert_object_string_field(state, fieldInfo, "fieldTypeSignatureTypeName", "bool");
    assert_object_bool_field(state, fieldInfo, "fieldTypeSignatureMatchesLayout", ZR_FALSE);
    assert_signature_node_object(state,
                                 fieldInfo,
                                 ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
                                 TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET,
                                 TEST_FIELD_SIGNATURE_FIELD_TYPE_NEXT_BLOB_OFFSET,
                                 ZR_VALUE_TYPE_BOOL,
                                 0u,
                                 0u,
                                 0u,
                                 0u,
                                 0u,
                                 0u,
                                 0u,
                                 "bool",
                                 ZR_FALSE);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureBaseTypeBlobOffset", 0);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureChildCount", 0);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureChildListBlobOffset", 0);
    assert_object_int_field(state, fieldInfo, "ownerTypeToken", TEST_TYPE_DEF_TOKEN);
    assert_object_int_field(state, fieldInfo, "fieldTypeToken", TEST_FIELD_TYPE_DEF_TOKEN);
    assert_object_int_field(state, fieldInfo, "offset", 24);
    assert_object_int_field(state, fieldInfo, "size", 16);
    assert_object_int_field(state, fieldInfo, "typeLayoutId", 42);
    assert_object_int_field(state, fieldInfo, "fieldTypeLayoutId", 42);
    assert_object_int_field(state, fieldInfo, "ownerTypeLayoutId", 7);

    fieldTypeValue = get_object_field_value(state, fieldInfo, "type");
    TEST_ASSERT_NOT_NULL(fieldTypeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, fieldTypeValue->type);
    fieldTypeInfo = ZR_CAST_OBJECT(state, fieldTypeValue->value.object);
    TEST_ASSERT_NOT_NULL(fieldTypeInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, fieldTypeInfo));
    assert_object_string_field(state, fieldTypeInfo, "kind", "type");
    assert_object_string_field(state, fieldTypeInfo, "name", "int");
    assert_object_string_field(state, fieldTypeInfo, "qualifiedName", "int");

    fieldTypeSignatureTypeValue = get_object_field_value(state, fieldInfo, "fieldTypeSignatureType");
    TEST_ASSERT_NOT_NULL(fieldTypeSignatureTypeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, fieldTypeSignatureTypeValue->type);
    fieldTypeSignatureTypeInfo = ZR_CAST_OBJECT(state, fieldTypeSignatureTypeValue->value.object);
    TEST_ASSERT_NOT_NULL(fieldTypeSignatureTypeInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, fieldTypeSignatureTypeInfo));
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "kind", "type");
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "name", "bool");
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "qualifiedName", "bool");

    declaringTypeValue = get_object_field_value(state, fieldInfo, "declaringType");
    TEST_ASSERT_NOT_NULL(declaringTypeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, declaringTypeValue->type);
    declaringTypeInfo = ZR_CAST_OBJECT(state, declaringTypeValue->value.object);
    TEST_ASSERT_NOT_NULL(declaringTypeInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, declaringTypeInfo));
    assert_object_string_field(state, declaringTypeInfo, "kind", "type");
    assert_object_string_field(state, declaringTypeInfo, "name", "Point");
    assert_object_string_field(state, declaringTypeInfo, "qualifiedName", "Point");

    ownerValue = get_object_field_value(state, fieldInfo, "owner");
    TEST_ASSERT_NOT_NULL(ownerValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, ownerValue->type);
    ownerInfo = ZR_CAST_OBJECT(state, ownerValue->value.object);
    TEST_ASSERT_EQUAL_PTR(declaringTypeInfo, ownerInfo);

    moduleValue = get_object_field_value(state, fieldInfo, "module");
    TEST_ASSERT_NOT_NULL(moduleValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleValue->type);
    moduleInfo = ZR_CAST_OBJECT(state, moduleValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, moduleInfo));
    assert_object_string_field(state, moduleInfo, "kind", "module");
    assert_object_string_field(state, moduleInfo, "name", "geometry");
    assert_object_string_field(state, moduleInfo, "qualifiedName", "geometry");

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_field_info_value_slot_from_inline_storage(void) {
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue storedValue;
    SZrTypeValue readValue;
    TZrByte inlineStorage[TEST_FIELD_VALUE_SLOT_OFFSET + sizeof(SZrTypeValue)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          ZR_NULL,
                                                          0u,
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);
    fieldRows[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;

    ZrCore_Value_InitAsInt(state, &storedValue, 314159);
    memcpy(inlineStorage + TEST_FIELD_VALUE_SLOT_OFFSET, &storedValue, sizeof(storedValue));
    ZrCore_Value_ResetAsNull(&readValue);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoTokenValue(ZR_NULL,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                                ZR_NULL,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_TYPE_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                TEST_FIELD_VALUE_SLOT_OFFSET,
                                                                &readValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(314159, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_field_info_object_value_from_inline_storage(void) {
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue storedValue;
    SZrTypeValue readValue;
    TZrByte inlineStorage[TEST_FIELD_VALUE_SLOT_OFFSET + sizeof(SZrTypeValue)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          ZR_NULL,
                                                          0u,
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);
    fieldRows[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_InitAsInt(state, &storedValue, 271828);
    memcpy(inlineStorage + TEST_FIELD_VALUE_SLOT_OFFSET, &storedValue, sizeof(storedValue));
    ZrCore_Value_ResetAsNull(&readValue);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectValue(ZR_NULL,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                 ZR_NULL,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 TEST_FIELD_VALUE_SLOT_OFFSET,
                                                                 &readValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                fieldInfo,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(271828, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static void test_reflection_writes_field_info_object_value_to_inline_storage(void) {
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue storedValue;
    SZrTypeValue writeValue;
    SZrTypeValue readValue;
    TZrByte inlineStorage[TEST_FIELD_VALUE_SLOT_OFFSET + sizeof(SZrTypeValue)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          ZR_NULL,
                                                          0u,
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);
    fieldRows[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_InitAsInt(state, &storedValue, 11);
    ZrCore_Value_InitAsInt(state, &writeValue, 161803);
    ZrCore_Value_ResetAsNull(&readValue);
    memcpy(inlineStorage + TEST_FIELD_VALUE_SLOT_OFFSET, &storedValue, sizeof(storedValue));

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(ZR_NULL,
                                                                  fieldInfo,
                                                                  inlineStorage,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  ZR_NULL,
                                                                  inlineStorage,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  fieldInfo,
                                                                  ZR_NULL,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  fieldInfo,
                                                                  inlineStorage,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  fieldInfo,
                                                                  inlineStorage,
                                                                  TEST_FIELD_VALUE_SLOT_OFFSET,
                                                                  &writeValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                fieldInfo,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(11, readValue.value.nativeObject.nativeInt64);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &writeValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                fieldInfo,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(161803, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_and_writes_field_info_object_primitive_pod_from_inline_storage(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT32, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue boolValue;
    TZrInt32 storedI32 = -12345;
    TZrInt32 preservedI32 = 0;
    TZrInt32 writtenI32 = 0;
    TZrByte inlineStorage[TEST_FIELD_RAW_I32_OFFSET + sizeof(TZrInt32)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_RAW_I32_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(TZrInt32);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrInt32);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(TZrInt32);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrInt32);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);
    fieldRows[0].byteOffset = TEST_FIELD_RAW_I32_OFFSET;
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, &writeValue, 4096);
    ZrCore_Value_InitAsBool(state, &boolValue, ZR_TRUE);
    memcpy(inlineStorage + TEST_FIELD_RAW_I32_OFFSET, &storedI32, sizeof(storedI32));

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 TEST_FIELD_RAW_I32_OFFSET,
                                                                 &readValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                fieldInfo,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(-12345, readValue.value.nativeObject.nativeInt64);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  fieldInfo,
                                                                  inlineStorage,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  &boolValue));
    memcpy(&preservedI32, inlineStorage + TEST_FIELD_RAW_I32_OFFSET, sizeof(preservedI32));
    TEST_ASSERT_EQUAL_INT32(-12345, preservedI32);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &writeValue));
    memcpy(&writtenI32, inlineStorage + TEST_FIELD_RAW_I32_OFFSET, sizeof(writtenI32));
    TEST_ASSERT_EQUAL_INT32(4096, writtenI32);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                fieldInfo,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(4096, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_field_info_object_inline_struct_borrowed_view(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[2] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue sourceView;
    SZrTypeValue nullSourceView;
    TZrInt32 storedPair[2] = {1234, -5678};
    TZrInt32 sourcePair[2] = {2468, -1357};
    TZrInt32 preservedPair[2] = {0, 0};
    TZrInt32 writtenPair[2] = {0, 0};
    TZrByte inlineStorage[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(storedPair)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(storedPair);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrInt32);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(storedPair);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrInt32);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    fieldLayout.copyKind = (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_POD;
    fieldLayout.dropKind = (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_NONE;
    fieldLayout.blittable = ZR_TRUE;
    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(TZrInt32);
    fieldTypeFields[0].typeLayoutIndex = 0u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    fieldTypeFields[1].byteOffset = (TZrUInt32)sizeof(TZrInt32);
    fieldTypeFields[1].byteSize = (TZrUInt32)sizeof(TZrInt32);
    fieldTypeFields[1].typeLayoutIndex = 0u;
    fieldTypeFields[1].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, &writeValue, 4096);
    ZrCore_Value_InitAsNativePointer(state, &sourceView, sourcePair);
    ZrCore_Value_InitAsNativePointer(state, &nullSourceView, ZR_NULL);
    memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, storedPair, sizeof(storedPair));

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 TEST_FIELD_INLINE_STRUCT_OFFSET,
                                                                 &readValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                fieldInfo,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NATIVE_POINTER, readValue.type);
    TEST_ASSERT_EQUAL_PTR(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET,
                          readValue.value.nativeObject.nativePointer);
    TEST_ASSERT_FALSE(readValue.isGarbageCollectable);
    TEST_ASSERT_TRUE(readValue.isNative);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, readValue.ownershipKind);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  fieldInfo,
                                                                  inlineStorage,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  &writeValue));
    memcpy(preservedPair, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(preservedPair));
    TEST_ASSERT_EQUAL_INT32(1234, preservedPair[0]);
    TEST_ASSERT_EQUAL_INT32(-5678, preservedPair[1]);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                  fieldInfo,
                                                                  inlineStorage,
                                                                  (TZrUInt32)sizeof(inlineStorage),
                                                                  &nullSourceView));
    memcpy(preservedPair, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(preservedPair));
    TEST_ASSERT_EQUAL_INT32(1234, preservedPair[0]);
    TEST_ASSERT_EQUAL_INT32(-5678, preservedPair[1]);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &sourceView));
    memcpy(writtenPair, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenPair));
    TEST_ASSERT_EQUAL_INT32(2468, writtenPair[0]);
    TEST_ASSERT_EQUAL_INT32(-1357, writtenPair[1]);

    fieldLayout.blittable = ZR_FALSE;
    fieldLayout.copyKind = (TZrUInt8)ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY;
    fieldLayout.dropKind = (TZrUInt8)ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP;
    fieldLayout.fields = fieldTypeFields;
    fieldLayout.fieldCount = 2u;
    sourcePair[0] = 11;
    sourcePair[1] = 22;
    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &sourceView));
    memcpy(preservedPair, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(preservedPair));
    TEST_ASSERT_EQUAL_INT32(11, preservedPair[0]);
    TEST_ASSERT_EQUAL_INT32(22, preservedPair[1]);

    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &readValue));

    destroy_reflection_test_state(state);
}

static void test_reflection_writes_field_info_object_inline_struct_drops_replaced_owned_value_field(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[1] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrString *oldText;
    SZrString *newText;
    SZrRawObject *oldRaw;
    SZrRawObject *newRaw;
    SZrTypeValue sourceView;
    union {
        TZrByte bytes[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(SZrTypeValue)];
        SZrTypeValue alignValue;
        TZrPtr alignPtr;
    } inlineStorage = {{0}};
    union {
        TZrByte bytes[sizeof(SZrTypeValue)];
        SZrTypeValue value;
    } sourceAggregate = {{0}};
    SZrTypeValue *destinationSlot =
            (SZrTypeValue *)(void *)(inlineStorage.bytes + TEST_FIELD_INLINE_STRUCT_OFFSET);
    SZrTypeValue *sourceSlot = &sourceAggregate.value;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&ownerLayout,
                                 (TZrUInt32)sizeof(inlineStorage.bytes),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 ownerFields,
                                 1u);
    ownerLayout.cTypeId = 7u;

    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldTypeFields[0].typeLayoutIndex = 0u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(&fieldLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 fieldTypeFields,
                                 1u);
    fieldLayout.cTypeId = 42u;

    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldRows[0].signatureBlobLength = (TZrUInt32)sizeof(fieldSignatureBlob);
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    oldText = ZrCore_String_CreateFromNative(state, "old-owned-inline-field");
    newText = ZrCore_String_CreateFromNative(state, "new-owned-inline-field");
    TEST_ASSERT_NOT_NULL(oldText);
    TEST_ASSERT_NOT_NULL(newText);
    oldRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(oldText);
    newRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(newText);

    ZrCore_Value_ResetAsNull(destinationSlot);
    ZrCore_Value_ResetAsNull(sourceSlot);
    ZrCore_Value_InitAsRawObject(state, sourceSlot, newRaw);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(state, destinationSlot, oldRaw));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(newRaw));

    ZrCore_Value_InitAsNativePointer(state, &sourceView, sourceAggregate.bytes);
    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectValue(state,
                                                                 fieldInfo,
                                                                 inlineStorage.bytes,
                                                                 (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                 &sourceView));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, destinationSlot->type);
    TEST_ASSERT_TRUE(destinationSlot->isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(newRaw, destinationSlot->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, destinationSlot->ownershipKind);
    TEST_ASSERT_NULL(destinationSlot->ownershipControl);
    TEST_ASSERT_NULL(destinationSlot->ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(newRaw));

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_field_info_object_nested_value_slot_from_inline_struct(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[1] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue readValue;
    union {
        TZrByte bytes[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(SZrTypeValue)];
        SZrTypeValue alignValue;
        TZrPtr alignPtr;
    } inlineStorage = {{0}};
    SZrTypeValue *nestedSlot =
            (SZrTypeValue *)(void *)(inlineStorage.bytes + TEST_FIELD_INLINE_STRUCT_OFFSET);
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&ownerLayout,
                                 (TZrUInt32)sizeof(inlineStorage.bytes),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 ownerFields,
                                 1u);
    ownerLayout.cTypeId = 7u;

    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldTypeFields[0].typeLayoutIndex = 0u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(&fieldLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 fieldTypeFields,
                                 1u);
    fieldLayout.cTypeId = 42u;

    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldRows[0].signatureBlobLength = (TZrUInt32)sizeof(fieldSignatureBlob);
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, nestedSlot, 314159);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedValue(state,
                                                                       fieldInfo,
                                                                       inlineStorage.bytes,
                                                                       TEST_FIELD_INLINE_STRUCT_OFFSET,
                                                                       0u,
                                                                       &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedValue(state,
                                                                       fieldInfo,
                                                                       inlineStorage.bytes,
                                                                       (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                       1u,
                                                                       &readValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedValue(state,
                                                                      fieldInfo,
                                                                      inlineStorage.bytes,
                                                                      (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                      0u,
                                                                      &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(314159, readValue.value.nativeObject.nativeInt64);

    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedValue(state,
                                                                       fieldInfo,
                                                                       inlineStorage.bytes,
                                                                       (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                       0u,
                                                                       &readValue));

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_field_info_object_nested_path_value_slot_from_inline_struct(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrUInt32 pathToNestedSlot[] = {0u, 0u};
    static const TZrUInt32 pathToOutOfRangeNestedSlot[] = {0u, 1u};
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[1] = {0};
    SZrTypeLayout innerLayout = {0};
    SZrTypeLayoutField innerFields[1] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue readValue;
    union {
        TZrByte bytes[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(SZrTypeValue)];
        SZrTypeValue alignValue;
        TZrPtr alignPtr;
    } inlineStorage = {{0}};
    SZrTypeValue *nestedSlot =
            (SZrTypeValue *)(void *)(inlineStorage.bytes + TEST_FIELD_INLINE_STRUCT_OFFSET);
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&ownerLayout,
                                 (TZrUInt32)sizeof(inlineStorage.bytes),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 ownerFields,
                                 1u);
    ownerLayout.cTypeId = 7u;

    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldTypeFields[0].typeLayoutIndex = 41u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&fieldLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 fieldTypeFields,
                                 1u);
    fieldLayout.cTypeId = 42u;

    innerFields[0].byteOffset = 0u;
    innerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    innerFields[0].typeLayoutIndex = 0u;
    innerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(&innerLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 innerFields,
                                 1u);
    innerLayout.cTypeId = 41u;

    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[41] = &innerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldRows[0].signatureBlobLength = (TZrUInt32)sizeof(fieldSignatureBlob);
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, nestedSlot, 424242);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            0u,
            &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToOutOfRangeNestedSlot,
            2u,
            &readValue));

    registeredLayouts[41] = ZR_NULL;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            2u,
            &readValue));
    registeredLayouts[41] = &innerLayout;

    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            2u,
            &readValue));
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            2u,
            &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(424242, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static void test_reflection_writes_field_info_object_nested_value_slot_from_inline_struct(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[1] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrString *oldText;
    SZrString *newText;
    SZrRawObject *oldRaw;
    SZrRawObject *newRaw;
    SZrTypeValue newValue;
    union {
        TZrByte bytes[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(SZrTypeValue)];
        SZrTypeValue alignValue;
        TZrPtr alignPtr;
    } inlineStorage = {{0}};
    SZrTypeValue *nestedSlot =
            (SZrTypeValue *)(void *)(inlineStorage.bytes + TEST_FIELD_INLINE_STRUCT_OFFSET);
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&ownerLayout,
                                 (TZrUInt32)sizeof(inlineStorage.bytes),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 ownerFields,
                                 1u);
    ownerLayout.cTypeId = 7u;

    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldTypeFields[0].typeLayoutIndex = 0u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(&fieldLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 fieldTypeFields,
                                 1u);
    fieldLayout.cTypeId = 42u;

    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldRows[0].signatureBlobLength = (TZrUInt32)sizeof(fieldSignatureBlob);
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    oldText = ZrCore_String_CreateFromNative(state, "old-nested-owned-field");
    newText = ZrCore_String_CreateFromNative(state, "new-nested-field");
    TEST_ASSERT_NOT_NULL(oldText);
    TEST_ASSERT_NOT_NULL(newText);
    oldRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(oldText);
    newRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(newText);

    ZrCore_Value_ResetAsNull(nestedSlot);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(state, nestedSlot, oldRaw));
    ZrCore_Value_InitAsRawObject(state, &newValue, newRaw);
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(newRaw));

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedValue(state,
                                                                        fieldInfo,
                                                                        inlineStorage.bytes,
                                                                        TEST_FIELD_INLINE_STRUCT_OFFSET,
                                                                        0u,
                                                                        &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedValue(state,
                                                                        fieldInfo,
                                                                        inlineStorage.bytes,
                                                                        (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                        1u,
                                                                        &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));

    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedValue(state,
                                                                        fieldInfo,
                                                                        inlineStorage.bytes,
                                                                        (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                        0u,
                                                                        &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedValue(state,
                                                                       fieldInfo,
                                                                       inlineStorage.bytes,
                                                                       (TZrUInt32)sizeof(inlineStorage.bytes),
                                                                       0u,
                                                                       &newValue));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, nestedSlot->type);
    TEST_ASSERT_TRUE(nestedSlot->isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(newRaw, nestedSlot->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, nestedSlot->ownershipKind);
    TEST_ASSERT_NULL(nestedSlot->ownershipControl);
    TEST_ASSERT_NULL(nestedSlot->ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(newRaw));

    destroy_reflection_test_state(state);
}

static void test_reflection_writes_field_info_object_nested_path_value_slot_from_inline_struct(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrUInt32 pathToNestedSlot[] = {0u, 0u};
    static const TZrUInt32 pathToOutOfRangeNestedSlot[] = {0u, 1u};
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[1] = {0};
    SZrTypeLayout innerLayout = {0};
    SZrTypeLayoutField innerFields[1] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrString *oldText;
    SZrString *newText;
    SZrRawObject *oldRaw;
    SZrRawObject *newRaw;
    SZrTypeValue newValue;
    union {
        TZrByte bytes[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(SZrTypeValue)];
        SZrTypeValue alignValue;
        TZrPtr alignPtr;
    } inlineStorage = {{0}};
    SZrTypeValue *nestedSlot =
            (SZrTypeValue *)(void *)(inlineStorage.bytes + TEST_FIELD_INLINE_STRUCT_OFFSET);
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&ownerLayout,
                                 (TZrUInt32)sizeof(inlineStorage.bytes),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 ownerFields,
                                 1u);
    ownerLayout.cTypeId = 7u;

    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldTypeFields[0].typeLayoutIndex = 41u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&fieldLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 fieldTypeFields,
                                 1u);
    fieldLayout.cTypeId = 42u;

    innerFields[0].byteOffset = 0u;
    innerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    innerFields[0].typeLayoutIndex = 0u;
    innerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                           ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(&innerLayout,
                                 (TZrUInt32)sizeof(SZrTypeValue),
                                 (TZrUInt32)ZR_ALIGN_SIZE,
                                 ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
                                 ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
                                 innerFields,
                                 1u);
    innerLayout.cTypeId = 41u;

    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[41] = &innerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldRows[0].signatureBlobLength = (TZrUInt32)sizeof(fieldSignatureBlob);
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    oldText = ZrCore_String_CreateFromNative(state, "old-nested-path-owned-field");
    newText = ZrCore_String_CreateFromNative(state, "new-nested-path-field");
    TEST_ASSERT_NOT_NULL(oldText);
    TEST_ASSERT_NOT_NULL(newText);
    oldRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(oldText);
    newRaw = ZR_CAST_RAW_OBJECT_AS_SUPER(newText);

    ZrCore_Value_ResetAsNull(nestedSlot);
    TEST_ASSERT_TRUE(ZrCore_Ownership_InitUniqueValue(state, nestedSlot, oldRaw));
    ZrCore_Value_InitAsRawObject(state, &newValue, newRaw);
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(newRaw));

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            TEST_FIELD_INLINE_STRUCT_OFFSET,
            pathToNestedSlot,
            2u,
            &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            0u,
            &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToOutOfRangeNestedSlot,
            2u,
            &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));

    registeredLayouts[41] = ZR_NULL;
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            2u,
            &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    registeredLayouts[41] = &innerLayout;

    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            2u,
            &newValue));
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue(
            state,
            fieldInfo,
            inlineStorage.bytes,
            (TZrUInt32)sizeof(inlineStorage.bytes),
            pathToNestedSlot,
            2u,
            &newValue));

    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(oldRaw));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, nestedSlot->type);
    TEST_ASSERT_TRUE(nestedSlot->isGarbageCollectable);
    TEST_ASSERT_EQUAL_PTR(newRaw, nestedSlot->value.object);
    TEST_ASSERT_EQUAL_INT(ZR_OWNERSHIP_VALUE_KIND_NONE, nestedSlot->ownershipKind);
    TEST_ASSERT_NULL(nestedSlot->ownershipControl);
    TEST_ASSERT_NULL(nestedSlot->ownershipWeakRef);
    TEST_ASSERT_EQUAL_UINT32(0u, ZrCore_Ownership_GetStrongRefCount(newRaw));

    destroy_reflection_test_state(state);
}

static void configure_nested_primitive_path_field_sizes(SZrTypeLayoutField *ownerField,
                                                        SZrTypeLayout *fieldLayout,
                                                        SZrTypeLayoutField *fieldTypeField,
                                                        SZrTypeLayout *innerLayout,
                                                        SZrTypeLayoutField *innerField,
                                                        TZrUInt32 byteSize,
                                                        TZrUInt32 byteAlign) {
    ownerField->byteSize = byteSize;
    fieldTypeField->byteSize = byteSize;
    fieldLayout->byteSize = byteSize;
    fieldLayout->byteAlign = byteAlign;
    innerField->byteSize = byteSize;
    innerLayout->byteSize = byteSize;
    innerLayout->byteAlign = byteAlign;
}

static void test_reflection_reads_and_writes_field_info_object_nested_path_primitive_pod_from_inline_struct(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrUInt32 pathToNestedPrimitive[] = {0u, 0u};
    static const TZrUInt32 pathToOutOfRangeNestedPrimitive[] = {0u, 1u};
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    SZrTypeLayoutField fieldTypeFields[1] = {0};
    SZrTypeLayout innerLayout = {0};
    SZrTypeLayoutField innerFields[1] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue rejectedBoolValue;
    TZrInt32 storedI32 = -12345;
    TZrInt32 preservedI32 = 0;
    TZrInt32 writtenI32 = 0;
    TZrByte inlineStorage[TEST_FIELD_INLINE_STRUCT_OFFSET + sizeof(TZrDouble)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(TZrInt32);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&ownerLayout,
                                 (TZrUInt32)sizeof(inlineStorage),
                                 (TZrUInt32)sizeof(TZrInt32),
                                 ZR_TYPE_LAYOUT_COPY_KIND_POD,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 ownerFields,
                                 1u);
    ownerLayout.cTypeId = 7u;

    fieldTypeFields[0].byteOffset = 0u;
    fieldTypeFields[0].byteSize = (TZrUInt32)sizeof(TZrInt32);
    fieldTypeFields[0].typeLayoutIndex = 41u;
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&fieldLayout,
                                 (TZrUInt32)sizeof(TZrInt32),
                                 (TZrUInt32)sizeof(TZrInt32),
                                 ZR_TYPE_LAYOUT_COPY_KIND_POD,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 fieldTypeFields,
                                 1u);
    fieldLayout.cTypeId = 42u;

    innerFields[0].byteOffset = 0u;
    innerFields[0].byteSize = (TZrUInt32)sizeof(TZrInt32);
    innerFields[0].typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    innerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ZrCore_TypeLayout_InitStruct(&innerLayout,
                                 (TZrUInt32)sizeof(TZrInt32),
                                 (TZrUInt32)sizeof(TZrInt32),
                                 ZR_TYPE_LAYOUT_COPY_KIND_POD,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 innerFields,
                                 1u);
    innerLayout.cTypeId = 41u;

    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[41] = &innerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    fieldRows[0].byteOffset = TEST_FIELD_INLINE_STRUCT_OFFSET;
    fieldRows[0].signatureBlobLength = (TZrUInt32)sizeof(fieldSignatureBlob);
    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);

    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, &writeValue, 2048);
    ZrCore_Value_InitAsBool(state, &rejectedBoolValue, ZR_TRUE);
    memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedI32, sizeof(storedI32));

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            TEST_FIELD_INLINE_STRUCT_OFFSET,
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            0u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToOutOfRangeNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));

    registeredLayouts[41] = ZR_NULL;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    registeredLayouts[41] = &innerLayout;

    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                               ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    fieldTypeFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;

    innerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    innerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;

    innerFields[0].typeLayoutIndex = 42u;
    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &writeValue));
    innerFields[0].typeLayoutIndex = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    memcpy(&preservedI32, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(preservedI32));
    TEST_ASSERT_EQUAL_INT32(-12345, preservedI32);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_DOUBLE,
            &readValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(-12345, readValue.value.nativeObject.nativeInt64);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &rejectedBoolValue));
    memcpy(&preservedI32, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(preservedI32));
    TEST_ASSERT_EQUAL_INT32(-12345, preservedI32);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_DOUBLE,
            &writeValue));
    memcpy(&preservedI32, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(preservedI32));
    TEST_ASSERT_EQUAL_INT32(-12345, preservedI32);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &writeValue));
    memcpy(&writtenI32, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenI32));
    TEST_ASSERT_EQUAL_INT32(2048, writtenI32);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
            state,
            fieldInfo,
            inlineStorage,
            (TZrUInt32)sizeof(inlineStorage),
            pathToNestedPrimitive,
            2u,
            (TZrUInt32)ZR_VALUE_TYPE_INT32,
            &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(2048, readValue.value.nativeObject.nativeInt64);

    configure_nested_primitive_path_field_sizes(&ownerFields[0],
                                                &fieldLayout,
                                                &fieldTypeFields[0],
                                                &innerLayout,
                                                &innerFields[0],
                                                (TZrUInt32)sizeof(TZrBool),
                                                (TZrUInt32)sizeof(TZrBool));
    {
        TZrBool storedBool = ZR_TRUE;
        TZrBool writtenBool = ZR_TRUE;
        ZrCore_Value_InitAsBool(state, &writeValue, ZR_FALSE);
        memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedBool, sizeof(storedBool));
        TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_BOOL,
                &readValue));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, readValue.type);
        TEST_ASSERT_TRUE(readValue.value.nativeObject.nativeBool);
        TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_BOOL,
                &writeValue));
        memcpy(&writtenBool, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenBool));
        TEST_ASSERT_FALSE(writtenBool);
    }

    configure_nested_primitive_path_field_sizes(&ownerFields[0],
                                                &fieldLayout,
                                                &fieldTypeFields[0],
                                                &innerLayout,
                                                &innerFields[0],
                                                (TZrUInt32)sizeof(TZrUInt32),
                                                (TZrUInt32)sizeof(TZrUInt32));
    {
        TZrUInt32 storedU32 = 0xFEDC1234u;
        TZrUInt32 writtenU32 = 0u;
        ZrCore_Value_InitAsUInt(state, &writeValue, 0xAABBCCDDULL);
        memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedU32, sizeof(storedU32));
        TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_UINT32,
                &readValue));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, readValue.type);
        TEST_ASSERT_EQUAL_UINT64(0xFEDC1234ULL, readValue.value.nativeObject.nativeUInt64);
        TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_UINT32,
                &writeValue));
        memcpy(&writtenU32, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenU32));
        TEST_ASSERT_EQUAL_UINT32(0xAABBCCDDu, writtenU32);
    }

    configure_nested_primitive_path_field_sizes(&ownerFields[0],
                                                &fieldLayout,
                                                &fieldTypeFields[0],
                                                &innerLayout,
                                                &innerFields[0],
                                                (TZrUInt32)sizeof(TZrDouble),
                                                (TZrUInt32)sizeof(TZrDouble));
    {
        TZrDouble storedDouble = 6.25;
        TZrDouble writtenDouble = 0.0;
        ZrCore_Value_InitAsFloat(state, &writeValue, -12.5);
        memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedDouble, sizeof(storedDouble));
        TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_DOUBLE,
                &readValue));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, readValue.type);
        TEST_ASSERT_DOUBLE_WITHIN(0.000001, 6.25, readValue.value.nativeObject.nativeDouble);
        TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_DOUBLE,
                &writeValue));
        memcpy(&writtenDouble, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenDouble));
        TEST_ASSERT_DOUBLE_WITHIN(0.000001, -12.5, writtenDouble);
    }

#define ASSERT_NESTED_SIGNED_PRIMITIVE_PATH_WIDTH(valueType, cType, initialValue, writeRawValue)                     \
    do {                                                                                                             \
        cType storedRaw = (cType)(initialValue);                                                                      \
        cType writtenRaw = (cType)0;                                                                                  \
        configure_nested_primitive_path_field_sizes(&ownerFields[0],                                                  \
                                                    &fieldLayout,                                                     \
                                                    &fieldTypeFields[0],                                              \
                                                    &innerLayout,                                                     \
                                                    &innerFields[0],                                                  \
                                                    (TZrUInt32)sizeof(cType),                                         \
                                                    (TZrUInt32)sizeof(cType));                                        \
        ZrCore_Value_InitAsInt(state, &writeValue, (TZrInt64)(writeRawValue));                                        \
        memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedRaw, sizeof(storedRaw));                      \
        TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(                               \
                state,                                                                                               \
                fieldInfo,                                                                                           \
                inlineStorage,                                                                                       \
                (TZrUInt32)sizeof(inlineStorage),                                                                    \
                pathToNestedPrimitive,                                                                               \
                2u,                                                                                                  \
                (TZrUInt32)(valueType),                                                                              \
                &readValue));                                                                                        \
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);                                                   \
        TEST_ASSERT_EQUAL_INT64((TZrInt64)(initialValue), readValue.value.nativeObject.nativeInt64);                  \
        TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(                              \
                state,                                                                                               \
                fieldInfo,                                                                                           \
                inlineStorage,                                                                                       \
                (TZrUInt32)sizeof(inlineStorage),                                                                    \
                pathToNestedPrimitive,                                                                               \
                2u,                                                                                                  \
                (TZrUInt32)(valueType),                                                                              \
                &writeValue));                                                                                       \
        memcpy(&writtenRaw, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenRaw));                    \
        TEST_ASSERT_EQUAL_INT64((TZrInt64)(writeRawValue), (TZrInt64)writtenRaw);                                     \
    } while (0)

#define ASSERT_NESTED_UNSIGNED_PRIMITIVE_PATH_WIDTH(valueType, cType, initialValue, writeRawValue)                   \
    do {                                                                                                             \
        cType storedRaw = (cType)(initialValue);                                                                      \
        cType writtenRaw = (cType)0;                                                                                  \
        configure_nested_primitive_path_field_sizes(&ownerFields[0],                                                  \
                                                    &fieldLayout,                                                     \
                                                    &fieldTypeFields[0],                                              \
                                                    &innerLayout,                                                     \
                                                    &innerFields[0],                                                  \
                                                    (TZrUInt32)sizeof(cType),                                         \
                                                    (TZrUInt32)sizeof(cType));                                        \
        ZrCore_Value_InitAsUInt(state, &writeValue, (TZrUInt64)(writeRawValue));                                      \
        memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedRaw, sizeof(storedRaw));                      \
        TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(                               \
                state,                                                                                               \
                fieldInfo,                                                                                           \
                inlineStorage,                                                                                       \
                (TZrUInt32)sizeof(inlineStorage),                                                                    \
                pathToNestedPrimitive,                                                                               \
                2u,                                                                                                  \
                (TZrUInt32)(valueType),                                                                              \
                &readValue));                                                                                        \
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, readValue.type);                                                  \
        TEST_ASSERT_EQUAL_UINT64((TZrUInt64)(initialValue), readValue.value.nativeObject.nativeUInt64);               \
        TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(                              \
                state,                                                                                               \
                fieldInfo,                                                                                           \
                inlineStorage,                                                                                       \
                (TZrUInt32)sizeof(inlineStorage),                                                                    \
                pathToNestedPrimitive,                                                                               \
                2u,                                                                                                  \
                (TZrUInt32)(valueType),                                                                              \
                &writeValue));                                                                                       \
        memcpy(&writtenRaw, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenRaw));                    \
        TEST_ASSERT_EQUAL_UINT64((TZrUInt64)(writeRawValue), (TZrUInt64)writtenRaw);                                  \
    } while (0)

    ASSERT_NESTED_SIGNED_PRIMITIVE_PATH_WIDTH(ZR_VALUE_TYPE_INT8, TZrInt8, -7, 42);
    ASSERT_NESTED_SIGNED_PRIMITIVE_PATH_WIDTH(ZR_VALUE_TYPE_INT16, TZrInt16, -1234, 2345);
    ASSERT_NESTED_SIGNED_PRIMITIVE_PATH_WIDTH(ZR_VALUE_TYPE_INT64, TZrInt64, -1234567890LL, 9876543210LL);
    ASSERT_NESTED_UNSIGNED_PRIMITIVE_PATH_WIDTH(ZR_VALUE_TYPE_UINT8, TZrUInt8, 0x7Au, 0xC3u);
    ASSERT_NESTED_UNSIGNED_PRIMITIVE_PATH_WIDTH(ZR_VALUE_TYPE_UINT16, TZrUInt16, 0x1234u, 0xBEEFu);
    ASSERT_NESTED_UNSIGNED_PRIMITIVE_PATH_WIDTH(ZR_VALUE_TYPE_UINT64,
                                                TZrUInt64,
                                                0x1122334455667788ULL,
                                                0x8877665544332211ULL);

#undef ASSERT_NESTED_SIGNED_PRIMITIVE_PATH_WIDTH
#undef ASSERT_NESTED_UNSIGNED_PRIMITIVE_PATH_WIDTH

    configure_nested_primitive_path_field_sizes(&ownerFields[0],
                                                &fieldLayout,
                                                &fieldTypeFields[0],
                                                &innerLayout,
                                                &innerFields[0],
                                                (TZrUInt32)sizeof(TZrFloat32),
                                                (TZrUInt32)sizeof(TZrFloat32));
    {
        TZrFloat32 storedFloat = 1.25f;
        TZrFloat32 writtenFloat = 0.0f;
        ZrCore_Value_InitAsFloat(state, &writeValue, -3.5);
        memcpy(inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, &storedFloat, sizeof(storedFloat));
        TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_FLOAT,
                &readValue));
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, readValue.type);
        TEST_ASSERT_DOUBLE_WITHIN(0.000001, 1.25, readValue.value.nativeObject.nativeDouble);
        TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue(
                state,
                fieldInfo,
                inlineStorage,
                (TZrUInt32)sizeof(inlineStorage),
                pathToNestedPrimitive,
                2u,
                (TZrUInt32)ZR_VALUE_TYPE_FLOAT,
                &writeValue));
        memcpy(&writtenFloat, inlineStorage + TEST_FIELD_INLINE_STRUCT_OFFSET, sizeof(writtenFloat));
        TEST_ASSERT_DOUBLE_WITHIN(0.000001, -3.5, (TZrDouble)writtenFloat);
    }

    destroy_reflection_test_state(state);
}

static void test_reflection_writes_field_info_value_slot_to_inline_storage(void) {
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue storedValue;
    SZrTypeValue writeValue;
    SZrTypeValue readValue;
    TZrByte inlineStorage[TEST_FIELD_VALUE_SLOT_OFFSET + sizeof(SZrTypeValue)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrPtr);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          ZR_NULL,
                                                          0u,
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);
    fieldRows[0].byteOffset = TEST_FIELD_VALUE_SLOT_OFFSET;

    ZrCore_Value_InitAsInt(state, &storedValue, 11);
    ZrCore_Value_InitAsInt(state, &writeValue, 271828);
    ZrCore_Value_ResetAsNull(&readValue);
    memcpy(inlineStorage + TEST_FIELD_VALUE_SLOT_OFFSET, &storedValue, sizeof(storedValue));

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(ZR_NULL,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 ZR_NULL,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_TYPE_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 ZR_NULL,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &writeValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 TEST_FIELD_VALUE_SLOT_OFFSET,
                                                                 &writeValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(11, readValue.value.nativeObject.nativeInt64);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(readValue.type));
    TEST_ASSERT_EQUAL_INT64(271828, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static void test_reflection_reads_and_writes_field_info_primitive_pod_from_inline_storage(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT32, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue boolValue;
    TZrInt32 storedI32 = -12345;
    TZrInt32 writtenI32 = 0;
    TZrByte inlineStorage[TEST_FIELD_RAW_I32_OFFSET + sizeof(TZrInt32)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    TEST_ASSERT_NOT_NULL(state);

    ownerFields[0].byteOffset = TEST_FIELD_RAW_I32_OFFSET;
    ownerFields[0].byteSize = (TZrUInt32)sizeof(TZrInt32);
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = (TZrUInt32)sizeof(inlineStorage);
    ownerLayout.byteAlign = (TZrUInt32)sizeof(TZrInt32);
    ownerLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout.fields = ownerFields;
    ownerLayout.fieldCount = 1u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = (TZrUInt32)sizeof(TZrInt32);
    fieldLayout.byteAlign = (TZrUInt32)sizeof(TZrInt32);
    fieldLayout.kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          ZR_NULL,
                                                          0u,
                                                          &typeRows,
                                                          &fieldRows);
    fieldRows[0].byteOffset = TEST_FIELD_RAW_I32_OFFSET;

    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, &writeValue, 2048);
    ZrCore_Value_InitAsBool(state, &boolValue, ZR_TRUE);
    memcpy(inlineStorage + TEST_FIELD_RAW_I32_OFFSET, &storedI32, sizeof(storedI32));

    TEST_ASSERT_FALSE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                TEST_FIELD_RAW_I32_OFFSET,
                                                                &readValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(-12345, readValue.value.nativeObject.nativeInt64);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &boolValue));

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeValue));
    memcpy(&writtenI32, inlineStorage + TEST_FIELD_RAW_I32_OFFSET, sizeof(writtenI32));
    TEST_ASSERT_EQUAL_INT32(2048, writtenI32);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(2048, readValue.value.nativeObject.nativeInt64);

    destroy_reflection_test_state(state);
}

static SZrMetadataRuntime *attach_runtime_with_raw_primitive_field(
        SZrObjectModule *module,
        SZrFunction *metadataFunction,
        SZrAotCodeRegistration *registration,
        SZrMetadataTokenRecord *records,
        SZrTypeLayout *ownerLayout,
        SZrTypeLayoutField *ownerFields,
        SZrTypeLayout *fieldLayout,
        const SZrTypeLayout **registeredLayouts,
        TZrByte *metadataBytes,
        TZrSize metadataByteLength,
        EZrValueType valueType,
        TZrUInt32 fieldByteSize,
        TZrUInt32 fieldByteAlign,
        SZrZrpMetadataTypeDefRow **outTypeRows,
        SZrZrpMetadataFieldDefRow **outFieldRows) {
    TZrByte fieldSignatureBlob[TEST_FIELD_SIGNATURE_BLOB_LENGTH] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)valueType, 0u, 0u, 0u,
    };
    SZrMetadataRuntime *runtime;

    ownerFields[0].byteOffset = TEST_FIELD_RAW_MATRIX_OFFSET;
    ownerFields[0].byteSize = fieldByteSize;
    ownerFields[0].typeLayoutIndex = 42u;
    ownerFields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NONE;
    ownerLayout->cTypeId = 7u;
    ownerLayout->byteSize = TEST_FIELD_RAW_MATRIX_OFFSET + fieldByteSize;
    ownerLayout->byteAlign = fieldByteAlign;
    ownerLayout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_STRUCT;
    ownerLayout->fields = ownerFields;
    ownerLayout->fieldCount = 1u;
    fieldLayout->cTypeId = 42u;
    fieldLayout->byteSize = fieldByteSize;
    fieldLayout->byteAlign = fieldByteAlign;
    fieldLayout->kind = (TZrUInt8)ZR_TYPE_LAYOUT_KIND_VALUE;
    registeredLayouts[7] = ownerLayout;
    registeredLayouts[42] = fieldLayout;

    runtime = attach_runtime_with_type_and_field_metadata(module,
                                                          metadataFunction,
                                                          registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          metadataByteLength,
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          ZR_NULL,
                                                          0u,
                                                          outTypeRows,
                                                          outFieldRows);
    (*outFieldRows)[0].byteOffset = TEST_FIELD_RAW_MATRIX_OFFSET;
    return runtime;
}

static void assert_field_info_bool_raw_primitive_case(SZrState *state) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue intValue;
    TZrBool storedBool = ZR_TRUE;
    TZrBool writtenBool = ZR_TRUE;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + sizeof(TZrBool)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      ZR_VALUE_TYPE_BOOL,
                                                      (TZrUInt32)sizeof(TZrBool),
                                                      (TZrUInt32)sizeof(TZrBool),
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsBool(state, &writeValue, ZR_FALSE);
    ZrCore_Value_InitAsInt(state, &intValue, 1);
    memcpy(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, &storedBool, sizeof(storedBool));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, readValue.type);
    TEST_ASSERT_TRUE(readValue.value.nativeObject.nativeBool);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &intValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeValue));
    memcpy(&writtenBool, inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, sizeof(writtenBool));
    TEST_ASSERT_FALSE(writtenBool);
}

static void assert_field_info_uint32_raw_primitive_case(SZrState *state) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue boolValue;
    TZrUInt32 storedU32 = 0xFEDC1234u;
    TZrUInt32 writtenU32 = 0u;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + sizeof(TZrUInt32)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      ZR_VALUE_TYPE_UINT32,
                                                      (TZrUInt32)sizeof(TZrUInt32),
                                                      (TZrUInt32)sizeof(TZrUInt32),
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsUInt(state, &writeValue, 0xAABBCCDDULL);
    ZrCore_Value_InitAsBool(state, &boolValue, ZR_TRUE);
    memcpy(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, &storedU32, sizeof(storedU32));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, readValue.type);
    TEST_ASSERT_EQUAL_UINT64(0xFEDC1234ULL, readValue.value.nativeObject.nativeUInt64);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &boolValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeValue));
    memcpy(&writtenU32, inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, sizeof(writtenU32));
    TEST_ASSERT_EQUAL_UINT32(0xAABBCCDDu, writtenU32);
}

static void assert_field_info_double_raw_primitive_case(SZrState *state) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeValue;
    SZrTypeValue boolValue;
    TZrDouble storedDouble = 6.25;
    TZrDouble writtenDouble = 0.0;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + sizeof(TZrDouble)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      ZR_VALUE_TYPE_DOUBLE,
                                                      (TZrUInt32)sizeof(TZrDouble),
                                                      (TZrUInt32)sizeof(TZrDouble),
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsFloat(state, &writeValue, -12.5);
    ZrCore_Value_InitAsBool(state, &boolValue, ZR_TRUE);
    memcpy(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, &storedDouble, sizeof(storedDouble));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, readValue.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, 6.25, readValue.value.nativeObject.nativeDouble);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &boolValue));
    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeValue));
    memcpy(&writtenDouble, inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, sizeof(writtenDouble));
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, -12.5, writtenDouble);
}

static void store_test_signed_raw_primitive(TZrByte *fieldAddress, EZrValueType valueType, TZrInt64 value) {
    switch (valueType) {
        case ZR_VALUE_TYPE_INT8: {
            TZrInt8 storedValue = (TZrInt8)value;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            break;
        }
        case ZR_VALUE_TYPE_INT16: {
            TZrInt16 storedValue = (TZrInt16)value;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            break;
        }
        case ZR_VALUE_TYPE_INT64: {
            TZrInt64 storedValue = value;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            break;
        }
        default:
            break;
    }
}

static TZrInt64 load_test_signed_raw_primitive(const TZrByte *fieldAddress, EZrValueType valueType) {
    TZrInt64 loadedValue = 0;

    switch (valueType) {
        case ZR_VALUE_TYPE_INT8: {
            TZrInt8 storedValue;
            memcpy(&storedValue, fieldAddress, sizeof(storedValue));
            loadedValue = storedValue;
            break;
        }
        case ZR_VALUE_TYPE_INT16: {
            TZrInt16 storedValue;
            memcpy(&storedValue, fieldAddress, sizeof(storedValue));
            loadedValue = storedValue;
            break;
        }
        case ZR_VALUE_TYPE_INT64: {
            TZrInt64 storedValue;
            memcpy(&storedValue, fieldAddress, sizeof(storedValue));
            loadedValue = storedValue;
            break;
        }
        default:
            break;
    }
    return loadedValue;
}

static void store_test_unsigned_raw_primitive(TZrByte *fieldAddress, EZrValueType valueType, TZrUInt64 value) {
    switch (valueType) {
        case ZR_VALUE_TYPE_UINT8: {
            TZrUInt8 storedValue = (TZrUInt8)value;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            break;
        }
        case ZR_VALUE_TYPE_UINT16: {
            TZrUInt16 storedValue = (TZrUInt16)value;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            break;
        }
        case ZR_VALUE_TYPE_UINT64: {
            TZrUInt64 storedValue = value;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            break;
        }
        default:
            break;
    }
}

static TZrUInt64 load_test_unsigned_raw_primitive(const TZrByte *fieldAddress, EZrValueType valueType) {
    TZrUInt64 loadedValue = 0u;

    switch (valueType) {
        case ZR_VALUE_TYPE_UINT8: {
            TZrUInt8 storedValue;
            memcpy(&storedValue, fieldAddress, sizeof(storedValue));
            loadedValue = storedValue;
            break;
        }
        case ZR_VALUE_TYPE_UINT16: {
            TZrUInt16 storedValue;
            memcpy(&storedValue, fieldAddress, sizeof(storedValue));
            loadedValue = storedValue;
            break;
        }
        case ZR_VALUE_TYPE_UINT64: {
            TZrUInt64 storedValue;
            memcpy(&storedValue, fieldAddress, sizeof(storedValue));
            loadedValue = storedValue;
            break;
        }
        default:
            break;
    }
    return loadedValue;
}

static void assert_field_info_signed_raw_primitive_width_case(SZrState *state,
                                                              EZrValueType valueType,
                                                              TZrUInt32 fieldByteSize,
                                                              TZrInt64 initialValue,
                                                              TZrInt64 writeValue) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeTypeValue;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + TEST_FIELD_RAW_MATRIX_MAX_BYTES] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      valueType,
                                                      fieldByteSize,
                                                      fieldByteSize,
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsInt(state, &writeTypeValue, writeValue);
    store_test_signed_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType, initialValue);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, readValue.type);
    TEST_ASSERT_EQUAL_INT64(initialValue, readValue.value.nativeObject.nativeInt64);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeTypeValue));
    TEST_ASSERT_EQUAL_INT64(writeValue,
                            load_test_signed_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType));
}

static void assert_field_info_unsigned_raw_primitive_width_case(SZrState *state,
                                                                EZrValueType valueType,
                                                                TZrUInt32 fieldByteSize,
                                                                TZrUInt64 initialValue,
                                                                TZrUInt64 writeValue) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeTypeValue;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + TEST_FIELD_RAW_MATRIX_MAX_BYTES] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      valueType,
                                                      fieldByteSize,
                                                      fieldByteSize,
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsUInt(state, &writeTypeValue, writeValue);
    store_test_unsigned_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType, initialValue);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_UINT64, readValue.type);
    TEST_ASSERT_EQUAL_UINT64(initialValue, readValue.value.nativeObject.nativeUInt64);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeTypeValue));
    TEST_ASSERT_EQUAL_UINT64(writeValue,
                             load_test_unsigned_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType));
}

static void assert_field_info_float32_raw_primitive_width_case(SZrState *state) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue readValue;
    SZrTypeValue writeTypeValue;
    TZrFloat32 storedFloat = 1.25f;
    TZrFloat32 writtenFloat = 0.0f;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + TEST_FIELD_RAW_MATRIX_MAX_BYTES] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      ZR_VALUE_TYPE_FLOAT,
                                                      (TZrUInt32)sizeof(TZrFloat32),
                                                      (TZrUInt32)sizeof(TZrFloat32),
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_ResetAsNull(&readValue);
    ZrCore_Value_InitAsFloat(state, &writeTypeValue, -3.5);
    memcpy(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, &storedFloat, sizeof(storedFloat));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ReadFieldInfoTokenValue(state,
                                                               runtime,
                                                               TEST_FIELD_DEF_TOKEN,
                                                               inlineStorage,
                                                               (TZrUInt32)sizeof(inlineStorage),
                                                               &readValue));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_DOUBLE, readValue.type);
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, 1.25, readValue.value.nativeObject.nativeDouble);

    TEST_ASSERT_TRUE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                runtime,
                                                                TEST_FIELD_DEF_TOKEN,
                                                                inlineStorage,
                                                                (TZrUInt32)sizeof(inlineStorage),
                                                                &writeTypeValue));
    memcpy(&writtenFloat, inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, sizeof(writtenFloat));
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, -3.5, (TZrDouble)writtenFloat);
}

static void assert_field_info_signed_raw_primitive_rejects_out_of_range_write(SZrState *state,
                                                                              EZrValueType valueType,
                                                                              TZrUInt32 fieldByteSize,
                                                                              TZrInt64 initialValue,
                                                                              const SZrTypeValue *rejectedValue) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + TEST_FIELD_RAW_MATRIX_MAX_BYTES] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      valueType,
                                                      fieldByteSize,
                                                      fieldByteSize,
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    store_test_signed_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType, initialValue);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 rejectedValue));
    TEST_ASSERT_EQUAL_INT64(initialValue,
                            load_test_signed_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType));
}

static void assert_field_info_unsigned_raw_primitive_rejects_out_of_range_write(SZrState *state,
                                                                                EZrValueType valueType,
                                                                                TZrUInt32 fieldByteSize,
                                                                                TZrUInt64 initialValue,
                                                                                const SZrTypeValue *rejectedValue) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + TEST_FIELD_RAW_MATRIX_MAX_BYTES] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      valueType,
                                                      fieldByteSize,
                                                      fieldByteSize,
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    store_test_unsigned_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType, initialValue);

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 rejectedValue));
    TEST_ASSERT_EQUAL_UINT64(initialValue,
                             load_test_unsigned_raw_primitive(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, valueType));
}

static void assert_field_info_float32_raw_primitive_rejects_write(SZrState *state,
                                                                  TZrDouble rejectedValue) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayoutField ownerFields[1] = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrTypeValue rejectedTypeValue;
    TZrFloat32 storedFloat = 1.25f;
    TZrFloat32 preservedFloat = 0.0f;
    TZrByte inlineStorage[TEST_FIELD_RAW_MATRIX_OFFSET + TEST_FIELD_RAW_MATRIX_MAX_BYTES] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_POOL_LENGTH] = {0};

    runtime = attach_runtime_with_raw_primitive_field(&module,
                                                      &metadataFunction,
                                                      &registration,
                                                      records,
                                                      &ownerLayout,
                                                      ownerFields,
                                                      &fieldLayout,
                                                      registeredLayouts,
                                                      metadataBytes,
                                                      sizeof(metadataBytes),
                                                      ZR_VALUE_TYPE_FLOAT,
                                                      (TZrUInt32)sizeof(TZrFloat32),
                                                      (TZrUInt32)sizeof(TZrFloat32),
                                                      &typeRows,
                                                      &fieldRows);
    ZR_UNUSED_PARAMETER(typeRows);
    ZrCore_Value_InitAsFloat(state, &rejectedTypeValue, rejectedValue);
    memcpy(inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, &storedFloat, sizeof(storedFloat));

    TEST_ASSERT_FALSE(ZrCore_Reflection_WriteFieldInfoTokenValue(state,
                                                                 runtime,
                                                                 TEST_FIELD_DEF_TOKEN,
                                                                 inlineStorage,
                                                                 (TZrUInt32)sizeof(inlineStorage),
                                                                 &rejectedTypeValue));
    memcpy(&preservedFloat, inlineStorage + TEST_FIELD_RAW_MATRIX_OFFSET, sizeof(preservedFloat));
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, 1.25, (TZrDouble)preservedFloat);
}

static void test_reflection_reads_and_writes_field_info_primitive_pod_matrix(void) {
    SZrState *state = create_reflection_test_state();

    TEST_ASSERT_NOT_NULL(state);
    assert_field_info_bool_raw_primitive_case(state);
    assert_field_info_uint32_raw_primitive_case(state);
    assert_field_info_double_raw_primitive_case(state);
    destroy_reflection_test_state(state);
}

static void test_reflection_reads_and_writes_field_info_primitive_pod_width_matrix(void) {
    SZrState *state = create_reflection_test_state();

    TEST_ASSERT_NOT_NULL(state);
    assert_field_info_signed_raw_primitive_width_case(state,
                                                      ZR_VALUE_TYPE_INT8,
                                                      (TZrUInt32)sizeof(TZrInt8),
                                                      -12,
                                                      42);
    assert_field_info_signed_raw_primitive_width_case(state,
                                                      ZR_VALUE_TYPE_INT16,
                                                      (TZrUInt32)sizeof(TZrInt16),
                                                      -1234,
                                                      2345);
    assert_field_info_signed_raw_primitive_width_case(state,
                                                      ZR_VALUE_TYPE_INT64,
                                                      (TZrUInt32)sizeof(TZrInt64),
                                                      -1234567890123LL,
                                                      987654321012LL);
    assert_field_info_unsigned_raw_primitive_width_case(state,
                                                        ZR_VALUE_TYPE_UINT8,
                                                        (TZrUInt32)sizeof(TZrUInt8),
                                                        0xABu,
                                                        0x7Fu);
    assert_field_info_unsigned_raw_primitive_width_case(state,
                                                        ZR_VALUE_TYPE_UINT16,
                                                        (TZrUInt32)sizeof(TZrUInt16),
                                                        0xABCDu,
                                                        0x1357u);
    assert_field_info_unsigned_raw_primitive_width_case(state,
                                                        ZR_VALUE_TYPE_UINT64,
                                                        (TZrUInt32)sizeof(TZrUInt64),
                                                        0xFEDCBA9876543210ULL,
                                                        0x1122334455667788ULL);
    assert_field_info_float32_raw_primitive_width_case(state);
    destroy_reflection_test_state(state);
}

static void test_reflection_rejects_out_of_range_field_info_primitive_pod_integer_writes(void) {
    SZrState *state = create_reflection_test_state();
    SZrTypeValue rejectedValue;

    TEST_ASSERT_NOT_NULL(state);

    ZrCore_Value_InitAsInt(state, &rejectedValue, ZR_TYPE_RANGE_INT8_MAX + 1);
    assert_field_info_signed_raw_primitive_rejects_out_of_range_write(state,
                                                                      ZR_VALUE_TYPE_INT8,
                                                                      (TZrUInt32)sizeof(TZrInt8),
                                                                      -12,
                                                                      &rejectedValue);

    ZrCore_Value_InitAsInt(state, &rejectedValue, ZR_TYPE_RANGE_INT8_MIN - 1);
    assert_field_info_signed_raw_primitive_rejects_out_of_range_write(state,
                                                                      ZR_VALUE_TYPE_INT8,
                                                                      (TZrUInt32)sizeof(TZrInt8),
                                                                      -12,
                                                                      &rejectedValue);

    ZrCore_Value_InitAsUInt(state, &rejectedValue, (TZrUInt64)ZR_TYPE_RANGE_INT8_MAX + 1u);
    assert_field_info_signed_raw_primitive_rejects_out_of_range_write(state,
                                                                      ZR_VALUE_TYPE_INT8,
                                                                      (TZrUInt32)sizeof(TZrInt8),
                                                                      -12,
                                                                      &rejectedValue);

    ZrCore_Value_InitAsInt(state, &rejectedValue, -1);
    assert_field_info_unsigned_raw_primitive_rejects_out_of_range_write(state,
                                                                        ZR_VALUE_TYPE_UINT8,
                                                                        (TZrUInt32)sizeof(TZrUInt8),
                                                                        0xABu,
                                                                        &rejectedValue);

    ZrCore_Value_InitAsUInt(state, &rejectedValue, (TZrUInt64)ZR_TYPE_RANGE_UINT8_MAX + 1u);
    assert_field_info_unsigned_raw_primitive_rejects_out_of_range_write(state,
                                                                        ZR_VALUE_TYPE_UINT8,
                                                                        (TZrUInt32)sizeof(TZrUInt8),
                                                                        0xABu,
                                                                        &rejectedValue);

    ZrCore_Value_InitAsUInt(state, &rejectedValue, (TZrUInt64)ZR_TYPE_RANGE_INT64_MAX + 1u);
    assert_field_info_signed_raw_primitive_rejects_out_of_range_write(state,
                                                                      ZR_VALUE_TYPE_INT64,
                                                                      (TZrUInt32)sizeof(TZrInt64),
                                                                      -1234567890123LL,
                                                                      &rejectedValue);

    destroy_reflection_test_state(state);
}

static void test_reflection_rejects_out_of_range_field_info_primitive_pod_float32_writes(void) {
    SZrState *state = create_reflection_test_state();

    TEST_ASSERT_NOT_NULL(state);
    assert_field_info_float32_raw_primitive_rejects_write(state, ((TZrDouble)FLT_MAX) * 2.0);
    assert_field_info_float32_raw_primitive_rejects_write(state, -((TZrDouble)FLT_MAX) * 2.0);
    destroy_reflection_test_state(state);
}

static void test_reflection_rejects_nan_field_info_primitive_pod_float32_writes(void) {
    SZrState *state = create_reflection_test_state();
    TZrDouble nanValue = strtod("nan", NULL);

    TEST_ASSERT_NOT_NULL(state);
    assert_field_info_float32_raw_primitive_rejects_write(state, nanValue);
    destroy_reflection_test_state(state);
}

static void test_reflection_rejects_precision_loss_field_info_primitive_pod_float32_writes(void) {
    SZrState *state = create_reflection_test_state();
    TZrDouble precisionLossValue = 1.0 + ((TZrDouble)FLT_EPSILON / 4.0);

    TEST_ASSERT_NOT_NULL(state);
    assert_field_info_float32_raw_primitive_rejects_write(state, precisionLossValue);
    destroy_reflection_test_state(state);
}

static void test_reflection_builds_field_info_signature_typedef_carrier(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    const SZrTypeValue *fieldTypeSignatureTypeValue;
    SZrObject *fieldTypeSignatureTypeInfo;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = 64u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = 16u;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);

    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);
    assert_object_int_field(state,
                            fieldInfo,
                            "fieldTypeSignatureNode",
                            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignaturePayload0", ZR_VALUE_TYPE_OBJECT);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignaturePayload1", 17);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeToken", TEST_FIELD_TYPE_DEF_TOKEN);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeLayoutId", 42);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeSize", 16);
    assert_object_bool_field(state, fieldInfo, "fieldTypeSignatureMatchesLayout", ZR_TRUE);
    assert_object_string_field(state, fieldInfo, "fieldTypeSignatureTypeName", "int");
    assert_signature_node_object(state,
                                 fieldInfo,
                                 ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
                                 TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET,
                                 (TZrUInt32)sizeof(fieldSignatureBlob),
                                 ZR_VALUE_TYPE_OBJECT,
                                 17u,
                                 0u,
                                 0u,
                                 0u,
                                 TEST_FIELD_TYPE_DEF_TOKEN,
                                 42u,
                                 16u,
                                 "int",
                                 ZR_TRUE);

    fieldTypeSignatureTypeValue = get_object_field_value(state, fieldInfo, "fieldTypeSignatureType");
    TEST_ASSERT_NOT_NULL(fieldTypeSignatureTypeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, fieldTypeSignatureTypeValue->type);
    fieldTypeSignatureTypeInfo = ZR_CAST_OBJECT(state, fieldTypeSignatureTypeValue->value.object);
    TEST_ASSERT_NOT_NULL(fieldTypeSignatureTypeInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, fieldTypeSignatureTypeInfo));
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "kind", "type");
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "name", "int");
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "qualifiedName", "int");

    destroy_reflection_test_state(state);
}

static void test_reflection_builds_field_info_signature_typeref_carrier(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            23u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeRefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            23u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrMetadataTokenRecord moduleRecords[2] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    const TZrUInt32 fieldTypeRefSignatureBlobOffset =
            TEST_FIELD_SIGNATURE_BLOB_OFFSET + (TZrUInt32)sizeof(fieldSignatureBlob);
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    const SZrTypeValue *fieldTypeSignatureTypeValue;
    SZrObject *fieldTypeSignatureTypeInfo;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeRefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = 64u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = 16u;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;

    moduleRecords[0].token = TEST_TYPE_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[0].targetMetadataToken = TEST_FIELD_TYPE_DEF_TOKEN;
    moduleRecords[0].layoutVersion = 11u;
    moduleRecords[0].layoutHash = 0x9999888877776666ULL;
    moduleRecords[1].token = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[1].relatedToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].ownerToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].signatureBlobOffset = fieldTypeRefSignatureBlobOffset;
    moduleRecords[1].signatureBlobLength = (TZrUInt32)sizeof(fieldTypeRefSignatureBlob);
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 2u;

    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeRefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeRefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);

    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);
    assert_object_int_field(state,
                            fieldInfo,
                            "fieldTypeSignatureNode",
                            ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignaturePayload0", ZR_VALUE_TYPE_OBJECT);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignaturePayload1", 23);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeToken", TEST_TYPE_REF_TOKEN);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeLayoutId", 42);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeSize", 16);
    assert_object_bool_field(state, fieldInfo, "fieldTypeSignatureMatchesLayout", ZR_TRUE);
    assert_object_string_field(state, fieldInfo, "fieldTypeSignatureTypeName", "int");
    assert_signature_node_object(state,
                                 fieldInfo,
                                 ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
                                 TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET,
                                 (TZrUInt32)sizeof(fieldSignatureBlob),
                                 ZR_VALUE_TYPE_OBJECT,
                                 23u,
                                 0u,
                                 0u,
                                 0u,
                                 TEST_TYPE_REF_TOKEN,
                                 42u,
                                 16u,
                                 "int",
                                 ZR_TRUE);

    fieldTypeSignatureTypeValue = get_object_field_value(state, fieldInfo, "fieldTypeSignatureType");
    TEST_ASSERT_NOT_NULL(fieldTypeSignatureTypeValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, fieldTypeSignatureTypeValue->type);
    fieldTypeSignatureTypeInfo = ZR_CAST_OBJECT(state, fieldTypeSignatureTypeValue->value.object);
    TEST_ASSERT_NOT_NULL(fieldTypeSignatureTypeInfo);
    TEST_ASSERT_TRUE(ZrCore_Reflection_IsReflectionObject(state, fieldTypeSignatureTypeInfo));
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "kind", "type");
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "name", "int");
    assert_object_string_field(state, fieldTypeSignatureTypeInfo, "qualifiedName", "int");

    destroy_reflection_test_state(state);
}

static void test_reflection_builds_field_info_signature_generic_base_type_node_object(void) {
    static const TZrByte fieldSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            3u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            23u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeDefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    static const TZrByte fieldTypeRefSignatureBlob[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            23u, 0u, 0u, 0u,
    };
    SZrState *state = create_reflection_test_state();
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[8] = {0};
    SZrMetadataTokenRecord moduleRecords[2] = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    const TZrUInt32 fieldTypeRefSignatureBlobOffset =
            TEST_FIELD_SIGNATURE_BLOB_OFFSET + 30u;
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    SZrMetadataRuntime *runtime;
    SZrObject *fieldInfo;
    SZrObject *signatureNodeObject;
    SZrObject *baseTypeNodeObject;
    SZrObject *childNodeObjects;
    SZrObject *childNodeObject;
    SZrObject *typeDefChildNodeObject;
    SZrObject *typeRefChildNodeObject;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          (2u * sizeof(SZrZrpMetadataTypeDefRow)) +
                          sizeof(SZrZrpMetadataFieldDefRow) +
                          12u +
                          TEST_FIELD_SIGNATURE_BLOB_OFFSET +
                          sizeof(fieldSignatureBlob) +
                          sizeof(fieldTypeDefSignatureBlob)] = {0};

    TEST_ASSERT_NOT_NULL(state);
    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = 64u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = 16u;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;

    moduleRecords[0].token = TEST_TYPE_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[0].targetMetadataToken = TEST_FIELD_TYPE_DEF_TOKEN;
    moduleRecords[0].layoutVersion = 11u;
    moduleRecords[0].layoutHash = 0x9999888877776666ULL;
    moduleRecords[1].token = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[1].relatedToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].ownerToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].signatureBlobOffset = fieldTypeRefSignatureBlobOffset;
    moduleRecords[1].signatureBlobLength = (TZrUInt32)sizeof(fieldTypeRefSignatureBlob);
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 2u;

    runtime = attach_runtime_with_type_and_field_metadata(&module,
                                                          &metadataFunction,
                                                          &registration,
                                                          records,
                                                          registeredLayouts,
                                                          metadataBytes,
                                                          sizeof(metadataBytes),
                                                          fieldSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldSignatureBlob),
                                                          fieldTypeDefSignatureBlob,
                                                          (TZrUInt32)sizeof(fieldTypeDefSignatureBlob),
                                                          &typeRows,
                                                          &fieldRows);

    fieldInfo = ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_NOT_NULL(fieldInfo);
    assert_object_int_field(state,
                            fieldInfo,
                            "fieldTypeSignatureNode",
                            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureBaseTypeBlobOffset", 3);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureChildCount", 3);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureChildListBlobOffset", 16);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeToken", 0);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeLayoutId", 0);
    assert_object_int_field(state, fieldInfo, "fieldTypeSignatureTypeSize", 0);
    assert_object_null_field(state, fieldInfo, "fieldTypeSignatureTypeName");
    assert_object_bool_field(state, fieldInfo, "fieldTypeSignatureMatchesLayout", ZR_FALSE);
    signatureNodeObject = assert_signature_node_object(state,
                                                       fieldInfo,
                                                       ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
                                                       TEST_FIELD_SIGNATURE_FIELD_TYPE_BLOB_OFFSET,
                                                       (TZrUInt32)sizeof(fieldSignatureBlob),
                                                       0u,
                                                       0u,
                                                       3u,
                                                       3u,
                                                       16u,
                                                       0u,
                                                       0u,
                                                       0u,
                                                       "",
                                                       ZR_FALSE);
    baseTypeNodeObject = assert_object_object_field(state, signatureNodeObject, "baseTypeNodeObject");
    assert_object_string_field(state, baseTypeNodeObject, "kind", "signatureTypeNode");
    assert_object_int_field(state, baseTypeNodeObject, "node", ZR_METADATA_SIGNATURE_NODE_TYPE_DEF);
    assert_object_int_field(state, baseTypeNodeObject, "blobOffset", 3);
    assert_object_int_field(state, baseTypeNodeObject, "nextBlobOffset", 12);
    assert_object_int_field(state, baseTypeNodeObject, "payload0", ZR_VALUE_TYPE_OBJECT);
    assert_object_int_field(state, baseTypeNodeObject, "payload1", 17);
    assert_object_int_field(state, baseTypeNodeObject, "baseTypeBlobOffset", 0);
    assert_object_int_field(state, baseTypeNodeObject, "childCount", 0);
    assert_object_int_field(state, baseTypeNodeObject, "childListBlobOffset", 0);
    assert_object_int_field(state, baseTypeNodeObject, "typeToken", TEST_FIELD_TYPE_DEF_TOKEN);
    assert_object_int_field(state, baseTypeNodeObject, "typeLayoutId", 42);
    assert_object_int_field(state, baseTypeNodeObject, "typeSize", 16);
    assert_object_string_field(state, baseTypeNodeObject, "typeName", "int");
    assert_type_literal_field(state, baseTypeNodeObject, "type", "int");
    assert_object_bool_field(state, baseTypeNodeObject, "matchesLayout", ZR_FALSE);

    childNodeObjects = assert_object_array_field(state, signatureNodeObject, "childNodeObjects");
    assert_array_length(childNodeObjects, 3u);
    childNodeObject = assert_array_object_entry(state, childNodeObjects, 0u);
    assert_object_string_field(state, childNodeObject, "kind", "signatureTypeNode");
    assert_object_int_field(state, childNodeObject, "node", ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
    assert_object_int_field(state, childNodeObject, "blobOffset", 16);
    assert_object_int_field(state, childNodeObject, "nextBlobOffset", 21);
    assert_object_int_field(state, childNodeObject, "payload0", ZR_VALUE_TYPE_INT64);
    assert_object_int_field(state, childNodeObject, "payload1", 0);
    assert_object_int_field(state, childNodeObject, "baseTypeBlobOffset", 0);
    assert_object_int_field(state, childNodeObject, "childCount", 0);
    assert_object_int_field(state, childNodeObject, "childListBlobOffset", 0);
    assert_object_int_field(state, childNodeObject, "typeToken", 0);
    assert_object_int_field(state, childNodeObject, "typeLayoutId", 0);
    assert_object_int_field(state, childNodeObject, "typeSize", 0);
    assert_object_string_field(state, childNodeObject, "typeName", "int");
    assert_type_literal_field(state, childNodeObject, "type", "int");
    assert_object_bool_field(state, childNodeObject, "matchesLayout", ZR_FALSE);

    typeDefChildNodeObject = assert_array_object_entry(state, childNodeObjects, 1u);
    assert_object_string_field(state, typeDefChildNodeObject, "kind", "signatureTypeNode");
    assert_object_int_field(state, typeDefChildNodeObject, "node", ZR_METADATA_SIGNATURE_NODE_TYPE_DEF);
    assert_object_int_field(state, typeDefChildNodeObject, "blobOffset", 21);
    assert_object_int_field(state,
                            typeDefChildNodeObject,
                            "nextBlobOffset",
                            30u);
    assert_object_int_field(state, typeDefChildNodeObject, "payload0", ZR_VALUE_TYPE_OBJECT);
    assert_object_int_field(state, typeDefChildNodeObject, "payload1", 17);
    assert_object_int_field(state, typeDefChildNodeObject, "baseTypeBlobOffset", 0);
    assert_object_int_field(state, typeDefChildNodeObject, "childCount", 0);
    assert_object_int_field(state, typeDefChildNodeObject, "childListBlobOffset", 0);
    assert_object_int_field(state, typeDefChildNodeObject, "typeToken", TEST_FIELD_TYPE_DEF_TOKEN);
    assert_object_int_field(state, typeDefChildNodeObject, "typeLayoutId", 42);
    assert_object_int_field(state, typeDefChildNodeObject, "typeSize", 16);
    assert_object_string_field(state, typeDefChildNodeObject, "typeName", "int");
    assert_type_literal_field(state, typeDefChildNodeObject, "type", "int");
    assert_object_bool_field(state, typeDefChildNodeObject, "matchesLayout", ZR_FALSE);

    typeRefChildNodeObject = assert_array_object_entry(state, childNodeObjects, 2u);
    assert_object_string_field(state, typeRefChildNodeObject, "kind", "signatureTypeNode");
    assert_object_int_field(state, typeRefChildNodeObject, "node", ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
    assert_object_int_field(state, typeRefChildNodeObject, "blobOffset", 30);
    assert_object_int_field(state,
                            typeRefChildNodeObject,
                            "nextBlobOffset",
                            (TZrUInt32)sizeof(fieldSignatureBlob));
    assert_object_int_field(state, typeRefChildNodeObject, "payload0", ZR_VALUE_TYPE_OBJECT);
    assert_object_int_field(state, typeRefChildNodeObject, "payload1", 23);
    assert_object_int_field(state, typeRefChildNodeObject, "baseTypeBlobOffset", 0);
    assert_object_int_field(state, typeRefChildNodeObject, "childCount", 0);
    assert_object_int_field(state, typeRefChildNodeObject, "childListBlobOffset", 0);
    assert_object_int_field(state, typeRefChildNodeObject, "typeToken", TEST_TYPE_REF_TOKEN);
    assert_object_int_field(state, typeRefChildNodeObject, "typeLayoutId", 42);
    assert_object_int_field(state, typeRefChildNodeObject, "typeSize", 16);
    assert_object_string_field(state, typeRefChildNodeObject, "typeName", "int");
    assert_type_literal_field(state, typeRefChildNodeObject, "type", "int");
    assert_object_bool_field(state, typeRefChildNodeObject, "matchesLayout", ZR_FALSE);

    destroy_reflection_test_state(state);
}

static void test_reflection_resolve_method_token_keeps_record_without_aot_binding(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    SZrMetadataRuntime *runtime;
    SZrReflectionResolvedToken resolved;

    records[0].token = TEST_MEMBER_DEF_TOKEN;
    records[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[0].signatureHash = 0x1234432111223344ULL;
    records[1].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    records[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    records[1].signatureHash = records[0].signatureHash;
    metadataFunction.metadataTokenRecords = records;
    metadataFunction.metadataTokenRecordLength = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveToken(runtime, TEST_MEMBER_DEF_TOKEN, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_RESOLVED_TOKEN_METHOD, resolved.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, resolved.methodToken);
    TEST_ASSERT_EQUAL_PTR(&records[0], resolved.methodRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_SIGNATURE_TOKEN, resolved.methodSignatureToken);
    TEST_ASSERT_EQUAL_PTR(&records[1], resolved.methodSignatureRecord);
    TEST_ASSERT_EQUAL_UINT64(records[1].signatureHash, resolved.methodSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(0u, resolved.methodFunctionIndex);
    TEST_ASSERT_NULL(resolved.methodInfo);
    TEST_ASSERT_NULL(resolved.methodFunctionPointer);
    TEST_ASSERT_NULL(resolved.methodInvoker);
}

static void test_reflection_invoke_method_token_dispatches_aot_invoker(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[2] = {
            test_reflection_aot_entry,
            test_reflection_aot_entry,
    };
    SZrAotMethodInfo methodInfo0 = {0};
    SZrAotMethodInfo methodInfo1 = {0};
    const SZrAotMethodInfo *methodInfos[2] = {
            &methodInfo0,
            &methodInfo1,
    };
    TZrUInt32 methodTokens[2] = {
            0u,
            TEST_MEMBER_DEF_TOKEN,
    };
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue selfValue = {0};
    SZrTypeValue argumentValues[2] = {{0}};
    SZrTypeValue returnValue = {0};

    records[0].token = TEST_MEMBER_DEF_TOKEN;
    records[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[0].signatureHash = 0x1234432111223344ULL;
    records[1].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    records[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    records[1].signatureHash = records[0].signatureHash;
    metadataFunction.metadataTokenRecords = records;
    metadataFunction.metadataTokenRecordLength = 2u;
    methodInfo0.functionIndex = 0u;
    methodInfo0.invoker = test_reflection_aot_invoker;
    methodInfo1.functionIndex = 1u;
    methodInfo1.invoker = test_reflection_aot_invoker;
    registration.functionCount = 2u;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = 2u;
    registration.methodTokens = methodTokens;
    registration.methodTokenCount = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);

    reset_reflection_invoker_capture();
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodToken(state,
                                                         runtime,
                                                         TEST_MEMBER_DEF_TOKEN,
                                                         &selfValue,
                                                         argumentValues,
                                                         &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_reflection_invoker_call_count);
    TEST_ASSERT_EQUAL_PTR(state, test_reflection_invoker_state);
    TEST_ASSERT_TRUE(test_reflection_invoker_target == test_reflection_aot_entry);
    TEST_ASSERT_EQUAL_PTR(&methodInfo1, test_reflection_invoker_method);
    TEST_ASSERT_EQUAL_PTR(&selfValue, test_reflection_invoker_self);
    TEST_ASSERT_EQUAL_PTR(argumentValues, test_reflection_invoker_args);
    TEST_ASSERT_EQUAL_PTR(&returnValue, test_reflection_invoker_out_return);

    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodToken(ZR_NULL,
                                                          runtime,
                                                          TEST_MEMBER_DEF_TOKEN,
                                                          &selfValue,
                                                          argumentValues,
                                                          &returnValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodToken(state,
                                                          ZR_NULL,
                                                          TEST_MEMBER_DEF_TOKEN,
                                                          &selfValue,
                                                          argumentValues,
                                                          &returnValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodToken(state,
                                                          runtime,
                                                          TEST_MEMBER_DEF_TOKEN,
                                                          &selfValue,
                                                          argumentValues,
                                                          ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodToken(state,
                                                          runtime,
                                                          TEST_TYPE_REF_TOKEN,
                                                          &selfValue,
                                                          argumentValues,
                                                          &returnValue));
}

static void test_reflection_invoke_method_token_checks_signature_argument_count(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord records[2] = {0};
    FZrAotEntryThunk functionPointers[2] = {
            ZR_NULL,
            test_reflection_aot_entry,
    };
    SZrAotMethodInfo methodInfo0 = {0};
    SZrAotMethodInfo methodInfo1 = {0};
    const SZrAotMethodInfo *methodInfos[2] = {
            &methodInfo0,
            &methodInfo1,
    };
    TZrUInt32 methodTokens[2] = {
            0u,
            TEST_MEMBER_DEF_TOKEN,
    };
    SZrAotSignatureType parameterTypes[2] = {{0}};
    SZrAotSignatureType returnType = {0};
    SZrAotSignature signature = {0};
    SZrMetadataRuntime *runtime;
    struct SZrState *state = (struct SZrState *)(void *)&module;
    SZrTypeValue selfValue = {0};
    SZrTypeValue argumentValues[3] = {{0}};
    SZrTypeValue returnValue = {0};

    records[0].token = TEST_MEMBER_DEF_TOKEN;
    records[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[0].signatureHash = 0x1234432111223344ULL;
    records[1].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    records[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    records[1].signatureHash = records[0].signatureHash;
    metadataFunction.metadataTokenRecords = records;
    metadataFunction.metadataTokenRecordLength = 2u;
    parameterTypes[0].baseType = (TZrUInt16)ZR_VALUE_TYPE_INT64;
    parameterTypes[1].baseType = (TZrUInt16)ZR_VALUE_TYPE_UINT64;
    returnType.baseType = (TZrUInt16)ZR_VALUE_TYPE_INT64;
    signature.parameterCount = 2u;
    signature.returnType = &returnType;
    signature.parameterTypes = parameterTypes;
    signature.hasReturnValue = 1u;
    argumentValues[0].type = ZR_VALUE_TYPE_INT64;
    argumentValues[1].type = ZR_VALUE_TYPE_UINT64;
    returnValue.type = ZR_VALUE_TYPE_INT64;
    methodInfo0.functionIndex = 0u;
    methodInfo0.invoker = test_reflection_aot_invoker;
    methodInfo1.functionIndex = 1u;
    methodInfo1.signature = &signature;
    methodInfo1.invoker = test_reflection_aot_invoker;
    registration.functionCount = 2u;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = 2u;
    registration.methodTokens = methodTokens;
    registration.methodTokenCount = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);

    reset_reflection_invoker_capture();
    test_reflection_invoker_return_type = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_MEMBER_DEF_TOKEN,
                                                                     &selfValue,
                                                                     argumentValues,
                                                                     2u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_reflection_invoker_call_count);
    TEST_ASSERT_EQUAL_PTR(argumentValues, test_reflection_invoker_args);

    reset_reflection_invoker_capture();
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_MEMBER_DEF_TOKEN,
                                                                      &selfValue,
                                                                      argumentValues,
                                                                      1u,
                                                                      &returnValue));
    TEST_ASSERT_EQUAL_UINT32(0u, test_reflection_invoker_call_count);
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_MEMBER_DEF_TOKEN,
                                                                      &selfValue,
                                                                      argumentValues,
                                                                      3u,
                                                                      &returnValue));
    TEST_ASSERT_FALSE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                      runtime,
                                                                      TEST_MEMBER_DEF_TOKEN,
                                                                      &selfValue,
                                                                      ZR_NULL,
                                                                      2u,
                                                                      &returnValue));

    signature.hasVarArgs = 1u;
    reset_reflection_invoker_capture();
    test_reflection_invoker_return_type = ZR_VALUE_TYPE_INT64;
    TEST_ASSERT_TRUE(ZrCore_Reflection_InvokeMethodTokenWithArgCount(state,
                                                                     runtime,
                                                                     TEST_MEMBER_DEF_TOKEN,
                                                                     &selfValue,
                                                                     argumentValues,
                                                                     3u,
                                                                     &returnValue));
    TEST_ASSERT_EQUAL_UINT32(1u, test_reflection_invoker_call_count);
}

static void test_reflection_resolve_token_exposes_typespec_generic_arguments(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
            2u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            21u, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeRefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
    };
    static const TZrByte argumentTypeRefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            21u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrMetadataTokenRecord moduleRecords[4] = {0};
    SZrTypeLayout typeSpecLayout = {0};
    const SZrTypeLayout *registeredLayouts[34] = {0};
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeSpecRow *typeSpecRows;
    SZrMetadataRuntime *runtime;
    SZrReflectionResolvedToken resolved;
    SZrReflectionResolvedGenericArgument argument;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) +
                             sizeof(baseTypeRefSignature) +
                             sizeof(argumentTypeRefSignature)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          sizeof(SZrZrpMetadataTypeSpecRow) +
                          sizeof(signaturePayload)] = {0};

    functionRecords[0].token = TEST_TYPE_SPEC_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[0].signatureHash = 0x123456789ABCDEF0ULL;
    functionRecords[1].token = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[1].signatureBlobOffset = 0u;
    functionRecords[1].signatureBlobLength = (TZrUInt32)sizeof(genericInstanceSignature);
    functionRecords[1].signatureHash = functionRecords[0].signatureHash;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 2u;

    moduleRecords[0].token = TEST_TYPE_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[0].signatureHash = 0xAABBCCDDEEFF0011ULL;
    moduleRecords[1].token = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[1].relatedToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].ownerToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].signatureBlobOffset = (TZrUInt32)sizeof(genericInstanceSignature);
    moduleRecords[1].signatureBlobLength = (TZrUInt32)sizeof(baseTypeRefSignature);
    moduleRecords[1].signatureHash = moduleRecords[0].signatureHash;
    moduleRecords[2].token = TEST_GENERIC_ARG_TYPE_REF_TOKEN;
    moduleRecords[2].relatedToken = TEST_GENERIC_ARG_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[2].signatureHash = 0x0F0E0D0C0B0A0908ULL;
    moduleRecords[3].token = TEST_GENERIC_ARG_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[3].relatedToken = TEST_GENERIC_ARG_TYPE_REF_TOKEN;
    moduleRecords[3].ownerToken = TEST_GENERIC_ARG_TYPE_REF_TOKEN;
    moduleRecords[3].signatureBlobOffset =
            (TZrUInt32)(sizeof(genericInstanceSignature) + sizeof(baseTypeRefSignature));
    moduleRecords[3].signatureBlobLength = (TZrUInt32)sizeof(argumentTypeRefSignature);
    moduleRecords[3].signatureHash = moduleRecords[2].signatureHash;
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 4u;

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature),
           baseTypeRefSignature,
           sizeof(baseTypeRefSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature) + sizeof(baseTypeRefSignature),
           argumentTypeRefSignature,
           sizeof(argumentTypeRefSignature));

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeSpecs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeSpecRow));
    set_counted_section(&header.signatureBlobPool, &nextOffset, (TZrUInt32)sizeof(signaturePayload), 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(metadataBytes, sizeof(metadataBytes), &header));
    typeSpecRows = (SZrZrpMetadataTypeSpecRow *)(void *)(metadataBytes + header.typeSpecs.offset);
    typeSpecRows[0].token = TEST_TYPE_SPEC_TOKEN;
    typeSpecRows[0].signatureBlobOffset = 0u;
    typeSpecRows[0].signatureBlobLength = (TZrUInt32)sizeof(genericInstanceSignature);
    typeSpecRows[0].typeLayoutId = 33u;
    typeSpecRows[0].signatureHash = functionRecords[0].signatureHash;
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(metadataBytes,
                                                        sizeof(metadataBytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    typeSpecLayout.cTypeId = 33u;
    typeSpecLayout.byteSize = 96u;
    registeredLayouts[33] = &typeSpecLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 34u;
    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, metadataBytes, sizeof(metadataBytes)));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveToken(runtime, TEST_TYPE_SPEC_TOKEN, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_RESOLVED_TOKEN_TYPE, resolved.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, resolved.token);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_REF_TOKEN, resolved.genericBaseToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], resolved.genericBaseRecord);
    TEST_ASSERT_EQUAL_UINT32(2u, resolved.genericArgumentCount);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_SIGNATURE_TOKEN, resolved.genericSignatureToken);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[0].signatureHash, resolved.genericSignatureHash);
    TEST_ASSERT_EQUAL_PTR(&typeSpecLayout, resolved.typeLayout);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveTypeSpecGenericArgument(
            runtime, TEST_TYPE_SPEC_TOKEN, 0u, &argument));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, argument.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(0u, argument.argumentIndex);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_REF_TOKEN, argument.genericBaseToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], argument.genericBaseRecord);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, argument.argumentNodeKind);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, argument.argumentPayload0);
    TEST_ASSERT_EQUAL_UINT32(0u, argument.argumentToken);
    TEST_ASSERT_NULL(argument.argumentRecord);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveTypeSpecGenericArgument(
            runtime, TEST_TYPE_SPEC_TOKEN, 1u, &argument));
    TEST_ASSERT_EQUAL_UINT32(1u, argument.argumentIndex);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_METADATA_SIGNATURE_NODE_TYPE_REF, argument.argumentNodeKind);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, argument.argumentPayload0);
    TEST_ASSERT_EQUAL_UINT32(21u, argument.argumentPayload1);
    TEST_ASSERT_EQUAL_UINT32(TEST_GENERIC_ARG_TYPE_REF_TOKEN, argument.argumentToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[2], argument.argumentRecord);
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveTypeSpecGenericArgument(
            runtime, TEST_TYPE_SPEC_TOKEN, 2u, &argument));
}

static void test_reflection_resolves_generic_parameter_constraints(void) {
    static const TZrByte constraintSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            31u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrMetadataTokenRecord moduleRecords[1] = {0};
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataMethodDefRow *methodRows;
    SZrZrpMetadataGenericParamRow *genericParamRows;
    SZrZrpMetadataGenericParamConstraintRow *constraintRows;
    SZrMetadataRuntime *runtime;
    SZrReflectionResolvedGenericParameter parameter;
    SZrReflectionResolvedGenericParameterConstraint constraint;
    TZrUInt32 nextOffset;
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE +
                          sizeof(SZrZrpMetadataTypeDefRow) +
                          sizeof(SZrZrpMetadataMethodDefRow) +
                          (2u * sizeof(SZrZrpMetadataGenericParamRow)) +
                          sizeof(SZrZrpMetadataGenericParamConstraintRow) +
                          sizeof(constraintSignature)] = {0};

    functionRecords[0].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[1].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[2].token = TEST_MEMBER_DEF_TOKEN;
    functionRecords[2].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[2].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[3].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[3].ownerToken = TEST_MEMBER_DEF_TOKEN;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    moduleRecords[0].token = TEST_GENERIC_CONSTRAINT_TYPE_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_GENERIC_CONSTRAINT_TYPE_REF_SIGNATURE_TOKEN;
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 1u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.typeDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_counted_section(&header.methodDefs,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow));
    set_counted_section(&header.genericParams,
                        &nextOffset,
                        2u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamRow));
    set_counted_section(&header.genericParamConstraints,
                        &nextOffset,
                        1u,
                        (TZrUInt32)sizeof(SZrZrpMetadataGenericParamConstraintRow));
    set_counted_section(&header.signatureBlobPool,
                        &nextOffset,
                        (TZrUInt32)sizeof(constraintSignature),
                        1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(metadataBytes, sizeof(metadataBytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(metadataBytes,
                                                        sizeof(metadataBytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        constraintSignature,
                                                        (TZrUInt32)sizeof(constraintSignature)));

    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(metadataBytes + header.typeDefs.offset);
    methodRows = (SZrZrpMetadataMethodDefRow *)(void *)(metadataBytes + header.methodDefs.offset);
    genericParamRows = (SZrZrpMetadataGenericParamRow *)(void *)(metadataBytes + header.genericParams.offset);
    constraintRows = (SZrZrpMetadataGenericParamConstraintRow *)(void *)(metadataBytes +
                                                                         header.genericParamConstraints.offset);

    typeRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeRows[0].firstMethodDefIndex = 0u;
    typeRows[0].methodDefCount = 1u;
    typeRows[0].firstGenericParamIndex = 0u;
    typeRows[0].genericParamCount = 1u;
    methodRows[0].token = TEST_MEMBER_DEF_TOKEN;
    methodRows[0].ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    methodRows[0].firstGenericParamIndex = 1u;
    methodRows[0].genericParamCount = 1u;
    genericParamRows[0].ownerToken = TEST_TYPE_DEF_TOKEN;
    genericParamRows[0].nameStringOffset = 17u;
    genericParamRows[0].parameterIndex = 0u;
    genericParamRows[0].firstConstraintIndex = 0u;
    genericParamRows[0].constraintCount = 1u;
    genericParamRows[0].flags = 0xC1u;
    genericParamRows[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    genericParamRows[1].nameStringOffset = 23u;
    genericParamRows[1].parameterIndex = 0u;
    genericParamRows[1].flags = 0xD2u;
    constraintRows[0].genericParamIndex = 0u;
    constraintRows[0].constraintTypeToken = TEST_GENERIC_CONSTRAINT_TYPE_REF_TOKEN;
    constraintRows[0].signatureBlobOffset = 0u;
    constraintRows[0].signatureBlobLength = (TZrUInt32)sizeof(constraintSignature);

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, metadataBytes, sizeof(metadataBytes)));

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveGenericParameter(ZR_NULL,
                                                                TEST_TYPE_DEF_TOKEN,
                                                                0u,
                                                                &parameter));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveGenericParameter(runtime,
                                                                TEST_TYPE_DEF_TOKEN,
                                                                0u,
                                                                ZR_NULL));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveGenericParameter(runtime,
                                                               TEST_TYPE_DEF_TOKEN,
                                                               0u,
                                                               &parameter));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, parameter.ownerToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], parameter.ownerRecord);
    TEST_ASSERT_EQUAL_PTR(&genericParamRows[0], parameter.genericParamRow);
    TEST_ASSERT_EQUAL_UINT32(0u, parameter.genericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(0u, parameter.parameterIndex);
    TEST_ASSERT_EQUAL_UINT32(17u, parameter.nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, parameter.firstConstraintIndex);
    TEST_ASSERT_EQUAL_UINT32(1u, parameter.constraintCount);
    TEST_ASSERT_EQUAL_UINT32(0xC1u, parameter.flags);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveGenericParameterConstraint(runtime,
                                                                         TEST_TYPE_DEF_TOKEN,
                                                                         0u,
                                                                         0u,
                                                                         &constraint));
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, constraint.genericParameter.ownerToken);
    TEST_ASSERT_EQUAL_PTR(&genericParamRows[0], constraint.genericParameter.genericParamRow);
    TEST_ASSERT_EQUAL_PTR(&constraintRows[0], constraint.constraintRow);
    TEST_ASSERT_EQUAL_UINT32(0u, constraint.constraintIndex);
    TEST_ASSERT_EQUAL_UINT32(TEST_GENERIC_CONSTRAINT_TYPE_REF_TOKEN, constraint.constraintTypeToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], constraint.constraintTypeRecord);
    TEST_ASSERT_EQUAL_UINT32(0u, constraint.signatureBlobOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(constraintSignature), constraint.signatureBlobLength);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(constraintSignature),
                             (TZrUInt32)constraint.signatureBlobByteLength);
    TEST_ASSERT_EQUAL_PTR(metadataBytes + header.signatureBlobPool.offset, constraint.signatureBlobData);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveGenericParameter(runtime,
                                                               TEST_MEMBER_DEF_TOKEN,
                                                               0u,
                                                               &parameter));
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, parameter.ownerToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[2], parameter.ownerRecord);
    TEST_ASSERT_EQUAL_PTR(&genericParamRows[1], parameter.genericParamRow);
    TEST_ASSERT_EQUAL_UINT32(1u, parameter.genericParamIndex);
    TEST_ASSERT_EQUAL_UINT32(23u, parameter.nameStringOffset);
    TEST_ASSERT_EQUAL_UINT32(0xD2u, parameter.flags);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveGenericParameter(runtime,
                                                                TEST_TYPE_DEF_TOKEN,
                                                                1u,
                                                                &parameter));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveGenericParameterConstraint(runtime,
                                                                         TEST_TYPE_DEF_TOKEN,
                                                                         0u,
                                                                         1u,
                                                                         &constraint));
}

static void test_reflection_resolves_method_spec_generic_arguments(void) {
    static const TZrByte methodSpecSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_MEMBER_REF,
            (TZrByte)(TEST_MEMBER_DEF_TOKEN & 0xFFu),
            (TZrByte)((TEST_MEMBER_DEF_TOKEN >> 8u) & 0xFFu),
            (TZrByte)((TEST_MEMBER_DEF_TOKEN >> 16u) & 0xFFu),
            (TZrByte)((TEST_MEMBER_DEF_TOKEN >> 24u) & 0xFFu),
            2u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_UINT64, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            41u, 0u, 0u, 0u,
    };
    static const TZrByte argumentTypeRefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            41u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    FZrAotEntryThunk functionPointers[2] = {
            test_reflection_aot_entry,
            test_reflection_aot_entry,
    };
    SZrAotMethodInfo methodInfo0 = {0};
    SZrAotMethodInfo methodInfo1 = {0};
    const SZrAotMethodInfo *methodInfos[2] = {
            &methodInfo0,
            &methodInfo1,
    };
    TZrUInt32 methodTokens[2] = {
            0u,
            TEST_MEMBER_DEF_TOKEN,
    };
    SZrAotCodeRegistration registration = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrMetadataTokenRecord moduleRecords[2] = {0};
    SZrZrpMetadataHeader header;
    SZrMetadataRuntime *runtime;
    SZrReflectionResolvedToken resolved;
    SZrReflectionResolvedMethodSpecGenericArgument argument;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(methodSpecSignature) + sizeof(argumentTypeRefSignature)] = {0};
    TZrByte metadataBytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(signaturePayload)] = {0};

    functionRecords[0].token = TEST_MEMBER_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[1].token = TEST_METHOD_SPEC_TOKEN;
    functionRecords[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].signatureBlobOffset = 0u;
    functionRecords[1].signatureBlobLength = (TZrUInt32)sizeof(methodSpecSignature);
    functionRecords[1].signatureHash = 0xA0B0C0D0E0F00123ULL;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 2u;
    methodInfo0.functionIndex = 0u;
    methodInfo0.invoker = test_reflection_aot_invoker;
    methodInfo1.functionIndex = 1u;
    methodInfo1.invoker = test_reflection_aot_invoker;
    registration.functionCount = 2u;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = 2u;
    registration.methodTokens = methodTokens;
    registration.methodTokenCount = 2u;

    moduleRecords[0].token = TEST_GENERIC_ARG_TYPE_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_GENERIC_ARG_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[1].token = TEST_GENERIC_ARG_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[1].relatedToken = TEST_GENERIC_ARG_TYPE_REF_TOKEN;
    moduleRecords[1].ownerToken = TEST_GENERIC_ARG_TYPE_REF_TOKEN;
    moduleRecords[1].signatureBlobOffset = (TZrUInt32)sizeof(methodSpecSignature);
    moduleRecords[1].signatureBlobLength = (TZrUInt32)sizeof(argumentTypeRefSignature);
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 2u;

    memcpy(signaturePayload, methodSpecSignature, sizeof(methodSpecSignature));
    memcpy(signaturePayload + sizeof(methodSpecSignature),
           argumentTypeRefSignature,
           sizeof(argumentTypeRefSignature));

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_counted_section(&header.signatureBlobPool, &nextOffset, (TZrUInt32)sizeof(signaturePayload), 1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(metadataBytes, sizeof(metadataBytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(metadataBytes,
                                                        sizeof(metadataBytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, metadataBytes, sizeof(metadataBytes)));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveToken(runtime, TEST_METHOD_SPEC_TOKEN, &resolved));
    TEST_ASSERT_EQUAL_INT(ZR_REFLECTION_RESOLVED_TOKEN_METHOD, resolved.kind);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_SPEC_TOKEN, resolved.token);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[1], resolved.record);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, resolved.methodToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], resolved.methodRecord);
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_SPEC_TOKEN, resolved.methodSignatureToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[1], resolved.methodSignatureRecord);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[1].signatureHash, resolved.methodSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(1u, resolved.methodFunctionIndex);
    TEST_ASSERT_EQUAL_PTR(&methodInfo1, resolved.methodInfo);
    TEST_ASSERT_TRUE(resolved.methodFunctionPointer == test_reflection_aot_entry);
    TEST_ASSERT_TRUE(resolved.methodInvoker == test_reflection_aot_invoker);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[1].signatureHash, resolved.genericSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(2u, resolved.genericArgumentCount);
    TEST_ASSERT_GREATER_THAN_UINT32(0u, resolved.genericArgumentListBlobOffset);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveMethodSpecGenericArgument(ZR_NULL,
                                                                         TEST_METHOD_SPEC_TOKEN,
                                                                         0u,
                                                                         &argument));
    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveMethodSpecGenericArgument(runtime,
                                                                         TEST_METHOD_SPEC_TOKEN,
                                                                         0u,
                                                                         ZR_NULL));

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveMethodSpecGenericArgument(runtime,
                                                                        TEST_METHOD_SPEC_TOKEN,
                                                                        0u,
                                                                        &argument));
    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_SPEC_TOKEN, argument.methodSpecToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, argument.methodToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], argument.methodRecord);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[1].signatureHash, argument.genericSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(0u, argument.argumentIndex);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, argument.argumentNodeKind);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_UINT64, argument.argumentPayload0);
    TEST_ASSERT_EQUAL_UINT32(0u, argument.argumentToken);
    TEST_ASSERT_NULL(argument.argumentRecord);

    TEST_ASSERT_TRUE(ZrCore_Reflection_ResolveMethodSpecGenericArgument(runtime,
                                                                        TEST_METHOD_SPEC_TOKEN,
                                                                        1u,
                                                                        &argument));
    TEST_ASSERT_EQUAL_UINT32(1u, argument.argumentIndex);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_METADATA_SIGNATURE_NODE_TYPE_REF, argument.argumentNodeKind);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, argument.argumentPayload0);
    TEST_ASSERT_EQUAL_UINT32(41u, argument.argumentPayload1);
    TEST_ASSERT_EQUAL_UINT32(TEST_GENERIC_ARG_TYPE_REF_TOKEN, argument.argumentToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], argument.argumentRecord);

    TEST_ASSERT_FALSE(ZrCore_Reflection_ResolveMethodSpecGenericArgument(runtime,
                                                                         TEST_METHOD_SPEC_TOKEN,
                                                                         2u,
                                                                         &argument));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reflection_resolve_token_returns_type_field_and_method_entities);
    RUN_TEST(test_reflection_builds_field_info_object_from_fielddef_token);
    RUN_TEST(test_reflection_reads_field_info_value_slot_from_inline_storage);
    RUN_TEST(test_reflection_reads_field_info_object_value_from_inline_storage);
    RUN_TEST(test_reflection_writes_field_info_object_value_to_inline_storage);
    RUN_TEST(test_reflection_reads_and_writes_field_info_object_primitive_pod_from_inline_storage);
    RUN_TEST(test_reflection_reads_field_info_object_inline_struct_borrowed_view);
    RUN_TEST(test_reflection_writes_field_info_object_inline_struct_drops_replaced_owned_value_field);
    RUN_TEST(test_reflection_reads_field_info_object_nested_value_slot_from_inline_struct);
    RUN_TEST(test_reflection_reads_field_info_object_nested_path_value_slot_from_inline_struct);
    RUN_TEST(test_reflection_writes_field_info_object_nested_value_slot_from_inline_struct);
    RUN_TEST(test_reflection_writes_field_info_object_nested_path_value_slot_from_inline_struct);
    RUN_TEST(test_reflection_reads_and_writes_field_info_object_nested_path_primitive_pod_from_inline_struct);
    RUN_TEST(test_reflection_writes_field_info_value_slot_to_inline_storage);
    RUN_TEST(test_reflection_reads_and_writes_field_info_primitive_pod_from_inline_storage);
    RUN_TEST(test_reflection_reads_and_writes_field_info_primitive_pod_matrix);
    RUN_TEST(test_reflection_reads_and_writes_field_info_primitive_pod_width_matrix);
    RUN_TEST(test_reflection_rejects_out_of_range_field_info_primitive_pod_integer_writes);
    RUN_TEST(test_reflection_rejects_out_of_range_field_info_primitive_pod_float32_writes);
    RUN_TEST(test_reflection_rejects_nan_field_info_primitive_pod_float32_writes);
    RUN_TEST(test_reflection_rejects_precision_loss_field_info_primitive_pod_float32_writes);
    RUN_TEST(test_reflection_builds_field_info_signature_typedef_carrier);
    RUN_TEST(test_reflection_builds_field_info_signature_typeref_carrier);
    RUN_TEST(test_reflection_builds_field_info_signature_generic_base_type_node_object);
    RUN_TEST(test_reflection_resolve_method_token_keeps_record_without_aot_binding);
    RUN_TEST(test_reflection_invoke_method_token_dispatches_aot_invoker);
    RUN_TEST(test_reflection_invoke_method_token_checks_signature_argument_count);
    RUN_TEST(test_reflection_resolve_token_exposes_typespec_generic_arguments);
    RUN_TEST(test_reflection_resolves_generic_parameter_constraints);
    RUN_TEST(test_reflection_resolves_method_spec_generic_arguments);
    return UNITY_END();
}
