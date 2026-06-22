#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#ifndef ZR_VM_TESTS_REPO_ROOT
#define ZR_VM_TESTS_REPO_ROOT "."
#endif

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    char path[1024];
    int written;

    if (relativePath == NULL) {
        return NULL;
    }

    written = snprintf(path, sizeof(path), "%s/%s", ZR_VM_TESTS_REPO_ROOT, relativePath);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return NULL;
    }

    return read_text_file_owned(path);
}

static char *copy_section_owned(const char *text, const char *beginNeedle, const char *endNeedle) {
    const char *begin;
    const char *end;
    size_t length;
    char *section;

    if (text == NULL || beginNeedle == NULL || endNeedle == NULL) {
        return NULL;
    }

    begin = strstr(text, beginNeedle);
    if (begin == NULL) {
        return NULL;
    }

    end = strstr(begin + strlen(beginNeedle), endNeedle);
    if (end == NULL) {
        return NULL;
    }

    length = (size_t)(end - begin);
    section = (char *)malloc(length + 1u);
    if (section == NULL) {
        return NULL;
    }

    memcpy(section, begin, length);
    section[length] = '\0';
    return section;
}

static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    TEST_ASSERT_NOT_NULL(text);
    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void assert_text_contains_none(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    TEST_ASSERT_NOT_NULL(text);
    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) != NULL) {
            printf("Unexpected source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("found forbidden source contract text");
        }
    }
}

static void test_debug_snapshot_stack_reads_use_core_introspection(void) {
    static const char *const requiredSourceNeedles[] = {
            "#include \"zr_vm_core/debug.h\"",
            "ZrCore_Debug_GetStack(agent->state, frameId - 1u,",
            "ZrCore_Debug_GetStack(agent->state, frameCount,",
            "ZrCore_Debug_GetInfo(agent->state,",
    };
    static const char *const forbiddenStackNeedles[] = {
            "agent->state->callInfoList",
            "ZrCore_Closure_GetMetadataFunctionFromCallInfo(agent->state, callInfo)",
    };
    char *sourceText = read_repo_text_file_owned("zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c");
    char *findFrameSection;
    char *readStackSection;

    TEST_ASSERT_NOT_NULL(sourceText);
    findFrameSection = copy_section_owned(sourceText,
                                          "SZrCallInfo *zr_debug_find_call_info_by_frame_id",
                                          "SZrObjectPrototype *zr_debug_resolve_value_prototype");
    readStackSection = copy_section_owned(sourceText,
                                          "TZrBool ZrDebug_ReadStack",
                                          "TZrBool ZrDebug_ReadScopes");

    TEST_ASSERT_NOT_NULL(findFrameSection);
    TEST_ASSERT_NOT_NULL(readStackSection);
    assert_text_contains_all(sourceText, requiredSourceNeedles, ARRAY_COUNT(requiredSourceNeedles));
    assert_text_contains_none(findFrameSection, forbiddenStackNeedles, ARRAY_COUNT(forbiddenStackNeedles));
    assert_text_contains_none(readStackSection, forbiddenStackNeedles, ARRAY_COUNT(forbiddenStackNeedles));

    free(findFrameSection);
    free(readStackSection);
    free(sourceText);
}

static void test_debug_snapshot_variables_use_core_local_and_upvalue_apis(void) {
    static const char *const requiredVariablesNeedles[] = {
            "SZrDebugActivation activation;",
            "ZrCore_Debug_GetLocal(agent->state, &activation,",
            "ZrCore_Debug_GetUpvalue(agent->state,",
            "&previewValue",
    };
    static const char *const forbiddenLocalNeedles[] = {
            "ZrCore_Function_GetLocalVariableName(function, slotIndex, pc)",
            "zr_debug_frame_value_slot(agent->state, function, callInfo, slotIndex)",
    };
    static const char *const forbiddenClosureNeedles[] = {
            "ZrCore_ClosureValue_GetValue(upvalueId)",
    };
    char *sourceText = read_repo_text_file_owned("zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c");
    char *variablesSection;
    char *localSection;
    char *closureSection;

    TEST_ASSERT_NOT_NULL(sourceText);
    variablesSection = copy_section_owned(sourceText, "TZrBool ZrDebug_ReadVariables", "void ZrDebug_Free");
    localSection = copy_section_owned(sourceText,
                                      "if (scopeKind == ZR_DEBUG_SCOPE_KIND_ARGUMENTS ||",
                                      "if (scopeKind == ZR_DEBUG_SCOPE_KIND_CLOSURES)");
    closureSection = copy_section_owned(sourceText,
                                        "if (scopeKind == ZR_DEBUG_SCOPE_KIND_CLOSURES)",
                                        "if (scopeKind == ZR_DEBUG_SCOPE_KIND_GLOBALS)");

    TEST_ASSERT_NOT_NULL(variablesSection);
    TEST_ASSERT_NOT_NULL(localSection);
    TEST_ASSERT_NOT_NULL(closureSection);
    assert_text_contains_all(variablesSection, requiredVariablesNeedles, ARRAY_COUNT(requiredVariablesNeedles));
    assert_text_contains_none(localSection, forbiddenLocalNeedles, ARRAY_COUNT(forbiddenLocalNeedles));
    assert_text_contains_none(closureSection, forbiddenClosureNeedles, ARRAY_COUNT(forbiddenClosureNeedles));

    free(variablesSection);
    free(localSection);
    free(closureSection);
    free(sourceText);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_snapshot_stack_reads_use_core_introspection);
    RUN_TEST(test_debug_snapshot_variables_use_core_local_and_upvalue_apis);
    return UNITY_END();
}
