#include <string.h>

/* Keep CRT declarations ahead of Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"

/*
 * Ordinary Source_Compile publishes an address-free identity witness for a
 * declared noargs i64 child whose body is one integer-literal return.
 * Context-local IDs are retained only as provenance: this test never resolves
 * them after Source_Compile returns. Wider source forms remain uncertified.
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
            index == 0u ? "ssa_callable_identity_first.zr"
                        : "ssa_callable_identity_second.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: source name allocation");
    g_entries[index] = ZrParser_Source_Compile(g_state, source, strlen(source), name);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_entries[index],
            "PRECONDITION: ordinary Source_Compile must accept the source");
    g_rooted[index] = ZrCore_GarbageCollector_IgnoreObject(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(g_entries[index]));
    TEST_ASSERT_TRUE_MESSAGE(g_rooted[index],
            "PRECONDITION: root returned function during lifetime check");
    return g_entries[index];
}

static SZrFunction *assert_child_callable_return(const char *source, TZrUInt32 index) {
    SZrFunction *entry = compile_entry(source, index);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, entry->parameterCount,
            "PRECONDITION: script entry has no parameters");
    TEST_ASSERT_FALSE_MESSAGE(entry->hasVariableArguments,
            "PRECONDITION: script entry has no varargs");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, entry->childFunctionLength,
            "PRECONDITION: source contains exactly one child function");
    TEST_ASSERT_NOT_NULL_MESSAGE(entry->childFunctionList,
            "PRECONDITION: child function storage exists");

    {
        SZrFunction *child = &entry->childFunctionList[0];
        TEST_ASSERT_TRUE_MESSAGE(child->hasCallableReturnType,
                "PRECONDITION: child callable return metadata is published");
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64,
                child->callableReturnType.baseType);
        TEST_ASSERT_FALSE(child->callableReturnType.isNullable);
        TEST_ASSERT_FALSE(child->callableReturnType.isArray);
        TEST_ASSERT_NULL(child->callableReturnType.typeName);
        return child;
    }
}

static void assert_identity_witness(const SZrFunction *child) {
    const SZrFunctionSourceCallableIdentity *identity = &child->sourceCallableIdentity;
    TEST_ASSERT_TRUE_MESSAGE(child->hasSourceCallableIdentity == ZR_TRUE &&
            identity->hasExplicitNoArgsI64 == ZR_TRUE,
            "FEATURE: callable identity witness absent after callableReturnType");
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_SOURCE_CALLABLE_IDENTITY_SCHEMA_V1,
            identity->schemaVersion);
    TEST_ASSERT_TRUE(identity->symbolId != 0u);
    TEST_ASSERT_TRUE(identity->typeId != 0u);
    TEST_ASSERT_TRUE(identity->canonicalSignatureHash != 0u);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, identity->returnPrimitive);
    TEST_ASSERT_EQUAL_UINT32(0u, identity->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, identity->receiverFlags);
    TEST_ASSERT_EQUAL_UINT32(0u, identity->effectFlags);
    TEST_ASSERT_TRUE(identity->declarationRange.startLine > 0u);
    TEST_ASSERT_TRUE(identity->declarationRange.endLine >= identity->declarationRange.startLine);
}

static void assert_identity_unchanged(const SZrFunctionSourceCallableIdentity *expected,
                                    const SZrFunctionSourceCallableIdentity *actual) {
    TEST_ASSERT_EQUAL_UINT32(expected->schemaVersion, actual->schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(expected->symbolId, actual->symbolId);
    TEST_ASSERT_EQUAL_UINT32(expected->typeId, actual->typeId);
    TEST_ASSERT_EQUAL_UINT64(expected->canonicalSignatureHash, actual->canonicalSignatureHash);
    TEST_ASSERT_EQUAL_INT(expected->returnPrimitive, actual->returnPrimitive);
    TEST_ASSERT_EQUAL_UINT32(expected->parameterCount, actual->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(expected->receiverFlags, actual->receiverFlags);
    TEST_ASSERT_EQUAL_UINT32(expected->effectFlags, actual->effectFlags);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.startLine, actual->declarationRange.startLine);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.startColumn, actual->declarationRange.startColumn);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.endLine, actual->declarationRange.endLine);
    TEST_ASSERT_EQUAL_UINT32(expected->declarationRange.endColumn, actual->declarationRange.endColumn);
    TEST_ASSERT_EQUAL(expected->hasExplicitNoArgsI64, actual->hasExplicitNoArgsI64);
}

static void test_source_child_callable_identity_published(void) {
    assert_identity_witness(assert_child_callable_return(
            "fn answer(): int { return 9; }\n", 0u));
}

static void test_source_child_callable_identity_survives_second_compile(void) {
    SZrFunction *first = assert_child_callable_return(
            "fn first(): int { return 9; }\n", 0u);
    TZrBool hadReturn = first->hasCallableReturnType;
    EZrValueType returnType = first->callableReturnType.baseType;
    TZrBool hadIdentity = first->hasSourceCallableIdentity;
    SZrFunctionSourceCallableIdentity identity = first->sourceCallableIdentity;
    SZrFunction *second = assert_child_callable_return(
            "fn second(): int { return 8; }\n", 1u);
    TEST_ASSERT_EQUAL(hadReturn, first->hasCallableReturnType);
    TEST_ASSERT_EQUAL_INT(returnType, first->callableReturnType.baseType);
    TEST_ASSERT_EQUAL(hadIdentity, first->hasSourceCallableIdentity);
    assert_identity_unchanged(&identity, &first->sourceCallableIdentity);
    assert_identity_witness(first);
    assert_identity_witness(second);
}

static void test_source_two_children_keep_distinct_declaration_identity(void) {
    SZrFunction *entry = compile_entry(
            "fn first(): int { return 9; }\n"
            "fn second(): int { return 8; }\n", 0u);
    const SZrFunction *first = ZR_NULL;
    const SZrFunction *second = ZR_NULL;
    TZrUInt32 index;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, entry->childFunctionLength,
            "PRECONDITION: source must retain both declared children");
    TEST_ASSERT_NOT_NULL(entry->childFunctionList);
    for (index = 0u; index < entry->childFunctionLength; ++index) {
        const SZrFunction *child = &entry->childFunctionList[index];
        const char *name;
        TEST_ASSERT_NOT_NULL_MESSAGE(child->functionName,
                "PRECONDITION: named child retains its source name");
        name = ZrCore_String_GetNativeString(child->functionName);
        if (strcmp(name, "first") == 0) {
            TEST_ASSERT_NULL(first);
            first = child;
        } else if (strcmp(name, "second") == 0) {
            TEST_ASSERT_NULL(second);
            second = child;
        }
        TEST_ASSERT_TRUE_MESSAGE(child->hasCallableReturnType,
                "PRECONDITION: child callable return metadata is published");
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, child->callableReturnType.baseType);
        assert_identity_witness(child);
    }
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_TRUE(first->sourceCallableIdentity.symbolId !=
            second->sourceCallableIdentity.symbolId);
    TEST_ASSERT_EQUAL_UINT32(1u, first->sourceCallableIdentity.declarationRange.startLine);
    TEST_ASSERT_EQUAL_UINT32(1u, first->sourceCallableIdentity.declarationRange.endLine);
    TEST_ASSERT_EQUAL_UINT32(2u, second->sourceCallableIdentity.declarationRange.startLine);
    TEST_ASSERT_EQUAL_UINT32(2u, second->sourceCallableIdentity.declarationRange.endLine);
    /* InternFunction hashes parameter contracts, return TypeId, receiver and
     * effects in this same context; declaration name/body do not enter it. */
    TEST_ASSERT_EQUAL_UINT64(first->sourceCallableIdentity.canonicalSignatureHash,
            second->sourceCallableIdentity.canonicalSignatureHash);
}

static void assert_tree_has_no_identity(const SZrFunction *function) {
    TZrUInt32 index;
    TEST_ASSERT_FALSE_MESSAGE(function->hasSourceCallableIdentity,
            "guard source must not receive a source callable identity witness");
    TEST_ASSERT_FALSE(function->sourceCallableIdentity.hasExplicitNoArgsI64);
    if (function->childFunctionLength == 0u) return;
    TEST_ASSERT_NOT_NULL_MESSAGE(function->childFunctionList,
            "PRECONDITION: child storage must match its count");
    for (index = 0u; index < function->childFunctionLength; ++index) {
        assert_tree_has_no_identity(&function->childFunctionList[index]);
    }
}

static void assert_no_identity_certification(const char *source) {
    assert_tree_has_no_identity(compile_entry(source, 0u));
}

static void test_guard_bool_return(void) {
    assert_no_identity_certification("fn answer(): bool { return true; }\n");
}

static void test_guard_parameterized_function(void) {
    assert_no_identity_certification("fn answer(value: int): int { return value; }\n");
}

static void test_guard_closure_function(void) {
    assert_no_identity_certification(
            "fn outer(): int { let offset = 4; fn answer(): int { return offset; } return answer(); }\n");
}

static void test_guard_implicit_return(void) {
    assert_no_identity_certification("fn answer() { let value = 1; }\n");
}

static void test_guard_unannotated_function(void) {
    assert_no_identity_certification("fn answer() { return 9; }\n");
}

static void test_guard_binary_integer_return(void) {
    assert_no_identity_certification("fn answer(): int { return 4 + 5; }\n");
}

static void test_guard_multiple_statement_body(void) {
    assert_no_identity_certification("fn answer(): int { let value = 9; return value; }\n");
}

static void test_guard_failed_compile(void) {
    const char *source = "fn answer(: int { return 9; }\n";
    SZrString *name = ZrCore_String_CreateFromNative(g_state,
            "ssa_callable_identity_failed.zr");
    TEST_ASSERT_NOT_NULL_MESSAGE(name, "PRECONDITION: failed source name allocation");
    SZrFunction *entry = ZrParser_Source_Compile(g_state,
            source, strlen(source), name);
    TEST_ASSERT_NULL_MESSAGE(entry,
            "PRECONDITION: malformed source must not publish callable identity");
    /* Retry proves that ordinary compilation remains usable after rejection. */
    assert_child_callable_return("fn retry(): int { return 8; }\n", 0u);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_source_child_callable_identity_published);
    RUN_TEST(test_source_child_callable_identity_survives_second_compile);
    RUN_TEST(test_source_two_children_keep_distinct_declaration_identity);
    RUN_TEST(test_guard_bool_return);
    RUN_TEST(test_guard_parameterized_function);
    RUN_TEST(test_guard_closure_function);
    RUN_TEST(test_guard_implicit_return);
    RUN_TEST(test_guard_unannotated_function);
    RUN_TEST(test_guard_binary_integer_return);
    RUN_TEST(test_guard_multiple_statement_body);
    RUN_TEST(test_guard_failed_compile);
    return UNITY_END();
}
