#include "unity.h"

#include "backend_aot_c_zrp_metadata_remap.h"

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/zrp_metadata.h"

#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_aot_c_zrp_metadata_export_token_remap_compacts_retained_method_export_tokens(void) {
    SZrZrpMetadataMethodDefRow methodDefs[3];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    TZrUInt32 retainedMethodDefCount;
    TZrMetadataToken exportToken;
    TZrMetadataToken removedBeforeToken;
    TZrMetadataToken removedAfterToken;
    const TZrMetadataToken removedBeforeMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken removedAfterMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken compactedExportToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    memset(methodDefs, 0, sizeof(methodDefs));
    methodDefs[0].token = removedBeforeMethodToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].functionIndex = 1u;
    methodDefs[2].token = removedAfterMethodToken;
    methodDefs[2].functionIndex = 2u;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 3u;

    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 3u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, retainedMethodDefCount);

    exportToken = keptMethodToken;
    TEST_ASSERT_TRUE(backend_aot_c_zrp_remap_export_member_token(&exportToken,
                                                                 methodDefs,
                                                                 3u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 &functionTable,
                                                                 retainedMethodDefCount));
    TEST_ASSERT_EQUAL_UINT32(compactedExportToken, exportToken);

    removedBeforeToken = removedBeforeMethodToken;
    TEST_ASSERT_FALSE(backend_aot_c_zrp_remap_export_member_token(&removedBeforeToken,
                                                                  methodDefs,
                                                                  3u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  &functionTable,
                                                                  retainedMethodDefCount));

    removedAfterToken = removedAfterMethodToken;
    TEST_ASSERT_FALSE(backend_aot_c_zrp_remap_export_member_token(&removedAfterToken,
                                                                  methodDefs,
                                                                  3u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  &functionTable,
                                                                  retainedMethodDefCount));
}

static void test_aot_c_zrp_metadata_export_token_remap_compacts_field_export_tokens_after_methods(void) {
    SZrZrpMetadataMethodDefRow methodDefs[2];
    SZrZrpMetadataFieldDefRow fieldDefs[2];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    TZrUInt32 retainedMethodDefCount;
    TZrMetadataToken firstFieldExportToken;
    TZrMetadataToken secondFieldExportToken;
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken firstFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken secondFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 4u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(fieldDefs, 0, sizeof(fieldDefs));
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].functionIndex = 1u;
    fieldDefs[0].token = firstFieldToken;
    fieldDefs[1].token = secondFieldToken;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;

    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 2u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, retainedMethodDefCount);

    firstFieldExportToken = firstFieldToken;
    TEST_ASSERT_TRUE(backend_aot_c_zrp_remap_export_member_token(&firstFieldExportToken,
                                                                 methodDefs,
                                                                 2u,
                                                                 fieldDefs,
                                                                 2u,
                                                                 &functionTable,
                                                                 retainedMethodDefCount));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u),
                             firstFieldExportToken);

    secondFieldExportToken = secondFieldToken;
    TEST_ASSERT_TRUE(backend_aot_c_zrp_remap_export_member_token(&secondFieldExportToken,
                                                                 methodDefs,
                                                                 2u,
                                                                 fieldDefs,
                                                                 2u,
                                                                 &functionTable,
                                                                 retainedMethodDefCount));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u),
                             secondFieldExportToken);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_export_token_remap_compacts_retained_method_export_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_remap_compacts_field_export_tokens_after_methods);
    return UNITY_END();
}
