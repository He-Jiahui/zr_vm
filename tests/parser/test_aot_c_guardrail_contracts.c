#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "harness/path_support.h"
#include "harness/runtime_support.h"
#include "zr_vm_common/zr_hash_conf.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static SZrFunction *aot_c_guardrail_compile_source(SZrState *state,
                                                   const char *source,
                                                   const char *sourceNameText) {
    SZrString *sourceName;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(sourceNameText);

    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, strlen(sourceNameText));
    TEST_ASSERT_NOT_NULL(sourceName);
    return ZrParser_Source_Compile(state, source, strlen(source), sourceName);
}

static void aot_c_guardrail_write_text_file_or_fail(const TZrChar *path, const char *text) {
    FILE *file;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_TRUE(ZrTests_Path_EnsureParentDirectory(path));

    file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static char *aot_c_guardrail_read_text_file_owned_or_fail(const TZrChar *path) {
    TZrBytePtr bytes = ZR_NULL;
    TZrSize byteLength = 0u;
    char *text;

    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(path, &bytes, &byteLength));
    text = (char *)malloc((size_t)byteLength + 1u);
    TEST_ASSERT_NOT_NULL(text);
    if (byteLength > 0u) {
        memcpy(text, bytes, (size_t)byteLength);
    }
    text[byteLength] = '\0';
    free(bytes);
    return text;
}

static void aot_c_guardrail_hash_file_or_fail(const TZrChar *path, TZrChar *buffer, TZrSize bufferSize) {
    FILE *file;
    TZrByte chunk[ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH];
    TZrUInt64 hash = ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS;
    TZrSize readSize;

    TEST_ASSERT_NOT_NULL(path);
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, bufferSize);

    file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    while ((readSize = fread(chunk, 1u, sizeof(chunk), file)) > 0u) {
        TZrSize index;

        for (index = 0u; index < readSize; index++) {
            hash ^= chunk[index];
            hash *= ZR_STABLE_HASH_FNV1A64_PRIME;
        }
    }
    TEST_ASSERT_TRUE(feof(file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    snprintf(buffer, bufferSize, ZR_STABLE_HASH_HEX_PRINTF_FORMAT, (unsigned long long)hash);
}

static char *aot_c_guardrail_generate_c_text(const char *source,
                                             const char *projectDirectory,
                                             const char *artifactName) {
    const char *projectJson =
            "{"
            "\"name\":\"aot-c-typed-direct-scalar-guardrail\","
            "\"source\":\"src\","
            "\"binary\":\"bin\","
            "\"entry\":\"main\""
            "}";
    SZrState *state;
    SZrFunction *function;
    SZrBinaryWriterOptions binaryOptions;
    SZrAotWriterOptions aotOptions;
    TZrBytePtr embeddedBlob = ZR_NULL;
    TZrSize embeddedBlobLength = 0u;
    TZrChar zroHash[ZR_STABLE_HASH_HEX_BUFFER_LENGTH];
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar zroPath[ZR_TESTS_PATH_MAX];
    TZrChar generatedCPath[ZR_TESTS_PATH_MAX];
    char sourceDirectory[ZR_TESTS_PATH_MAX];
    char binaryDirectory[ZR_TESTS_PATH_MAX];
    char generatedSourceDirectory[ZR_TESTS_PATH_MAX];
    char *generatedCText;
    int written;

    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(projectDirectory);
    TEST_ASSERT_NOT_NULL(artifactName);

    written = snprintf(sourceDirectory, sizeof(sourceDirectory), "%s/src", projectDirectory);
    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_TRUE((size_t)written < sizeof(sourceDirectory));
    written = snprintf(binaryDirectory, sizeof(binaryDirectory), "%s/bin", projectDirectory);
    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_TRUE((size_t)written < sizeof(binaryDirectory));
    written = snprintf(generatedSourceDirectory, sizeof(generatedSourceDirectory), "%s/bin/aot_c/src", projectDirectory);
    TEST_ASSERT_TRUE(written > 0);
    TEST_ASSERT_TRUE((size_t)written < sizeof(generatedSourceDirectory));

    state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(state);
    function = aot_c_guardrail_compile_source(state, source, "guardrail.zr");
    TEST_ASSERT_NOT_NULL(function);

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_guardrail_contracts",
                                                       projectDirectory,
                                                       artifactName,
                                                       ".zrp",
                                                       projectPath,
                                                       sizeof(projectPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_guardrail_contracts",
                                                       sourceDirectory,
                                                       "main",
                                                       ".zr",
                                                       sourcePath,
                                                       sizeof(sourcePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_guardrail_contracts",
                                                       binaryDirectory,
                                                       "main",
                                                       ".zro",
                                                       zroPath,
                                                       sizeof(zroPath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("aot_c_guardrail_contracts",
                                                       generatedSourceDirectory,
                                                       "main",
                                                       ".c",
                                                       generatedCPath,
                                                       sizeof(generatedCPath)));

    aot_c_guardrail_write_text_file_or_fail(projectPath, projectJson);
    aot_c_guardrail_write_text_file_or_fail(sourcePath, source);

    memset(&binaryOptions, 0, sizeof(binaryOptions));
    binaryOptions.moduleName = "main";
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteBinaryFileWithOptions(state, function, zroPath, &binaryOptions));
    aot_c_guardrail_hash_file_or_fail(zroPath, zroHash, sizeof(zroHash));
    TEST_ASSERT_TRUE(ZrTests_ReadFileBytes(zroPath, &embeddedBlob, &embeddedBlobLength));
    TEST_ASSERT_NOT_NULL(embeddedBlob);
    TEST_ASSERT_GREATER_THAN_UINT64(0u, embeddedBlobLength);

    memset(&aotOptions, 0, sizeof(aotOptions));
    aotOptions.moduleName = "main";
    aotOptions.inputKind = ZR_AOT_INPUT_KIND_BINARY;
    aotOptions.inputHash = zroHash;
    aotOptions.embeddedModuleBlob = embeddedBlob;
    aotOptions.embeddedModuleBlobLength = embeddedBlobLength;
    aotOptions.requireExecutableLowering = ZR_TRUE;
    TEST_ASSERT_TRUE(ZrParser_Writer_WriteAotCFileWithOptions(state, function, generatedCPath, &aotOptions));

    generatedCText = aot_c_guardrail_read_text_file_owned_or_fail(generatedCPath);

    free(embeddedBlob);
    ZrCore_Function_Free(state, function);
    ZrTests_Runtime_State_Destroy(state);
    return generatedCText;
}

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
            "ZrCore_Gc_SafePoint(",
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
            "ZrLibrary_AotRuntime_SetMemberNewOwnerNoWriteBarrier(",
            "ZrLibrary_AotRuntime_GetMemberSlot(",
            "ZrLibrary_AotRuntime_SetMemberSlot(",
            "ZrLibrary_AotRuntime_SetMemberSlotNewOwnerNoWriteBarrier(",
            "ZrLibrary_AotRuntime_GetByIndex(",
            "ZrLibrary_AotRuntime_SetByIndex(",
            "ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(",
            "ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(",
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

static const char *aot_c_guardrail_find_stateful_typed_thunk_use(const char *text, const char *functionPrefix) {
    const char *cursor;
    size_t functionPrefixLength;

    if (text == NULL || functionPrefix == NULL) {
        return NULL;
    }

    cursor = text;
    functionPrefixLength = strlen(functionPrefix);
    while ((cursor = strstr(cursor, functionPrefix)) != NULL) {
        const char *lineEnd = strpbrk(cursor, "\r\n");
        const char *openParen = strchr(cursor, '(');
        const char *firstArgument;

        if (openParen != NULL && (lineEnd == NULL || openParen < lineEnd)) {
            firstArgument = openParen + 1;
            while (*firstArgument == ' ' || *firstArgument == '\t') {
                firstArgument++;
            }
            if (strncmp(firstArgument, "struct SZrState *state", strlen("struct SZrState *state")) == 0 ||
                strncmp(firstArgument, "state", strlen("state")) == 0) {
                return cursor;
            }
        }
        cursor += functionPrefixLength;
    }
    return NULL;
}

static const char *aot_c_guardrail_find_matching_body_end(const char *openBrace) {
    const char *cursor;
    int depth = 0;

    if (openBrace == NULL || *openBrace != '{') {
        return NULL;
    }

    for (cursor = openBrace; *cursor != '\0'; cursor++) {
        if (*cursor == '{') {
            depth++;
        } else if (*cursor == '}') {
            depth--;
            if (depth == 0) {
                return cursor;
            }
        }
    }
    return NULL;
}

static const char *aot_c_guardrail_find_forbidden_token_in_function_body(const char *text,
                                                                        const char *functionPrefix,
                                                                        const char *const *tokens,
                                                                        size_t tokenCount) {
    const char *cursor;
    size_t functionPrefixLength;
    size_t tokenIndex;

    if (text == NULL || functionPrefix == NULL || tokens == NULL) {
        return NULL;
    }

    cursor = text;
    functionPrefixLength = strlen(functionPrefix);
    while ((cursor = strstr(cursor, functionPrefix)) != NULL) {
        const char *lineEnd = strpbrk(cursor, "\r\n");
        const char *semicolon = strchr(cursor, ';');
        const char *bodyStart = strchr(cursor, '{');
        const char *bodyEnd;

        if (bodyStart != NULL && (lineEnd == NULL || bodyStart < lineEnd) &&
            (semicolon == NULL || bodyStart < semicolon)) {
            bodyEnd = aot_c_guardrail_find_matching_body_end(bodyStart);
            if (bodyEnd == NULL) {
                return functionPrefix;
            }
            for (tokenIndex = 0u; tokenIndex < tokenCount; tokenIndex++) {
                const char *token = tokens[tokenIndex];
                const char *hit = token != NULL ? strstr(bodyStart, token) : NULL;
                if (hit != NULL && hit < bodyEnd) {
                    return token;
                }
            }
            return NULL;
        }
        cursor += functionPrefixLength;
    }
    return NULL;
}

static const char *aot_c_guardrail_scalar_typed_direct_call_fixture_source(void) {
    return "func add_i64(left: int, right: int): int {\n"
           "    return left + right;\n"
           "}\n"
           "func add_u64(left: uint, right: uint): uint {\n"
           "    return left + right;\n"
           "}\n"
           "func add_f64(left: float, right: float): float {\n"
           "    return left + right;\n"
           "}\n"
           "func both(left: bool, right: bool): bool {\n"
           "    return left && right;\n"
           "}\n"
           "var i64Left: int = 19;\n"
           "var i64Right: int = 23;\n"
           "var u64Left: uint = 11;\n"
           "var u64Right: uint = 31;\n"
           "var f64Left: float = 18.0;\n"
           "var f64Right: float = 24.0;\n"
           "var boolLeft: bool = true;\n"
           "var boolRight: bool = true;\n"
           "var i64Result: int = add_i64(i64Left, i64Right);\n"
           "var u64Result: uint = add_u64(u64Left, u64Right);\n"
           "var f64Result: float = add_f64(f64Left, f64Right);\n"
           "var boolResult: bool = both(boolLeft, boolRight);\n"
           "if (!boolResult) {\n"
           "    return 0;\n"
           "}\n"
           "return i64Result + <int> u64Result + <int> f64Result;";
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
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrCore_Gc_SafePoint(state);"));
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
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetMemberNewOwnerNoWriteBarrier(state, &frame, receiver, member, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_GetMemberSlot(state, &frame, dst, receiver, slot);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetMemberSlot(state, &frame, receiver, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetMemberSlotNewOwnerNoWriteBarrier(state, &frame, receiver, slot, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_GetByIndex(state, &frame, dst, receiver, index);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetByIndex(state, &frame, receiver, index, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SetByIndexNewOwnerNoWriteBarrier(state, &frame, receiver, index, value);"));
    TEST_ASSERT_TRUE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_SuperArraySetIntNewOwnerNoWriteBarrier(state, &frame, receiver, index, value);"));
}

static void test_aot_c_guardrail_rejects_vm_fallback_runtime_calls(void) {
    TEST_ASSERT_FALSE(aot_c_guardrail_runtime_call_allowed("ZrCore_Stack_GetValue(frame.slotBase + 2);"));
    TEST_ASSERT_FALSE(aot_c_guardrail_runtime_call_allowed("ZR_VALUE_FAST_SET(dst, nativeInt64, 42, ZR_VALUE_TYPE_INT64);"));
    TEST_ASSERT_FALSE(aot_c_guardrail_runtime_call_allowed("ZrLibrary_AotRuntime_Add(state, &frame, 2, 0, 1);"));
}

static void test_aot_c_guardrail_generated_scalar_typed_direct_calls_stay_value_free(void) {
    static const char *const requiredTokens[] = {
            "static TZrInt64 zr_aot_typed_i64_fn_",
            "static TZrUInt64 zr_aot_typed_u64_fn_",
            "static TZrFloat64 zr_aot_typed_f64_fn_",
            "static TZrBool zr_aot_typed_bool_fn_",
            "/* zr_aot_static_i64_two_arg_direct_call */",
            "/* zr_aot_static_u64_two_arg_direct_call */",
            "/* zr_aot_static_f64_two_arg_direct_call */",
            "/* zr_aot_static_bool_two_arg_direct_call */",
    };
    static const char *const forbiddenTokens[] = {
            "/* zr_aot_static_i64_two_arg_direct_call_sync_stack_slot */",
            "/* zr_aot_static_u64_two_arg_direct_call_sync_stack_slot */",
            "/* zr_aot_static_f64_two_arg_direct_call_sync_stack_slot */",
            "/* zr_aot_static_bool_two_arg_direct_call_sync_stack_slot */",
            "SZrTypeValue *zr_aot_typed_destination",
            "ZR_VALUE_FAST_SET(zr_aot_typed_destination,",
            "ZrLibrary_AotRuntime_CallStaticDirect(state,",
            "ZrLibrary_AotRuntime_CallStackValue(state,",
    };
    static const char *const typedThunkPrefixes[] = {
            "zr_aot_typed_i64_fn_",
            "zr_aot_typed_u64_fn_",
            "zr_aot_typed_f64_fn_",
            "zr_aot_typed_bool_fn_",
    };
    char *generatedCText;
    size_t index;

    generatedCText = aot_c_guardrail_generate_c_text(aot_c_guardrail_scalar_typed_direct_call_fixture_source(),
                                                     "typed_direct_scalar_guardrail_project",
                                                     "typed_direct_scalar_guardrail");
    for (index = 0u; index < ARRAY_COUNT(requiredTokens); index++) {
        TEST_ASSERT_NOT_NULL(strstr(generatedCText, requiredTokens[index]));
    }
    TEST_ASSERT_NULL(aot_c_guardrail_find_forbidden_token(generatedCText,
                                                          forbiddenTokens,
                                                          ARRAY_COUNT(forbiddenTokens)));
    for (index = 0u; index < ARRAY_COUNT(typedThunkPrefixes); index++) {
        TEST_ASSERT_NULL(aot_c_guardrail_find_stateful_typed_thunk_use(generatedCText, typedThunkPrefixes[index]));
    }

    free(generatedCText);
}

static void test_aot_c_guardrail_generated_scalar_typed_thunk_bodies_stay_environment_free(void) {
    static const char *const typedThunkDefinitionPrefixes[] = {
            "static TZrInt64 zr_aot_typed_i64_fn_",
            "static TZrUInt64 zr_aot_typed_u64_fn_",
            "static TZrFloat64 zr_aot_typed_f64_fn_",
            "static TZrBool zr_aot_typed_bool_fn_",
    };
    static const char *const forbiddenBodyTokens[] = {
            "zr_aot_begin_instruction",
            "SZrCallInfo",
            "frame.",
            "state",
            "state->stackTop",
            "programCounter",
            "publishAllInstructions",
            "Debug_Hook",
            "SZrTypeValue",
            "->ownershipKind",
            "ZR_VALUE_FAST_SET",
            "ZrCore_Stack_GetValue(",
            "ZrCore_Ownership_ReleaseValue",
            "ZrLibrary_AotRuntime_",
    };
    char *generatedCText;
    size_t index;

    generatedCText = aot_c_guardrail_generate_c_text(aot_c_guardrail_scalar_typed_direct_call_fixture_source(),
                                                     "typed_direct_scalar_body_guardrail_project",
                                                     "typed_direct_scalar_body_guardrail");
    for (index = 0u; index < ARRAY_COUNT(typedThunkDefinitionPrefixes); index++) {
        TEST_ASSERT_NOT_NULL(strstr(generatedCText, typedThunkDefinitionPrefixes[index]));
        TEST_ASSERT_NULL(aot_c_guardrail_find_forbidden_token_in_function_body(
                generatedCText,
                typedThunkDefinitionPrefixes[index],
                forbiddenBodyTokens,
                ARRAY_COUNT(forbiddenBodyTokens)));
    }

    free(generatedCText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_guardrail_reports_first_forbidden_vm_fallback_token);
    RUN_TEST(test_aot_c_guardrail_accepts_pure_scalar_c_lowering);
    RUN_TEST(test_aot_c_guardrail_classifies_allowed_runtime_boundary_calls);
    RUN_TEST(test_aot_c_guardrail_rejects_vm_fallback_runtime_calls);
    RUN_TEST(test_aot_c_guardrail_generated_scalar_typed_direct_calls_stay_value_free);
    RUN_TEST(test_aot_c_guardrail_generated_scalar_typed_thunk_bodies_stay_environment_free);
    return UNITY_END();
}
