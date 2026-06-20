#include "unity.h"

#include "harness/path_support.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/zrm.h"

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

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

static TZrBool read_entry_text_contains(const SZrLibrary_ZrmArchive *archive,
                                        const TZrChar *entryName,
                                        const TZrChar *fragment) {
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0;
    TZrBool result;
    TZrChar error[256];

    memset(error, 0, sizeof(error));
    if (!ZrLibrary_Zrm_ReadEntry(archive, entryName, &bytes, &byteCount, error, sizeof(error))) {
        return ZR_FALSE;
    }

    result = byteCount >= strlen(fragment) &&
             strstr((const TZrChar *)bytes, fragment) != ZR_NULL;
    ZrLibrary_Zrm_FreeBytes(bytes);
    return result;
}

static TZrBool write_text_file(const TZrChar *path, const TZrChar *text) {
    return write_bytes_file(path, (const TZrByte *)text, text != ZR_NULL ? strlen(text) : 0U);
}

static TZrBool write_zip_with_entries(const TZrChar *path,
                                      const TZrChar *firstName,
                                      const TZrChar *firstText,
                                      const TZrChar *secondName,
                                      const TZrChar *secondText) {
    mz_zip_archive zip;
    TZrBool success = ZR_FALSE;

    if (path == ZR_NULL || firstName == ZR_NULL || firstText == ZR_NULL ||
        !ZrTests_Path_EnsureParentDirectory(path)) {
        return ZR_FALSE;
    }

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, path, 0)) {
        return ZR_FALSE;
    }

    if (!mz_zip_writer_add_mem(&zip, firstName, firstText, strlen(firstText), MZ_NO_COMPRESSION)) {
        mz_zip_writer_end(&zip);
        return ZR_FALSE;
    }
    if (secondName != ZR_NULL && secondText != ZR_NULL &&
        !mz_zip_writer_add_mem(&zip, secondName, secondText, strlen(secondText), MZ_NO_COMPRESSION)) {
        mz_zip_writer_end(&zip);
        return ZR_FALSE;
    }

    success = mz_zip_writer_finalize_archive(&zip) ? ZR_TRUE : ZR_FALSE;
    mz_zip_writer_end(&zip);
    return success;
}

static TZrBool write_empty_zip_without_manifest(const TZrChar *path) {
    return write_zip_with_entries(path, "payload.txt", "payload", ZR_NULL, ZR_NULL);
}

static TZrBool write_zip_with_manifest_and_entry(const TZrChar *path,
                                                 const TZrChar *manifestText,
                                                 const TZrChar *entryName,
                                                 const TZrChar *entryText) {
    return write_zip_with_entries(path, ZR_LIBRARY_ZRM_MANIFEST_ENTRY, manifestText, entryName, entryText);
}

static void test_zrm_pack_writes_manifest_modules_and_deflated_resources(void) {
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar modulePath[ZR_TESTS_PATH_MAX];
    TZrChar resourcePath[ZR_TESTS_PATH_MAX];
    TZrChar error[512];
    SZrLibrary_ZrmAssemblyInfo assembly;
    SZrLibrary_ZrmPackModule modules[1];
    SZrLibrary_ZrmPackResource resources[1];
    SZrLibrary_ZrmPackRequest request;
    SZrLibrary_ZrmArchive archive;
    const SZrLibrary_ZrmEntryInfo *moduleEntry;
    const SZrLibrary_ZrmEntryInfo *resourceEntry;
    TZrByte *moduleBytes = ZR_NULL;
    TZrByte *resourceBytes = ZR_NULL;
    TZrSize moduleByteCount = 0;
    TZrSize resourceByteCount = 0;
    static const TZrByte fakeModule[] = "single-zro-module-bytes";
    static const TZrByte resourceText[] = "hello from compressed zrm resource\n";

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library", "zrm_container", "math", ".zrm", archivePath, sizeof(archivePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library", "zrm_container", "ops_sum", ".zro", modulePath, sizeof(modulePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library", "zrm_container", "default_config", ".txt", resourcePath, sizeof(resourcePath)));
    TEST_ASSERT_TRUE(write_bytes_file(modulePath, fakeModule, sizeof(fakeModule) - 1u));
    TEST_ASSERT_TRUE(write_bytes_file(resourcePath, resourceText, sizeof(resourceText) - 1u));

    memset(error, 0, sizeof(error));
    memset(&assembly, 0, sizeof(assembly));
    assembly.name = "zr.math";
    assembly.version = "2.1.0";
    assembly.kind = "library";
    assembly.entryModule = "ops/sum";

    memset(modules, 0, sizeof(modules));
    modules[0].moduleKey = "ops/sum";
    modules[0].sourcePath = modulePath;
    modules[0].hash = "module-hash";

    memset(resources, 0, sizeof(resources));
    resources[0].logicalName = "config/default.txt";
    resources[0].sourcePath = resourcePath;
    resources[0].compress = ZR_TRUE;
    resources[0].hash = "resource-hash";

    memset(&request, 0, sizeof(request));
    request.outputPath = archivePath;
    request.assembly = assembly;
    request.modules = modules;
    request.moduleCount = 1;
    request.resources = resources;
    request.resourceCount = 1;

    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error)), error);

    memset(&archive, 0, sizeof(archive));
    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_Zrm_Open(archivePath, &archive, error, sizeof(error)), error);
    TEST_ASSERT_EQUAL_STRING("zr.math", archive.assemblyName);
    TEST_ASSERT_EQUAL_STRING("2.1.0", archive.assemblyVersion);
    TEST_ASSERT_EQUAL_STRING("ops/sum", archive.entryModule);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)archive.moduleCount);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)archive.resourceCount);

    moduleEntry = ZrLibrary_Zrm_FindModule(&archive, "ops/sum");
    resourceEntry = ZrLibrary_Zrm_FindResource(&archive, "config/default.txt");
    TEST_ASSERT_NOT_NULL(moduleEntry);
    TEST_ASSERT_NOT_NULL(resourceEntry);
    TEST_ASSERT_EQUAL_STRING("modules/ops/sum.zro", moduleEntry->entryName);
    TEST_ASSERT_EQUAL_STRING("resources/config/default.txt", resourceEntry->entryName);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_ZRM_COMPRESSION_STORE, moduleEntry->compression);
    TEST_ASSERT_EQUAL_INT(ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE, resourceEntry->compression);

    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_Zrm_ReadEntry(&archive,
                                                     moduleEntry->entryName,
                                                     &moduleBytes,
                                                     &moduleByteCount,
                                                     error,
                                                     sizeof(error)), error);
    TEST_ASSERT_EQUAL_UINT32(sizeof(fakeModule) - 1u, (TZrUInt32)moduleByteCount);
    TEST_ASSERT_EQUAL_MEMORY(fakeModule, moduleBytes, sizeof(fakeModule) - 1u);
    ZrLibrary_Zrm_FreeBytes(moduleBytes);

    TEST_ASSERT_TRUE_MESSAGE(ZrLibrary_Zrm_ReadEntry(&archive,
                                                     resourceEntry->entryName,
                                                     &resourceBytes,
                                                     &resourceByteCount,
                                                     error,
                                                     sizeof(error)), error);
    TEST_ASSERT_EQUAL_UINT32(sizeof(resourceText) - 1u, (TZrUInt32)resourceByteCount);
    TEST_ASSERT_EQUAL_MEMORY(resourceText, resourceBytes, sizeof(resourceText) - 1u);
    ZrLibrary_Zrm_FreeBytes(resourceBytes);

    TEST_ASSERT_TRUE(read_entry_text_contains(&archive, ZR_LIBRARY_ZRM_MANIFEST_ENTRY, "\"format\":\"zr.zrm/v1\""));
    TEST_ASSERT_TRUE(read_entry_text_contains(&archive, ZR_LIBRARY_ZRM_MANIFEST_ENTRY, "\"assembly\""));
    TEST_ASSERT_TRUE(read_entry_text_contains(&archive, ZR_LIBRARY_ZRM_MANIFEST_ENTRY, "\"resources\""));

    ZrLibrary_Zrm_Close(&archive);
}

static void test_zrm_rejects_unsafe_and_duplicate_logical_names(void) {
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar resourcePath[ZR_TESTS_PATH_MAX];
    TZrChar error[512];
    SZrLibrary_ZrmAssemblyInfo assembly;
    SZrLibrary_ZrmPackResource resources[2];
    SZrLibrary_ZrmPackRequest request;
    static const TZrByte resourceText[] = "resource\n";

    TEST_ASSERT_TRUE(ZrLibrary_Zrm_ValidateLogicalName("assets/config.json"));
    TEST_ASSERT_TRUE(ZrLibrary_Zrm_ValidateLogicalName("assets/icons/logo.bin"));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_ValidateLogicalName("../secret.txt"));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_ValidateLogicalName("/absolute.txt"));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_ValidateLogicalName("assets\\bad.txt"));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_ValidateLogicalName("assets/../bad.txt"));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library", "zrm_container", "duplicate", ".zrm", archivePath, sizeof(archivePath)));
    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library", "zrm_container", "resource", ".txt", resourcePath, sizeof(resourcePath)));
    TEST_ASSERT_TRUE(write_bytes_file(resourcePath, resourceText, sizeof(resourceText) - 1u));

    memset(&assembly, 0, sizeof(assembly));
    assembly.name = "zr.assets";
    assembly.version = "1.0.0";
    assembly.kind = "library";
    assembly.entryModule = "main";

    memset(resources, 0, sizeof(resources));
    resources[0].logicalName = "config/default.txt";
    resources[0].sourcePath = resourcePath;
    resources[0].compress = ZR_TRUE;
    resources[1].logicalName = "config/default.txt";
    resources[1].sourcePath = resourcePath;
    resources[1].compress = ZR_TRUE;

    memset(&request, 0, sizeof(request));
    request.outputPath = archivePath;
    request.assembly = assembly;
    request.resources = resources;
    request.resourceCount = 2;

    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_WriteArchive(&request, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "duplicate resource"));
}

static void test_zrm_open_rejects_missing_manifest_and_corrupt_zip(void) {
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar error[512];
    SZrLibrary_ZrmArchive archive;
    static const TZrChar corruptText[] = "this is not a zip archive";

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library",
                                                       "zrm_container",
                                                       "corrupt",
                                                       ".zrm",
                                                       archivePath,
                                                       sizeof(archivePath)));
    TEST_ASSERT_TRUE(write_text_file(archivePath, corruptText));

    memset(&archive, 0, sizeof(archive));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_Open(archivePath, &archive, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "failed to open"));

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library",
                                                       "zrm_container",
                                                       "missing_manifest",
                                                       ".zrm",
                                                       archivePath,
                                                       sizeof(archivePath)));
    TEST_ASSERT_TRUE(write_empty_zip_without_manifest(archivePath));

    memset(&archive, 0, sizeof(archive));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_Open(archivePath, &archive, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "manifest is missing"));
}

static void test_zrm_open_rejects_manifest_path_traversal_entries(void) {
    TZrChar archivePath[ZR_TESTS_PATH_MAX];
    TZrChar error[512];
    SZrLibrary_ZrmArchive archive;
    static const TZrChar manifestText[] =
            "{"
            "\"format\":\"zr.zrm/v1\","
            "\"assembly\":{\"name\":\"evil\",\"version\":\"1.0.0\",\"culture\":\"\",\"publicKeyToken\":\"\",\"kind\":\"library\"},"
            "\"entry\":\"main\","
            "\"modules\":[{\"name\":\"../escape\",\"entry\":\"modules/../escape.zro\",\"hash\":\"\",\"size\":1,\"crc32\":0,\"compression\":\"store\"}],"
            "\"resources\":[]"
            "}";

    TEST_ASSERT_TRUE(ZrTests_Path_GetGeneratedArtifact("library",
                                                       "zrm_container",
                                                       "traversal_manifest",
                                                       ".zrm",
                                                       archivePath,
                                                       sizeof(archivePath)));
    TEST_ASSERT_TRUE(write_zip_with_manifest_and_entry(archivePath,
                                                       manifestText,
                                                       "modules/../escape.zro",
                                                       "x"));

    memset(&archive, 0, sizeof(archive));
    memset(error, 0, sizeof(error));
    TEST_ASSERT_FALSE(ZrLibrary_Zrm_Open(archivePath, &archive, error, sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "unsafe"));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_zrm_pack_writes_manifest_modules_and_deflated_resources);
    RUN_TEST(test_zrm_rejects_unsafe_and_duplicate_logical_names);
    RUN_TEST(test_zrm_open_rejects_missing_manifest_and_corrupt_zip);
    RUN_TEST(test_zrm_open_rejects_manifest_path_traversal_entries);

    return UNITY_END();
}
