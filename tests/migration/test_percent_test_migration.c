#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/legacy_migration.h"
#include "zr_vm_parser/test_contract.h"

/* Unity 为每个迁移用例管理一个状态，使断言失败后仍能进入 tearDown。 */
static SZrState *g_state;

/** @brief 为迁移规划和编译器检查创建用例专属 VM。 */
void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

/** @brief 即使 Unity 因断言中断用例体，也释放该 VM。 */
void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

/** @brief 在所属迁移计划释放前，按旧语法类别定位诊断项。
 * @return 借用的计划项；PlanFree 之后调用方不得继续持有。
 */
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

/* BUG: 每个用例只在最后一次断言后释放由 RawFree 管理的 migrated 缓冲区。
 * 后续断言失败触发 Unity longjmp 会跳过释放；tearDown 只释放 VM 状态，
 * 不持有 migrated。 */
/** @brief 验证机器迁移产出带类型的测试函数，并被测试编译器写入清单。
 * 第二次规划应确认改写后的源码不再需要迁移。
 */
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
            "percent_test_migration::testParsesEmptyInput",
            manifest.entries[0].qualifiedName);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, manifest.entries[0].functionSymbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(0U, manifest.entries[0].functionTypeId);
    ZrParser_TestManifest_Free(g_state, &manifest);
    ZrCore_Function_Free(g_state, function);

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, migrated, migratedLength, sourceName, &second));
    TEST_ASSERT_EQUAL_UINT32(0U, second.items.length);

    ZrParser_LegacyMigration_PlanFree(g_state, &second);
    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

/** @brief 应用独立的安全改写，同时让旧式返回值测试继续留待人工复核。 */
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

/** @brief 验证草案测试声明和 zr.test 属性收敛到 zr.testing 名称。 */
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

/** @brief 当生成的测试名称已存在时，阻止自动修复覆盖已有函数。 */
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

/** @brief 仅改写裸 debug 导入调用，保留含相似文本的字符串和注释。 */
static void test_bare_debug_import_migrates_to_canonical_module_idempotently(void) {
    static const TZrChar source[] =
            "let debug = import(\"debug\");\n"
            "let debugWithTrivia = import /* provider */ (\n"
            "    \"debug\"\n"
            ");\n"
            "let preserved = \"import(\\\"debug\\\")\";\n"
            "// import(\"debug\")\n";
    static const TZrChar expected[] =
            "let debug = import(\"zr.debug\");\n"
            "let debugWithTrivia = import /* provider */ (\n"
            "    \"zr.debug\"\n"
            ");\n"
            "let preserved = \"import(\\\"debug\\\")\";\n"
            "// import(\"debug\")\n";
    SZrString *sourceName =
            ZrCore_String_CreateFromNative(g_state, "debug_module_migration.zr");
    SZrLegacyMigrationPlan plan = {0};
    SZrLegacyMigrationPlan second = {0};
    const SZrLegacyMigrationItem *item;
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;

    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, source, strlen(source), sourceName, &plan));
    item = find_item(&plan, "legacyDebugModuleSpecifier");
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
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, migrated, migratedLength, sourceName, &second));
    TEST_ASSERT_EQUAL_UINT32(0U, second.items.length);

    ZrParser_LegacyMigration_PlanFree(g_state, &second);
    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

/** @brief 在顶层 CMake 的 Unity 目标中运行五项迁移契约。 */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_percent_test_becomes_typed_ordinary_function);
    RUN_TEST(test_return_convention_requires_review_and_is_not_applied);
    RUN_TEST(test_draft_test_functions_and_attributes_migrate_idempotently);
    RUN_TEST(test_generated_identifier_collision_never_auto_applies);
    RUN_TEST(test_bare_debug_import_migrates_to_canonical_module_idempotently);
    return UNITY_END();
}
