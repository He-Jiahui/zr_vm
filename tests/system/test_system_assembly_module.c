//
// zr.system.assembly resource access tests.
//

#include "unity.h"

#include "harness/path_support.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library/common_state.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_library/zrm.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static TZrBool write_bytes_file(const TZrChar *path, const TZrByte *bytes, TZrSize byteCount) {
    FILE *file;
    size_t written;

    if (path == ZR_NULL || bytes == ZR_NULL || !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    file = fopen(path, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    written = fwrite(bytes, 1, byteCount, file);
    fclose(file);
    return written == byteCount;
}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *text) {
    return write_bytes_file(path, (const TZrByte *)text, text != ZR_NULL ? strlen(text) : 0U);
}

static TZrBool join_path_suffix_checked(const TZrChar *base,
                                        const TZrChar *suffix,
                                        TZrChar *buffer,
                                        TZrSize bufferSize) {
    int written;

    if (base == ZR_NULL || suffix == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    written = snprintf(buffer, bufferSize, "%s%s", base, suffix);
    return written >= 0 && (TZrSize)written + 1U <= bufferSize;
}

static const TZrChar *string_value_text(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

static TZrBool bool_value_is(const SZrTypeValue *value, TZrBool expected) {
    return value != ZR_NULL && value->type == ZR_VALUE_TYPE_BOOL &&
           (value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE) == expected;
}

static TZrBool array_matches_bytes(SZrState *state,
                                   const SZrTypeValue *value,
                                   const TZrByte *expected,
                                   TZrSize expectedCount) {
    SZrObject *array;
    TZrSize index;

    if (state == ZR_NULL || value == ZR_NULL || expected == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_ARRAY || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    array = ZR_CAST_OBJECT(state, value->value.object);
    if (array == ZR_NULL || ZrLib_Array_Length(array) != expectedCount) {
        return ZR_FALSE;
    }

    for (index = 0; index < expectedCount; index++) {
        const SZrTypeValue *entry = ZrLib_Array_Get(state, array, index);
        TZrInt64 actual;
        if (entry == ZR_NULL || !ZR_VALUE_IS_TYPE_SIGNED_INT(entry->type)) {
            return ZR_FALSE;
        }
        actual = entry->value.nativeObject.nativeInt64;
        if (actual < 0 || actual > 255 || (TZrByte)actual != expected[index]) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool create_assembly_resource_fixture(TZrChar *projectPath,
                                                TZrSize projectPathSize,
                                                const TZrByte *resourceBytes,
                                                TZrSize resourceByteCount) {
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    TZrChar resourcePath[ZR_TESTS_PATH_MAX];
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    SZrLibrary_ZrmAssemblyInfo assembly;
    SZrLibrary_ZrmPackModule modules[1];
    SZrLibrary_ZrmPackResource resources[1];
    SZrLibrary_ZrmPackRequest request;
    static const TZrByte fakeModule[] = "assembly-resource-test-zro";
    static const TZrChar projectJson[] =
            "{\n"
            "  \"manifestVersion\": 1,\n"
            "  \"name\": \"assembly-resource-test\",\n"
            "  \"assembly\": { \"name\": \"assembly-resource-test\", \"version\": \"1.0.0\", \"output\": \"dist/app.zrm\" },\n"
            "  \"source\": \"src\",\n"
            "  \"binary\": \"bin\",\n"
            "  \"entry\": \"main\"\n"
            "}\n";

    if (projectPath == ZR_NULL || projectPathSize == 0 || resourceBytes == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrTests_Path_GetGeneratedArtifact("system",
                                           "assembly_runtime",
                                           "resource_project",
                                           "",
                                           rootPath,
                                           sizeof(rootPath))) {
        return ZR_FALSE;
    }

    if (!join_path_suffix_checked(rootPath, "/app.zrp", projectPath, projectPathSize) ||
        !join_path_suffix_checked(rootPath, "/bin/main.zro", modulePath, sizeof(modulePath)) ||
        !join_path_suffix_checked(rootPath, "/resources/config/default.txt", resourcePath, sizeof(resourcePath)) ||
        !join_path_suffix_checked(rootPath, "/dist/app.zrm", archivePath, sizeof(archivePath))) {
        return ZR_FALSE;
    }

    if (!write_text_file(projectPath, projectJson) ||
        !write_bytes_file(modulePath, fakeModule, sizeof(fakeModule) - 1U) ||
        !write_bytes_file(resourcePath, resourceBytes, resourceByteCount)) {
        return ZR_FALSE;
    }

    memset(&assembly, 0, sizeof(assembly));
    assembly.name = "assembly-resource-test";
    assembly.version = "1.0.0";
    assembly.kind = "library";
    assembly.entryModule = "main";

    memset(modules, 0, sizeof(modules));
    modules[0].moduleKey = "main";
    modules[0].sourcePath = modulePath;

    memset(resources, 0, sizeof(resources));
    resources[0].logicalName = "config/default.txt";
    resources[0].sourcePath = resourcePath;
    resources[0].compress = ZR_TRUE;

    memset(&request, 0, sizeof(request));
    request.outputPath = archivePath;
    request.assembly = assembly;
    request.modules = modules;
    request.moduleCount = 1;
    request.resources = resources;
    request.resourceCount = 1;

    memset(error, 0, sizeof(error));
    return ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error));
}

static void test_system_assembly_module_links_and_exports_resource_api(void) {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    static const TZrByte resourceText[] = "assembly resource payload\n";
    SZrGlobalState *global;
    SZrState *state;
    SZrString *rootPath;
    SZrString *assemblyPath;
    SZrObjectModule *rootModule;
    SZrObjectModule *assemblyModule;

    TEST_ASSERT_TRUE(create_assembly_resource_fixture(projectPath,
                                                      sizeof(projectPath),
                                                      resourceText,
                                                      sizeof(resourceText) - 1U));
    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    rootPath = ZrCore_String_CreateFromNative(state, "zr.system");
    assemblyPath = ZrCore_String_CreateFromNative(state, "zr.system.assembly");
    TEST_ASSERT_NOT_NULL(rootPath);
    TEST_ASSERT_NOT_NULL(assemblyPath);

    rootModule = ZrCore_Module_ImportByPath(state, rootPath);
    assemblyModule = ZrCore_Module_ImportByPath(state, assemblyPath);
    TEST_ASSERT_NOT_NULL(rootModule);
    TEST_ASSERT_NOT_NULL(assemblyModule);

    TEST_ASSERT_NOT_NULL(ZrLib_Module_GetExport(state, "zr.system", "assembly"));
    TEST_ASSERT_NOT_NULL(ZrLib_Module_GetExport(state, "zr.system.assembly", "resourceExists"));
    TEST_ASSERT_NOT_NULL(ZrLib_Module_GetExport(state, "zr.system.assembly", "readResourceText"));
    TEST_ASSERT_NOT_NULL(ZrLib_Module_GetExport(state, "zr.system.assembly", "readResourceBytes"));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

static void test_system_assembly_reads_current_project_zrm_resources(void) {
    TZrChar projectPath[ZR_TESTS_PATH_MAX];
    static const TZrByte resourceText[] = "assembly resource payload\n";
    SZrGlobalState *global;
    SZrState *state;
    SZrTypeValue argument;
    SZrTypeValue result;

    TEST_ASSERT_TRUE(create_assembly_resource_fixture(projectPath,
                                                      sizeof(projectPath),
                                                      resourceText,
                                                      sizeof(resourceText) - 1U));
    global = ZrLibrary_CommonState_CommonGlobalState_New(projectPath);
    TEST_ASSERT_NOT_NULL(global);
    TEST_ASSERT_TRUE(ZrVmLibSystem_Register(global));
    state = global->mainThreadState;
    TEST_ASSERT_NOT_NULL(state);

    ZrLib_Value_SetString(state, &argument, "config/default.txt");
    ZrLib_Value_SetNull(&result);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state,
                                            "zr.system.assembly",
                                            "resourceExists",
                                            &argument,
                                            1,
                                            &result));
    TEST_ASSERT_TRUE(bool_value_is(&result, ZR_TRUE));

    ZrLib_Value_SetString(state, &argument, "config/missing.txt");
    ZrLib_Value_SetNull(&result);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state,
                                            "zr.system.assembly",
                                            "resourceExists",
                                            &argument,
                                            1,
                                            &result));
    TEST_ASSERT_TRUE(bool_value_is(&result, ZR_FALSE));

    ZrLib_Value_SetString(state, &argument, "config/default.txt");
    ZrLib_Value_SetNull(&result);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state,
                                            "zr.system.assembly",
                                            "readResourceText",
                                            &argument,
                                            1,
                                            &result));
    TEST_ASSERT_EQUAL_STRING((const TZrChar *)resourceText, string_value_text(state, &result));

    ZrLib_Value_SetString(state, &argument, "config/default.txt");
    ZrLib_Value_SetNull(&result);
    TEST_ASSERT_TRUE(ZrLib_CallModuleExport(state,
                                            "zr.system.assembly",
                                            "readResourceBytes",
                                            &argument,
                                            1,
                                            &result));
    TEST_ASSERT_TRUE(array_matches_bytes(state, &result, resourceText, sizeof(resourceText) - 1U));

    ZrLibrary_CommonState_CommonGlobalState_Free(global);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_system_assembly_module_links_and_exports_resource_api);
    RUN_TEST(test_system_assembly_reads_current_project_zrm_resources);

    return UNITY_END();
}
