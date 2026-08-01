#include "unity.h"

#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_parser/compile_tool.h"

#include <string.h>

static void test_builtin_roles_have_canonical_owner_and_stable_id(void) {
    const SZrParserAttributeSchema *usage =
            ZrParser_AttributeContract_FindBuiltinByRole(
                    ZR_PARSER_ATTRIBUTE_ROLE_USAGE);
    const SZrParserAttributeSchema *conditional =
            ZrParser_AttributeContract_FindBuiltin(
                    ZR_PARSER_ATTRIBUTE_CONDITIONAL_QUALIFIED_NAME);
    const SZrParserAttributeSchema *transform =
            ZrParser_AttributeContract_FindBuiltinByRole(
                    ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM);
    const SZrParserCompileToolModuleDescriptor *buildModule =
            ZrParser_CompileTool_FindModule("zr.compile");
    const SZrParserCompileToolModuleDescriptor *declarationModule =
            ZrParser_CompileTool_FindModule("zr.compile.declaration");

    TEST_ASSERT_NOT_NULL(usage);
    TEST_ASSERT_EQUAL_STRING("zr.reflection", usage->ownerModule);
    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_RUNTIME, usage->providerPhase);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_AttributeContract_ComputeId(usage->qualifiedName),
            usage->attributeId);

    TEST_ASSERT_NOT_NULL(conditional);
    TEST_ASSERT_EQUAL_STRING("zr.compile", conditional->ownerModule);
    TEST_ASSERT_EQUAL(ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
                      conditional->providerPhase);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_AttributeContract_ComputeId(conditional->qualifiedName),
            conditional->attributeId);

    TEST_ASSERT_NOT_NULL(transform);
    TEST_ASSERT_EQUAL_STRING("zr.compile.declaration", transform->ownerModule);
    TEST_ASSERT_EQUAL_UINT32(
            ZrParser_AttributeContract_ComputeId(transform->qualifiedName),
            transform->attributeId);
    TEST_ASSERT_EQUAL_PTR(
            conditional,
            ZrParser_CompileTool_FindMetadataRole(
                    buildModule, ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL));
    TEST_ASSERT_EQUAL_PTR(
            transform,
            ZrParser_CompileTool_FindMetadataRole(
                    declarationModule,
                    ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM));
    TEST_ASSERT_EQUAL_UINT64(
            buildModule->computedPublicContractHash,
            ZrParser_CompileTool_ComputePublicContractHash(buildModule));
    TEST_ASSERT_EQUAL_UINT64(
            declarationModule->computedPublicContractHash,
            ZrParser_CompileTool_ComputePublicContractHash(declarationModule));
    TEST_ASSERT_NOT_NULL(strstr(
            declarationModule->canonicalContract,
            "typed-constructor:CompileDiagnostic(isError:bool,message:string,target:SymbolId)|Expansion"));
    TEST_ASSERT_NOT_NULL(ZrParser_CompileTool_FindType(
            declarationModule, "AttributeData"));
    TEST_ASSERT_NOT_NULL(ZrParser_CompileTool_FindType(
            declarationModule, "GeneratedField"));
    TEST_ASSERT_NULL(ZrParser_CompileTool_FindType(
            declarationModule, "GeneratedType"));
    TEST_ASSERT_NULL(ZrParser_CompileTool_FindType(
            declarationModule, "GeneratedMethod"));
    TEST_ASSERT_NULL(ZrParser_CompileTool_FindType(
            declarationModule, "GeneratedProperty"));
    TEST_ASSERT_NOT_NULL(strstr(
            declarationModule->canonicalContract,
            "typed-constructor:AttributeData(typeId:TypeId,fieldValues:ConstantValue[])|Expansion"));
}

static void test_schema_requires_readonly_public_let_constant_safe_fields(void) {
    SZrParserAttributeFieldSchema fields[] = {
            {"min", ZR_PARSER_ATTRIBUTE_VALUE_INT, ZR_FALSE},
            {"max", ZR_PARSER_ATTRIBUTE_VALUE_INT, ZR_FALSE},
    };
    TZrBool publicLet[] = {ZR_TRUE, ZR_TRUE};

    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_NOT_READONLY,
            ZrParser_AttributeContract_ValidateSchema(
                    ZR_FALSE, fields, publicLet, ZR_ARRAY_COUNT(fields)));
    publicLet[1] = ZR_FALSE;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_ERROR_SCHEMA_FIELD_NOT_PUBLIC_LET,
            ZrParser_AttributeContract_ValidateSchema(
                    ZR_TRUE, fields, publicLet, ZR_ARRAY_COUNT(fields)));
    publicLet[1] = ZR_TRUE;
    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_VALID,
            ZrParser_AttributeContract_ValidateSchema(
                    ZR_TRUE, fields, publicLet, ZR_ARRAY_COUNT(fields)));
}

static void test_application_enforces_target_repeatability_and_typed_values(void) {
    const SZrParserAttributeSchema *conditional =
            ZrParser_AttributeContract_FindBuiltinByRole(
                    ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL);
    SZrParserAttributeConstant feature = {
            .kind = ZR_PARSER_ATTRIBUTE_VALUE_STRING,
            .value.stringValue = "trace",
    };

    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_ERROR_TARGET,
            ZrParser_AttributeContract_ValidateApplication(
                    conditional, ZR_PARSER_ATTRIBUTE_TARGET_FIELD, 0, &feature, 1));
    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_ERROR_REPEATABILITY,
            ZrParser_AttributeContract_ValidateApplication(
                    conditional, ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, 1, &feature, 1));
    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_VALID,
            ZrParser_AttributeContract_ValidateApplication(
                    conditional, ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, 0, &feature, 1));
    feature.value.stringValue = "";
    TEST_ASSERT_EQUAL(
            ZR_PARSER_ATTRIBUTE_ERROR_EMPTY_CONDITIONAL_FEATURE,
            ZrParser_AttributeContract_ValidateApplication(
                    conditional, ZR_PARSER_ATTRIBUTE_TARGET_FUNCTION, 0, &feature, 1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_builtin_roles_have_canonical_owner_and_stable_id);
    RUN_TEST(test_schema_requires_readonly_public_let_constant_safe_fields);
    RUN_TEST(test_application_enforces_target_repeatability_and_typed_values);
    return UNITY_END();
}
