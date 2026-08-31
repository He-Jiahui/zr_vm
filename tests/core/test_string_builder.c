#include <string.h>

#include "unity.h"

#include "tests/harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/string_builder.h"

void setUp(void) {}

void tearDown(void) {}

static void test_builder_appends_binary_fragments_and_freezes_exact_bytes(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrStringBuilder builder;
    static const TZrChar embedded[] = {'a', '\0', 'b'};
    static const TZrChar utf8[] = "\xE2\x82\xAC";
    SZrString *result;
    TZrNativeString bytes;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_Init(state, &builder, 1u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendNative(&builder, "prefix:", 7u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendNative(&builder, embedded, sizeof(embedded)));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendNative(&builder, utf8, sizeof(utf8) - 1u));
    TEST_ASSERT_EQUAL_UINT32(13u, (TZrUInt32)ZrCore_StringBuilder_GetLength(&builder));

    result = ZrCore_StringBuilder_Freeze(&builder);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_UINT32(13u, (TZrUInt32)ZrCore_String_GetByteLength(result));
    bytes = ZrCore_String_GetNativeString(result);
    TEST_ASSERT_EQUAL_MEMORY("prefix:", bytes, 7u);
    TEST_ASSERT_EQUAL_UINT8('a', (TZrUInt8)bytes[7]);
    TEST_ASSERT_EQUAL_UINT8(0u, (TZrUInt8)bytes[8]);
    TEST_ASSERT_EQUAL_UINT8('b', (TZrUInt8)bytes[9]);
    TEST_ASSERT_EQUAL_UINT8(0xE2u, (TZrUInt8)bytes[10]);
    TEST_ASSERT_EQUAL_UINT8(0x82u, (TZrUInt8)bytes[11]);
    TEST_ASSERT_EQUAL_UINT8(0xACu, (TZrUInt8)bytes[12]);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)ZrCore_StringBuilder_GetLength(&builder));
    TEST_ASSERT_NULL(ZrCore_StringBuilder_GetNativeString(&builder));

    ZrCore_StringBuilder_Dispose(&builder);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_builder_freeze_preserves_string_interning_and_hash_contract(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrStringBuilder builder;
    SZrString *direct;
    SZrString *frozen;
    SZrString *shortDirect;
    SZrString *shortFrozen;
    static const TZrChar text[] =
            "interned builder text that deliberately exceeds the short-string storage threshold so the "
            "builder freeze path also exercises long-string allocation and cached hash identity";
    static const TZrChar shortText[] = "interned";

    TEST_ASSERT_NOT_NULL(state);
    direct = ZrCore_String_Create(state, (TZrNativeString)text, sizeof(text) - 1u);
    TEST_ASSERT_NOT_NULL(direct);
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_Init(state, &builder, 0u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendNative(&builder, (TZrNativeString)text, sizeof(text) - 1u));
    frozen = ZrCore_StringBuilder_Freeze(&builder);
    TEST_ASSERT_NOT_NULL(frozen);
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)(sizeof(text) - 1u),
                             (TZrUInt32)ZrCore_String_GetByteLength(frozen));
    TEST_ASSERT_EQUAL_MEMORY(text, ZrCore_String_GetNativeString(frozen), sizeof(text) - 1u);
    TEST_ASSERT_EQUAL_UINT64(direct->super.hash, frozen->super.hash);

    ZrCore_StringBuilder_Dispose(&builder);

    shortDirect = ZrCore_String_Create(state, (TZrNativeString)shortText, sizeof(shortText) - 1u);
    TEST_ASSERT_NOT_NULL(shortDirect);
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_Init(state, &builder, 0u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendNative(&builder,
                                                       (TZrNativeString)shortText,
                                                       sizeof(shortText) - 1u));
    shortFrozen = ZrCore_StringBuilder_Freeze(&builder);
    TEST_ASSERT_EQUAL_PTR(shortDirect, shortFrozen);

    ZrCore_StringBuilder_Dispose(&builder);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_builder_append_string_copies_before_gc_move(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrStringBuilder builder;
    SZrString *source;
    SZrString *result;
    static const TZrChar text[] =
            "a long string copied into a native builder before collection; this deliberately crosses the "
            "short-string threshold so the source uses an externally allocated native byte buffer while "
            "the collector relocates the managed string object";

    TEST_ASSERT_NOT_NULL(state);
    source = ZrCore_String_Create(state, (TZrNativeString)text, sizeof(text) - 1u);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_Init(state, &builder, 0u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendString(&builder, source));
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    result = ZrCore_StringBuilder_Freeze(&builder);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_UINT32(sizeof(text) - 1u, (TZrUInt32)ZrCore_String_GetByteLength(result));
    TEST_ASSERT_EQUAL_MEMORY(text, ZrCore_String_GetNativeString(result), sizeof(text) - 1u);

    ZrCore_StringBuilder_Dispose(&builder);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_builder_rejects_invalid_inputs_without_allocating(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrStringBuilder builder;
    SZrStringBuilder overflowBuilder;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_FALSE(ZrCore_StringBuilder_Init(ZR_NULL, &builder, 0u));
    TEST_ASSERT_FALSE(ZrCore_StringBuilder_Init(state, ZR_NULL, 0u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_Init(state, &builder, 0u));
    TEST_ASSERT_FALSE(ZrCore_StringBuilder_AppendNative(&builder, ZR_NULL, 1u));
    TEST_ASSERT_TRUE(ZrCore_StringBuilder_AppendNative(&builder, ZR_NULL, 0u));
    TEST_ASSERT_FALSE(ZrCore_StringBuilder_AppendNative(&builder, "", ZR_MAX_SIZE));
    TEST_ASSERT_NULL(ZrCore_StringBuilder_Freeze(ZR_NULL));
    TEST_ASSERT_FALSE(ZrCore_StringBuilder_Init(state, &overflowBuilder, ZR_MAX_SIZE));

    ZrCore_StringBuilder_Dispose(&builder);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_builder_appends_binary_fragments_and_freezes_exact_bytes);
    RUN_TEST(test_builder_freeze_preserves_string_interning_and_hash_contract);
    RUN_TEST(test_builder_append_string_copies_before_gc_move);
    RUN_TEST(test_builder_rejects_invalid_inputs_without_allocating);
    return UNITY_END();
}
