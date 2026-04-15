#include "unity.h"

#include "harness/runtime_support.h"
#include "zr_vm_library/project.h"

#include <string.h>

extern TZrBool ZrLibrary_Project_ResolveImportModuleKey(const SZrLibrary_Project *project,
                                                        const TZrChar *currentModuleKey,
                                                        const TZrChar *rawSpecifier,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);
extern TZrBool ZrLibrary_Project_DeriveCurrentModuleKey(const SZrLibrary_Project *project,
                                                        const TZrChar *sourceName,
                                                        const TZrChar *explicitModuleKey,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);

void setUp(void) {}

void tearDown(void) {}

static SZrLibrary_Project *create_test_project(SZrState **outState) {
    static const TZrChar manifestText[] =
            "{"
            "\"name\":\"resolver-fixture\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"feature/app/main\","
            "\"pathAliases\":{"
            "\"@app\":\"feature/app\","
            "\"@shared\":\"common/shared\""
            "}"
            "}";
    SZrState *state;
    SZrLibrary_Project *project;

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    project = ZrLibrary_Project_New(state, manifestText, "E:/repo/resolver-fixture/resolver-fixture.zrp");
    TEST_ASSERT_NOT_NULL(project);
    *outState = state;
    return project;
}

static void destroy_test_project(SZrState *state, SZrLibrary_Project *project) {
    if (state != ZR_NULL && project != ZR_NULL) {
        ZrLibrary_Project_Free(state, project);
    }
    if (state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
    }
}

static void assert_import_resolves(const SZrLibrary_Project *project,
                                   const TZrChar *currentModuleKey,
                                   const TZrChar *rawSpecifier,
                                   const TZrChar *expectedModuleKey) {
    TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_ResolveImportModuleKey(project,
                                                             currentModuleKey,
                                                             rawSpecifier,
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING(expectedModuleKey, resolved);
}

static void assert_import_rejected(const SZrLibrary_Project *project,
                                   const TZrChar *currentModuleKey,
                                   const TZrChar *rawSpecifier) {
    TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Project_ResolveImportModuleKey(project,
                                                              currentModuleKey,
                                                              rawSpecifier,
                                                              resolved,
                                                              sizeof(resolved),
                                                              error,
                                                              sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);
}

static void test_project_import_resolver_accepts_relative_and_alias_specifiers(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_test_project(&state);

    assert_import_resolves(project, "feature/app/main", "feature/app/absolute", "feature/app/absolute");
    assert_import_resolves(project, "feature/app/main", ".helper.util", "feature/app/helper/util");
    assert_import_resolves(project, "feature/app/main", "..common.math", "feature/common/math");
    assert_import_resolves(project, "feature/app/main", "...root.entry", "root/entry");
    assert_import_resolves(project, "feature/app/main", "@app", "feature/app");
    assert_import_resolves(project, "feature/app/main", "@app.widgets.card", "feature/app/widgets/card");
    assert_import_resolves(project, "feature/app/main", "@shared.crypto.hash", "common/shared/crypto/hash");

    destroy_test_project(state, project);
}

static void test_project_import_resolver_rejects_invalid_relative_and_alias_forms(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_test_project(&state);

    assert_import_rejected(project, "feature/app/main", "./foo");
    assert_import_rejected(project, "feature/app/main", "../foo");
    assert_import_rejected(project, "feature/app/main", ".");
    assert_import_rejected(project, "feature/app/main", "..");
    assert_import_rejected(project, "feature/app/main", ".foo..bar");
    assert_import_rejected(project, "feature/app/main", "@missing");
    assert_import_rejected(project, "feature/app/main", "@app/foo");
    assert_import_rejected(project, "feature/app/main", "@app.");
    assert_import_rejected(project, "main", "..shared.tools");

    destroy_test_project(state, project);
}

static void test_project_import_resolver_derives_current_module_key_from_source_root_and_detects_mismatch(void) {
    SZrState *state = ZR_NULL;
    SZrLibrary_Project *project = create_test_project(&state);
    TZrChar resolved[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_MAX_PATH_LENGTH];

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                             "E:/repo/resolver-fixture/src/feature/app/main.zr",
                                                             ZR_NULL,
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("feature/app/main", resolved);

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                             "feature/app/helper",
                                                             ZR_NULL,
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("feature/app/helper", resolved);

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_TRUE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                             "E:/repo/resolver-fixture/src/feature/app/main.zr",
                                                             "feature/app/main",
                                                             resolved,
                                                             sizeof(resolved),
                                                             error,
                                                             sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("feature/app/main", resolved);

    memset(resolved, 0, sizeof(resolved));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                              "E:/repo/resolver-fixture/src/feature/app/main.zr",
                                                              "feature/other/main",
                                                              resolved,
                                                              sizeof(resolved),
                                                              error,
                                                              sizeof(error)));
    TEST_ASSERT_NOT_EQUAL('\0', error[0]);

    destroy_test_project(state, project);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_project_import_resolver_accepts_relative_and_alias_specifiers);
    RUN_TEST(test_project_import_resolver_rejects_invalid_relative_and_alias_forms);
    RUN_TEST(test_project_import_resolver_derives_current_module_key_from_source_root_and_detects_mismatch);

    return UNITY_END();
}
