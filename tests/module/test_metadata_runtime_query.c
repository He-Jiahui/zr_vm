#include <string.h>

#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/zrp_metadata.h"

#define TEST_MODULE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u)
#define TEST_MODULE_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_TYPE_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u)
#define TEST_TYPE_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 5u)
#define TEST_TYPE_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 1u)
#define TEST_TYPE_SPEC_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 7u)
#define TEST_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_MEMBER_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u)
#define TEST_FIELD_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u)
#define TEST_FIELD_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 8u)
#define TEST_ASSEMBLY_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u)
#define TEST_ASSEMBLY_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 3u)
#define TEST_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 1u)
#define TEST_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 6u)
#define TEST_GENERIC_ARG_TYPE_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 2u)
#define TEST_GENERIC_ARG_TYPE_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 10u)
#define TEST_METHOD_SPEC_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 11u)
#define TEST_MEMBER_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u)
#define TEST_MEMBER_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 4u)
#define TEST_FIELD_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 2u)
#define TEST_FIELD_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 9u)

void setUp(void) {}

void tearDown(void) {}

static void attach_function_metadata_records(SZrFunction *function, SZrMetadataTokenRecord *records) {
    records[0].token = TEST_MODULE_TOKEN;
    records[0].relatedToken = TEST_MODULE_SIGNATURE_TOKEN;
    records[0].signatureHash = 0x1111222233334444ULL;

    records[1].token = TEST_MODULE_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_MODULE_TOKEN;
    records[1].ownerToken = TEST_MODULE_TOKEN;
    records[1].signatureHash = records[0].signatureHash;

    records[2].token = TEST_MEMBER_DEF_TOKEN;
    records[2].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[2].signatureHash = 0x5555666677778888ULL;

    records[3].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[3].relatedToken = TEST_MEMBER_DEF_TOKEN;
    records[3].ownerToken = TEST_MEMBER_DEF_TOKEN;
    records[3].signatureHash = records[2].signatureHash;

    function->metadataTokenRecords = records;
    function->metadataTokenRecordLength = 4u;
}

static void attach_module_metadata_records(SZrFunction *function, SZrMetadataTokenRecord *records) {
    records[0].token = TEST_ASSEMBLY_REF_TOKEN;
    records[0].relatedToken = TEST_ASSEMBLY_REF_SIGNATURE_TOKEN;
    records[0].signatureHash = 0x9999AAAABBBBCCCCULL;

    records[1].token = TEST_ASSEMBLY_REF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_ASSEMBLY_REF_TOKEN;
    records[1].ownerToken = TEST_ASSEMBLY_REF_TOKEN;
    records[1].signatureHash = records[0].signatureHash;

    records[2].token = TEST_MEMBER_REF_TOKEN;
    records[2].relatedToken = TEST_MEMBER_REF_SIGNATURE_TOKEN;
    records[2].ownerToken = TEST_ASSEMBLY_REF_TOKEN;
    records[2].signatureHash = 0xDDDDEEEEFFFF0001ULL;

    records[3].token = TEST_MEMBER_REF_SIGNATURE_TOKEN;
    records[3].relatedToken = TEST_MEMBER_REF_TOKEN;
    records[3].ownerToken = TEST_MEMBER_REF_TOKEN;
    records[3].signatureHash = records[2].signatureHash;

    function->moduleMetadataTokenRecords = records;
    function->moduleMetadataTokenRecordLength = 4u;
}

static void set_runtime_test_counted_section(SZrZrpMetadataSection *section,
                                             TZrUInt32 *nextOffset,
                                             TZrUInt32 count,
                                             TZrUInt32 elementSize) {
    section->offset = *nextOffset;
    section->count = count;
    section->elementSize = elementSize;
    section->byteLength = count * elementSize;
    *nextOffset += section->byteLength;
}

static void test_function_metadata_records_can_be_queried_by_token(void) {
    SZrFunction function = {0};
    SZrMetadataTokenRecord records[4] = {0};

    attach_function_metadata_records(&function, records);

    TEST_ASSERT_EQUAL_PTR(&records[0],
                          ZrCore_Function_FindMetadataTokenRecord(&function, TEST_MODULE_TOKEN));
    TEST_ASSERT_EQUAL_PTR(&records[2],
                          ZrCore_Function_FindMetadataTokenRecord(&function, TEST_MEMBER_DEF_TOKEN));
    TEST_ASSERT_EQUAL_PTR(&records[1],
                          ZrCore_Function_FindMetadataSignatureRecord(&function, TEST_MODULE_TOKEN));
    TEST_ASSERT_EQUAL_PTR(&records[3],
                          ZrCore_Function_FindMetadataSignatureRecord(&function, TEST_MEMBER_DEF_TOKEN));

    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataTokenRecord(&function, 0u));
    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataSignatureRecord(&function, 0u));
    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataTokenRecord(ZR_NULL, TEST_MODULE_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataSignatureRecord(ZR_NULL, TEST_MODULE_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataTokenRecord(
            &function,
            ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 99u)));
}

static void test_module_metadata_records_are_queried_from_entry_ref_table(void) {
    SZrFunction function = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrMetadataTokenRecord moduleRecords[4] = {0};

    attach_function_metadata_records(&function, functionRecords);
    attach_module_metadata_records(&function, moduleRecords);

    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0],
                          ZrCore_Function_FindModuleMetadataTokenRecord(&function, TEST_ASSEMBLY_REF_TOKEN));
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[2],
                          ZrCore_Function_FindModuleMetadataTokenRecord(&function, TEST_MEMBER_REF_TOKEN));
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[1],
                          ZrCore_Function_FindModuleMetadataSignatureRecord(&function, TEST_ASSEMBLY_REF_TOKEN));
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[3],
                          ZrCore_Function_FindModuleMetadataSignatureRecord(&function, TEST_MEMBER_REF_TOKEN));

    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataTokenRecord(&function, TEST_MODULE_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Function_FindModuleMetadataSignatureRecord(&function, TEST_MODULE_TOKEN));
}

static void test_signature_query_requires_related_signature_owner_pair(void) {
    SZrFunction function = {0};
    SZrMetadataTokenRecord records[3] = {0};

    records[0].token = TEST_MEMBER_DEF_TOKEN;
    records[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[1].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    records[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    records[1].ownerToken = TEST_MODULE_TOKEN;
    records[2].token = TEST_MODULE_TOKEN;
    records[2].relatedToken = TEST_MEMBER_DEF_TOKEN;
    function.metadataTokenRecords = records;
    function.metadataTokenRecordLength = 3u;

    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataSignatureRecord(&function, TEST_MEMBER_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_Function_FindMetadataSignatureRecord(&function, TEST_MODULE_TOKEN));
}

static TZrInt64 test_metadata_runtime_aot_entry(struct SZrState *state) {
    (void)state;
    return 0;
}

static void test_metadata_runtime_aot_invoker(struct SZrState *state,
                                              FZrAotEntryThunk target,
                                              const SZrAotMethodInfo *method,
                                              SZrTypeValue *self,
                                              SZrTypeValue *args,
                                              SZrTypeValue *outReturn) {
    (void)state;
    (void)target;
    (void)method;
    (void)self;
    (void)args;
    (void)outReturn;
}

static void test_module_metadata_runtime_attaches_code_registration(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    FZrAotEntryThunk functionPointers[2] = {
            test_metadata_runtime_aot_entry,
            test_metadata_runtime_aot_entry,
    };
    SZrAotMethodInfo methodInfo = {0};
    const SZrAotMethodInfo *methodInfos[1] = {&methodInfo};
    FZrAotReflectionInvoker invokers[1] = {test_metadata_runtime_aot_invoker};
    const SZrAotGcDescriptor *gcDescriptors[1] = {ZR_NULL};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;

    registration.functionCount = 2u;
    registration.functionPointers = functionPointers;
    registration.methodInfos = methodInfos;
    registration.methodInfoCount = 1u;
    registration.invokers = invokers;
    registration.invokerCount = 1u;
    registration.typeLayoutCount = 3u;
    registration.gcDescriptors = gcDescriptors;
    registration.gcDescriptorCount = 1u;

    TEST_ASSERT_NULL(ZrCore_Module_GetMetadataRuntime(ZR_NULL));
    TEST_ASSERT_NULL(ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, ZR_NULL));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_PTR(runtime, ZrCore_Module_GetMetadataRuntime(&module));
    TEST_ASSERT_EQUAL_PTR(&module, runtime->module);
    TEST_ASSERT_EQUAL_PTR(&metadataFunction, runtime->metadataFunction);
    TEST_ASSERT_EQUAL_PTR(&registration, runtime->codeRegistration);
    TEST_ASSERT_EQUAL_UINT32(2u, runtime->functionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, runtime->methodInfoCount);
    TEST_ASSERT_EQUAL_UINT32(1u, runtime->invokerCount);
    TEST_ASSERT_EQUAL_UINT32(3u, runtime->typeLayoutCount);
    TEST_ASSERT_EQUAL_UINT32(1u, runtime->gcDescriptorCount);
}

static void test_metadata_runtime_resolves_method_records_lazily(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrMetadataTokenRecord moduleRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    const SZrMetadataTokenRecord *localMethod;
    const SZrMetadataTokenRecord *cachedLocalMethod;
    const SZrMetadataTokenRecord *importedMethod;

    attach_function_metadata_records(&metadataFunction, functionRecords);
    attach_module_metadata_records(&metadataFunction, moduleRecords);
    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveMethodRecord(ZR_NULL, TEST_MEMBER_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, 0u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, TEST_MODULE_TOKEN));

    localMethod = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, TEST_MEMBER_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[2], localMethod);

    functionRecords[2].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 42u);
    cachedLocalMethod = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, TEST_MEMBER_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(localMethod, cachedLocalMethod);

    importedMethod = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, TEST_MEMBER_REF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[2], importedMethod);
}

static void test_metadata_runtime_resolves_type_records_lazily(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrMetadataTokenRecord moduleRecords[2] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    const SZrMetadataTokenRecord *localType;
    const SZrMetadataTokenRecord *cachedLocalType;
    const SZrMetadataTokenRecord *importedType;

    functionRecords[0].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[0].signatureHash = 0x0102030405060708ULL;
    functionRecords[1].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[1].signatureHash = functionRecords[0].signatureHash;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 2u;

    moduleRecords[0].token = TEST_TYPE_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[0].signatureHash = 0x1122334455667788ULL;
    moduleRecords[1].token = TEST_TYPE_REF_SIGNATURE_TOKEN;
    moduleRecords[1].relatedToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].ownerToken = TEST_TYPE_REF_TOKEN;
    moduleRecords[1].signatureHash = moduleRecords[0].signatureHash;
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeRecord(ZR_NULL, TEST_TYPE_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, 0u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, TEST_MEMBER_DEF_TOKEN));

    localType = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, TEST_TYPE_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], localType);

    functionRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 42u);
    cachedLocalType = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, TEST_TYPE_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(localType, cachedLocalType);

    importedType = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, TEST_TYPE_REF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], importedType);
}

static void test_metadata_runtime_resolves_signature_records_lazily(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrMetadataTokenRecord moduleRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    const SZrMetadataTokenRecord *localSignature;
    const SZrMetadataTokenRecord *cachedLocalSignature;
    const SZrMetadataTokenRecord *importedSignature;

    attach_function_metadata_records(&metadataFunction, functionRecords);
    attach_module_metadata_records(&metadataFunction, moduleRecords);
    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveSignatureRecord(ZR_NULL, TEST_MEMBER_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, 0u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, TEST_MEMBER_DEF_SIGNATURE_TOKEN));

    localSignature = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, TEST_MEMBER_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[3], localSignature);

    functionRecords[3].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 42u);
    cachedLocalSignature = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, TEST_MEMBER_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(localSignature, cachedLocalSignature);

    importedSignature = ZrCore_MetadataRuntime_ResolveSignatureRecord(runtime, TEST_MEMBER_REF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[3], importedSignature);
}

static void test_metadata_runtime_resolves_type_spec_records_as_type_records(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    const SZrMetadataTokenRecord *typeSpec;
    const SZrMetadataTokenRecord *cachedTypeSpec;

    functionRecords[0].token = TEST_TYPE_SPEC_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[0].signatureHash = 0x8877665544332211ULL;
    functionRecords[1].token = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[1].signatureHash = functionRecords[0].signatureHash;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    typeSpec = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, TEST_TYPE_SPEC_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], typeSpec);

    functionRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 77u);
    cachedTypeSpec = ZrCore_MetadataRuntime_ResolveTypeRecord(runtime, TEST_TYPE_SPEC_TOKEN);
    TEST_ASSERT_EQUAL_PTR(typeSpec, cachedTypeSpec);
}

static void test_metadata_runtime_resolves_field_records_with_independent_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrMetadataTokenRecord moduleRecords[2] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    const SZrMetadataTokenRecord *localField;
    const SZrMetadataTokenRecord *cachedLocalField;
    const SZrMetadataTokenRecord *importedField;

    functionRecords[0].token = TEST_FIELD_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[0].signatureHash = 0x1020304050607080ULL;
    functionRecords[1].token = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[1].signatureHash = functionRecords[0].signatureHash;
    functionRecords[2].token = TEST_MEMBER_DEF_TOKEN;
    functionRecords[2].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[2].signatureHash = 0x5555666677778888ULL;
    functionRecords[3].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[3].ownerToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[3].signatureHash = functionRecords[2].signatureHash;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    moduleRecords[0].token = TEST_FIELD_REF_TOKEN;
    moduleRecords[0].relatedToken = TEST_FIELD_REF_SIGNATURE_TOKEN;
    moduleRecords[0].signatureHash = 0x8877665544332211ULL;
    moduleRecords[1].token = TEST_FIELD_REF_SIGNATURE_TOKEN;
    moduleRecords[1].relatedToken = TEST_FIELD_REF_TOKEN;
    moduleRecords[1].ownerToken = TEST_FIELD_REF_TOKEN;
    moduleRecords[1].signatureHash = moduleRecords[0].signatureHash;
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 2u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveFieldRecord(ZR_NULL, TEST_FIELD_DEF_TOKEN));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, 0u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, TEST_TYPE_DEF_TOKEN));

    localField = ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], localField);

    TEST_ASSERT_EQUAL_PTR(&functionRecords[2],
                          ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, TEST_MEMBER_DEF_TOKEN));
    functionRecords[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 88u);
    cachedLocalField = ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, TEST_FIELD_DEF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(localField, cachedLocalField);

    importedField = ZrCore_MetadataRuntime_ResolveFieldRecord(runtime, TEST_FIELD_REF_TOKEN);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], importedField);
}

static void test_metadata_runtime_attaches_zrp_metadata_view(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataSectionView view;
    SZrZrpMetadataTypeDefRow *typeRows;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.typeDefs,
                                     &nextOffset,
                                     1u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    typeRows[0].token = TEST_TYPE_DEF_TOKEN;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_AttachZrpMetadata(ZR_NULL, bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, ZR_NULL, sizeof(bytes)));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, ZR_ZRP_METADATA_HEADER_SIZE - 1u));
    TEST_ASSERT_FALSE(runtime->hasZrpMetadata);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(runtime->hasZrpMetadata);
    TEST_ASSERT_EQUAL_PTR(bytes, runtime->zrpMetadataBuffer);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(bytes), (TZrUInt32)runtime->zrpMetadataBufferLength);
    TEST_ASSERT_EQUAL_UINT32(1u, runtime->zrpMetadataHeader.typeDefs.count);

    memset(&view, 0, sizeof(view));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetZrpSectionView(runtime,
                                                             ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                             &view));
    TEST_ASSERT_EQUAL_PTR(bytes + header.typeDefs.offset, view.data);
    TEST_ASSERT_EQUAL_UINT32(1u, view.count);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow), view.elementSize);
    TEST_ASSERT_EQUAL_PTR(&runtime->zrpMetadataHeader.typeDefs, view.section);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_GetZrpSectionView(ZR_NULL,
                                                              ZR_ZRP_METADATA_SECTION_TYPE_DEFS,
                                                              &view));
}

static void test_metadata_runtime_gets_validated_signature_blob_view(void) {
    static const TZrByte methodSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
            1u,
            0u,
            0u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            0u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataPoolSliceView slice;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(methodSignature)] = {0};

    attach_function_metadata_records(&metadataFunction, functionRecords);
    functionRecords[3].signatureBlobOffset = 0u;
    functionRecords[3].signatureBlobLength = (TZrUInt32)sizeof(methodSignature);

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(methodSignature),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        methodSignature,
                                                        (TZrUInt32)sizeof(methodSignature)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    memset(&slice, 0xCC, sizeof(slice));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_GetSignatureBlob(ZR_NULL, TEST_MEMBER_DEF_TOKEN, &slice));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_GetSignatureBlob(runtime, 0u, &slice));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_GetSignatureBlob(runtime, TEST_MEMBER_DEF_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_GetSignatureBlob(runtime, TEST_MEMBER_DEF_TOKEN, &slice));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetSignatureBlob(runtime, TEST_MEMBER_DEF_TOKEN, &slice));
    TEST_ASSERT_EQUAL_PTR(bytes + header.signatureBlobPool.offset, slice.data);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(methodSignature), (TZrUInt32)slice.byteLength);
    TEST_ASSERT_EQUAL_INT(0, memcmp(methodSignature, slice.data, sizeof(methodSignature)));

    functionRecords[3].signatureBlobLength = (TZrUInt32)sizeof(methodSignature) - 1u;
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_GetSignatureBlob(runtime, TEST_MEMBER_DEF_TOKEN, &slice));
    TEST_ASSERT_NULL(slice.data);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)slice.byteLength);
}

static void test_metadata_runtime_reads_method_and_field_signature_views(void) {
    static const TZrByte methodSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
            1u,
            0u,
            0u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte fieldSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
            1u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_BOOL, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeSignatureView view;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(methodSignature) + sizeof(fieldSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(signaturePayload)] = {0};

    functionRecords[0].token = TEST_MEMBER_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[1].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].signatureBlobOffset = 0u;
    functionRecords[1].signatureBlobLength = (TZrUInt32)sizeof(methodSignature);
    functionRecords[2].token = TEST_FIELD_DEF_TOKEN;
    functionRecords[2].relatedToken = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[3].token = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[3].ownerToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[3].signatureBlobOffset = (TZrUInt32)sizeof(methodSignature);
    functionRecords[3].signatureBlobLength = (TZrUInt32)sizeof(fieldSignature);
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    memcpy(signaturePayload, methodSignature, sizeof(methodSignature));
    memcpy(signaturePayload + sizeof(methodSignature), fieldSignature, sizeof(fieldSignature));

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(signaturePayload),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadSignatureView(ZR_NULL, TEST_MEMBER_DEF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadSignatureView(runtime, TEST_MEMBER_DEF_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadSignatureView(runtime, TEST_MEMBER_DEF_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureView(runtime, TEST_MEMBER_DEF_TOKEN, &view));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_METHOD_SIG, view.rootNode);
    TEST_ASSERT_EQUAL_UINT8(1u, view.callingConvention);
    TEST_ASSERT_EQUAL_UINT8(0u, view.flags);
    TEST_ASSERT_EQUAL_UINT32(0u, view.genericParameterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, view.parameterCount);
    TEST_ASSERT_EQUAL_UINT32(7u, view.returnTypeBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(16u, view.parameterListBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, view.fieldTypeBlobOffset);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureView(runtime, TEST_FIELD_DEF_TOKEN, &view));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_FIELD_SIG, view.rootNode);
    TEST_ASSERT_EQUAL_UINT8(0u, view.callingConvention);
    TEST_ASSERT_EQUAL_UINT8(1u, view.flags);
    TEST_ASSERT_EQUAL_UINT32(0u, view.genericParameterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, view.parameterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, view.returnTypeBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(0u, view.parameterListBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(2u, view.fieldTypeBlobOffset);
}

static void test_metadata_runtime_reads_signature_type_node_views(void) {
    static const TZrByte methodSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
            1u,
            0u,
            0u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeSignatureView signatureView;
    SZrMetadataRuntimeSignatureTypeNodeView nodeView;
    SZrMetadataRuntimeSignatureTypeNodeView baseNodeView;
    SZrMetadataRuntimeSignatureTypeNodeView argNodeView;
    SZrZrpMetadataPoolSliceView typeSpecBlob;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(methodSignature) + sizeof(genericInstanceSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(signaturePayload)] = {0};

    functionRecords[0].token = TEST_MEMBER_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[1].token = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].signatureBlobOffset = 0u;
    functionRecords[1].signatureBlobLength = (TZrUInt32)sizeof(methodSignature);
    functionRecords[2].token = TEST_TYPE_SPEC_TOKEN;
    functionRecords[2].relatedToken = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[3].token = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[3].ownerToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[3].signatureBlobOffset = (TZrUInt32)sizeof(methodSignature);
    functionRecords[3].signatureBlobLength = (TZrUInt32)sizeof(genericInstanceSignature);
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    memcpy(signaturePayload, methodSignature, sizeof(methodSignature));
    memcpy(signaturePayload + sizeof(methodSignature), genericInstanceSignature, sizeof(genericInstanceSignature));

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(signaturePayload),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureView(runtime, TEST_MEMBER_DEF_TOKEN, &signatureView));

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(ZR_NULL,
                                                                  signatureView.returnTypeBlobOffset,
                                                                  &nodeView));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob,
                                                                  signatureView.returnTypeBlobOffset,
                                                                  ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob,
                                                                  (TZrUInt32)signatureView.blob.byteLength,
                                                                  &nodeView));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob,
                                                                 signatureView.returnTypeBlobOffset,
                                                                 &nodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, nodeView.node);
    TEST_ASSERT_EQUAL_UINT32(signatureView.returnTypeBlobOffset, nodeView.blobOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, nodeView.payload0);
    TEST_ASSERT_EQUAL_UINT32(12u, nodeView.nextBlobOffset);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&signatureView.blob,
                                                                 signatureView.parameterListBlobOffset + 1u,
                                                                 &nodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, nodeView.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, nodeView.payload0);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(methodSignature), nodeView.nextBlobOffset);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_GetSignatureBlob(runtime, TEST_TYPE_SPEC_TOKEN, &typeSpecBlob));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&typeSpecBlob, 0u, &nodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, nodeView.node);
    TEST_ASSERT_EQUAL_UINT32(0u, nodeView.blobOffset);
    TEST_ASSERT_EQUAL_UINT32(1u, nodeView.baseTypeBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(1u, nodeView.childCount);
    TEST_ASSERT_EQUAL_UINT32(14u, nodeView.childListBlobOffset);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(genericInstanceSignature), nodeView.nextBlobOffset);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&typeSpecBlob,
                                                                 nodeView.baseTypeBlobOffset,
                                                                 &baseNodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, baseNodeView.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, baseNodeView.payload0);
    TEST_ASSERT_EQUAL_UINT32(13u, baseNodeView.payload1);
    TEST_ASSERT_EQUAL_UINT32(10u, baseNodeView.nextBlobOffset);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&typeSpecBlob,
                                                                 nodeView.childListBlobOffset,
                                                                 &argNodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, argNodeView.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, argNodeView.payload0);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(genericInstanceSignature), argNodeView.nextBlobOffset);
}

static void test_metadata_runtime_reads_generic_type_spec_signature_view(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeSpecSignatureView view;
    SZrMetadataRuntimeSignatureTypeNodeView argumentNodeView;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(genericInstanceSignature)] = {0};

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

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(genericInstanceSignature),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        genericInstanceSignature,
                                                        (TZrUInt32)sizeof(genericInstanceSignature)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(ZR_NULL, TEST_TYPE_SPEC_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(runtime, TEST_TYPE_SPEC_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(runtime, TEST_TYPE_REF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, view.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_SIGNATURE_TOKEN, view.signatureToken);
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)0x123456789ABCDEF0ULL, view.signatureHash);
    TEST_ASSERT_EQUAL_PTR(bytes + ZR_ZRP_METADATA_HEADER_SIZE, view.blob.data);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(genericInstanceSignature), (TZrUInt32)view.blob.byteLength);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, view.genericInstanceNode.node);
    TEST_ASSERT_EQUAL_UINT32(0u, view.genericInstanceNode.blobOffset);
    TEST_ASSERT_EQUAL_UINT32(1u, view.genericInstanceNode.baseTypeBlobOffset);
    TEST_ASSERT_EQUAL_UINT32(1u, view.argumentCount);
    TEST_ASSERT_EQUAL_UINT32(14u, view.argumentListBlobOffset);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, view.baseTypeNode.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, view.baseTypeNode.payload0);
    TEST_ASSERT_EQUAL_UINT32(13u, view.baseTypeNode.payload1);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&view.blob,
                                                                 view.argumentListBlobOffset,
                                                                 &argumentNodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, argumentNodeView.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, argumentNodeView.payload0);
}

static void test_metadata_runtime_reads_type_spec_generic_base_binding_view(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeRefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            13u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrMetadataTokenRecord moduleRecords[2] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeSpecGenericBindingView view;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeRefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(signaturePayload)] = {0};

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
    metadataFunction.moduleMetadataTokenRecords = moduleRecords;
    metadataFunction.moduleMetadataTokenRecordLength = 2u;

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeRefSignature, sizeof(baseTypeRefSignature));

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(signaturePayload),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(ZR_NULL, TEST_TYPE_SPEC_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, TEST_TYPE_SPEC_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, TEST_TYPE_REF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_TOKEN, view.signatureView.typeSpecToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_SPEC_SIGNATURE_TOKEN, view.signatureView.signatureToken);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[0].signatureHash, view.signatureView.signatureHash);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, view.signatureView.genericInstanceNode.node);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, view.signatureView.baseTypeNode.node);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_REF_TOKEN, view.baseToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[0], view.baseRecord);
}

static void test_metadata_runtime_reads_type_spec_generic_typedef_base_binding_view(void) {
    static const TZrByte genericInstanceSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
            1u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
    };
    static const TZrByte baseTypeDefSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_TYPE_DEF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            17u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeSpecGenericBindingView view;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) + sizeof(baseTypeDefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(signaturePayload)] = {0};

    functionRecords[0].token = TEST_TYPE_SPEC_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[0].signatureHash = 0x123456789ABCDEF0ULL;
    functionRecords[1].token = TEST_TYPE_SPEC_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_SPEC_TOKEN;
    functionRecords[1].signatureBlobOffset = 0u;
    functionRecords[1].signatureBlobLength = (TZrUInt32)sizeof(genericInstanceSignature);
    functionRecords[1].signatureHash = functionRecords[0].signatureHash;
    functionRecords[2].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[2].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[2].signatureHash = 0x2233445566778899ULL;
    functionRecords[3].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[3].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[3].signatureBlobOffset = (TZrUInt32)sizeof(genericInstanceSignature);
    functionRecords[3].signatureBlobLength = (TZrUInt32)sizeof(baseTypeDefSignature);
    functionRecords[3].signatureHash = functionRecords[2].signatureHash;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    memcpy(signaturePayload, genericInstanceSignature, sizeof(genericInstanceSignature));
    memcpy(signaturePayload + sizeof(genericInstanceSignature), baseTypeDefSignature, sizeof(baseTypeDefSignature));

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(signaturePayload),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, TEST_TYPE_SPEC_TOKEN, &view));

    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_TYPE_DEF, view.signatureView.baseTypeNode.node);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, view.baseToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[2], view.baseRecord);
}

static void test_metadata_runtime_reads_type_spec_generic_argument_binding_view(void) {
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
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrMetadataTokenRecord moduleRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeSpecGenericArgumentView argumentView;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte signaturePayload[sizeof(genericInstanceSignature) +
                             sizeof(baseTypeRefSignature) +
                             sizeof(argumentTypeRefSignature)] = {0};
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(signaturePayload)] = {0};

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
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(signaturePayload),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        signaturePayload,
                                                        (TZrUInt32)sizeof(signaturePayload)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            ZR_NULL, TEST_TYPE_SPEC_TOKEN, 0u, &argumentView));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            runtime, TEST_TYPE_SPEC_TOKEN, 0u, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            runtime, TEST_TYPE_REF_TOKEN, 0u, &argumentView));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            runtime, TEST_TYPE_SPEC_TOKEN, 0u, &argumentView));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            runtime, TEST_TYPE_SPEC_TOKEN, 0u, &argumentView));
    TEST_ASSERT_EQUAL_UINT32(0u, argumentView.argumentIndex);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_REF_TOKEN, argumentView.bindingView.baseToken);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, argumentView.argumentNode.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, argumentView.argumentNode.payload0);
    TEST_ASSERT_EQUAL_UINT32(0u, argumentView.argumentToken);
    TEST_ASSERT_NULL(argumentView.argumentRecord);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            runtime, TEST_TYPE_SPEC_TOKEN, 1u, &argumentView));
    TEST_ASSERT_EQUAL_UINT32(1u, argumentView.argumentIndex);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_TYPE_REF, argumentView.argumentNode.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_OBJECT, argumentView.argumentNode.payload0);
    TEST_ASSERT_EQUAL_UINT32(21u, argumentView.argumentNode.payload1);
    TEST_ASSERT_EQUAL_UINT32(TEST_GENERIC_ARG_TYPE_REF_TOKEN, argumentView.argumentToken);
    TEST_ASSERT_EQUAL_PTR(&moduleRecords[2], argumentView.argumentRecord);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
            runtime, TEST_TYPE_SPEC_TOKEN, 2u, &argumentView));
}

static void test_metadata_runtime_reads_method_spec_signature_view(void) {
    static const TZrByte methodSpecSignature[] = {
            ZR_METADATA_SIGNATURE_NODE_GENERIC_INST,
            ZR_METADATA_SIGNATURE_NODE_MEMBER_REF,
            (TZrByte)(TEST_MEMBER_DEF_TOKEN & 0xFFu),
            (TZrByte)((TEST_MEMBER_DEF_TOKEN >> 8u) & 0xFFu),
            (TZrByte)((TEST_MEMBER_DEF_TOKEN >> 16u) & 0xFFu),
            (TZrByte)((TEST_MEMBER_DEF_TOKEN >> 24u) & 0xFFu),
            2u, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64, 0u, 0u, 0u,
            ZR_METADATA_SIGNATURE_NODE_TYPE_REF,
            (TZrByte)ZR_VALUE_TYPE_OBJECT, 0u, 0u, 0u,
            23u, 0u, 0u, 0u,
    };
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[2] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeMethodSpecSignatureView view;
    SZrMetadataRuntimeSignatureTypeNodeView argumentNodeView;
    SZrZrpMetadataHeader header;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(methodSpecSignature)] = {0};

    functionRecords[0].token = TEST_MEMBER_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_MEMBER_DEF_SIGNATURE_TOKEN;
    functionRecords[0].signatureHash = 0x5555666677778888ULL;
    functionRecords[1].token = TEST_METHOD_SPEC_TOKEN;
    functionRecords[1].relatedToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_MEMBER_DEF_TOKEN;
    functionRecords[1].signatureBlobOffset = 0u;
    functionRecords[1].signatureBlobLength = (TZrUInt32)sizeof(methodSpecSignature);
    functionRecords[1].signatureHash = 0x33445566778899AAULL;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 2u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.signatureBlobPool,
                                     &nextOffset,
                                     (TZrUInt32)sizeof(methodSpecSignature),
                                     1u);
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WritePoolPayload(bytes,
                                                        sizeof(bytes),
                                                        &header,
                                                        ZR_ZRP_METADATA_SECTION_SIGNATURE_BLOB_POOL,
                                                        methodSpecSignature,
                                                        (TZrUInt32)sizeof(methodSpecSignature)));

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(ZR_NULL, TEST_METHOD_SPEC_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, TEST_METHOD_SPEC_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, TEST_MEMBER_DEF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, TEST_METHOD_SPEC_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, TEST_METHOD_SPEC_TOKEN, &view));

    TEST_ASSERT_EQUAL_UINT32(TEST_METHOD_SPEC_TOKEN, view.methodSpecToken);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, view.methodToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], view.methodRecord);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[1].signatureHash, view.signatureHash);
    TEST_ASSERT_EQUAL_PTR(bytes + ZR_ZRP_METADATA_HEADER_SIZE, view.blob.data);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(methodSpecSignature), (TZrUInt32)view.blob.byteLength);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_GENERIC_INST, view.genericInstanceNode.node);
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_MEMBER_REF, view.methodNode.node);
    TEST_ASSERT_EQUAL_UINT32(TEST_MEMBER_DEF_TOKEN, view.methodNode.payload0);
    TEST_ASSERT_EQUAL_UINT32(2u, view.argumentCount);
    TEST_ASSERT_EQUAL_UINT32(10u, view.argumentListBlobOffset);

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadSignatureTypeNode(&view.blob,
                                                                 view.argumentListBlobOffset,
                                                                 &argumentNodeView));
    TEST_ASSERT_EQUAL_INT(ZR_METADATA_SIGNATURE_NODE_PRIMITIVE, argumentNodeView.node);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_VALUE_TYPE_INT64, argumentNodeView.payload0);
}

static void test_metadata_runtime_reads_typedef_layout_binding_view(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout typeLayouts[43] = {0};
    const SZrTypeLayout *registeredTypeLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeDefLayoutBindingView view;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeRows;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow) * 2u] = {0};

    attach_function_metadata_records(&metadataFunction, functionRecords);
    functionRecords[0].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[0].layoutVersion = 3u;
    functionRecords[0].layoutHash = 0x1020304050607080ULL;
    typeLayouts[42].cTypeId = 42u;
    registeredTypeLayouts[42] = &typeLayouts[42];
    registration.typeLayouts = registeredTypeLayouts;
    registration.typeLayoutCount = 43u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.typeDefs,
                                     &nextOffset,
                                     2u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    typeRows[0].token = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 2u);
    typeRows[0].typeLayoutId = 99u;
    typeRows[1].token = TEST_TYPE_DEF_TOKEN;
    typeRows[1].typeLayoutId = 42u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
            ZR_NULL, TEST_TYPE_DEF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
            runtime, TEST_TYPE_DEF_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
            runtime, TEST_TYPE_REF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
            runtime, TEST_TYPE_DEF_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
            runtime, TEST_TYPE_DEF_TOKEN, &view));

    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, view.typeDefToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], view.typeRecord);
    TEST_ASSERT_EQUAL_PTR(&typeRows[1], view.typeDefRow);
    TEST_ASSERT_EQUAL_UINT32(42u, view.typeLayoutId);
    TEST_ASSERT_EQUAL_UINT32(42u, view.cTypeId);
    TEST_ASSERT_EQUAL_UINT32(functionRecords[0].layoutVersion, view.layoutVersion);
    TEST_ASSERT_EQUAL_UINT64(functionRecords[0].layoutHash, view.layoutHash);
    TEST_ASSERT_EQUAL_PTR(&typeLayouts[42], view.typeLayout);
}

static void test_metadata_runtime_typedef_layout_binding_uses_code_registration_registry(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    SZrTypeLayout registeredLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeTypeDefLayoutBindingView view;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeRows;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE + sizeof(SZrZrpMetadataTypeDefRow)] = {0};

    attach_function_metadata_records(&metadataFunction, functionRecords);
    functionRecords[0].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[0].layoutVersion = 5u;
    functionRecords[0].layoutHash = 0x8877665544332211ULL;

    stalePrototypeLayouts[42].cTypeId = 42u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;

    registeredLayout.cTypeId = 42u;
    registeredLayout.byteSize = 24u;
    registeredLayouts[42] = &registeredLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.typeDefs,
                                     &nextOffset,
                                     1u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    typeRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeRows[0].typeLayoutId = 42u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView(
            runtime, TEST_TYPE_DEF_TOKEN, &view));

    TEST_ASSERT_EQUAL_PTR(&registeredLayout, view.typeLayout);
    TEST_ASSERT_EQUAL_UINT32(24u, view.typeLayout->byteSize);
}

static void test_metadata_runtime_reads_fielddef_layout_binding_view(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout fieldLayout = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeFieldDefLayoutBindingView view;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeDefRow) +
                  sizeof(SZrZrpMetadataFieldDefRow)] = {0};

    functionRecords[0].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[0].layoutVersion = 7u;
    functionRecords[0].layoutHash = 0x0102030405060708ULL;
    functionRecords[1].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[1].signatureHash = functionRecords[0].signatureHash;
    functionRecords[2].token = TEST_FIELD_DEF_TOKEN;
    functionRecords[2].relatedToken = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[2].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[2].signatureHash = 0x1112131415161718ULL;
    functionRecords[3].token = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[3].ownerToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[3].signatureHash = functionRecords[2].signatureHash;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    ownerLayout.cTypeId = 7u;
    ownerLayout.byteSize = 64u;
    fieldLayout.cTypeId = 42u;
    fieldLayout.byteSize = 16u;
    registeredLayouts[7] = &ownerLayout;
    registeredLayouts[42] = &fieldLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.typeDefs,
                                     &nextOffset,
                                     1u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_runtime_test_counted_section(&header.fieldDefs,
                                     &nextOffset,
                                     1u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    fieldRows = (SZrZrpMetadataFieldDefRow *)(void *)(bytes + header.fieldDefs.offset);
    typeRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeRows[0].firstFieldDefIndex = 0u;
    typeRows[0].fieldDefCount = 1u;
    typeRows[0].typeLayoutId = 7u;
    fieldRows[0].token = TEST_FIELD_DEF_TOKEN;
    fieldRows[0].ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    fieldRows[0].byteOffset = 24u;
    fieldRows[0].typeLayoutId = 42u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
            ZR_NULL, TEST_FIELD_DEF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
            runtime, TEST_FIELD_DEF_TOKEN, ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
            runtime, TEST_TYPE_DEF_TOKEN, &view));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
            runtime, TEST_FIELD_DEF_TOKEN, &view));

    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
            runtime, TEST_FIELD_DEF_TOKEN, &view));

    TEST_ASSERT_EQUAL_UINT32(TEST_FIELD_DEF_TOKEN, view.fieldDefToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[2], view.fieldRecord);
    TEST_ASSERT_EQUAL_PTR(&fieldRows[0], view.fieldDefRow);
    TEST_ASSERT_EQUAL_UINT32(TEST_TYPE_DEF_TOKEN, view.ownerTypeToken);
    TEST_ASSERT_EQUAL_PTR(&functionRecords[0], view.ownerTypeRecord);
    TEST_ASSERT_EQUAL_PTR(&typeRows[0], view.ownerTypeDefRow);
    TEST_ASSERT_EQUAL_UINT32(24u, view.byteOffset);
    TEST_ASSERT_EQUAL_UINT32(42u, view.fieldTypeLayoutId);
    TEST_ASSERT_EQUAL_UINT32(7u, view.ownerTypeLayoutId);
    TEST_ASSERT_EQUAL_PTR(&fieldLayout, view.fieldTypeLayout);
    TEST_ASSERT_EQUAL_PTR(&ownerLayout, view.ownerTypeLayout);
}

static void test_metadata_runtime_fielddef_layout_binding_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrMetadataTokenRecord functionRecords[4] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout ownerLayout = {0};
    SZrTypeLayout stalePrototypeLayouts[43] = {0};
    const SZrTypeLayout *registeredLayouts[43] = {0};
    SZrMetadataRuntime *runtime;
    SZrMetadataRuntimeFieldDefLayoutBindingView view;
    SZrZrpMetadataHeader header;
    SZrZrpMetadataTypeDefRow *typeRows;
    SZrZrpMetadataFieldDefRow *fieldRows;
    TZrUInt32 nextOffset;
    TZrByte bytes[ZR_ZRP_METADATA_HEADER_SIZE +
                  sizeof(SZrZrpMetadataTypeDefRow) +
                  sizeof(SZrZrpMetadataFieldDefRow)] = {0};

    functionRecords[0].token = TEST_TYPE_DEF_TOKEN;
    functionRecords[0].relatedToken = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[1].token = TEST_TYPE_DEF_SIGNATURE_TOKEN;
    functionRecords[1].relatedToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[1].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[2].token = TEST_FIELD_DEF_TOKEN;
    functionRecords[2].relatedToken = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[2].ownerToken = TEST_TYPE_DEF_TOKEN;
    functionRecords[3].token = TEST_FIELD_DEF_SIGNATURE_TOKEN;
    functionRecords[3].relatedToken = TEST_FIELD_DEF_TOKEN;
    functionRecords[3].ownerToken = TEST_FIELD_DEF_TOKEN;
    metadataFunction.metadataTokenRecords = functionRecords;
    metadataFunction.metadataTokenRecordLength = 4u;

    ownerLayout.cTypeId = 7u;
    registeredLayouts[7] = &ownerLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 43u;

    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 128u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 43u;

    ZrCore_ZrpMetadata_InitHeader(&header);
    nextOffset = ZR_ZRP_METADATA_HEADER_SIZE;
    set_runtime_test_counted_section(&header.typeDefs,
                                     &nextOffset,
                                     1u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataTypeDefRow));
    set_runtime_test_counted_section(&header.fieldDefs,
                                     &nextOffset,
                                     1u,
                                     (TZrUInt32)sizeof(SZrZrpMetadataFieldDefRow));
    TEST_ASSERT_TRUE(ZrCore_ZrpMetadata_WriteHeader(bytes, sizeof(bytes), &header));
    typeRows = (SZrZrpMetadataTypeDefRow *)(void *)(bytes + header.typeDefs.offset);
    fieldRows = (SZrZrpMetadataFieldDefRow *)(void *)(bytes + header.fieldDefs.offset);
    typeRows[0].token = TEST_TYPE_DEF_TOKEN;
    typeRows[0].firstFieldDefIndex = 0u;
    typeRows[0].fieldDefCount = 1u;
    typeRows[0].typeLayoutId = 7u;
    fieldRows[0].token = TEST_FIELD_DEF_TOKEN;
    fieldRows[0].ownerTypeToken = TEST_TYPE_DEF_TOKEN;
    fieldRows[0].byteOffset = 24u;
    fieldRows[0].typeLayoutId = 42u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);
    TEST_ASSERT_TRUE(ZrCore_MetadataRuntime_AttachZrpMetadata(runtime, bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(
            runtime, TEST_FIELD_DEF_TOKEN, &view));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_metadata_records_can_be_queried_by_token);
    RUN_TEST(test_module_metadata_records_are_queried_from_entry_ref_table);
    RUN_TEST(test_signature_query_requires_related_signature_owner_pair);
    RUN_TEST(test_module_metadata_runtime_attaches_code_registration);
    RUN_TEST(test_metadata_runtime_resolves_method_records_lazily);
    RUN_TEST(test_metadata_runtime_resolves_type_records_lazily);
    RUN_TEST(test_metadata_runtime_resolves_signature_records_lazily);
    RUN_TEST(test_metadata_runtime_resolves_type_spec_records_as_type_records);
    RUN_TEST(test_metadata_runtime_resolves_field_records_with_independent_cache);
    RUN_TEST(test_metadata_runtime_attaches_zrp_metadata_view);
    RUN_TEST(test_metadata_runtime_gets_validated_signature_blob_view);
    RUN_TEST(test_metadata_runtime_reads_method_and_field_signature_views);
    RUN_TEST(test_metadata_runtime_reads_signature_type_node_views);
    RUN_TEST(test_metadata_runtime_reads_generic_type_spec_signature_view);
    RUN_TEST(test_metadata_runtime_reads_type_spec_generic_base_binding_view);
    RUN_TEST(test_metadata_runtime_reads_type_spec_generic_typedef_base_binding_view);
    RUN_TEST(test_metadata_runtime_reads_type_spec_generic_argument_binding_view);
    RUN_TEST(test_metadata_runtime_reads_method_spec_signature_view);
    RUN_TEST(test_metadata_runtime_reads_typedef_layout_binding_view);
    RUN_TEST(test_metadata_runtime_typedef_layout_binding_uses_code_registration_registry);
    RUN_TEST(test_metadata_runtime_reads_fielddef_layout_binding_view);
    RUN_TEST(test_metadata_runtime_fielddef_layout_binding_does_not_fallback_to_prototype_cache);
    return UNITY_END();
}
