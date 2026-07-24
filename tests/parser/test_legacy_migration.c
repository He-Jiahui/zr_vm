#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/legacy_migration.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

typedef struct SZrMigrationExpectation {
    const TZrChar *oldConstructKind;
    EZrLegacyMigrationApplicability applicability;
    const TZrChar *targetPlanId;
    TZrBool hasFix;
} TZrMigrationExpectation;

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

static const TZrChar *migration_string_text(const SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString((SZrString *)value) : ZR_NULL;
}

static const SZrLegacyMigrationItem *migration_find_item(
        const SZrLegacyMigrationPlan *plan,
        const TZrChar *oldConstructKind) {
    TZrSize index;

    if (plan == ZR_NULL || oldConstructKind == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0U; plan->items.isValid && index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get(
                        (SZrArray *)&plan->items,
                        index);
        const TZrChar *kind = item != ZR_NULL
                                      ? migration_string_text(item->oldConstructKind)
                                      : ZR_NULL;
        if (kind != ZR_NULL && strcmp(kind, oldConstructKind) == 0) {
            return item;
        }
    }
    return ZR_NULL;
}

static TZrSize migration_offset_of(const TZrChar *source, const TZrChar *needle) {
    const TZrChar *found = strstr(source, needle);

    TEST_ASSERT_NOT_NULL(found);
    return (TZrSize)(found - source);
}

static void test_legacy_migration_plan_classifies_token_aware_candidates(void) {
    const TZrChar *source =
            "%module app.tools\n"
            "%owned class FileHandle {}\n"
            "let upgraded = %upgrade(weakHandle);\n"
            "let remainder = 7 % 2;\n"
            "// %module ignored.comment\n"
            "let text = \"%release(value)\";\n"
            "%async fn delayed(): int {}\n"
            "%unknown thing;\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "legacy_migration_token_boundaries.zr",
            strlen("legacy_migration_token_boundaries.zr"));
    SZrLegacyMigrationPlan plan = {0};
    const SZrLegacyMigrationItem *moduleItem;
    const SZrLegacyMigrationItem *ownedItem;
    const SZrLegacyMigrationItem *upgradeItem;
    const SZrLegacyMigrationItem *asyncItem;
    const SZrLegacyMigrationItem *unknownItem;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &plan));
    TEST_ASSERT_TRUE(plan.items.isValid);
    TEST_ASSERT_FALSE(plan.hasOverlap);
    TEST_ASSERT_NOT_EQUAL_UINT64(0U, plan.sourceHash);
    TEST_ASSERT_EQUAL_UINT32(5U, plan.items.length);

    moduleItem = migration_find_item(&plan, "percentModule");
    ownedItem = migration_find_item(&plan, "percentOwned");
    upgradeItem = migration_find_item(&plan, "percentUpgrade");
    asyncItem = migration_find_item(&plan, "percentAsync");
    unknownItem = migration_find_item(&plan, "unrecognizedPercentDirective");

    TEST_ASSERT_NOT_NULL(moduleItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, moduleItem->applicability);
    TEST_ASSERT_FALSE(moduleItem->hasFix);
    TEST_ASSERT_EQUAL_UINT64(
            migration_offset_of(source, "%module"),
            moduleItem->range.start.offset);
    TEST_ASSERT_EQUAL_STRING("06B", migration_string_text(moduleItem->targetPlanId));

    TEST_ASSERT_NOT_NULL(ownedItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE, ownedItem->applicability);
    TEST_ASSERT_TRUE(ownedItem->hasFix);
    TEST_ASSERT_EQUAL_STRING("resource", migration_string_text(ownedItem->fix.editText));

    TEST_ASSERT_NOT_NULL(upgradeItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, upgradeItem->applicability);
    TEST_ASSERT_FALSE(upgradeItem->hasFix);

    TEST_ASSERT_NOT_NULL(asyncItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, asyncItem->applicability);
    TEST_ASSERT_FALSE(asyncItem->hasFix);
    TEST_ASSERT_EQUAL_STRING("12", migration_string_text(asyncItem->targetPlanId));

    TEST_ASSERT_NOT_NULL(unknownItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_BLOCKED, unknownItem->applicability);
    TEST_ASSERT_FALSE(unknownItem->hasFix);
    TEST_ASSERT_EQUAL_STRING("06A", migration_string_text(unknownItem->targetPlanId));

    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_legacy_migration_plan_is_repeatable_and_ignores_non_code(void) {
    const TZrChar *source =
            "let remainder = 17 % 4;\n"
            "let text = `%shared(owner)`;\n"
            "/* %owned class CommentOnly {} */\n"
            "%module app.repeatable\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "legacy_migration_repeatable.zr",
            strlen("legacy_migration_repeatable.zr"));
    SZrLegacyMigrationPlan first = {0};
    SZrLegacyMigrationPlan second = {0};

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &first));
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &second));
    TEST_ASSERT_EQUAL_UINT32(1U, first.items.length);
    TEST_ASSERT_EQUAL_UINT32(first.items.length, second.items.length);
    TEST_ASSERT_EQUAL_UINT64(first.sourceHash, second.sourceHash);
    TEST_ASSERT_FALSE(first.hasOverlap);
    TEST_ASSERT_FALSE(second.hasOverlap);
    TEST_ASSERT_EQUAL_UINT64(
            migration_offset_of(source, "%module"),
            ((const SZrLegacyMigrationItem *)ZrCore_Array_Get(&first.items, 0U))->range.start.offset);

    ZrParser_LegacyMigration_PlanFree(g_state, &second);
    ZrParser_LegacyMigration_PlanFree(g_state, &first);
}

static void test_legacy_migration_owned_requires_a_class_declaration_shell(void) {
    const TZrChar *source = "let invalid = %owned(value);\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "legacy_migration_owned_expression.zr",
            strlen("legacy_migration_owned_expression.zr"));
    SZrLegacyMigrationPlan plan = {0};
    const SZrLegacyMigrationItem *ownedItem;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &plan));
    ownedItem = migration_find_item(&plan, "percentOwned");
    TEST_ASSERT_NOT_NULL(ownedItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, ownedItem->applicability);
    TEST_ASSERT_FALSE(ownedItem->hasFix);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_legacy_migration_consumes_paired_property_producer_fix(void) {
    const TZrChar *source =
            "class Meter {\n"
            "  pub get value: int { return this.stored; }\n"
            "  pub set value(input: int) { this.stored = input; }\n"
            "}\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "legacy_migration_property_pair.zr",
            strlen("legacy_migration_property_pair.zr"));
    SZrLegacyMigrationPlan plan = {0};
    const SZrLegacyMigrationItem *propertyItem;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state, source, strlen(source), sourceName, &plan));
    propertyItem = migration_find_item(&plan, "legacyPropertyAccessor");
    TEST_ASSERT_NOT_NULL(propertyItem);
    TEST_ASSERT_EQUAL_INT(ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE, propertyItem->applicability);
    TEST_ASSERT_TRUE(propertyItem->hasFix);
    TEST_ASSERT_NOT_NULL(strstr(migration_string_text(propertyItem->fix.editText), "pub property value: int"));
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_legacy_migration_plan_covers_inventory_classification_contract(void) {
    static const TZrMigrationExpectation expectations[] = {
        {"percentModule", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "06B", ZR_FALSE},
        {"percentImport", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"percentAsync", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "12", ZR_FALSE},
        {"percentAwait", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "12", ZR_FALSE},
        {"percentExtern", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "10", ZR_FALSE},
        {"percentTest", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "14", ZR_FALSE},
        {"percentCompileTime", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "11", ZR_FALSE},
        {"percentFunc", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"percentOwned", ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE, "04", ZR_TRUE},
        {"percentRelease", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "04", ZR_FALSE},
        {"percentUpgrade", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "04", ZR_FALSE},
        {"percentWeak", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "04", ZR_FALSE},
        {"percentShared", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "04", ZR_FALSE},
        {"percentDetach", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "04", ZR_FALSE},
        {"percentUnique", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "04", ZR_FALSE},
        {"percentIn", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentRef", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentOut", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentBorrow", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentLoan", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentBorrowed", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentLoaned", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "02", ZR_FALSE},
        {"percentType", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "08", ZR_FALSE},
        {"percentUsing", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"legacyFuncKeyword", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"keywordlessFunction", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"legacyDefinitionArrow", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"legacyFunctionTypeArrow", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"legacyDollarConstruct", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "03", ZR_FALSE},
        {"legacyDynamicDollarConstruct", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"legacyBareTypeCall", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"legacyNewStruct", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "06A", ZR_FALSE},
        {"nativePrototypeFactory", ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED, "10", ZR_FALSE},
        {"legacyPropertyAccessor", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW, "05", ZR_FALSE},
        {"unrecognizedPercentDirective", ZR_LEGACY_MIGRATION_BLOCKED, "06A", ZR_FALSE},
    };
    const TZrChar *source =
            "%module migration.matrix\n"
            "%import zr.math\n"
            "%async worker(): int\n"
            "%await worker()\n"
            "%extern(\"matrix\") {}\n"
            "%test(\"legacy matrix\") {}\n"
            "%compileTime {}\n"
            "%func(int)=>int\n"
            "%owned class Handle {}\n"
            "%release(value)\n"
            "%upgrade(weakValue)\n"
            "%weak(sharedValue)\n"
            "%shared(uniqueValue)\n"
            "%detach(uniqueValue)\n"
            "%unique new Handle()\n"
            "fn transfer(%in first: int, %ref second: int, %out third: int): void {}\n"
            "let view = %borrow(owner);\n"
            "let loan = %loan(owner);\n"
            "let borrowedType: %borrowed Handle;\n"
            "let loanedType: %loaned Handle;\n"
            "let info = %type(Handle);\n"
            "%using(resource) {}\n"
            "%unknownToken legacy\n"
            "func oldStyle(value: int) -> int { return value; }\n"
            "keywordless(value: int) -> int { return value; }\n"
            "let callback: (int)=>int;\n"
            "let staticConstructed = $Point(1);\n"
            "let dynamicConstructed = $(prototype)(1);\n"
            "Point(1);\n"
            "new Point(1);\n"
            "nativeFactory(Point, 1);\n"
            "pub get value: int { return 1; }\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "legacy_migration_inventory_contract.zr",
            strlen("legacy_migration_inventory_contract.zr"));
    SZrLegacyMigrationPlan plan = {0};
    TZrSize index;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &plan));
    for (index = 0U; index < sizeof(expectations) / sizeof(expectations[0]); index++) {
        const SZrLegacyMigrationItem *item = migration_find_item(
                &plan,
                expectations[index].oldConstructKind);

        TEST_ASSERT_NOT_NULL_MESSAGE(item, expectations[index].oldConstructKind);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
                expectations[index].applicability,
                item->applicability,
                expectations[index].oldConstructKind);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(
                expectations[index].targetPlanId,
                migration_string_text(item->targetPlanId),
                expectations[index].oldConstructKind);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
                expectations[index].hasFix,
                item->hasFix,
                expectations[index].oldConstructKind);
    }

    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_legacy_migration_apply_machine_edits_is_idempotent(void) {
    const TZrChar *source =
            "%module app.current\n"
            "%owned class Handle {}\n"
            "let upgraded = %upgrade(weakHandle);\n";
    const TZrChar *expected =
            "%module app.current\n"
            "resource class Handle {}\n"
            "let upgraded = %upgrade(weakHandle);\n";
    SZrString *sourceName = ZrCore_String_Create(
            g_state,
            "legacy_migration_apply.zr",
            strlen("legacy_migration_apply.zr"));
    SZrLegacyMigrationPlan first = {0};
    SZrLegacyMigrationPlan second = {0};
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &first));
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &first,
            source,
            strlen(source),
            &migrated,
            &migratedLength));
    TEST_ASSERT_NOT_NULL(migrated);
    TEST_ASSERT_EQUAL_UINT64(strlen(expected), migratedLength);
    TEST_ASSERT_EQUAL_STRING(expected, migrated);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            migrated,
            migratedLength,
            sourceName,
            &second));
    TEST_ASSERT_EQUAL_UINT32(2U, second.items.length);
    TEST_ASSERT_FALSE(((const SZrLegacyMigrationItem *)ZrCore_Array_Get(&second.items, 0U))->hasFix);
    TEST_ASSERT_FALSE(((const SZrLegacyMigrationItem *)ZrCore_Array_Get(&second.items, 1U))->hasFix);

    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &second);
    ZrParser_LegacyMigration_PlanFree(g_state, &first);
}

static void test_legacy_migration_machine_edit_compiles_with_current_parser(void) {
    const TZrChar *source =
            "%owned class FileHandle {}\n"
            "var file: Unique<FileHandle> = own FileHandle();\n"
            "drop(file);\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "legacy_migration_machine.zr");
    SZrLegacyMigrationPlan plan = {0};
    TZrChar *migrated = ZR_NULL;
    TZrSize migratedLength = 0U;
    SZrAstNode *ast;
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &plan));
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            source,
            strlen(source),
            &migrated,
            &migratedLength));
    TEST_ASSERT_EQUAL_STRING(
            "resource class FileHandle {}\n"
            "var file: Unique<FileHandle> = own FileHandle();\n"
            "drop(file);\n",
            migrated);
    ast = ZrParser_Parse(g_state, migrated, migratedLength, sourceName);
    TEST_ASSERT_NOT_NULL(ast);
    function = ZrParser_Compiler_Compile(g_state, ast);
    TEST_ASSERT_NOT_NULL(function);

    ZrCore_Function_Free(g_state, function);
    ZrParser_Ast_Free(g_state, ast);
    ZrCore_Memory_RawFree(g_state->global, migrated, migratedLength + 1U);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

static void test_legacy_migration_apply_rejects_stale_or_overlapping_plan(void) {
    const TZrChar *source = "%owned class FileHandle {}\n";
    const TZrChar *changedSource = "%owned class FileHandle { }\n";
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "legacy_migration_guard.zr");
    SZrLegacyMigrationPlan plan = {0};
    TZrChar *result = ZR_NULL;
    TZrSize resultLength = 0U;

    TEST_ASSERT_NOT_NULL(sourceName);
    TEST_ASSERT_TRUE(ZrParser_LegacyMigration_PlanSource(
            g_state,
            source,
            strlen(source),
            sourceName,
            &plan));
    TEST_ASSERT_FALSE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            changedSource,
            strlen(changedSource),
            &result,
            &resultLength));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_UINT32(0U, resultLength);

    plan.hasOverlap = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_LegacyMigration_ApplyMachineEdits(
            g_state,
            &plan,
            source,
            strlen(source),
            &result,
            &resultLength));
    TEST_ASSERT_NULL(result);
    TEST_ASSERT_EQUAL_UINT32(0U, resultLength);
    ZrParser_LegacyMigration_PlanFree(g_state, &plan);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_legacy_migration_plan_classifies_token_aware_candidates);
    RUN_TEST(test_legacy_migration_plan_is_repeatable_and_ignores_non_code);
    RUN_TEST(test_legacy_migration_owned_requires_a_class_declaration_shell);
    RUN_TEST(test_legacy_migration_consumes_paired_property_producer_fix);
    RUN_TEST(test_legacy_migration_plan_covers_inventory_classification_contract);
    RUN_TEST(test_legacy_migration_apply_machine_edits_is_idempotent);
    RUN_TEST(test_legacy_migration_machine_edit_compiles_with_current_parser);
    RUN_TEST(test_legacy_migration_apply_rejects_stale_or_overlapping_plan);
    return UNITY_END();
}
