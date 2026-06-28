#include "unity.h"

#include "backend_aot_c_zrp_metadata_size.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void assert_text_contains(const char *text, const char *needle) {
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(needle);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(text, needle), needle);
}

static void assert_zrp_metadata_section_delta(const char *text,
                                              const char *sectionName,
                                              unsigned long long bytesBefore,
                                              unsigned long long bytesAfter,
                                              unsigned long long bytesRemoved) {
    char expected[160];

    snprintf(expected,
             sizeof(expected),
             "/* code_stripping.zrpMetadataSectionBytes.%sBefore = %llu */",
             sectionName,
             bytesBefore);
    assert_text_contains(text, expected);

    snprintf(expected,
             sizeof(expected),
             "/* code_stripping.zrpMetadataSectionBytes.%sAfter = %llu */",
             sectionName,
             bytesAfter);
    assert_text_contains(text, expected);

    snprintf(expected,
             sizeof(expected),
             "/* code_stripping.zrpMetadataSectionBytes.%sRemoved = %llu */",
             sectionName,
             bytesRemoved);
    assert_text_contains(text, expected);
}

static void assert_zrp_metadata_section_count_delta(const char *text,
                                                    const char *sectionName,
                                                    unsigned long long countBefore,
                                                    unsigned long long countAfter,
                                                    unsigned long long countRemoved) {
    char expected[160];

    snprintf(expected,
             sizeof(expected),
             "/* code_stripping.zrpMetadataSectionCounts.%sBefore = %llu */",
             sectionName,
             countBefore);
    assert_text_contains(text, expected);

    snprintf(expected,
             sizeof(expected),
             "/* code_stripping.zrpMetadataSectionCounts.%sAfter = %llu */",
             sectionName,
             countAfter);
    assert_text_contains(text, expected);

    snprintf(expected,
             sizeof(expected),
             "/* code_stripping.zrpMetadataSectionCounts.%sRemoved = %llu */",
             sectionName,
             countRemoved);
    assert_text_contains(text, expected);
}

static void assert_zrp_metadata_section_count_stat(const char *text,
                                                   const char *sectionName,
                                                   unsigned long long count) {
    char expected[128];

    snprintf(expected,
             sizeof(expected),
             "/* aot_size.zrpMetadataSectionCounts.%s = %llu */",
             sectionName,
             count);
    assert_text_contains(text, expected);
}

static void test_aot_c_zrp_metadata_size_deltas_emit_section_level_code_stripping_markers(void) {
    SZrAotZrpMetadataSizeStats beforeStats;
    SZrAotZrpMetadataSizeStats afterStats;
    FILE *file;
    char text[8192];
    size_t bytesRead;

    memset(&beforeStats, 0, sizeof(beforeStats));
    memset(&afterStats, 0, sizeof(afterStats));

    beforeStats.zrpMetadataBytes = 240u;
    beforeStats.tokenRecordBytes = 24u;
    beforeStats.typeDefBytes = 10u;
    beforeStats.methodDefBytes = 40u;
    beforeStats.fieldDefBytes = 8u;
    beforeStats.genericParamBytes = 6u;
    beforeStats.genericParamConstraintBytes = 12u;
    beforeStats.typeSpecBytes = 16u;
    beforeStats.methodSpecBytes = 20u;
    beforeStats.moduleRefBytes = 9u;
    beforeStats.stringPoolBytes = 25u;
    beforeStats.signatureBlobPoolBytes = 30u;
    beforeStats.constantPoolBytes = 11u;
    beforeStats.definitionTableBytes = 121u;
    beforeStats.poolBytes = 66u;
    beforeStats.tokenRecordCount = 6u;
    beforeStats.typeDefCount = 1u;
    beforeStats.methodDefCount = 4u;
    beforeStats.fieldDefCount = 2u;
    beforeStats.genericParamCount = 3u;
    beforeStats.genericParamConstraintCount = 4u;
    beforeStats.typeSpecCount = 2u;
    beforeStats.methodSpecCount = 5u;
    beforeStats.moduleRefCount = 1u;
    beforeStats.stringPoolCount = 25u;
    beforeStats.signatureBlobPoolCount = 30u;
    beforeStats.constantPoolCount = 11u;

    afterStats.zrpMetadataBytes = 151u;
    afterStats.tokenRecordBytes = 10u;
    afterStats.typeDefBytes = 10u;
    afterStats.methodDefBytes = 10u;
    afterStats.fieldDefBytes = 8u;
    afterStats.genericParamBytes = 0u;
    afterStats.genericParamConstraintBytes = 4u;
    afterStats.typeSpecBytes = 16u;
    afterStats.methodSpecBytes = 12u;
    afterStats.moduleRefBytes = 9u;
    afterStats.stringPoolBytes = 15u;
    afterStats.signatureBlobPoolBytes = 22u;
    afterStats.constantPoolBytes = 0u;
    afterStats.definitionTableBytes = 69u;
    afterStats.poolBytes = 37u;
    afterStats.tokenRecordCount = 3u;
    afterStats.typeDefCount = 1u;
    afterStats.methodDefCount = 1u;
    afterStats.fieldDefCount = 2u;
    afterStats.genericParamCount = 0u;
    afterStats.genericParamConstraintCount = 1u;
    afterStats.typeSpecCount = 2u;
    afterStats.methodSpecCount = 3u;
    afterStats.moduleRefCount = 1u;
    afterStats.stringPoolCount = 15u;
    afterStats.signatureBlobPoolCount = 22u;
    afterStats.constantPoolCount = 0u;

    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    backend_aot_write_code_stripping_zrp_metadata_size_deltas(file, &beforeStats, &afterStats);
    rewind(file);
    bytesRead = fread(text, 1u, sizeof(text) - 1u, file);
    TEST_ASSERT_LESS_THAN(sizeof(text) - 1u, bytesRead);
    text[bytesRead] = '\0';
    fclose(file);

    assert_text_contains(text, "/* code_stripping.zrpMetadataBytesRemoved = 89 */");
    assert_text_contains(text, "/* code_stripping.zrpMetadataDefinitionTableBytesRemoved = 52 */");
    assert_text_contains(text, "/* code_stripping.zrpMetadataPoolBytesRemoved = 29 */");

    assert_zrp_metadata_section_delta(text, "tokenRecords", 24u, 10u, 14u);
    assert_zrp_metadata_section_delta(text, "typeDefs", 10u, 10u, 0u);
    assert_zrp_metadata_section_delta(text, "methodDefs", 40u, 10u, 30u);
    assert_zrp_metadata_section_delta(text, "fieldDefs", 8u, 8u, 0u);
    assert_zrp_metadata_section_delta(text, "genericParams", 6u, 0u, 6u);
    assert_zrp_metadata_section_delta(text, "genericParamConstraints", 12u, 4u, 8u);
    assert_zrp_metadata_section_delta(text, "typeSpecs", 16u, 16u, 0u);
    assert_zrp_metadata_section_delta(text, "methodSpecs", 20u, 12u, 8u);
    assert_zrp_metadata_section_delta(text, "moduleRefs", 9u, 9u, 0u);
    assert_zrp_metadata_section_delta(text, "stringPool", 25u, 15u, 10u);
    assert_zrp_metadata_section_delta(text, "signatureBlobPool", 30u, 22u, 8u);
    assert_zrp_metadata_section_delta(text, "constantPool", 11u, 0u, 11u);

    assert_zrp_metadata_section_count_delta(text, "tokenRecords", 6u, 3u, 3u);
    assert_zrp_metadata_section_count_delta(text, "typeDefs", 1u, 1u, 0u);
    assert_zrp_metadata_section_count_delta(text, "methodDefs", 4u, 1u, 3u);
    assert_zrp_metadata_section_count_delta(text, "fieldDefs", 2u, 2u, 0u);
    assert_zrp_metadata_section_count_delta(text, "genericParams", 3u, 0u, 3u);
    assert_zrp_metadata_section_count_delta(text, "genericParamConstraints", 4u, 1u, 3u);
    assert_zrp_metadata_section_count_delta(text, "typeSpecs", 2u, 2u, 0u);
    assert_zrp_metadata_section_count_delta(text, "methodSpecs", 5u, 3u, 2u);
    assert_zrp_metadata_section_count_delta(text, "moduleRefs", 1u, 1u, 0u);
    assert_zrp_metadata_section_count_delta(text, "stringPool", 25u, 15u, 10u);
    assert_zrp_metadata_section_count_delta(text, "signatureBlobPool", 30u, 22u, 8u);
    assert_zrp_metadata_section_count_delta(text, "constantPool", 11u, 0u, 11u);
}

static void test_aot_c_zrp_metadata_size_stats_emit_section_count_markers(void) {
    SZrAotZrpMetadataSizeStats stats;
    FILE *file;
    char text[8192];
    size_t bytesRead;

    memset(&stats, 0, sizeof(stats));
    stats.tokenRecordCount = 6u;
    stats.typeDefCount = 1u;
    stats.methodDefCount = 4u;
    stats.fieldDefCount = 2u;
    stats.genericParamCount = 3u;
    stats.genericParamConstraintCount = 4u;
    stats.typeSpecCount = 2u;
    stats.methodSpecCount = 5u;
    stats.moduleRefCount = 1u;
    stats.stringPoolCount = 25u;
    stats.signatureBlobPoolCount = 30u;
    stats.constantPoolCount = 11u;

    file = tmpfile();
    TEST_ASSERT_NOT_NULL(file);
    backend_aot_write_zrp_metadata_size_stats(file, &stats);
    rewind(file);
    bytesRead = fread(text, 1u, sizeof(text) - 1u, file);
    TEST_ASSERT_LESS_THAN(sizeof(text) - 1u, bytesRead);
    text[bytesRead] = '\0';
    fclose(file);

    assert_zrp_metadata_section_count_stat(text, "tokenRecords", 6u);
    assert_zrp_metadata_section_count_stat(text, "typeDefs", 1u);
    assert_zrp_metadata_section_count_stat(text, "methodDefs", 4u);
    assert_zrp_metadata_section_count_stat(text, "fieldDefs", 2u);
    assert_zrp_metadata_section_count_stat(text, "genericParams", 3u);
    assert_zrp_metadata_section_count_stat(text, "genericParamConstraints", 4u);
    assert_zrp_metadata_section_count_stat(text, "typeSpecs", 2u);
    assert_zrp_metadata_section_count_stat(text, "methodSpecs", 5u);
    assert_zrp_metadata_section_count_stat(text, "moduleRefs", 1u);
    assert_zrp_metadata_section_count_stat(text, "stringPool", 25u);
    assert_zrp_metadata_section_count_stat(text, "signatureBlobPool", 30u);
    assert_zrp_metadata_section_count_stat(text, "constantPool", 11u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_zrp_metadata_size_deltas_emit_section_level_code_stripping_markers);
    RUN_TEST(test_aot_c_zrp_metadata_size_stats_emit_section_count_markers);
    return UNITY_END();
}
