#include <stddef.h>
#include <string.h>

#include "unity.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static const char *aot_c_guardrail_find_forbidden_token(const char *text,
                                                        const char *const *tokens,
                                                        size_t tokenCount) {
    size_t tokenIndex;

    if (text == NULL || tokens == NULL) {
        return NULL;
    }

    for (tokenIndex = 0u; tokenIndex < tokenCount; tokenIndex++) {
        const char *token = tokens[tokenIndex];
        if (token != NULL && strstr(text, token) != NULL) {
            return token;
        }
    }
    return NULL;
}

static int aot_c_guardrail_has_prefix(const char *text, const char *prefix) {
    size_t prefixLength;

    if (text == NULL || prefix == NULL) {
        return 0;
    }

    prefixLength = strlen(prefix);
    return strncmp(text, prefix, prefixLength) == 0;
}

static int aot_c_guardrail_runtime_call_allowed(const char *callText) {
    static const char *const allowedPrefixes[] = {
            "ZrCore_Gc_WriteBarrier(",
            "ZrCore_Ownership_Retain(",
            "ZrCore_Ownership_Release(",
            "ZrCore_Exception_Throw(",
            "ZrCore_Debug_RunError(",
            "ZrCore_Bridge_BoxTyped(",
            "ZrCore_Bridge_UnboxTyped(",
            "ZrLibrary_AotRuntime_ReturnI64(",
            "ZrLibrary_AotRuntime_ReturnBool(",
            "ZrLibrary_AotRuntime_ReturnU64(",
            "ZrLibrary_AotRuntime_ReturnF64(",
            "ZrLibrary_AotRuntime_ReturnInlineStruct(",
            "ZrLibrary_AotRuntime_SyncSignedIntLocal(",
            "ZrLibrary_AotRuntime_SyncUnsignedIntLocal(",
            "ZrLibrary_AotRuntime_SyncFloatLocal(",
            "ZrLibrary_AotRuntime_SyncBoolLocal(",
            "ZrLibrary_AotRuntime_CallStackValue(",
            "ZrLibrary_AotRuntime_CallStaticDirect(",
            "ZrLibrary_AotRuntime_CallInlineStruct(",
            "ZrLibrary_AotRuntime_CallDynamicDeoptBridge(",
            "ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(",
            "ZrLibrary_AotRuntime_GetMember(",
            "ZrLibrary_AotRuntime_SetMember(",
            "ZrLibrary_AotRuntime_GetMemberSlot(",
            "ZrLibrary_AotRuntime_SetMemberSlot(",
            "ZrLibrary_AotRuntime_GetByIndex(",
            "ZrLibrary_AotRuntime_SetByIndex(",
    };
    size_t index;

    if (callText == NULL) {
        return 0;
    }

    for (index = 0u; index < ARRAY_COUNT(allowedPrefixes); index++) {
        if (aot_c_guardrail_has_prefix(callText, allowedPrefixes[index])) {
            return 1;
        }
    }
    return 0;
}

static void test_aot_c_guardrail_reports_first_forbidden_vm_fallback_token(void) {
    static const char *const forbiddenTokens[] = {
            "ZrCore_Stack_GetValue(",
            "ZR_VALUE_FAST_SET(",
    };
    const char *generatedC =
            "static TZrInt64 zr_fn(SZrState *state) {\n"
            "    SZrTypeValue *dst = ZrCore_Stack_GetValue(frame.slotBase + 2);\n"
            "    ZR_VALUE_FAST_SET(dst, nativeInt64, 42, ZR_VALUE_TYPE_INT64);\n"
            "    return 42;\n"
            "}\n";

    TEST_ASSERT_EQUAL_STRING("ZrCore_Stack_GetValue(",
                             aot_c_guardrail_find_forbidden_token(generatedC,
                                                                  forbiddenTokens,
                                                                  ARRAY_COUNT(forbiddenTokens)));
}

static void test_aot_c_guardrail_accepts_pure_scalar_c_lowering(void) {
    static const char *const forbiddenTokens[] = {
            "ZrCore_Stack_GetValue(",
            "ZR_VALUE_FAST_SET(",
            "ZrLibrary_AotRuntime_Add(state, &frame",
    };
    const char *generatedC =
            "static TZrInt64 zr_fn(SZrState *state, TZrInt64 s0, TZrInt64 s1) {\n"
            "    TZrInt64 s2 = s0 + s1;\n"
            "    return s2;\n"
            "}\n";

    TEST_ASSERT_NULL(aot_c_guardrail_find_forbidden_token(generatedC,
                                                          forbiddenTokens,
                                                          ARRAY_COUNT(forbiddenTokens)));
}

static void test_aot_c_guardrail_classifies_allowed_runtime_boundary_calls(void) {
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrCore_Gc_WriteBarrier(state, owner, newref);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrCore_Ownership_Retain(state, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrCore_Exception_Throw(state, state->currentExceptionStatus);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s0);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b0);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u0);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f0);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_ReturnInlineStruct(state, source, size);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_CallStackValue(state, &frame, callee, args);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_CallStaticDirect(state, &frame, functionIndex, args);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_CallInlineStruct(state, &frame, callee, args);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state, &frame, deoptId, callee, args);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(state, &frame, deoptId, kind);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_GetMember(state, &frame, dst, receiver, member);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetMember(state, &frame, receiver, member, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_GetMemberSlot(state, &frame, dst, receiver, slot);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetMemberSlot(state, &frame, receiver, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_GetByIndex(state, &frame, dst, receiver, index);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetByIndex(state, &frame, receiver, index, value);"));
}

static void test_aot_c_guardrail_rejects_vm_fallback_runtime_calls(void) {
    TEST_ASSERT_FALSE(aot_c_guardrail_runtime_call_allowed("ZrCore_Stack_GetValue(frame.slotBase + 2);"));
    TEST_ASSERT_FALSE(aot_c_guardrail_runtime_call_allowed("ZR_VALUE_FAST_SET(dst, nativeInt64, 42, ZR_VALUE_TYPE_INT64);"));
    TEST_ASSERT_FALSE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_Add(state, &frame, 2, 0, 1);"));
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_guardrail_reports_first_forbidden_vm_fallback_token);
    RUN_TEST(test_aot_c_guardrail_accepts_pure_scalar_c_lowering);
    RUN_TEST(test_aot_c_guardrail_classifies_allowed_runtime_boundary_calls);
    RUN_TEST(test_aot_c_guardrail_rejects_vm_fallback_runtime_calls);
    return UNITY_END();
}
