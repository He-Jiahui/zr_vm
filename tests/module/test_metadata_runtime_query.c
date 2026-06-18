#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_token.h"

#define TEST_MODULE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u)
#define TEST_MODULE_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 1u)
#define TEST_MEMBER_DEF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u)
#define TEST_MEMBER_DEF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 2u)
#define TEST_ASSEMBLY_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_ASSEMBLY_REF, 1u)
#define TEST_ASSEMBLY_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 3u)
#define TEST_MEMBER_REF_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_REF, 1u)
#define TEST_MEMBER_REF_SIGNATURE_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 4u)

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_metadata_records_can_be_queried_by_token);
    RUN_TEST(test_module_metadata_records_are_queried_from_entry_ref_table);
    RUN_TEST(test_signature_query_requires_related_signature_owner_pair);
    return UNITY_END();
}
