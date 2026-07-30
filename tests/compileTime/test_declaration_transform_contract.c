#include "unity.h"

#include "zr_vm_parser/declaration_transform_contract.h"

static SZrParserDeclarationView make_view(const TZrChar *const *names, TZrSize count) {
    SZrParserDeclarationView view = {0};
    view.symbolId = 41U;
    view.kind = ZR_PARSER_DECLARATION_KIND_TYPE;
    view.name = "Meter";
    view.existingMemberNames = names;
    view.existingMemberCount = count;
    return view;
}

static SZrParserGeneratedDeclaration make_field(const TZrChar *name) {
    SZrParserGeneratedDeclaration field = {0};
    field.kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    field.name = name;
    field.typeId = 7U;
    field.visibility = ZR_PARSER_GENERATED_VISIBILITY_PUBLIC;
    field.mutability = ZR_PARSER_GENERATED_MUTABILITY_LET;
    return field;
}

static void test_patch_accepts_append_only_first_round_additions(void) {
    static const TZrChar *const existing[] = {"value"};
    SZrParserGeneratedDeclaration additions[] = {
            make_field("minimum"),
            make_field("maximum"),
    };
    SZrParserDeclarationView view = make_view(existing, ZR_ARRAY_COUNT(existing));
    SZrParserDeclarationPatch patch = {0};
    patch.targetSymbolId = view.symbolId;
    patch.additions = additions;
    patch.additionCount = ZR_ARRAY_COUNT(additions);

    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_VALID,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
}

static void test_patch_rejects_target_round_collision_and_recursive_transform(void) {
    static const TZrChar *const existing[] = {"value"};
    SZrParserGeneratedDeclaration addition = make_field("value");
    SZrParserDeclarationView view = make_view(existing, ZR_ARRAY_COUNT(existing));
    SZrParserDeclarationPatch patch = {0};
    SZrParserAttributeData transformAttribute = {0};

    patch.targetSymbolId = 99U;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_TARGET,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    patch.targetSymbolId = view.symbolId;
    patch.expansionRound = 1U;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_ROUND,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    patch.expansionRound = 0U;
    patch.additions = &addition;
    patch.additionCount = 1U;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_COLLISION,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    addition.name = "generated";
    addition.attributes = &transformAttribute;
    addition.attributeCount = 1U;
    transformAttribute.role = ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_RECURSIVE_TRANSFORM,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
}

static void test_patch_enforces_phase_and_ten_thousand_addition_limit(void) {
    SZrParserDeclarationView view = make_view(ZR_NULL, 0U);
    SZrParserDeclarationPatch patch = {0};
    SZrParserGeneratedDeclaration sentinel = make_field("generated");

    patch.targetSymbolId = view.symbolId;
    patch.additions = &sentinel;
    patch.additionCount = ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS + 1U;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_BUDGET,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_PHASE_CYCLE,
            ZrParser_DeclarationView_ValidatePhaseAccess(
                    ZR_PARSER_COMPILE_PHASE_EXPANSION,
                    ZR_PARSER_COMPILE_PHASE_LAYOUT));
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_VALID,
            ZrParser_DeclarationView_ValidatePhaseAccess(
                    ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
                    ZR_PARSER_COMPILE_PHASE_LAYOUT));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_patch_accepts_append_only_first_round_additions);
    RUN_TEST(test_patch_rejects_target_round_collision_and_recursive_transform);
    RUN_TEST(test_patch_enforces_phase_and_ten_thousand_addition_limit);
    return UNITY_END();
}
