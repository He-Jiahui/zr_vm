#include "unity.h"

#include "path_support.h"
#include "runtime_support.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/native_registry.h"
#include "../../zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH
#define ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH ""
#endif

static const TZrChar *kDescriptorPluginModuleName = "zr.pluginprobe";

void setUp(void) {}

void tearDown(void) {}

static TZrChar *find_last_path_separator(TZrChar *path) {
    TZrChar *forwardSlash;
    TZrChar *backSlash;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    forwardSlash = strrchr(path, '/');
    backSlash = strrchr(path, '\\');
    if (forwardSlash == ZR_NULL) {
        return backSlash;
    }
    if (backSlash == ZR_NULL) {
        return forwardSlash;
    }
    return forwardSlash > backSlash ? forwardSlash : backSlash;
}

static const TZrChar *path_extension(const TZrChar *path) {
    const TZrChar *cursor;
    const TZrChar *lastDot = ZR_NULL;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            lastDot = ZR_NULL;
            continue;
        }
        if (*cursor == '.') {
            lastDot = cursor;
        }
    }

    return lastDot;
}

static TZrBool copy_binary_file(const TZrChar *sourcePath, const TZrChar *targetPath) {
    TZrBytePtr buffer = ZR_NULL;
    TZrSize length = 0;
    FILE *file;
    size_t written;

    if (!ZrTests_ReadFileBytes(sourcePath, &buffer, &length) ||
        !ZrTests_Path_EnsureParentDirectory(targetPath)) {
        free(buffer);
        return ZR_FALSE;
    }

    file = fopen(targetPath, "wb");
    if (file == ZR_NULL) {
        free(buffer);
        return ZR_FALSE;
    }

    written = fwrite(buffer, 1u, length, file);
    fclose(file);
    free(buffer);
    return written == length ? ZR_TRUE : ZR_FALSE;
}

static TZrBool prepare_descriptor_plugin_project(TZrChar *projectRoot,
                                                 TZrSize projectRootSize,
                                                 TZrChar *pluginPath,
                                                 TZrSize pluginPathSize) {
    TZrChar projectFile[ZR_TESTS_PATH_MAX];
    TZrChar nativeDirectory[ZR_TESTS_PATH_MAX];
    TZrChar pluginFileName[ZR_TESTS_PATH_MAX];
    const TZrChar *pluginExtension;
    TZrChar *lastSeparator;

    if (projectRoot == ZR_NULL || projectRootSize == 0 || pluginPath == ZR_NULL || pluginPathSize == 0 ||
        ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH[0] == '\0' ||
        !ZrTests_File_Exists(ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH) ||
        !ZrTests_Path_GetGeneratedArtifact("native_registry",
                                           "descriptor_invalidation",
                                           "project",
                                           ".zrp",
                                           projectFile,
                                           sizeof(projectFile))) {
        return ZR_FALSE;
    }

    snprintf(projectRoot, projectRootSize, "%s", projectFile);
    lastSeparator = find_last_path_separator(projectRoot);
    if (lastSeparator == ZR_NULL) {
        return ZR_FALSE;
    }
    *lastSeparator = '\0';

    pluginExtension = path_extension(ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH);
    if (pluginExtension == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(pluginFileName, sizeof(pluginFileName), "zrvm_native_zr_pluginprobe%s", pluginExtension);
    ZrLibrary_File_PathJoin(projectRoot, "native", nativeDirectory);
    ZrLibrary_File_PathJoin(nativeDirectory, pluginFileName, pluginPath);
    return copy_binary_file(ZR_VM_DESCRIPTOR_PLUGIN_FIXTURE_INT_PATH, pluginPath);
}

static ZrLibRegisteredModuleRecord *registered_descriptor_plugin_record(SZrGlobalState *global) {
    ZrLibrary_NativeRegistryState *registry = native_registry_get(global);
    TZrSize index;

    TEST_ASSERT_NOT_NULL(registry);
    TEST_ASSERT_TRUE(registry->moduleRecords.isValid);

    for (index = 0; index < registry->moduleRecords.length; index++) {
        ZrLibRegisteredModuleRecord *record =
                (ZrLibRegisteredModuleRecord *)(registry->moduleRecords.head +
                                                index * registry->moduleRecords.elementSize);
        if (record != ZR_NULL &&
            record->moduleName != ZR_NULL &&
            strcmp(record->moduleName, kDescriptorPluginModuleName) == 0) {
            return record;
        }
    }

    TEST_FAIL_MESSAGE("descriptor plugin module was not registered");
    return ZR_NULL;
}

static void test_descriptor_plugin_invalidation_rejects_live_owner_refs(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrLibRegisteredModuleRecord *record;
    ZrLibRegisteredModuleInfo info;
    const TZrChar *errorMessage;
    TZrChar projectRoot[ZR_TESTS_PATH_MAX];
    TZrChar pluginPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_TRUE(prepare_descriptor_plugin_project(projectRoot, sizeof(projectRoot), pluginPath, sizeof(pluginPath)));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(state, projectRoot, kDescriptorPluginModuleName));
    record = registered_descriptor_plugin_record(state->global);
    TEST_ASSERT_NOT_NULL(record->sourcePath);
    snprintf(sourcePath, sizeof(sourcePath), "%s", record->sourcePath);
    record->ownerRefCount = 1u;

    memset(&info, 0, sizeof(info));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(state->global, sourcePath));
    TEST_ASSERT_EQUAL_INT(ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE,
                          ZrLibrary_NativeRegistry_GetLastErrorCode(state->global));
    errorMessage = ZrLibrary_NativeRegistry_GetLastErrorMessage(state->global);
    TEST_ASSERT_NOT_NULL(errorMessage);
    TEST_ASSERT_NOT_NULL(strstr(errorMessage, kDescriptorPluginModuleName));
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(state->global, sourcePath, &info));
    TEST_ASSERT_NOT_NULL(info.descriptor);
    TEST_ASSERT_EQUAL_STRING(kDescriptorPluginModuleName, info.moduleName);
    TEST_ASSERT_EQUAL_UINT32(1u, info.ownerRefCount);

    record->ownerRefCount = 0u;
    TEST_ASSERT_TRUE(ZrLibrary_NativeRegistry_InvalidateDescriptorPluginSource(state->global, sourcePath));
    TEST_ASSERT_FALSE(ZrLibrary_NativeRegistry_GetModuleInfoBySourcePath(state->global, sourcePath, &info));

    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_descriptor_plugin_invalidation_rejects_live_owner_refs);
    return UNITY_END();
}
