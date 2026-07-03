#include "unity.h"

#include "backend_aot_c_zrp_metadata_member_token.h"
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

static void test_aot_c_zrp_metadata_export_token_remap_rejects_retained_method_count_drift(void) {
    SZrZrpMetadataMethodDefRow methodDefs[2];
    SZrZrpMetadataFieldDefRow fieldDefs[1];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    TZrUInt32 actualRetainedMethodDefCount;
    TZrMetadataToken fieldExportToken;
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(fieldDefs, 0, sizeof(fieldDefs));
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].functionIndex = 1u;
    fieldDefs[0].token = fieldToken;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;

    actualRetainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 2u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, actualRetainedMethodDefCount);

    fieldExportToken = fieldToken;
    TEST_ASSERT_FALSE(backend_aot_c_zrp_remap_export_member_token(&fieldExportToken,
                                                                  methodDefs,
                                                                  2u,
                                                                  fieldDefs,
                                                                  1u,
                                                                  &functionTable,
                                                                  actualRetainedMethodDefCount + 1u));
    TEST_ASSERT_EQUAL_UINT32(fieldToken, fieldExportToken);
}

static void test_aot_c_zrp_metadata_export_token_sidecar_publishes_compacted_member_tokens(void) {
    SZrZrpMetadataMethodDefRow methodDefs[3];
    SZrZrpMetadataFieldDefRow fieldDefs[1];
    SZrZrpMetadataTypeDefRow typeDefs[1];
    SZrMetadataTokenRecord tokenRecords[1];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata metadata;
    TZrUInt32 retainedMethodDefCount;
    TZrMetadataToken keptMethodToken;
    TZrMetadataToken removedMethodToken;
    TZrMetadataToken fieldToken;
    const TZrMetadataToken removedBeforeMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken originalKeptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken removedAfterMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);
    const TZrMetadataToken originalFieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 4u);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(fieldDefs, 0, sizeof(fieldDefs));
    memset(typeDefs, 0, sizeof(typeDefs));
    memset(tokenRecords, 0, sizeof(tokenRecords));
    memset(&metadata, 0, sizeof(metadata));
    methodDefs[0].token = removedBeforeMethodToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = originalKeptMethodToken;
    methodDefs[1].functionIndex = 1u;
    methodDefs[2].token = removedAfterMethodToken;
    methodDefs[2].functionIndex = 2u;
    fieldDefs[0].token = originalFieldToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    typeDefs[0].token = typeToken;
    tokenRecords[0].token = typeToken;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 3u;

    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 3u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, retainedMethodDefCount);

    TEST_ASSERT_TRUE(backend_aot_c_zrp_member_token_remap_build(&metadata,
                                                                 methodDefs,
                                                                 3u,
                                                                 fieldDefs,
                                                                 1u,
                                                                 typeDefs,
                                                                 1u,
                                                                 tokenRecords,
                                                                 1u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 &functionTable,
                                                                 retainedMethodDefCount,
                                                                 1u));
    TEST_ASSERT_EQUAL_UINT32(2u, metadata.memberTokenRemapCount);

    keptMethodToken = originalKeptMethodToken;
    TEST_ASSERT_TRUE(backend_aot_c_embedded_zrp_metadata_remap_member_token(&metadata, &keptMethodToken));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u),
                             keptMethodToken);

    fieldToken = originalFieldToken;
    TEST_ASSERT_TRUE(backend_aot_c_embedded_zrp_metadata_remap_member_token(&metadata, &fieldToken));
    TEST_ASSERT_EQUAL_UINT32(ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u), fieldToken);

    removedMethodToken = removedAfterMethodToken;
    TEST_ASSERT_FALSE(backend_aot_c_embedded_zrp_metadata_remap_member_token(&metadata, &removedMethodToken));

    backend_aot_c_zrp_member_token_remap_destroy(&metadata);
}

static void test_aot_c_zrp_metadata_manifest_export_table_publishes_remapped_member_tokens(void) {
    SZrAotCEmbeddedZrpMetadata metadata;
    SZrAotCZrpMemberTokenRemapEntry remapEntries[1];
    SZrAotManifestExportDeclaration declarations[3];
    const TZrMetadataToken sourceMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 3u);

    memset(&metadata, 0, sizeof(metadata));
    memset(remapEntries, 0, sizeof(remapEntries));
    memset(declarations, 0, sizeof(declarations));

    remapEntries[0].sourceToken = sourceMethodToken;
    remapEntries[0].targetToken = compactedMethodToken;
    metadata.memberTokenRemapEntries = remapEntries;
    metadata.memberTokenRemapCount = 1u;

    declarations[0].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declarations[0].target = "Factory.make";
    declarations[0].hasMemberTokenBinding = ZR_TRUE;
    declarations[0].memberToken = sourceMethodToken;
    declarations[1].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE;
    declarations[1].target = "Factory";
    declarations[1].hasTypeTokenBinding = ZR_TRUE;
    declarations[1].typeToken = typeToken;
    declarations[2].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD;
    declarations[2].target = "Factory.value";

    TEST_ASSERT_TRUE(backend_aot_c_zrp_manifest_export_table_build(&metadata, declarations, 3u));
    TEST_ASSERT_EQUAL_UINT32(3u, metadata.manifestExportCount);
    TEST_ASSERT_NOT_NULL(metadata.manifestExportEntries);

    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD,
                             metadata.manifestExportEntries[0].kind);
    TEST_ASSERT_EQUAL_STRING("Factory.make", metadata.manifestExportEntries[0].target);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN,
                             metadata.manifestExportEntries[0].flags);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportEntries[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(compactedMethodToken, metadata.manifestExportEntries[0].memberToken);

    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE,
                             metadata.manifestExportEntries[1].kind);
    TEST_ASSERT_EQUAL_STRING("Factory", metadata.manifestExportEntries[1].target);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN,
                             metadata.manifestExportEntries[1].flags);
    TEST_ASSERT_EQUAL_UINT32(typeToken, metadata.manifestExportEntries[1].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportEntries[1].memberToken);

    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)ZR_AOT_MANIFEST_EXPORT_DECLARATION_FIELD,
                             metadata.manifestExportEntries[2].kind);
    TEST_ASSERT_EQUAL_STRING("Factory.value", metadata.manifestExportEntries[2].target);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportEntries[2].flags);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportEntries[2].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportEntries[2].memberToken);

    backend_aot_c_zrp_manifest_export_table_destroy(&metadata);
}

static void test_aot_c_zrp_metadata_manifest_export_table_rejects_kind_token_mismatch(void) {
    SZrAotCEmbeddedZrpMetadata metadata;
    SZrAotManifestExportDeclaration declaration;
    TZrBool result;

    memset(&metadata, 0, sizeof(metadata));
    memset(&declaration, 0, sizeof(declaration));

    declaration.kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_TYPE;
    declaration.target = "Factory";
    declaration.hasMemberTokenBinding = ZR_TRUE;
    declaration.memberToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);

    result = backend_aot_c_zrp_manifest_export_table_build(&metadata, &declaration, 1u);
    if (result) {
        backend_aot_c_zrp_manifest_export_table_destroy(&metadata);
    }
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_NULL(metadata.manifestExportEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportCount);
    TEST_ASSERT_NULL(metadata.ownedManifestExportEntries);

    declaration.kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declaration.target = "Factory.make";
    declaration.hasMemberTokenBinding = ZR_FALSE;
    declaration.memberToken = 0u;
    declaration.hasTypeTokenBinding = ZR_TRUE;
    declaration.typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    result = backend_aot_c_zrp_manifest_export_table_build(&metadata, &declaration, 1u);
    if (result) {
        backend_aot_c_zrp_manifest_export_table_destroy(&metadata);
    }
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_NULL(metadata.manifestExportEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportCount);
    TEST_ASSERT_NULL(metadata.ownedManifestExportEntries);
}

static void test_aot_c_zrp_metadata_manifest_export_table_rejects_duplicate_kind_target(void) {
    SZrAotCEmbeddedZrpMetadata metadata;
    SZrAotCZrpMemberTokenRemapEntry remapEntries[1];
    SZrAotManifestExportDeclaration declarations[2];
    const TZrMetadataToken sourceMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 7u);
    const TZrMetadataToken compactedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);

    memset(&metadata, 0, sizeof(metadata));
    memset(remapEntries, 0, sizeof(remapEntries));
    memset(declarations, 0, sizeof(declarations));

    remapEntries[0].sourceToken = sourceMethodToken;
    remapEntries[0].targetToken = compactedMethodToken;
    metadata.memberTokenRemapEntries = remapEntries;
    metadata.memberTokenRemapCount = 1u;

    declarations[0].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declarations[0].target = "Factory.make";
    declarations[0].hasMemberTokenBinding = ZR_TRUE;
    declarations[0].memberToken = sourceMethodToken;
    declarations[1].kind = ZR_AOT_MANIFEST_EXPORT_DECLARATION_METHOD;
    declarations[1].target = "Factory.make";
    declarations[1].hasMemberTokenBinding = ZR_TRUE;
    declarations[1].memberToken = sourceMethodToken;

    TEST_ASSERT_FALSE(backend_aot_c_zrp_manifest_export_table_build(&metadata, declarations, 2u));
    TEST_ASSERT_NULL(metadata.manifestExportEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.manifestExportCount);
    TEST_ASSERT_NULL(metadata.ownedManifestExportEntries);
}

static void test_aot_c_zrp_metadata_export_token_sidecar_rejects_duplicate_source_tokens(void) {
    SZrZrpMetadataMethodDefRow methodDefs[1];
    SZrZrpMetadataFieldDefRow fieldDefs[1];
    SZrZrpMetadataTypeDefRow typeDefs[1];
    SZrMetadataTokenRecord tokenRecords[1];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata metadata;
    TZrUInt32 retainedMethodDefCount;
    const TZrMetadataToken duplicateSourceToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken typeToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(fieldDefs, 0, sizeof(fieldDefs));
    memset(typeDefs, 0, sizeof(typeDefs));
    memset(tokenRecords, 0, sizeof(tokenRecords));
    memset(&metadata, 0, sizeof(metadata));
    methodDefs[0].token = duplicateSourceToken;
    methodDefs[0].functionIndex = 0u;
    fieldDefs[0].token = duplicateSourceToken;
    fieldDefs[0].ownerTypeToken = typeToken;
    typeDefs[0].token = typeToken;
    tokenRecords[0].token = typeToken;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 0u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 1u;

    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 1u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, retainedMethodDefCount);

    TEST_ASSERT_FALSE(backend_aot_c_zrp_member_token_remap_build(&metadata,
                                                                  methodDefs,
                                                                  1u,
                                                                  fieldDefs,
                                                                  1u,
                                                                  typeDefs,
                                                                  1u,
                                                                  tokenRecords,
                                                                  1u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  &functionTable,
                                                                  retainedMethodDefCount,
                                                                  1u));
    TEST_ASSERT_NULL(metadata.memberTokenRemapEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.memberTokenRemapCount);
    TEST_ASSERT_NULL(metadata.ownedMemberTokenRemapEntries);
}

static void test_aot_c_zrp_metadata_export_token_sidecar_rejects_non_member_source_tokens(void) {
    SZrZrpMetadataMethodDefRow methodDefs[1];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata metadata;
    TZrUInt32 retainedMethodDefCount;
    const TZrMetadataToken invalidSourceToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 1u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(&metadata, 0, sizeof(metadata));
    methodDefs[0].token = invalidSourceToken;
    methodDefs[0].functionIndex = 0u;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 0u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 1u;

    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 1u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, retainedMethodDefCount);

    TEST_ASSERT_FALSE(backend_aot_c_zrp_member_token_remap_build(&metadata,
                                                                 methodDefs,
                                                                 1u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 &functionTable,
                                                                 retainedMethodDefCount,
                                                                 0u));
    TEST_ASSERT_NULL(metadata.memberTokenRemapEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.memberTokenRemapCount);
    TEST_ASSERT_NULL(metadata.ownedMemberTokenRemapEntries);
}

static void test_aot_c_zrp_metadata_export_token_sidecar_rejects_zero_rid_source_tokens(void) {
    SZrZrpMetadataMethodDefRow methodDefs[1];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata metadata;
    TZrUInt32 retainedMethodDefCount;
    const TZrMetadataToken invalidSourceToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 0u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(&metadata, 0, sizeof(metadata));
    methodDefs[0].token = invalidSourceToken;
    methodDefs[0].functionIndex = 0u;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 0u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 1u;

    retainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 1u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, retainedMethodDefCount);

    TEST_ASSERT_FALSE(backend_aot_c_zrp_member_token_remap_build(&metadata,
                                                                 methodDefs,
                                                                 1u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 ZR_NULL,
                                                                 0u,
                                                                 &functionTable,
                                                                 retainedMethodDefCount,
                                                                 0u));
    TEST_ASSERT_NULL(metadata.memberTokenRemapEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.memberTokenRemapCount);
    TEST_ASSERT_NULL(metadata.ownedMemberTokenRemapEntries);
}

static void test_aot_c_zrp_metadata_export_token_sidecar_rejects_retained_method_count_drift(void) {
    SZrZrpMetadataMethodDefRow methodDefs[2];
    SZrZrpMetadataFieldDefRow fieldDefs[1];
    SZrAotFunctionEntry retainedEntry;
    SZrAotFunctionTable functionTable;
    SZrAotCEmbeddedZrpMetadata metadata;
    TZrUInt32 actualRetainedMethodDefCount;
    const TZrMetadataToken removedMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 1u);
    const TZrMetadataToken keptMethodToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 2u);
    const TZrMetadataToken fieldToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 3u);

    memset(methodDefs, 0, sizeof(methodDefs));
    memset(fieldDefs, 0, sizeof(fieldDefs));
    memset(&metadata, 0, sizeof(metadata));
    methodDefs[0].token = removedMethodToken;
    methodDefs[0].functionIndex = 0u;
    methodDefs[1].token = keptMethodToken;
    methodDefs[1].functionIndex = 1u;
    fieldDefs[0].token = fieldToken;

    retainedEntry.function = ZR_NULL;
    retainedEntry.flatIndex = 1u;
    functionTable.entries = &retainedEntry;
    functionTable.count = 1u;
    functionTable.capacity = 1u;
    functionTable.indexSpace = 2u;

    actualRetainedMethodDefCount =
            backend_aot_c_zrp_count_retained_method_defs(methodDefs, 2u, &functionTable);
    TEST_ASSERT_EQUAL_UINT32(1u, actualRetainedMethodDefCount);

    TEST_ASSERT_FALSE(backend_aot_c_zrp_member_token_remap_build(&metadata,
                                                                  methodDefs,
                                                                  2u,
                                                                  fieldDefs,
                                                                  1u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  ZR_NULL,
                                                                  0u,
                                                                  &functionTable,
                                                                  actualRetainedMethodDefCount + 1u,
                                                                  0u));
    TEST_ASSERT_NULL(metadata.memberTokenRemapEntries);
    TEST_ASSERT_EQUAL_UINT32(0u, metadata.memberTokenRemapCount);
    TEST_ASSERT_NULL(metadata.ownedMemberTokenRemapEntries);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_export_token_remap_compacts_retained_method_export_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_remap_compacts_field_export_tokens_after_methods);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_remap_rejects_retained_method_count_drift);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_sidecar_publishes_compacted_member_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_manifest_export_table_publishes_remapped_member_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_manifest_export_table_rejects_kind_token_mismatch);
    RUN_TEST(test_aot_c_zrp_metadata_manifest_export_table_rejects_duplicate_kind_target);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_sidecar_rejects_duplicate_source_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_sidecar_rejects_non_member_source_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_sidecar_rejects_zero_rid_source_tokens);
    RUN_TEST(test_aot_c_zrp_metadata_export_token_sidecar_rejects_retained_method_count_drift);
    return UNITY_END();
}
