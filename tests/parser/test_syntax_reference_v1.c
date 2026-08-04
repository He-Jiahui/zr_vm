#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "test_support.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/writer.h"

void setUp(void) {}

void tearDown(void) {}

const ZrLibModuleDescriptor *ZrVm_GetSyntaxReferenceRenderModule_v1(void);

static const TZrChar *const kStableFeatureIds[] = {
        "function.definition.colon",
        "callable.type.arrow",
        "anonymous.function.expression_arrow",
        "reference.passing",
        "construct.init_new_own_call",
        "struct.ref_struct",
        "span.continuous_view",
        "receiver.effect",
        "resource.owner_lifetime",
        "gc.gcbox_bridge",
        "property.unified",
        "enum.union.generic_array_tuple",
        "lexical.literal_destructuring",
        "module.import_native_ffi",
        "semicolon.explicit",
        "module_namespace.import_binding",
        "reflection.type_and_construction",
        "pooling.handle_and_ref",
        "comptime.metadata_transform",
        "conditional.direct_call",
        "async.task_job_scheduler",
        "iterator.enumerator_yield",
        "testing.manifest_and_case",
        "module_specifier.package_artifact",
        "module_domain.provider_locator",
        "official_native_provider_contract",
        "canonical.callable_ffi_contract",
        "control_flow.cleanup",
        "legacy.percent_migration",
};

typedef struct SZrSyntaxReferenceFeatureMapping {
    const TZrChar *feature;
    const TZrChar *status;
    const TZrChar *collection;
    const TZrChar *source;
} SZrSyntaxReferenceFeatureMapping;

typedef struct SZrSyntaxReferenceCurrentEvidence {
    const TZrChar *source;
    const TZrChar *syntaxMarker;
} SZrSyntaxReferenceCurrentEvidence;

static const SZrSyntaxReferenceFeatureMapping kFeatureMappings[] = {
        {"function.definition.colon", "current", "current", "src/host.zr"},
        {"callable.type.arrow", "current", "current", "src/callables.zr"},
        {"anonymous.function.expression_arrow", "current", "current", "src/anonymous_expression_arrow.zr"},
        {"reference.passing", "current", "current", "src/algorithms.zr"},
        {"construct.init_new_own_call", "current", "current", "src/model.zr"},
        {"struct.ref_struct", "current", "current", "src/model.zr"},
        {"span.continuous_view", "current", "current", "src/model.zr"},
        {"receiver.effect", "current", "current", "src/object_model.zr"},
        {"resource.owner_lifetime", "current", "current", "src/ownership.zr"},
        {"gc.gcbox_bridge", "current", "current", "src/ownership.zr"},
        {"property.unified", "current", "current", "src/object_model.zr"},
        {"enum.union.generic_array_tuple", "current", "current", "src/model.zr"},
        {"lexical.literal_destructuring", "current", "current", "surface/lexical_and_literals.zr"},
        {"control_flow.cleanup", "current", "current", "src/ownership.zr"},
        {"legacy.percent_migration", "negative", "negative", "negative/legacy_percent_surface.zr"},
        {"semicolon.explicit", "current", "current", "src/semicolon_explicit.zr"},
        {"module.import_native_ffi", "current", "current", "src/native_ffi.zr"},
        {"module_namespace.import_binding", "current", "current", "src/modules.zr"},
        {"reflection.type_and_construction", "current", "current", "src/reflection.zr"},
        {"pooling.handle_and_ref", "current", "current", "src/pooling.zr"},
        {"comptime.metadata_transform", "current", "current", "src/compile_time_and_attributes.zr"},
        {"conditional.direct_call", "current", "current", "src/compile_time_and_attributes.zr"},
        {"async.task_job_scheduler", "current", "current", "src/async_jobs.zr"},
        {"iterator.enumerator_yield", "current", "current", "src/iterators.zr"},
        {"testing.manifest_and_case", "current", "current", "tests/syntax_tests.zr"},
        {"module_specifier.package_artifact", "current", "current", "src/modules.zr"},
        {"module_domain.provider_locator", "current", "current", "generated/file_locator_import.zr"},
        {"official_native_provider_contract", "current", "current", "native/syntax_reference_native.c"},
        {"canonical.callable_ffi_contract", "current", "current", "src/native_ffi.zr"},
};

static const TZrChar *const kCurrentCollectionFiles[] = {
        "src/host.zr",
        "src/host.min.zr",
        "src/callables.zr",
        "src/model.zr",
        "src/object_model.zr",
        "src/algorithms.zr",
        "src/effects.zr",
        "src/compile_time_and_attributes.zr",
        "src/ownership.zr",
        "src/main.zr",
        "surface/lexical_and_literals.zr",
        "src/semicolon_explicit.zr",
        "src/anonymous_expression_arrow.zr",
        "src/reflection.zr",
        "src/pooling.zr",
        "src/modules.zr",
        "src/native_ffi.zr",
        "src/engine/render.zr",
        "src/async_jobs.zr",
        "src/iterators.zr",
        "generated/file_locator_import.zr",
        "native/syntax_reference_native.c",
        "packages/fixturedep/fixturedep.zrp",
        "artifacts/fixturedep.zrm",
        "tests/syntax_tests.zr",
};

static const TZrChar *const kNegativeCollectionFiles[] = {
        "negative/function_delimiters.zr",
        "negative/legacy_percent_surface.zr",
};

static const SZrSyntaxReferenceCurrentEvidence kCurrentEvidence[] = {
        {"src/host.zr", "pub fn syntaxReferenceHost(): int"},
        {"src/callables.zr", "fn(int) -> int"},
        {"src/algorithms.zr", "in int"},
        {"src/model.zr", "init SyntaxReferencePoint"},
        {"src/model.zr", "ref struct SyntaxReferenceView"},
        {"src/model.zr", "Span<int>"},
        {"src/object_model.zr", "pub const fn read"},
        {"src/ownership.zr", "resource class SyntaxReferenceOwned"},
        {"src/ownership.zr", "owner.intoGc()"},
        {"src/object_model.zr", "pub property value"},
        {"src/model.zr", "union SyntaxReferenceChoice<T>"},
        {"surface/lexical_and_literals.zr", "var [first, second]"},
        {"src/ownership.zr", "var owner: Unique<SyntaxReferenceOwned>"},
        {"src/compile_time_and_attributes.zr", "#zr.compile.declarationTransform#"},
        {"src/compile_time_and_attributes.zr", "declaration.GeneratedField"},
        {"src/compile_time_and_attributes.zr", "#zr.compile.conditional(\"trace\")#"},
        {"src/anonymous_expression_arrow.zr", "var syntaxReferenceAnonymous = fn(value: int): int => value;"},
        {"src/semicolon_explicit.zr", "var syntaxReferenceSemicolon = 1;"},
        {"src/reflection.zr", "reflection.requireConstructible"},
        {"src/pooling.zr", "pooling.PoolHandle"},
        {"src/modules.zr", "let fixturePackage = import(\"@fixturedep\");"},
        {"src/native_ffi.zr", "native extern(\"syntax_reference_native\")"},
        {"src/async_jobs.zr", "async fn syntaxReferenceAsync"},
        {"src/iterators.zr", "yield value;"},
        {"tests/syntax_tests.zr", "#zr.testing.test#"},
        {"generated/file_locator_import.zr", "file:${SYNTAX_REFERENCE_FILE_URI}"},
        {"native/syntax_reference_native.c", "ZrVm_GetSyntaxReferenceRenderModule_v1"},
};

static TZrChar *read_syntax_reference_project_file(const TZrChar *relativePath, TZrSize *outLength) {
    TZrChar path[ZR_TESTS_PATH_MAX];

    if (!ZrTests_Path_GetProjectFile("syntax_reference_v1", relativePath, path, sizeof(path))) {
        return ZR_NULL;
    }

    return ZrTests_ReadTextFile(path, outLength);
}

static TZrBool syntax_reference_format_local_file_uri(const TZrChar *path,
                                                       TZrChar *outUri,
                                                       TZrSize outUriSize) {
    TZrChar normalizedPath[ZR_TESTS_PATH_MAX];
    TZrSize index;
    int written;

    if (path == ZR_NULL || outUri == ZR_NULL || outUriSize == 0u || strlen(path) >= sizeof(normalizedPath)) {
        return ZR_FALSE;
    }
    strcpy(normalizedPath, path);
    for (index = 0u; normalizedPath[index] != '\0'; index++) {
        if (normalizedPath[index] == '\\') {
            normalizedPath[index] = '/';
        }
    }
    if (normalizedPath[0] == '/' && normalizedPath[1] == '/' &&
        normalizedPath[2] != '\0' && normalizedPath[2] != '/') {
        written = snprintf(outUri, outUriSize, "file:%s", normalizedPath);
    } else if (normalizedPath[0] == '/') {
        written = snprintf(outUri, outUriSize, "file://%s", normalizedPath);
    } else if (((normalizedPath[0] >= 'A' && normalizedPath[0] <= 'Z') ||
                (normalizedPath[0] >= 'a' && normalizedPath[0] <= 'z')) &&
               normalizedPath[1] == ':' && normalizedPath[2] == '/') {
        written = snprintf(outUri, outUriSize, "file:///%s", normalizedPath);
    } else {
        return ZR_FALSE;
    }
    return written >= 0 && (TZrSize)written < outUriSize;
}

static void assert_project_file_exists(const TZrChar *relativePath) {
    TZrChar path[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_TRUE_MESSAGE(
            ZrTests_Path_GetProjectFile("syntax_reference_v1", relativePath, path, sizeof(path)),
            relativePath);
    TEST_ASSERT_TRUE_MESSAGE(ZrTests_File_Exists(path), relativePath);
}

static const TZrChar *find_collection_files_block(const TZrChar *manifest,
                                                   const TZrChar *collection,
                                                   const TZrChar **outEnd) {
    TZrChar marker[128];
    TZrChar crlfMarker[128];
    const TZrChar *start;
    const TZrChar *end;

    TEST_ASSERT_TRUE(snprintf(marker,
                              sizeof(marker),
                              "\"collection\": \"%s\",\n      \"files\": [",
                              collection) > 0);
    start = strstr(manifest, marker);
    if (start == ZR_NULL) {
        TEST_ASSERT_TRUE(snprintf(crlfMarker,
                                  sizeof(crlfMarker),
                                  "\"collection\": \"%s\",\r\n      \"files\": [",
                                  collection) > 0);
        start = strstr(manifest, crlfMarker);
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(start, collection);
    end = strstr(start, "\n      ]");
    TEST_ASSERT_NOT_NULL_MESSAGE(end, collection);
    *outEnd = end;
    return start;
}

static TZrBool collection_block_contains_file(const TZrChar *block,
                                               const TZrChar *end,
                                               const TZrChar *relativePath) {
    TZrChar marker[ZR_TESTS_PATH_MAX];
    const TZrChar *match;

    if (snprintf(marker, sizeof(marker), "\"%s\"", relativePath) <= 0) {
        return ZR_FALSE;
    }
    match = strstr(block, marker);
    return match != ZR_NULL && match < end;
}

static TZrBool syntax_reference_path_has_suffix(
        const TZrChar *path,
        const TZrChar *suffix) {
    TZrSize pathLength = strlen(path);
    TZrSize suffixLength = strlen(suffix);

    return pathLength >= suffixLength &&
           strcmp(path + pathLength - suffixLength, suffix) == 0;
}

static TZrSize collection_block_file_count(const TZrChar *block, const TZrChar *end) {
    const TZrChar *cursor = strchr(block, '[');
    TZrSize count = 0u;

    TEST_ASSERT_NOT_NULL(cursor);
    cursor++;
    while (cursor < end) {
        const TZrChar *start = strchr(cursor, '\"');
        const TZrChar *finish;

        if (start == ZR_NULL || start >= end) {
            break;
        }
        finish = strchr(start + 1, '\"');
        TEST_ASSERT_NOT_NULL(finish);
        TEST_ASSERT_TRUE(finish < end);
        count++;
        cursor = finish + 1;
    }
    return count;
}

static void assert_collection_block_matches_expected(const TZrChar *block,
                                                     const TZrChar *end,
                                                     const TZrChar *const *expectedFiles,
                                                     TZrSize expectedCount) {
    TZrSize index;

    TEST_ASSERT_EQUAL_UINT64(expectedCount, collection_block_file_count(block, end));
    for (index = 0u; index < expectedCount; index++) {
        TEST_ASSERT_TRUE_MESSAGE(
                collection_block_contains_file(block, end, expectedFiles[index]),
                expectedFiles[index]);
    }
}

static TZrBool manifest_record_contains_marker(const TZrChar *record,
                                                const TZrChar *recordEnd,
                                                const TZrChar *marker) {
    const TZrChar *match = strstr(record, marker);

    return match != ZR_NULL && match < recordEnd;
}

static void assert_collection_files_are_disjoint(const TZrChar *const *left,
                                                 TZrSize leftCount,
                                                 const TZrChar *const *right,
                                                 TZrSize rightCount) {
    TZrSize leftIndex;
    TZrSize rightIndex;

    for (leftIndex = 0u; leftIndex < leftCount; leftIndex++) {
        for (rightIndex = 0u; rightIndex < rightCount; rightIndex++) {
            TEST_ASSERT_TRUE_MESSAGE(strcmp(left[leftIndex], right[rightIndex]) != 0,
                                     left[leftIndex]);
        }
    }
}

static void test_syntax_reference_v1_feature_mappings_match_disjoint_source_collections(void) {
    TZrSize manifestLength = 0u;
    TZrChar *manifest = read_syntax_reference_project_file("golden/coverage.json", &manifestLength);
    const TZrChar *currentEnd;
    const TZrChar *negativeEnd;
    const TZrChar *pendingEnd;
    const TZrChar *currentBlock;
    const TZrChar *negativeBlock;
    const TZrChar *pendingBlock;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(manifest);
    TEST_ASSERT_TRUE(manifestLength > 0u);
    currentBlock = find_collection_files_block(manifest, "current", &currentEnd);
    negativeBlock = find_collection_files_block(manifest, "negative", &negativeEnd);
    pendingBlock = find_collection_files_block(manifest, "design-pending", &pendingEnd);
    assert_collection_files_are_disjoint(kCurrentCollectionFiles,
                                         sizeof(kCurrentCollectionFiles) / sizeof(kCurrentCollectionFiles[0]),
                                         kNegativeCollectionFiles,
                                         sizeof(kNegativeCollectionFiles) / sizeof(kNegativeCollectionFiles[0]));
    assert_collection_block_matches_expected(currentBlock,
                                             currentEnd,
                                             kCurrentCollectionFiles,
                                             sizeof(kCurrentCollectionFiles) / sizeof(kCurrentCollectionFiles[0]));
    assert_collection_block_matches_expected(negativeBlock,
                                             negativeEnd,
                                             kNegativeCollectionFiles,
                                             sizeof(kNegativeCollectionFiles) / sizeof(kNegativeCollectionFiles[0]));
    assert_collection_block_matches_expected(pendingBlock,
                                             pendingEnd,
                                             ZR_NULL,
                                             0u);

    for (index = 0u; index < sizeof(kFeatureMappings) / sizeof(kFeatureMappings[0]); index++) {
        const SZrSyntaxReferenceFeatureMapping *mapping = &kFeatureMappings[index];
        TZrChar recordPrefix[ZR_TESTS_PATH_MAX];
        TZrChar sourceMarker[ZR_TESTS_PATH_MAX];
        const TZrChar *record;
        const TZrChar *recordEnd;
        const TZrChar *sourceField;
        const TZrChar *collectionBlock;
        const TZrChar *collectionEnd;

        TEST_ASSERT_TRUE(snprintf(recordPrefix,
                                  sizeof(recordPrefix),
                                  "{\"feature\":\"%s\",\"status\":\"%s\",\"collection\":\"%s\"",
                                  mapping->feature,
                                  mapping->status,
                                  mapping->collection) > 0);
        record = strstr(manifest, recordPrefix);
        TEST_ASSERT_NOT_NULL_MESSAGE(record, mapping->feature);
        recordEnd = strchr(record, '}');
        TEST_ASSERT_NOT_NULL_MESSAGE(recordEnd, mapping->feature);
        TEST_ASSERT_TRUE(snprintf(sourceMarker,
                                  sizeof(sourceMarker),
                                  "\"source\":\"%s\"",
                                  mapping->source) > 0);
        sourceField = strstr(record, sourceMarker);
        TEST_ASSERT_TRUE_MESSAGE(sourceField != ZR_NULL && sourceField < recordEnd, mapping->feature);
        if (strcmp(mapping->status, "design-pending") == 0) {
            TEST_ASSERT_TRUE_MESSAGE(
                    manifest_record_contains_marker(record, recordEnd, "\"ownerPlan\":\""),
                    mapping->feature);
            TEST_ASSERT_TRUE_MESSAGE(
                    manifest_record_contains_marker(record, recordEnd, "\"ownerGate\":\""),
                    mapping->feature);
            TEST_ASSERT_TRUE_MESSAGE(
                    manifest_record_contains_marker(record, recordEnd, "\"expectAfterPromotion\":\""),
                    mapping->feature);
        }
        collectionBlock = strcmp(mapping->collection, "current") == 0
                                  ? currentBlock
                                  : strcmp(mapping->collection, "negative") == 0
                                            ? negativeBlock
                                            : pendingBlock;
        collectionEnd = strcmp(mapping->collection, "current") == 0
                                ? currentEnd
                                : strcmp(mapping->collection, "negative") == 0
                                          ? negativeEnd
                                          : pendingEnd;
        TEST_ASSERT_TRUE_MESSAGE(
                collection_block_contains_file(collectionBlock, collectionEnd, mapping->source),
                mapping->feature);
        assert_project_file_exists(mapping->source);
    }

    free(manifest);
}

static void test_syntax_reference_v1_current_feature_sources_contain_syntax_evidence_and_parse(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    TZrSize index;

    TEST_ASSERT_NOT_NULL(state);
    for (index = 0u; index < sizeof(kCurrentEvidence) / sizeof(kCurrentEvidence[0]); index++) {
        TZrSize sourceLength = 0u;
        TZrChar *source = read_syntax_reference_project_file(kCurrentEvidence[index].source, &sourceLength);

        TEST_ASSERT_NOT_NULL_MESSAGE(source, kCurrentEvidence[index].source);
        TEST_ASSERT_TRUE_MESSAGE(sourceLength > 0u, kCurrentEvidence[index].source);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(source, kCurrentEvidence[index].syntaxMarker),
                                     kCurrentEvidence[index].syntaxMarker);
        free(source);
    }
    for (index = 0u; index < sizeof(kCurrentCollectionFiles) / sizeof(kCurrentCollectionFiles[0]); index++) {
        TZrSize sourceLength = 0u;
        TZrChar *source;
        SZrString *sourceName;
        SZrAstNode *ast;

        if (!syntax_reference_path_has_suffix(kCurrentCollectionFiles[index], ".zr")) {
            continue;
        }
        source = read_syntax_reference_project_file(kCurrentCollectionFiles[index], &sourceLength);
        TEST_ASSERT_NOT_NULL_MESSAGE(source, kCurrentCollectionFiles[index]);
        sourceName = ZrCore_String_Create(state,
                                          (TZrNativeString)kCurrentCollectionFiles[index],
                                          strlen(kCurrentCollectionFiles[index]));
        TEST_ASSERT_NOT_NULL(sourceName);
        ast = ZrParser_Parse(state, source, sourceLength, sourceName);
        TEST_ASSERT_NOT_NULL_MESSAGE(ast, kCurrentCollectionFiles[index]);
        ZrParser_Ast_Free(state, ast);
        free(source);
    }
    ZrTests_Runtime_State_Destroy(state);
}

static void test_syntax_reference_v1_has_no_design_pending_features(void) {
    TZrSize manifestLength = 0u;
    TZrChar *manifest = read_syntax_reference_project_file(
            "golden/coverage.json", &manifestLength);

    TEST_ASSERT_NOT_NULL(manifest);
    TEST_ASSERT_TRUE(manifestLength > 0u);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            ZrTests_Reference_CountJsonStringFieldValueOccurrences(
                    manifest, "status", "design-pending"));
    TEST_ASSERT_EQUAL_UINT64(0u, ZrTests_Reference_CountOccurrences(manifest, "\"ownerPlan\""));
    TEST_ASSERT_EQUAL_UINT64(0u, ZrTests_Reference_CountOccurrences(manifest, "\"ownerGate\""));
    TEST_ASSERT_EQUAL_UINT64(0u, ZrTests_Reference_CountOccurrences(manifest, "\"expectAfterPromotion\""));

    free(manifest);
}

static void test_syntax_reference_v1_manifest_enumerates_stable_feature_ids(void) {
    TZrSize manifestLength = 0u;
    TZrChar *manifest = read_syntax_reference_project_file("golden/coverage.json", &manifestLength);
    TZrSize index;

    TEST_ASSERT_NOT_NULL(manifest);
    TEST_ASSERT_TRUE(manifestLength > 0u);
    TEST_ASSERT_EQUAL_UINT64(1u, ZrTests_Reference_CountJsonStringFieldValueOccurrences(
                                       manifest, "schema", "syntax-reference-v1-coverage"));
    TEST_ASSERT_EQUAL_UINT64(
            sizeof(kStableFeatureIds) / sizeof(kStableFeatureIds[0]),
            ZrTests_Reference_CountOccurrences(manifest, "\"feature\""));
    for (index = 0u; index < sizeof(kStableFeatureIds) / sizeof(kStableFeatureIds[0]); index++) {
        TEST_ASSERT_EQUAL_UINT64_MESSAGE(
                1u,
                ZrTests_Reference_CountJsonStringFieldValueOccurrences(
                        manifest, "feature", kStableFeatureIds[index]),
                kStableFeatureIds[index]);
    }

    free(manifest);
}

static void test_syntax_reference_v1_separates_current_and_negative_collections(void) {
    static const TZrChar *const kRequiredProjectFiles[] = {
            "syntax_reference_v1.zrp",
            "src/host.zr",
            "src/host.min.zr",
            "src/model.zr",
            "src/main.zr",
            "surface/lexical_and_literals.zr",
            "negative/function_delimiters.zr",
            "negative/legacy_percent_surface.zr",
            "generated/file_locator_import.zr",
            "native/syntax_reference_native.c",
            "golden/coverage.json",
            "golden/diagnostics.json",
            "golden/provider-locator.json",
    };
    TZrSize manifestLength = 0u;
    TZrChar *manifest = read_syntax_reference_project_file("golden/coverage.json", &manifestLength);
    TZrSize index;

    TEST_ASSERT_NOT_NULL(manifest);
    TEST_ASSERT_TRUE(manifestLength > 0u);
    {
        const TZrChar *collectionEnd;

        TEST_ASSERT_NOT_NULL(find_collection_files_block(manifest, "current", &collectionEnd));
        TEST_ASSERT_NOT_NULL(find_collection_files_block(manifest, "negative", &collectionEnd));
        TEST_ASSERT_NOT_NULL(find_collection_files_block(manifest, "design-pending", &collectionEnd));
    }
    TEST_ASSERT_TRUE(ZrTests_Reference_CountJsonStringFieldValueOccurrences(
                             manifest, "status", "current") >= 1u);
    TEST_ASSERT_TRUE(ZrTests_Reference_CountJsonStringFieldValueOccurrences(
                             manifest, "status", "negative") >= 1u);
    TEST_ASSERT_EQUAL_UINT64(
            0u,
            ZrTests_Reference_CountJsonStringFieldValueOccurrences(
                    manifest, "status", "design-pending"));

    for (index = 0u; index < sizeof(kRequiredProjectFiles) / sizeof(kRequiredProjectFiles[0]); index++) {
        assert_project_file_exists(kRequiredProjectFiles[index]);
    }

    free(manifest);
}

static void test_syntax_reference_v1_preserves_provider_identity_without_host_paths(void) {
    static const TZrChar *const kRequiredProviderFiles[] = {
            "src/engine/render.zr",
            "packages/fixturedep/fixturedep.zrp",
            "packages/fixturedep/src/index.zr",
            "packages/fixturedep/src/tool.zr",
            "artifacts/fixturedep.zrm",
            "tests/syntax_tests.zr",
    };
    TZrSize locatorLength = 0u;
    TZrSize projectManifestLength = 0u;
    TZrChar *locator = read_syntax_reference_project_file("golden/provider-locator.json", &locatorLength);
    TZrChar *projectManifest = read_syntax_reference_project_file("syntax_reference_v1.zrp", &projectManifestLength);
    TZrChar workspaceProviderPath[ZR_TESTS_PATH_MAX];
    TZrChar localFileUri[ZR_TESTS_PATH_MAX + 16u];
    TZrChar uncFileUri[ZR_TESTS_PATH_MAX + 16u];
    TZrSize index;

    TEST_ASSERT_NOT_NULL(locator);
    TEST_ASSERT_TRUE(locatorLength > 0u);
    TEST_ASSERT_NOT_NULL(projectManifest);
    TEST_ASSERT_TRUE(projectManifestLength > 0u);
    TEST_ASSERT_NOT_NULL(strstr(projectManifest, "\"kind\": \"application\""));
    TEST_ASSERT_NOT_NULL(strstr(projectManifest, "\"entry\": \"main\""));
    TEST_ASSERT_NOT_NULL(strstr(locator, "\"moduleIdentity\": \"native:engine.render\""));
    TEST_ASSERT_NOT_NULL(strstr(locator, "\"workspaceShadow\": \"engine.render\""));
    TEST_ASSERT_NOT_NULL(strstr(locator, "file:${SYNTAX_REFERENCE_FILE_URI}"));
    TEST_ASSERT_NOT_NULL(strstr(projectManifest, "\"nativeProviders\""));
    TEST_ASSERT_NOT_NULL(strstr(projectManifest, "\"engine.render\""));
    TEST_ASSERT_NOT_NULL(strstr(projectManifest, "\"entry\": \"ZrVm_GetSyntaxReferenceRenderModule_v1\""));
    TEST_ASSERT_TRUE(ZrTests_Path_GetProjectFile("syntax_reference_v1",
                                                 "src/engine/render.zr",
                                                 workspaceProviderPath,
                                                 sizeof(workspaceProviderPath)));
    TEST_ASSERT_TRUE(syntax_reference_format_local_file_uri(workspaceProviderPath,
                                                             localFileUri,
                                                             sizeof(localFileUri)));
    TEST_ASSERT_NOT_NULL(strstr(localFileUri, "file:///"));
    TEST_ASSERT_NULL(strstr(localFileUri, "${SYNTAX_REFERENCE_FILE_URI}"));
    TEST_ASSERT_NULL(strstr(localFileUri, "\\"));
    TEST_ASSERT_TRUE(syntax_reference_format_local_file_uri("\\\\syntax-reference-host\\fixture\\render.zr",
                                                             uncFileUri,
                                                             sizeof(uncFileUri)));
    TEST_ASSERT_EQUAL_STRING("file://syntax-reference-host/fixture/render.zr", uncFileUri);
    TEST_ASSERT_NULL(strstr(locator, "C:/"));
    TEST_ASSERT_NULL(strstr(locator, "\\\\"));
    TEST_ASSERT_NULL(strstr(locator, "/mnt/"));

    for (index = 0u; index < sizeof(kRequiredProviderFiles) / sizeof(kRequiredProviderFiles[0]); index++) {
        assert_project_file_exists(kRequiredProviderFiles[index]);
    }

    free(locator);
    free(projectManifest);
}

static void test_syntax_reference_v1_entry_executes_imported_checksum(void) {
    TZrSize mainLength = 0u;
    TZrSize hostLength = 0u;
    TZrChar *mainSource = read_syntax_reference_project_file(
            "src/main.zr", &mainLength);
    TZrChar *hostSource = read_syntax_reference_project_file(
            "src/host.zr", &hostLength);

    TEST_ASSERT_NOT_NULL(mainSource);
    TEST_ASSERT_NOT_NULL(hostSource);
    TEST_ASSERT_TRUE(mainLength > 0u);
    TEST_ASSERT_TRUE(hostLength > 0u);
    TEST_ASSERT_NOT_NULL(strstr(mainSource, "let host = import(\"host\");"));
    TEST_ASSERT_NOT_NULL(strstr(mainSource, "return syntaxReferenceMain();"));
    TEST_ASSERT_NOT_NULL(strstr(hostSource, "pub fn syntaxReferenceHost(): int"));

    free(mainSource);
    free(hostSource);
}

static void test_syntax_reference_v1_native_provider_is_a_current_descriptor(void) {
    const ZrLibModuleDescriptor *descriptor =
            ZrVm_GetSyntaxReferenceRenderModule_v1();

    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_EQUAL_STRING("engine.render", descriptor->moduleName);
    TEST_ASSERT_EQUAL_INT(
            ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, descriptor->providerPhase);
    TEST_ASSERT_EQUAL_STRING(
            "engine.render:v1:value-int", descriptor->publicContractHash);
    TEST_ASSERT_EQUAL_UINT64(1u, descriptor->functionCount);
    TEST_ASSERT_NOT_NULL(descriptor->functions);
    TEST_ASSERT_EQUAL_STRING("value", descriptor->functions[0].name);
    TEST_ASSERT_EQUAL_STRING("int", descriptor->functions[0].returnTypeName);
}

static TZrUInt64 syntax_reference_hash_bytes(const TZrByte *bytes, TZrSize length) {
    TZrUInt64 hash = 1469598103934665603ULL;
    TZrSize index;

    for (index = 0u; index < length; index++) {
        hash ^= (TZrUInt64)bytes[index];
        hash *= 1099511628211ULL;
    }

    return hash;
}

static TZrUInt64 syntax_reference_hash_generated_file(const TZrChar *path) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize length = 0u;
    TZrUInt64 hash;

    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &length));
    hash = syntax_reference_hash_bytes(bytes, length);
    free(bytes);
    return hash;
}

static TZrBool syntax_reference_line_contains(const TZrChar *line,
                                              TZrSize lineLength,
                                              const TZrChar *needle) {
    TZrSize needleLength = strlen(needle);
    TZrSize index;

    if (needleLength == 0u || lineLength < needleLength) {
        return ZR_FALSE;
    }

    for (index = 0u; index + needleLength <= lineLength; index++) {
        if (memcmp(line + index, needle, needleLength) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt64 syntax_reference_hash_semantic_file(const TZrChar *path) {
    TZrChar *text;
    const TZrChar *cursor;
    TZrUInt64 hash = 1469598103934665603ULL;

    text = ZrTests_ReadTextFile(path, ZR_NULL);
    TEST_ASSERT_NOT_NULL(text);
    cursor = text;
    while (*cursor != '\0') {
        const TZrChar *lineEnd = strchr(cursor, '\n');
        TZrSize lineLength = lineEnd == ZR_NULL ? strlen(cursor) : (TZrSize)(lineEnd - cursor);

        if (!syntax_reference_line_contains(cursor, lineLength, "START_LINE:") &&
            !syntax_reference_line_contains(cursor, lineLength, "END_LINE:")) {
            hash ^= syntax_reference_hash_bytes((const TZrByte *)cursor, lineLength);
            hash *= 1099511628211ULL;
        }
        cursor = lineEnd == ZR_NULL ? cursor + lineLength : lineEnd + 1;
    }

    free(text);
    return hash;
}

static void test_syntax_reference_v1_formatted_and_minified_current_source_have_identical_ast_and_semantic_hashes(void) {
    TZrSize formattedLength = 0u;
    TZrSize minifiedLength = 0u;
    TZrChar *formatted = read_syntax_reference_project_file("src/host.zr", &formattedLength);
    TZrChar *minified = read_syntax_reference_project_file("src/host.min.zr", &minifiedLength);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *formattedAst;
    SZrAstNode *minifiedAst;
    SZrFunction *formattedFunction;
    SZrFunction *minifiedFunction;
    TZrChar formattedAstPath[ZR_TESTS_PATH_MAX];
    TZrChar minifiedAstPath[ZR_TESTS_PATH_MAX];
    TZrChar formattedSemirPath[ZR_TESTS_PATH_MAX];
    TZrChar minifiedSemirPath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(formatted);
    TEST_ASSERT_NOT_NULL(minified);
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(state,
                                      "syntax_reference_equivalence.zr",
                                      strlen("syntax_reference_equivalence.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    formattedAst = ZrParser_Parse(state, formatted, formattedLength, sourceName);
    minifiedAst = ZrParser_Parse(state, minified, minifiedLength, sourceName);
    TEST_ASSERT_NOT_NULL(formattedAst);
    TEST_ASSERT_NOT_NULL(minifiedAst);
    formattedFunction = ZrParser_Compiler_Compile(state, formattedAst);
    minifiedFunction = ZrParser_Compiler_Compile(state, minifiedAst);
    TEST_ASSERT_NOT_NULL(formattedFunction);
    TEST_ASSERT_NOT_NULL(minifiedFunction);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "syntax_reference_v1", "hashes", "formatted", ".zrs", formattedAstPath, sizeof(formattedAstPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "syntax_reference_v1", "hashes", "minified", ".zrs", minifiedAstPath, sizeof(minifiedAstPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "syntax_reference_v1", "hashes", "formatted", ".zri", formattedSemirPath, sizeof(formattedSemirPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact(
            "syntax_reference_v1", "hashes", "minified", ".zri", minifiedSemirPath, sizeof(minifiedSemirPath)));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteSyntaxTreeFile(state, formattedAst, formattedAstPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteSyntaxTreeFile(state, minifiedAst, minifiedAstPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, formattedFunction, formattedSemirPath));
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteIntermediateFile(state, minifiedFunction, minifiedSemirPath));
    TEST_ASSERT_EQUAL_UINT64(syntax_reference_hash_generated_file(formattedAstPath),
                             syntax_reference_hash_generated_file(minifiedAstPath));
    TEST_ASSERT_EQUAL_UINT64(syntax_reference_hash_semantic_file(formattedSemirPath),
                             syntax_reference_hash_semantic_file(minifiedSemirPath));

    ZrCore_Function_Free(state, minifiedFunction);
    ZrCore_Function_Free(state, formattedFunction);
    ZrParser_Ast_Free(state, minifiedAst);
    ZrParser_Ast_Free(state, formattedAst);
    ZrTests_Runtime_State_Destroy(state);
    free(minified);
    free(formatted);
}

static void syntax_reference_assert_current_source_compiles(
        const TZrChar *relativePath) {
    TZrSize sourceLength = 0u;
    TZrChar *source = read_syntax_reference_project_file(
            relativePath,
            &sourceLength);
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(sourceLength > 0u);
    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state, (TZrNativeString)relativePath, strlen(relativePath));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(state, ast);
    TEST_ASSERT_NOT_NULL_MESSAGE(function, relativePath);

    ZrCore_Function_Free(state, function);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
    free(source);
}

static void test_syntax_reference_v1_gcbox_bridge_compiles_with_its_declared_type(void) {
    syntax_reference_assert_current_source_compiles("src/ownership.zr");
}

static void test_syntax_reference_v1_callable_parameter_retains_its_exact_signature(void) {
    syntax_reference_assert_current_source_compiles("src/callables.zr");
}

static void test_syntax_reference_v1_compile_time_metadata_fixture_compiles(void) {
    syntax_reference_assert_current_source_compiles(
            "src/compile_time_and_attributes.zr");
}

static const SZrFunction *syntax_reference_find_function(
        const SZrFunction *function,
        const TZrChar *expectedName) {
    TZrUInt32 index;

    if (function == ZR_NULL || expectedName == ZR_NULL) {
        return ZR_NULL;
    }
    if (function->functionName != ZR_NULL) {
        const TZrChar *actualName = ZrCore_String_GetNativeString(function->functionName);

        if (actualName != ZR_NULL && strcmp(actualName, expectedName) == 0) {
            return function;
        }
    }
    for (index = 0u; index < function->childFunctionLength; index++) {
        const SZrFunction *match = syntax_reference_find_function(
                &function->childFunctionList[index], expectedName);

        if (match != ZR_NULL) {
            return match;
        }
    }
    return ZR_NULL;
}

static void test_compiler_assigns_distinct_canonical_symbols_to_reused_parameter_names(void) {
    const TZrChar *source =
            "pub fn echo(value: int): int { return value; }\n"
            "pub fn echo_unsigned(value: uint): uint { return value; }\n"
            "pub fn sum_values(left: int, right: int): int { return left + right; }\n"
            "pub fn sum_unsigned(left: uint, right: uint): uint { return left + right; }\n"
            "pub fn sum_three(left: int, middle: int, right: int): int { return left + middle + right; }\n";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrAstNode *ast;
    SZrFunction *script;
    const SZrFunction *sumThree;
    TZrUInt32 parameterIndex;
    TZrUInt32 otherIndex;

    TEST_ASSERT_NOT_NULL(state);
    sourceName = ZrCore_String_Create(
            state, "canonical_parameter_identity.zr", strlen("canonical_parameter_identity.zr"));
    TEST_ASSERT_NOT_NULL(sourceName);
    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    script = ZrParser_Compiler_Compile(state, ast);
    TEST_ASSERT_NOT_NULL(script);
    sumThree = syntax_reference_find_function(script, "sum_three");
    TEST_ASSERT_NOT_NULL(sumThree);
    TEST_ASSERT_EQUAL_UINT16(3u, sumThree->parameterCount);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(sumThree->parameterCount,
                                        sumThree->typedLocalBindingLength);
    TEST_ASSERT_NOT_NULL(sumThree->typedLocalBindings);

    for (parameterIndex = 0u; parameterIndex < sumThree->parameterCount; parameterIndex++) {
        const SZrFunctionTypedLocalBinding *parameter =
                &sumThree->typedLocalBindings[parameterIndex];

        TEST_ASSERT_EQUAL_UINT32(parameterIndex, parameter->stackSlot);
        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, parameter->symbolId);
        for (otherIndex = parameterIndex + 1u;
             otherIndex < sumThree->parameterCount;
             otherIndex++) {
            TEST_ASSERT_NOT_EQUAL(parameter->symbolId,
                                  sumThree->typedLocalBindings[otherIndex].symbolId);
        }
    }

    ZrCore_Function_Free(state, script);
    ZrParser_Ast_Free(state, ast);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_syntax_reference_v1_manifest_enumerates_stable_feature_ids);
    RUN_TEST(test_syntax_reference_v1_separates_current_and_negative_collections);
    RUN_TEST(test_syntax_reference_v1_feature_mappings_match_disjoint_source_collections);
    RUN_TEST(test_syntax_reference_v1_current_feature_sources_contain_syntax_evidence_and_parse);
    RUN_TEST(test_syntax_reference_v1_has_no_design_pending_features);
    RUN_TEST(test_syntax_reference_v1_preserves_provider_identity_without_host_paths);
    RUN_TEST(test_syntax_reference_v1_entry_executes_imported_checksum);
    RUN_TEST(test_syntax_reference_v1_native_provider_is_a_current_descriptor);
    RUN_TEST(test_syntax_reference_v1_formatted_and_minified_current_source_have_identical_ast_and_semantic_hashes);
    RUN_TEST(test_syntax_reference_v1_gcbox_bridge_compiles_with_its_declared_type);
    RUN_TEST(test_syntax_reference_v1_callable_parameter_retains_its_exact_signature);
    RUN_TEST(test_syntax_reference_v1_compile_time_metadata_fixture_compiles);
    RUN_TEST(test_compiler_assigns_distinct_canonical_symbols_to_reused_parameter_names);
    return UNITY_END();
}
