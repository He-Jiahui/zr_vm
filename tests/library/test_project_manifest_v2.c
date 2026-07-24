#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"

void setUp(void) {
}

void tearDown(void) {
}

static const TZrChar *test_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }
    return ZrCore_String_GetNativeString(value);
}

static SZrLibrary_Project *new_project(SZrState *state, const TZrChar *manifest) {
    return ZrLibrary_Project_New(state,
                                 (TZrNativeString)manifest,
                                 "E:/repo/manifest-v2/manifest.zrp");
}

static void test_project_manifest_v2_reads_required_base_envelope(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    static const TZrChar manifest[] =
            "{"
            "\"manifestVersion\":2,"
            "\"name\":\"physics\","
            "\"version\":\"1.0.0\","
            "\"kind\":\"library\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"index\""
            "}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(2u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("physics", test_string_text(project->name));
    TEST_ASSERT_EQUAL_STRING("1.0.0", test_string_text(project->version));
    TEST_ASSERT_EQUAL_STRING("src", test_string_text(project->source));
    TEST_ASSERT_EQUAL_STRING("bin", test_string_text(project->binary));
    TEST_ASSERT_EQUAL_STRING("index", test_string_text(project->entry));

    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v2_rejects_incomplete_or_unsupported_base_envelopes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    static const TZrChar *missingRequiredFields[] = {
            "{\"manifestVersion\":2,\"version\":\"1.0.0\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"kind\":\"library\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"binary\":\"bin\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"entry\":\"index\"}",
            "{\"manifestVersion\":2,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\"}"
    };
    static const TZrChar fractionalVersion[] =
            "{\"manifestVersion\":2.5,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}";
    static const TZrChar futureVersion[] =
            "{\"manifestVersion\":3,\"name\":\"physics\",\"version\":\"1.0.0\","
            "\"kind\":\"library\",\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"index\"}";

    TEST_ASSERT_NOT_NULL(state);
    for (TZrSize index = 0u; index < ZR_ARRAY_COUNT(missingRequiredFields); index++) {
        TEST_ASSERT_NULL(new_project(state, missingRequiredFields[index]));
    }
    TEST_ASSERT_NULL(new_project(state, fractionalVersion));
    TEST_ASSERT_NULL(new_project(state, futureVersion));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_project_manifest_v1_remains_an_explicit_migration_input(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrLibrary_Project *project;
    static const TZrChar manifest[] =
            "{\"manifestVersion\":1,\"name\":\"legacy\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";
    static const TZrChar missingVersionManifest[] =
            "{\"name\":\"legacy-default\",\"version\":\"1.0.0\","
            "\"source\":\"src\",\"binary\":\"bin\",\"entry\":\"main\"}";

    TEST_ASSERT_NOT_NULL(state);
    project = new_project(state, manifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("legacy", test_string_text(project->name));
    ZrLibrary_Project_Free(state, project);

    project = new_project(state, missingVersionManifest);
    TEST_ASSERT_NOT_NULL(project);
    TEST_ASSERT_EQUAL_UINT32(1u, project->manifestVersion);
    TEST_ASSERT_EQUAL_STRING("legacy-default", test_string_text(project->name));
    ZrLibrary_Project_Free(state, project);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_manifest_v2_reads_required_base_envelope);
    RUN_TEST(test_project_manifest_v2_rejects_incomplete_or_unsupported_base_envelopes);
    RUN_TEST(test_project_manifest_v1_remains_an_explicit_migration_input);

    return UNITY_END();
}
