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
static SZrFunction *g_entries[2];
static TZrBool g_rooted[2];

void setUp(void) {
    memset(g_entries, 0, sizeof(g_entries));
    memset(g_rooted, 0, sizeof(g_rooted));
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    if (g_state == ZR_NULL) return;
    for (index = 0u; index < 2u; ++index) {
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

static SZrFunction *compile_entry(const char *source, TZrUInt32 index) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state,
            index == 0u ? "ssa_script_identity_first.zr"
                        : "ssa_script_identity_second.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: source name allocation");
    g_entries[index] = ZrParser_Source_Compile(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_entries[index],
            "PRECONDITION: legal script must compile through public Source_Compile");
    g_rooted[index] = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_entries[index]));
    TEST_ASSERT_TRUE_MESSAGE(g_rooted[index],
            "PRECONDITION: root returned script entry during metadata assertions");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, g_entries[index]->parameterCount,
            "PRECONDITION: script entry has no parameters");
    TEST_ASSERT_FALSE(g_entries[index]->hasVariableArguments);
    TEST_ASSERT_EQUAL_UINT32(0u, g_entries[index]->childFunctionLength);
    return g_entries[index];
}

static void assert_script_identity_witness(const SZrFunction *entry) {
    const SZrFunctionSourceCallableIdentity *identity =
            &entry->sourceCallableIdentity;
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
    TEST_ASSERT_TRUE(identity->declarationRange.endLine >=
            identity->declarationRange.startLine);
}

static void assert_no_script_identity(const char *source) {
    const SZrFunction *entry = compile_entry(source, 0u);
    TEST_ASSERT_FALSE_MESSAGE(entry->hasSourceCallableIdentity,
            "guard script must not publish a callable identity witness");
    TEST_ASSERT_FALSE(entry->sourceCallableIdentity.hasExplicitNoArgsI64);
}

static void test_script_entry_i64_literal_identity(void) {
    assert_script_identity_witness(compile_entry("return 9;\n", 0u));
}

static void test_script_entry_identity_survives_second_compile(void) {
    SZrFunction *first = compile_entry("return 9;\n", 0u);
    SZrFunctionSourceCallableIdentity expected = first->sourceCallableIdentity;
    SZrFunction *second = compile_entry("return 8;\n", 1u);
    assert_script_identity_witness(first);
    assert_script_identity_witness(second);
    TEST_ASSERT_EQUAL_UINT32(expected.symbolId, first->sourceCallableIdentity.symbolId);
    TEST_ASSERT_EQUAL_UINT64(expected.canonicalSignatureHash,
            first->sourceCallableIdentity.canonicalSignatureHash);
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

static void test_guard_child_with_parameters(void) {
    assert_no_script_identity("fn answer(value: int): int { return value; }\n");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_script_entry_i64_literal_identity);
    RUN_TEST(test_script_entry_identity_survives_second_compile);
    RUN_TEST(test_guard_script_bool_return);
    RUN_TEST(test_guard_script_conditional_paths);
    RUN_TEST(test_guard_script_implicit_return);
    RUN_TEST(test_guard_child_with_parameters);
    return UNITY_END();
}
