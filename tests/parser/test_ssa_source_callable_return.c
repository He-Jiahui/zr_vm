#include <string.h>

/* Parse CRT headers before Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

/* This tests the returned public function after Source_Compile has released
 * its compiler context and AST. It makes no retained ExecIR or AOT claim. */
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

static SZrFunction *compile_public_entry(const char *source, TZrUInt32 index) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state,
            index == 0u ? "ssa_callable_first.zr" : "ssa_callable_second.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: source name allocation");
    g_entries[index] = ZrParser_Source_Compile(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_entries[index],
            "PRECONDITION: legal script must compile through public Source_Compile");
    g_rooted[index] = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_entries[index]));
    TEST_ASSERT_TRUE_MESSAGE(g_rooted[index], "PRECONDITION: preserve returned entry during compilation");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, g_entries[index]->parameterCount,
            "PRECONDITION: script entry has no parameters");
    TEST_ASSERT_FALSE_MESSAGE(g_entries[index]->hasVariableArguments,
            "PRECONDITION: script entry has no variable arguments");
    TEST_ASSERT_EQUAL_UINT32(0u, g_entries[index]->childFunctionLength);
    TEST_ASSERT_EQUAL_UINT32(0u, g_entries[index]->staticImportLength);
    TEST_ASSERT_EQUAL_UINT32(0u, g_entries[index]->moduleEntryEffectLength);
    return g_entries[index];
}

static void assert_i64_return_metadata(const SZrFunction *entry) {
    TEST_ASSERT_TRUE_MESSAGE(entry->hasCallableReturnType,
            "script entry did not publish callable return metadata");
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, entry->callableReturnType.baseType);
    TEST_ASSERT_FALSE(entry->callableReturnType.isNullable);
    TEST_ASSERT_FALSE(entry->callableReturnType.isArray);
    TEST_ASSERT_NULL(entry->callableReturnType.typeName);
}

static void assert_public_i64_pair(const char *source, const char *secondSource) {
    SZrFunction *first = compile_public_entry(source, 0u);
    TZrBool published = first->hasCallableReturnType;
    SZrFunctionTypedTypeRef actual = first->callableReturnType;
    SZrFunction *second = compile_public_entry(secondSource, 1u);
    /* Compare actual public fields, never padding or invented type identities. */
    TEST_ASSERT_EQUAL(published, first->hasCallableReturnType);
    TEST_ASSERT_EQUAL_INT(actual.baseType, first->callableReturnType.baseType);
    TEST_ASSERT_EQUAL(actual.isNullable, first->callableReturnType.isNullable);
    TEST_ASSERT_EQUAL_UINT32(actual.ownershipQualifier, first->callableReturnType.ownershipQualifier);
    TEST_ASSERT_EQUAL(actual.isArray, first->callableReturnType.isArray);
    TEST_ASSERT_TRUE(actual.typeName == first->callableReturnType.typeName);
    TEST_ASSERT_EQUAL_INT(actual.elementBaseType, first->callableReturnType.elementBaseType);
    TEST_ASSERT_TRUE(actual.elementTypeName == first->callableReturnType.elementTypeName);
    TEST_ASSERT_EQUAL_INT(actual.staticCType, first->callableReturnType.staticCType);
    TEST_ASSERT_EQUAL_UINT32(actual.staticCTypeId, first->callableReturnType.staticCTypeId);
    assert_i64_return_metadata(first);
    assert_i64_return_metadata(second);
}

static void test_public_script_less_true_publishes_i64_return(void) {
    assert_public_i64_pair(
            "if (1 < 2) { return 9; } else { return 8; }\n",
            "if (2 < 1) { return 9; } else { return 8; }\n");
}

static void test_public_script_less_swapped_false_publishes_i64_return(void) {
    assert_public_i64_pair(
            "if (2 < 1) { return 9; } else { return 8; }\n",
            "if (1 < 2) { return 9; } else { return 8; }\n");
}

static void test_public_script_greater_true_publishes_i64_return(void) {
    assert_public_i64_pair(
            "if (2 > 1) { return 9; } else { return 8; }\n",
            "if (1 > 2) { return 9; } else { return 8; }\n");
}

static void test_public_script_greater_swapped_false_publishes_i64_return(void) {
    assert_public_i64_pair(
            "if (1 > 2) { return 9; } else { return 8; }\n",
            "if (2 > 1) { return 9; } else { return 8; }\n");
}

static void assert_public_script_does_not_advertise_i64(const char *source) {
    const SZrFunction *entry = compile_public_entry(source, 0u);
    TEST_ASSERT_FALSE_MESSAGE(entry->hasCallableReturnType != ZR_FALSE &&
            entry->callableReturnType.baseType == ZR_VALUE_TYPE_INT64,
            "script with an incompatible return path must not advertise i64");
}

static void test_public_script_fallthrough_does_not_advertise_i64(void) {
    assert_public_script_does_not_advertise_i64("if (1 < 2) { return 9; }\n");
}

static void test_public_script_mixed_bool_does_not_advertise_i64(void) {
    assert_public_script_does_not_advertise_i64(
            "if (1 < 2) { return 9; } else { return false; }\n");
}

static void test_public_script_none_return_does_not_advertise_i64(void) {
    assert_public_script_does_not_advertise_i64(
            "if (1 < 2) { return 9; } else { return; }\n");
}

static void test_public_script_implicit_return_does_not_advertise_i64(void) {
    assert_public_script_does_not_advertise_i64("let x = 1;\n");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_public_script_less_true_publishes_i64_return);
    RUN_TEST(test_public_script_less_swapped_false_publishes_i64_return);
    RUN_TEST(test_public_script_greater_true_publishes_i64_return);
    RUN_TEST(test_public_script_greater_swapped_false_publishes_i64_return);
    RUN_TEST(test_public_script_fallthrough_does_not_advertise_i64);
    RUN_TEST(test_public_script_mixed_bool_does_not_advertise_i64);
    RUN_TEST(test_public_script_none_return_does_not_advertise_i64);
    RUN_TEST(test_public_script_implicit_return_does_not_advertise_i64);
    return UNITY_END();
}
