#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/legacy_migration.h"
#include "zr_vm_parser/test_contract.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static const SZrLegacyMigrationItem *find_item(
        const SZrLegacyMigrationPlan *plan,
        const TZrChar *kind) {
    for (TZrSize index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get(
                        (SZrArray *)&plan->items, index);
        const TZrChar *itemKind = item != ZR_NULL && item->oldConstructKind != ZR_NULL
                                  ? ZrCore_String_GetNativeString(item->oldConstructKind)
                                  : ZR_NULL;
        if (itemKind != ZR_NULL && strcmp(itemKind, kind) == 0) {
            return item;
        }
    }
    return ZR_NULL;
}

static void test_percent_test_becomes_typed_ordinary_function(void) {
    static const TZrChar source[] =
            "%test(\"parses empty-input\") {\n"
            "    let observed: int = 1;\n"
            "}\n";
    static const TZrChar expected[] =
            "#zr.testing.test#\n"
            "fn testParsesEmptyInput(): void {\n"
            "    let observed: int = 1;\n"
            "}\n";
    SZrString *sourceName =
            ZrCore_String_CreateFromNative(g_state, "percent_test_migration.zr");
    SZrLegacyMigrationPlan plan = {0};
    SZrLegacyMigrationPlan second = {0};
    const SZrLegacyMigrationItem *item;
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;
    SZrFunction *function;
    SZrParserTestManifest manifest;

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, source, strlen(source), sourceName, &plan));
    item = find_item(&plan, "percentTest");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL(ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE, item->applicability);
    TEST_ASSERT_TRUE(item->hasFix);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            source,
            strlen(source),
            &migrated,
            &migratedLength));
    TEST_ASSERT_EQUAL_STRING(expected, migrated);

    function = ZrParser_Source_CompileTest(
            g_state, migrated, migratedLength, sourceName);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrParser_TestManifest_Decode(
            g_state,
            function->testManifestData,
            function->testManifestDataLength,
            &manifest));
    TEST_ASSERT_EQUAL_UINT32(1U, manifest.entryCount);
    TEST_ASSERT_EQUAL_STRING(
            "testParsesEmptyInput", manifest.entries[0].qualifiedName);
    ZrParser_TestManifest_Free(g_state, &manifest);
    ZrCore_Function_Free(g_state, function);

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, migrated, migratedLength, sourceName, &second));
    TEST_ASSERT_EQUAL_UINT32(0U, second.items.length);

    ZrParser_LegacyMigration_PlanFree(g_state, &second);
    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_return_convention_requires_review_and_is_not_applied(void) {
    static const TZrChar source[] =
            "%test(\"legacy result\") { return 0; }\n"
            "test fn draft(): void {}\n";
    static const TZrChar expected[] =
            "%test(\"legacy result\") { return 0; }\n"
            "#zr.testing.test# fn draft(): void {}\n";
    SZrString *sourceName =
            ZrCore_String_CreateFromNative(g_state, "percent_test_return_review.zr");
    SZrLegacyMigrationPlan plan = {0};
    const SZrLegacyMigrationItem *percentItem;
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, source, strlen(source), sourceName, &plan));
    percentItem = find_item(&plan, "percentTest");
    TEST_ASSERT_NOT_NULL(percentItem);
    TEST_ASSERT_EQUAL(
            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, percentItem->applicability);
    TEST_ASSERT_FALSE(percentItem->hasFix);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            source,
            strlen(source),
            &migrated,
            &migratedLength));
    TEST_ASSERT_EQUAL_STRING(expected, migrated);

    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_draft_test_functions_and_attributes_migrate_idempotently(void) {
    static const TZrChar source[] =
            "test fn drafted(): void {}\n"
            "test async fn asynchronous(): Task<void> {}\n"
            "#zr.test.test#\n"
            "#zr.test.case(1)#\n"
            "#zr.test.skip(reason: \"later\")#\n"
            "fn parameterized(value: int): void {}\n";
    static const TZrChar expected[] =
            "#zr.testing.test# fn drafted(): void {}\n"
            "#zr.testing.test# async fn asynchronous(): Task<void> {}\n"
            "#zr.testing.test#\n"
            "#zr.testing.case(1)#\n"
            "#zr.testing.skip(reason: \"later\")#\n"
            "fn parameterized(value: int): void {}\n";
    SZrString *sourceName =
            ZrCore_String_CreateFromNative(g_state, "draft_test_migration.zr");
    SZrLegacyMigrationPlan plan = {0};
    SZrLegacyMigrationPlan second = {0};
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, source, strlen(source), sourceName, &plan));
    TEST_ASSERT_NOT_NULL(find_item(&plan, "legacyTestFunctionKeyword"));
    TEST_ASSERT_NOT_NULL(find_item(&plan, "legacyTestAttribute"));
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            source,
            strlen(source),
            &migrated,
            &migratedLength));
    TEST_ASSERT_EQUAL_STRING(expected, migrated);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, migrated, migratedLength, sourceName, &second));
    TEST_ASSERT_EQUAL_UINT32(0U, second.items.length);

    ZrParser_LegacyMigration_PlanFree(g_state, &second);
    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_generated_identifier_collision_never_auto_applies(void) {
    static const TZrChar source[] =
            "fn testCollision(): void {}\n"
            "%test(\"collision\") {}\n";
    SZrString *sourceName =
            ZrCore_String_CreateFromNative(g_state, "percent_test_collision.zr");
    SZrLegacyMigrationPlan plan = {0};
    const SZrLegacyMigrationItem *item;
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, source, strlen(source), sourceName, &plan));
    item = find_item(&plan, "percentTest");
    TEST_ASSERT_NOT_NULL(item);
    TEST_ASSERT_EQUAL(ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, item->applicability);
    TEST_ASSERT_TRUE(item->hasFix);
    TEST_ASSERT_EQUAL(
            ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT, item->fix.applicability);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            source,
            strlen(source),
            &migrated,
            &migratedLength));
    TEST_ASSERT_EQUAL_STRING(source, migrated);

    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_percent_test_becomes_typed_ordinary_function);
    RUN_TEST(test_return_convention_requires_review_and_is_not_applied);
    RUN_TEST(test_draft_test_functions_and_attributes_migrate_idempotently);
    RUN_TEST(test_generated_identifier_collision_never_auto_applies);
    return UNITY_END();
}
