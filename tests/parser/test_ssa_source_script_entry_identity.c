#include <string.h>

/* Parse CRT declarations before Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

/*
 * RED contract for the script entry identity witness. This deliberately
 * exercises only the public Source_Compile result and address-free metadata;
 * it does not retain an AST, ExecIR, bytecode artifact, or external handle.
 */
static SZrState *g_state;
enum { SCRIPT_ENTRY_CAPACITY = 3 };
static SZrFunction *g_entries[SCRIPT_ENTRY_CAPACITY];
static TZrBool g_rooted[SCRIPT_ENTRY_CAPACITY];

void setUp(void) {
    memset(g_entries, 0, sizeof(g_entries));
    memset(g_rooted, 0, sizeof(g_rooted));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    if (g_state == ZR_NULL) return;
    for (index = 0u; index < SCRIPT_ENTRY_CAPACITY; ++index) {
        if (g_entries[index] == ZR_NULL) continue;
        if (g_rooted[index] != ZR_FALSE) {
            ZrCore_GarbageCollector_UnignoreObject(g_state->global,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(g_entries[index]));
        }
        ZrCore_Function_Free(g_state, g_entries[index]);
        g_entries[index] = ZR_NULL;
    }
    ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static SZrFunction *compile_named_entry(const char *source, const char *source_name,
        TZrUInt32 index, TZrBool require_no_args) {
    SZrString *name;
    TEST_ASSERT_TRUE(index < SCRIPT_ENTRY_CAPACITY);
    TEST_ASSERT_NULL_MESSAGE(g_entries[index],
            "PRECONDITION: each returned entry retains its own teardown slot");
    name = ZrCore_String_CreateFromNative(g_state, source_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: source name allocation");
    g_entries[index] = ZrParser_Source_Compile(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_entries[index],
            "PRECONDITION: legal script must compile through public Source_Compile");
    g_rooted[index] = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_entries[index]));
    TEST_ASSERT_TRUE_MESSAGE(g_rooted[index],
            "PRECONDITION: root returned script entry during metadata assertions");
    if (require_no_args != ZR_FALSE) {
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, g_entries[index]->parameterCount,
                "PRECONDITION: script entry has no parameters");
        TEST_ASSERT_FALSE(g_entries[index]->hasVariableArguments);
        TEST_ASSERT_EQUAL_UINT32(0u, g_entries[index]->childFunctionLength);
    }
    return g_entries[index];
}

static SZrFunction *compile_entry(const char *source, TZrUInt32 index,
        TZrBool require_no_args) {
    return compile_named_entry(source,
            index == 0u ? "ssa_script_identity_first.zr"
                        : "ssa_script_identity_second.zr", index, require_no_args);
}

static void assert_i64_return_metadata(const SZrFunction *entry) {
    const SZrFunctionTypedTypeRef *type = &entry->callableReturnType;
    TEST_ASSERT_TRUE_MESSAGE(entry->hasCallableReturnType == ZR_TRUE,
            "PRECONDITION: script entry already publishes callable return metadata");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, type->baseType);
    TEST_ASSERT_FALSE(type->isNullable);
    TEST_ASSERT_EQUAL_UINT32(0u, type->ownershipQualifier);
    TEST_ASSERT_FALSE(type->isArray);
    TEST_ASSERT_NULL(type->typeName);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, type->elementBaseType);
    TEST_ASSERT_NULL(type->elementTypeName);
    TEST_ASSERT_EQUAL_INT(ZR_STATIC_C_TYPE_DYNAMIC, type->staticCType);
    TEST_ASSERT_EQUAL_UINT32(0u, type->staticCTypeId);
    TEST_ASSERT_NULL(entry->moduleVersion);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->exportedVariableLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->typedExportedSymbolLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->moduleEntryEffectLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->closureValueLength);
    TEST_ASSERT_EQUAL_UINT32(0u, entry->typedClosureBindingLength);
}

static void assert_script_identity_witness(const SZrFunction *entry) {
    const SZrFunctionSourceCallableIdentity *identity =
            &entry->sourceCallableIdentity;
    assert_i64_return_metadata(entry);
    TEST_ASSERT_TRUE_MESSAGE(entry->hasSourceCallableIdentity == ZR_TRUE &&
            identity->hasExplicitNoArgsI64 == ZR_TRUE,
            "FEATURE: script entry callable identity witness is absent");
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1,
            identity->schemaVersion);
    TEST_ASSERT_TRUE(identity->symbolId != 0u);
    TEST_ASSERT_TRUE(identity->typeId != 0u);
    TEST_ASSERT_TRUE(identity->canonicalSignatureHash != 0u);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, identity->returnPrimitive);
    TEST_ASSERT_EQUAL_UINT32(0u, identity->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, identity->receiverFlags);
    TEST_ASSERT_EQUAL_UINT32(0u, identity->effectFlags);
    TEST_ASSERT_TRUE(identity->declarationRange.startLine > 0u);
    TEST_ASSERT_TRUE(identity->declarationRange.startColumn > 0u);
    TEST_ASSERT_TRUE(identity->declarationRange.endLine >=
            identity->declarationRange.startLine);
    TEST_ASSERT_TRUE(identity->declarationRange.endColumn > 0u);
    if (identity->declarationRange.endLine == identity->declarationRange.startLine) {
        TEST_ASSERT_TRUE(identity->declarationRange.endColumn >=
                identity->declarationRange.startColumn);
    }
}

static void assert_identity_unchanged(TZrBool had_identity,
        const SZrFunctionSourceCallableIdentity *expected, const SZrFunction *entry) {
    const SZrFunctionSourceCallableIdentity *actual = &entry->sourceCallableIdentity;
    TEST_ASSERT_EQUAL(had_identity, entry->hasSourceCallableIdentity);
    TEST_ASSERT_EQUAL_UINT32(expected->schemaVersion, actual->schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(expected->symbolId, actual->symbolId);
    TEST_ASSERT_EQUAL_UINT32(expected->typeId, actual->typeId);
    TEST_ASSERT_EQUAL_UINT64(expected->canonicalSignatureHash, actual->canonicalSignatureHash);
    TEST_ASSERT_EQUAL_UINT32(expected->returnPrimitive, actual->returnPrimitive);
    TEST_ASSERT_EQUAL_UINT32(expected->parameterCount, actual->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(expected->receiverFlags, actual->receiverFlags);
    TEST_ASSERT_EQUAL_UINT32(expected->effectFlags, actual->effectFlags);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.startLine, actual->declarationRange.startLine);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.startColumn, actual->declarationRange.startColumn);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.endLine, actual->declarationRange.endLine);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.endColumn, actual->declarationRange.endColumn);
    TEST_ASSERT_EQUAL(expected->hasExplicitNoArgsI64, actual->hasExplicitNoArgsI64);
}

static void assert_return_metadata_unchanged(TZrBool had_return,
        const SZrFunctionTypedTypeRef *expected, const SZrFunction *entry) {
    const SZrFunctionTypedTypeRef *actual = &entry->callableReturnType;
    TEST_ASSERT_EQUAL(had_return, entry->hasCallableReturnType);
    TEST_ASSERT_EQUAL_INT(expected->baseType, actual->baseType);
    TEST_ASSERT_EQUAL(expected->isNullable, actual->isNullable);
    TEST_ASSERT_EQUAL_UINT32(expected->ownershipQualifier, actual->ownershipQualifier);
    TEST_ASSERT_EQUAL(expected->isArray, actual->isArray);
    TEST_ASSERT_TRUE(expected->typeName == actual->typeName);
    TEST_ASSERT_EQUAL_INT(expected->elementBaseType, actual->elementBaseType);
    TEST_ASSERT_TRUE(expected->elementTypeName == actual->elementTypeName);
    TEST_ASSERT_EQUAL_INT(expected->staticCType, actual->staticCType);
    TEST_ASSERT_EQUAL_UINT32(expected->staticCTypeId, actual->staticCTypeId);
}

static void assert_entry_has_no_identity(const SZrFunction *entry) {
    TEST_ASSERT_FALSE_MESSAGE(entry->hasSourceCallableIdentity,
            "guard script must not publish a callable identity witness");
    TEST_ASSERT_FALSE(entry->sourceCallableIdentity.hasExplicitNoArgsI64);
}

static void assert_no_script_identity(const char *source) {
    const SZrFunction *entry = compile_entry(source, 0u, ZR_TRUE);
    assert_entry_has_no_identity(entry);
}

static void test_script_entry_i64_literal_identity(void) {
    assert_script_identity_witness(compile_entry("return 9;\n", 0u, ZR_TRUE));
}

static void test_script_entry_identity_survives_second_compile(void) {
    SZrFunction *first = compile_entry("return 9;\n", 0u, ZR_TRUE);
    TZrBool had_identity = first->hasSourceCallableIdentity;
    SZrFunctionSourceCallableIdentity expected = first->sourceCallableIdentity;
    TZrBool had_return = first->hasCallableReturnType;
    SZrFunctionTypedTypeRef expected_return = first->callableReturnType;
    SZrFunction *second = compile_entry("return 8;\n", 1u, ZR_TRUE);
    /* Compare every public field, including presence; never compare padding. */
    assert_identity_unchanged(had_identity, &expected, first);
    assert_return_metadata_unchanged(had_return, &expected_return, first);
    assert_script_identity_witness(first);
    assert_script_identity_witness(second);
}

static void test_guard_script_bool_return(void) {
    assert_no_script_identity("return true;\n");
}

static void test_guard_script_conditional_paths(void) {
    assert_no_script_identity("if (1 < 2) { return 9; } else { return 8; }\n");
}

static void test_guard_script_implicit_return(void) {
    assert_no_script_identity("let value = 1;\n");
}

static void test_guard_script_none_return(void) {
    assert_no_script_identity("return;\n");
}

static void test_guard_script_binary_return(void) {
    assert_no_script_identity("return 4 + 5;\n");
}

static void test_guard_script_multiple_statements(void) {
    assert_no_script_identity("let value = 1; return 9;\n");
}

static void test_guard_script_local_return(void) {
    assert_no_script_identity("let value = 9; return value;\n");
}

static void test_guard_script_fallthrough(void) {
    assert_no_script_identity("if (1 < 2) { return 9; }\n");
}

static void test_guard_child_with_parameters(void) {
    const SZrFunction *entry = compile_entry(
            "fn answer(value: int): int { return value; }\n", 0u, ZR_FALSE);
    assert_entry_has_no_identity(entry);
}

static void test_script_entry_identity_after_malformed_retry(void) {
    static const char source_name[] = "ssa_script_identity_retry.zr";
    static const char malformed[] = "return (9;\n";
    SZrString *name = ZrCore_String_CreateFromNative(g_state, source_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: source name allocation");
    /* Parser error capture is per Source_Compile call; do not reset VM state. */
    g_entries[0] = ZrParser_Source_Compile(g_state, malformed, strlen(malformed), name);
    TEST_ASSERT_NULL_MESSAGE(g_entries[0],
            "PRECONDITION: malformed syntax must fail public Source_Compile");
    assert_script_identity_witness(compile_named_entry(
            "return 9;\n", source_name, 1u, ZR_TRUE));
}

static void test_script_entry_identity_same_name_success_guard_success(void) {
    static const char source_name[] = "ssa_script_identity_same_name.zr";
    SZrFunction *first = compile_named_entry("return 9;\n", source_name, 0u, ZR_TRUE);
    TZrBool had_identity = first->hasSourceCallableIdentity;
    SZrFunctionSourceCallableIdentity expected = first->sourceCallableIdentity;
    TZrBool had_return = first->hasCallableReturnType;
    SZrFunctionTypedTypeRef expected_return = first->callableReturnType;
    TZrUInt64 expected_module_hash = first->moduleSignatureHash;
    SZrFunction *unsupported = compile_named_entry(
            "return 4 + 5;\n", source_name, 1u, ZR_TRUE);
    SZrFunction *third;
    assert_entry_has_no_identity(unsupported);
    assert_identity_unchanged(had_identity, &expected, first);
    assert_return_metadata_unchanged(had_return, &expected_return, first);
    TEST_ASSERT_EQUAL_UINT64(expected_module_hash, first->moduleSignatureHash);
    third = compile_named_entry("return 8;\n", source_name, 2u, ZR_TRUE);
    assert_entry_has_no_identity(unsupported);
    assert_identity_unchanged(had_identity, &expected, first);
    assert_return_metadata_unchanged(had_return, &expected_return, first);
    TEST_ASSERT_EQUAL_UINT64(expected_module_hash, first->moduleSignatureHash);
    assert_script_identity_witness(first);
    assert_script_identity_witness(third);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_script_entry_i64_literal_identity);
    RUN_TEST(test_script_entry_identity_survives_second_compile);
    RUN_TEST(test_guard_script_bool_return);
    RUN_TEST(test_guard_script_conditional_paths);
    RUN_TEST(test_guard_script_implicit_return);
    RUN_TEST(test_guard_script_none_return);
    RUN_TEST(test_guard_script_binary_return);
    RUN_TEST(test_guard_script_multiple_statements);
    RUN_TEST(test_guard_script_local_return);
    RUN_TEST(test_guard_script_fallthrough);
    RUN_TEST(test_guard_child_with_parameters);
    RUN_TEST(test_script_entry_identity_after_malformed_retry);
    RUN_TEST(test_script_entry_identity_same_name_success_guard_success);
    return UNITY_END();
}
