#include "unity.h"

#include "zr_vm_parser/declaration_transform_contract.h"

#include <stdio.h>
#include <stdlib.h>

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
    SZrParserGeneratedDeclaration *additions;
    TZrChar (*names)[32];

    additions = (SZrParserGeneratedDeclaration *)calloc(
            ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS,
            sizeof(*additions));
    names = (TZrChar (*)[32])calloc(
            ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS,
            sizeof(*names));
    TEST_ASSERT_NOT_NULL(additions);
    TEST_ASSERT_NOT_NULL(names);

    for (TZrSize index = 0;
         index < ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS;
         index++) {
        snprintf(names[index], sizeof(names[index]), "generated_%llu",
                 (unsigned long long)index);
        additions[index] = make_field(names[index]);
    }

    patch.targetSymbolId = view.symbolId;
    patch.additions = additions;
    patch.additionCount = ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_VALID,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

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

    free(names);
    free(additions);
}

static void test_patch_rejects_unpublished_generated_declaration_kinds(void) {
    SZrParserDeclarationView view = make_view(ZR_NULL, 0U);
    SZrParserDeclarationPatch patch = {0};
    SZrParserGeneratedDeclaration addition = make_field("generated");

    patch.targetSymbolId = view.symbolId;
    patch.additions = &addition;
    patch.additionCount = 1U;

    addition.kind = (EZrParserGeneratedDeclarationKind)1;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_KIND,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
    addition.kind = (EZrParserGeneratedDeclarationKind)3;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_KIND,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
    addition.kind = (EZrParserGeneratedDeclarationKind)4;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_KIND,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
}

static void test_patch_diagnostics_require_message_and_patch_target(void) {
    SZrParserDeclarationView view = make_view(ZR_NULL, 0U);
    SZrParserDeclarationPatch patch = {0};
    SZrParserCompileDiagnostic diagnostic = {0};

    patch.targetSymbolId = view.symbolId;
    patch.diagnostics = &diagnostic;
    patch.diagnosticCount = 1U;
    diagnostic.targetSymbolId = view.symbolId;
    diagnostic.message = "";
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_ARGUMENT,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    diagnostic.message = "generated warning";
    diagnostic.targetSymbolId = view.symbolId + 1U;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_TARGET,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    diagnostic.targetSymbolId = view.symbolId;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_VALID,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
}

static void test_patch_interface_adds_require_unique_canonical_type_ids(void) {
    SZrParserDeclarationView view = make_view(ZR_NULL, 0U);
    SZrParserDeclarationPatch patch = {0};
    TZrTypeId interfaceIds[] = {7U, 9U};

    patch.targetSymbolId = view.symbolId;
    patch.interfaceAdds = interfaceIds;
    patch.interfaceAddCount = ZR_ARRAY_COUNT(interfaceIds);
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_VALID,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    interfaceIds[1] = interfaceIds[0];
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_COLLISION,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    interfaceIds[1] = ZR_SEMANTIC_ID_INVALID;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_TYPE,
            ZrParser_DeclarationPatch_Validate(&view, &patch));

    interfaceIds[1] = 9U;
    patch.interfaceAddCount =
            ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS + 1U;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_DECLARATION_PATCH_ERROR_BUDGET,
            ZrParser_DeclarationPatch_Validate(&view, &patch));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_patch_accepts_append_only_first_round_additions);
    RUN_TEST(test_patch_rejects_target_round_collision_and_recursive_transform);
    RUN_TEST(test_patch_enforces_phase_and_ten_thousand_addition_limit);
    RUN_TEST(test_patch_rejects_unpublished_generated_declaration_kinds);
    RUN_TEST(test_patch_diagnostics_require_message_and_patch_target);
    RUN_TEST(test_patch_interface_adds_require_unique_canonical_type_ids);
    return UNITY_END();
}
