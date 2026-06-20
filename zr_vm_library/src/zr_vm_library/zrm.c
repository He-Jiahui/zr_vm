#include "zr_vm_library/zrm.h"

#include "cJSON/cJSON.h"
#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include "miniz.h"
#include "miniz_zip.h"
#include "zr_vm_library/file.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

#define ZR_LIBRARY_ZRM_ZIP_METHOD_STORE 0U
#define ZR_LIBRARY_ZRM_ZIP_METHOD_DEFLATE 8U

static void zrm_set_error(TZrChar *errorBuffer, TZrSize errorBufferSize, const TZrChar *format, ...) {
    va_list arguments;

    if (errorBuffer == ZR_NULL || errorBufferSize == 0) {
        return;
    }

    errorBuffer[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    vsnprintf(errorBuffer, errorBufferSize, format, arguments);
    va_end(arguments);
}

static TZrBool zrm_copy_text(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    if (text == ZR_NULL) {
        return ZR_TRUE;
    }

    length = strlen(text);
    if (length + 1 > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer, text, length + 1);
    return ZR_TRUE;
}

static const TZrChar *zrm_json_string(cJSON *object, const TZrChar *name) {
    cJSON *item;

    if (object == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring != ZR_NULL ? item->valuestring : ZR_NULL;
}

static TZrBool zrm_read_file(const TZrChar *path, TZrByte **outBytes, TZrSize *outByteCount) {
    FILE *file;
    long fileSize;
    TZrByte *bytes;
    size_t readSize;

    if (outBytes != ZR_NULL) {
        *outBytes = ZR_NULL;
    }
    if (outByteCount != ZR_NULL) {
        *outByteCount = 0;
    }
    if (path == ZR_NULL || outBytes == ZR_NULL || outByteCount == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_FALSE;
    }
    fileSize = ftell(file);
    if (fileSize < 0) {
        fclose(file);
        return ZR_FALSE;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    bytes = (TZrByte *)malloc(fileSize > 0 ? (size_t)fileSize : 1u);
    if (bytes == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    readSize = fileSize > 0 ? fread(bytes, 1, (size_t)fileSize, file) : 0;
    fclose(file);
    if (readSize != (size_t)fileSize) {
        free(bytes);
        return ZR_FALSE;
    }

    *outBytes = bytes;
    *outByteCount = (TZrSize)fileSize;
    return ZR_TRUE;
}

static TZrUInt32 zrm_crc32(const TZrByte *bytes, TZrSize byteCount) {
    return (TZrUInt32)mz_crc32(MZ_CRC32_INIT, bytes, byteCount);
}

static TZrBool zrm_add_manifest_string(cJSON *object, const TZrChar *name, const TZrChar *value) {
    if (object == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (value == ZR_NULL) {
        return cJSON_AddNullToObject(object, name) != ZR_NULL;
    }
    return cJSON_AddStringToObject(object, name, value) != ZR_NULL;
}

static TZrBool zrm_add_manifest_entry(cJSON *array,
                                      const TZrChar *logicalName,
                                      const TZrChar *entryName,
                                      const TZrChar *hash,
                                      TZrUInt64 size,
                                      TZrUInt32 crc32,
                                      EZrLibrary_ZrmCompression compression) {
    cJSON *entry;

    if (array == ZR_NULL || logicalName == ZR_NULL || entryName == ZR_NULL) {
        return ZR_FALSE;
    }

    entry = cJSON_CreateObject();
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zrm_add_manifest_string(entry, "name", logicalName) ||
        !zrm_add_manifest_string(entry, "entry", entryName) ||
        !zrm_add_manifest_string(entry, "hash", hash != ZR_NULL ? hash : "") ||
        cJSON_AddNumberToObject(entry, "size", (double)size) == ZR_NULL ||
        cJSON_AddNumberToObject(entry, "crc32", (double)crc32) == ZR_NULL ||
        !zrm_add_manifest_string(entry,
                                 "compression",
                                 compression == ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE ? "deflate" : "store")) {
        cJSON_Delete(entry);
        return ZR_FALSE;
    }

    cJSON_AddItemToArray(array, entry);
    return ZR_TRUE;
}

static TZrBool zrm_archive_entry_duplicate(const SZrLibrary_ZrmPackResource *resources,
                                           TZrSize resourceCount,
                                           TZrSize currentIndex) {
    if (resources == ZR_NULL || currentIndex >= resourceCount || resources[currentIndex].logicalName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < currentIndex; index++) {
        if (resources[index].logicalName != ZR_NULL &&
            strcmp(resources[index].logicalName, resources[currentIndex].logicalName) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zrm_module_duplicate(const SZrLibrary_ZrmPackModule *modules, TZrSize moduleCount, TZrSize currentIndex) {
    if (modules == ZR_NULL || currentIndex >= moduleCount || modules[currentIndex].moduleKey == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < currentIndex; index++) {
        if (modules[index].moduleKey != ZR_NULL &&
            strcmp(modules[index].moduleKey, modules[currentIndex].moduleKey) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zrm_prepare_parent_directory(const TZrChar *path) {
    TZrChar directory[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (path == ZR_NULL || !ZrLibrary_File_GetDirectory(path, directory)) {
        return ZR_FALSE;
    }
    if (directory[0] == '\0') {
        return ZR_TRUE;
    }
    return ZrLibrary_File_CreateDirectories(directory);
}

static TZrBool zrm_apply_zip_stat(mz_zip_archive *zip,
                                  SZrLibrary_ZrmEntryInfo *entry,
                                  TZrChar *errorBuffer,
                                  TZrSize errorBufferSize) {
    int fileIndex;
    mz_zip_archive_file_stat stat;

    if (zip == ZR_NULL || entry == ZR_NULL || entry->entryName[0] == '\0') {
        return ZR_FALSE;
    }

    fileIndex = mz_zip_reader_locate_file(zip, entry->entryName, ZR_NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex < 0) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm entry '%s' missing from zip", entry->entryName);
        return ZR_FALSE;
    }
    memset(&stat, 0, sizeof(stat));
    if (!mz_zip_reader_file_stat(zip, (mz_uint)fileIndex, &stat)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to stat entry '%s'", entry->entryName);
        return ZR_FALSE;
    }

    entry->uncompressedSize = (TZrUInt64)stat.m_uncomp_size;
    entry->compressedSize = (TZrUInt64)stat.m_comp_size;
    entry->crc32 = (TZrUInt32)stat.m_crc32;
    entry->compression = stat.m_method == ZR_LIBRARY_ZRM_ZIP_METHOD_DEFLATE ? ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE
                                                                             : ZR_LIBRARY_ZRM_COMPRESSION_STORE;
    return ZR_TRUE;
}

static TZrBool zrm_parse_entry_array(cJSON *array,
                                     TZrBool modules,
                                     SZrLibrary_ZrmEntryInfo **outEntries,
                                     TZrSize *outCount,
                                     TZrChar *errorBuffer,
                                     TZrSize errorBufferSize) {
    TZrSize count;
    TZrSize index = 0;
    SZrLibrary_ZrmEntryInfo *entries;
    cJSON *item;

    if (outEntries != ZR_NULL) {
        *outEntries = ZR_NULL;
    }
    if (outCount != ZR_NULL) {
        *outCount = 0;
    }
    if (outEntries == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }
    if (array == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!cJSON_IsArray(array)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest entry list is not an array");
        return ZR_FALSE;
    }

    count = (TZrSize)cJSON_GetArraySize(array);
    if (count == 0) {
        return ZR_TRUE;
    }

    entries = (SZrLibrary_ZrmEntryInfo *)calloc(count, sizeof(*entries));
    if (entries == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to allocate entry index");
        return ZR_FALSE;
    }

    cJSON_ArrayForEach(item, array) {
        const TZrChar *name = zrm_json_string(item, "name");
        const TZrChar *entryName = zrm_json_string(item, "entry");
        const TZrChar *hash = zrm_json_string(item, "hash");
        const TZrChar *compression = zrm_json_string(item, "compression");
        TZrChar expectedEntryName[ZR_LIBRARY_MAX_PATH_LENGTH];

        if (!cJSON_IsObject(item) ||
            name == ZR_NULL ||
            entryName == ZR_NULL ||
            !zrm_copy_text(entries[index].logicalName, sizeof(entries[index].logicalName), name) ||
            !zrm_copy_text(entries[index].entryName, sizeof(entries[index].entryName), entryName) ||
            !zrm_copy_text(entries[index].hash, sizeof(entries[index].hash), hash != ZR_NULL ? hash : "")) {
            free(entries);
            zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest contains an invalid entry");
            return ZR_FALSE;
        }

        if (!(modules ? ZrLibrary_Zrm_BuildModuleEntryName(name, expectedEntryName, sizeof(expectedEntryName))
                      : ZrLibrary_Zrm_BuildResourceEntryName(name, expectedEntryName, sizeof(expectedEntryName))) ||
            strcmp(entryName, expectedEntryName) != 0) {
            free(entries);
            zrm_set_error(errorBuffer,
                          errorBufferSize,
                          "zrm manifest contains an unsafe %s entry",
                          modules ? "module" : "resource");
            return ZR_FALSE;
        }

        if (compression != ZR_NULL && strcmp(compression, "store") != 0 && strcmp(compression, "deflate") != 0) {
            free(entries);
            zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest contains an invalid compression mode");
            return ZR_FALSE;
        }

        entries[index].compression = compression != ZR_NULL && strcmp(compression, "deflate") == 0
                                             ? ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE
                                             : ZR_LIBRARY_ZRM_COMPRESSION_STORE;
        index++;
    }

    *outEntries = entries;
    *outCount = count;
    return ZR_TRUE;
}

TZrBool ZrLibrary_Zrm_ValidateLogicalName(const TZrChar *name) {
    TZrSize segmentStart = 0;
    TZrSize index;

    if (name == ZR_NULL || name[0] == '\0' || name[0] == '/' || name[0] == '\\') {
        return ZR_FALSE;
    }

    for (index = 0;; index++) {
        TZrChar current = name[index];
        if (current == '\\' || current == ':' || current == '\r' || current == '\n' || current == '\t') {
            return ZR_FALSE;
        }

        if (current == '/' || current == '\0') {
            TZrSize segmentLength = index - segmentStart;
            if (segmentLength == 0) {
                return ZR_FALSE;
            }
            if ((segmentLength == 1 && name[segmentStart] == '.') ||
                (segmentLength == 2 && name[segmentStart] == '.' && name[segmentStart + 1] == '.')) {
                return ZR_FALSE;
            }
            if (current == '\0') {
                return ZR_TRUE;
            }
            segmentStart = index + 1;
        }
    }
}

TZrBool ZrLibrary_Zrm_BuildModuleEntryName(const TZrChar *moduleKey, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize prefixLength;
    TZrSize moduleLength;
    TZrSize extensionLength;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    if (!ZrLibrary_Zrm_ValidateLogicalName(moduleKey)) {
        return ZR_FALSE;
    }

    prefixLength = strlen(ZR_LIBRARY_ZRM_MODULE_ENTRY_PREFIX);
    moduleLength = strlen(moduleKey);
    extensionLength = strlen(ZR_VM_BINARY_MODULE_FILE_EXTENSION);
    if (prefixLength + moduleLength + extensionLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, ZR_LIBRARY_ZRM_MODULE_ENTRY_PREFIX, prefixLength);
    memcpy(buffer + prefixLength, moduleKey, moduleLength);
    memcpy(buffer + prefixLength + moduleLength, ZR_VM_BINARY_MODULE_FILE_EXTENSION, extensionLength + 1);
    return ZR_TRUE;
}

TZrBool ZrLibrary_Zrm_BuildResourceEntryName(const TZrChar *logicalName, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize prefixLength;
    TZrSize nameLength;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    if (!ZrLibrary_Zrm_ValidateLogicalName(logicalName)) {
        return ZR_FALSE;
    }

    prefixLength = strlen(ZR_LIBRARY_ZRM_RESOURCE_ENTRY_PREFIX);
    nameLength = strlen(logicalName);
    if (prefixLength + nameLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, ZR_LIBRARY_ZRM_RESOURCE_ENTRY_PREFIX, prefixLength);
    memcpy(buffer + prefixLength, logicalName, nameLength + 1);
    return ZR_TRUE;
}

TZrBool ZrLibrary_Zrm_WriteArchive(const SZrLibrary_ZrmPackRequest *request,
                                   TZrChar *errorBuffer,
                                   TZrSize errorBufferSize) {
    mz_zip_archive zip;
    cJSON *manifest = ZR_NULL;
    cJSON *assembly = ZR_NULL;
    cJSON *modulesJson = ZR_NULL;
    cJSON *resourcesJson = ZR_NULL;
    TZrChar **moduleEntryNames = ZR_NULL;
    TZrChar **resourceEntryNames = ZR_NULL;
    TZrByte **moduleBytes = ZR_NULL;
    TZrByte **resourceBytes = ZR_NULL;
    TZrSize *moduleByteCounts = ZR_NULL;
    TZrSize *resourceByteCounts = ZR_NULL;
    TZrBool ok = ZR_FALSE;
    TZrBool writerInitialized = ZR_FALSE;
    TZrChar *manifestText = ZR_NULL;

    zrm_set_error(errorBuffer, errorBufferSize, ZR_NULL);
    if (request == ZR_NULL || request->outputPath == ZR_NULL || request->assembly.name == ZR_NULL ||
        request->assembly.version == ZR_NULL || request->assembly.entryModule == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm pack request is missing required assembly fields");
        return ZR_FALSE;
    }
    if (!ZrLibrary_Zrm_ValidateLogicalName(request->assembly.entryModule)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm entry module '%s' is invalid", request->assembly.entryModule);
        return ZR_FALSE;
    }

    moduleEntryNames = request->moduleCount > 0 ? (TZrChar **)calloc(request->moduleCount, sizeof(*moduleEntryNames)) : ZR_NULL;
    resourceEntryNames = request->resourceCount > 0 ? (TZrChar **)calloc(request->resourceCount, sizeof(*resourceEntryNames)) : ZR_NULL;
    moduleBytes = request->moduleCount > 0 ? (TZrByte **)calloc(request->moduleCount, sizeof(*moduleBytes)) : ZR_NULL;
    resourceBytes = request->resourceCount > 0 ? (TZrByte **)calloc(request->resourceCount, sizeof(*resourceBytes)) : ZR_NULL;
    moduleByteCounts = request->moduleCount > 0 ? (TZrSize *)calloc(request->moduleCount, sizeof(*moduleByteCounts)) : ZR_NULL;
    resourceByteCounts = request->resourceCount > 0 ? (TZrSize *)calloc(request->resourceCount, sizeof(*resourceByteCounts)) : ZR_NULL;
    if ((request->moduleCount > 0 && (moduleEntryNames == ZR_NULL || moduleBytes == ZR_NULL || moduleByteCounts == ZR_NULL)) ||
        (request->resourceCount > 0 && (resourceEntryNames == ZR_NULL || resourceBytes == ZR_NULL || resourceByteCounts == ZR_NULL))) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to allocate pack buffers");
        goto cleanup;
    }

    manifest = cJSON_CreateObject();
    assembly = cJSON_CreateObject();
    modulesJson = cJSON_CreateArray();
    resourcesJson = cJSON_CreateArray();
    if (manifest == ZR_NULL || assembly == ZR_NULL || modulesJson == ZR_NULL || resourcesJson == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to allocate manifest");
        goto cleanup;
    }

    if (!zrm_add_manifest_string(manifest, "format", ZR_LIBRARY_ZRM_FORMAT) ||
        !zrm_add_manifest_string(assembly, "name", request->assembly.name) ||
        !zrm_add_manifest_string(assembly, "version", request->assembly.version) ||
        !zrm_add_manifest_string(assembly, "culture", request->assembly.culture) ||
        !zrm_add_manifest_string(assembly, "publicKeyToken", request->assembly.publicKeyToken) ||
        !zrm_add_manifest_string(assembly, "kind", request->assembly.kind != ZR_NULL ? request->assembly.kind : "library") ||
        !zrm_add_manifest_string(manifest, "entry", request->assembly.entryModule)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to build manifest identity");
        goto cleanup;
    }
    cJSON_AddItemToObject(manifest, "assembly", assembly);
    assembly = ZR_NULL;

    for (TZrSize index = 0; index < request->moduleCount; index++) {
        const SZrLibrary_ZrmPackModule *module = &request->modules[index];
        moduleEntryNames[index] = (TZrChar *)calloc(ZR_LIBRARY_MAX_PATH_LENGTH, sizeof(TZrChar));
        if (module == ZR_NULL ||
            module->moduleKey == ZR_NULL ||
            module->sourcePath == ZR_NULL ||
            moduleEntryNames[index] == ZR_NULL ||
            zrm_module_duplicate(request->modules, request->moduleCount, index) ||
            !ZrLibrary_Zrm_BuildModuleEntryName(module->moduleKey,
                                                moduleEntryNames[index],
                                                ZR_LIBRARY_MAX_PATH_LENGTH) ||
            !zrm_read_file(module->sourcePath, &moduleBytes[index], &moduleByteCounts[index]) ||
            !zrm_add_manifest_entry(modulesJson,
                                    module->moduleKey,
                                    moduleEntryNames[index],
                                    module->hash,
                                    moduleByteCounts[index],
                                    zrm_crc32(moduleBytes[index], moduleByteCounts[index]),
                                    ZR_LIBRARY_ZRM_COMPRESSION_STORE)) {
            zrm_set_error(errorBuffer,
                          errorBufferSize,
                          "zrm failed to add module '%s'",
                          module != ZR_NULL && module->moduleKey != ZR_NULL ? module->moduleKey : "<null>");
            goto cleanup;
        }
    }

    for (TZrSize index = 0; index < request->resourceCount; index++) {
        const SZrLibrary_ZrmPackResource *resource = &request->resources[index];
        resourceEntryNames[index] = (TZrChar *)calloc(ZR_LIBRARY_MAX_PATH_LENGTH, sizeof(TZrChar));
        if (resource == ZR_NULL ||
            resource->logicalName == ZR_NULL ||
            resource->sourcePath == ZR_NULL ||
            resourceEntryNames[index] == ZR_NULL ||
            !ZrLibrary_Zrm_ValidateLogicalName(resource->logicalName) ||
            zrm_archive_entry_duplicate(request->resources, request->resourceCount, index)) {
            zrm_set_error(errorBuffer,
                          errorBufferSize,
                          zrm_archive_entry_duplicate(request->resources, request->resourceCount, index)
                                  ? "duplicate resource '%s'"
                                  : "invalid resource '%s'",
                          resource != ZR_NULL && resource->logicalName != ZR_NULL ? resource->logicalName : "<null>");
            goto cleanup;
        }
        if (!ZrLibrary_Zrm_BuildResourceEntryName(resource->logicalName,
                                                  resourceEntryNames[index],
                                                  ZR_LIBRARY_MAX_PATH_LENGTH) ||
            !zrm_read_file(resource->sourcePath, &resourceBytes[index], &resourceByteCounts[index]) ||
            !zrm_add_manifest_entry(resourcesJson,
                                    resource->logicalName,
                                    resourceEntryNames[index],
                                    resource->hash,
                                    resourceByteCounts[index],
                                    zrm_crc32(resourceBytes[index], resourceByteCounts[index]),
                                    resource->compress ? ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE : ZR_LIBRARY_ZRM_COMPRESSION_STORE)) {
            zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to add resource '%s'", resource->logicalName);
            goto cleanup;
        }
    }

    cJSON_AddItemToObject(manifest, "modules", modulesJson);
    modulesJson = ZR_NULL;
    cJSON_AddItemToObject(manifest, "resources", resourcesJson);
    resourcesJson = ZR_NULL;

    manifestText = cJSON_PrintUnformatted(manifest);
    if (manifestText == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to print manifest");
        goto cleanup;
    }

    if (!zrm_prepare_parent_directory(request->outputPath)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to create output directory");
        goto cleanup;
    }

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, request->outputPath, 0)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to create '%s'", request->outputPath);
        goto cleanup;
    }
    writerInitialized = ZR_TRUE;

    if (!mz_zip_writer_add_mem(&zip,
                               ZR_LIBRARY_ZRM_MANIFEST_ENTRY,
                               manifestText,
                               strlen(manifestText),
                               MZ_NO_COMPRESSION)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to write manifest");
        goto cleanup;
    }
    for (TZrSize index = 0; index < request->moduleCount; index++) {
        if (!mz_zip_writer_add_mem(&zip,
                                   moduleEntryNames[index],
                                   moduleBytes[index],
                                   moduleByteCounts[index],
                                   MZ_NO_COMPRESSION)) {
            zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to write module '%s'", request->modules[index].moduleKey);
            goto cleanup;
        }
    }
    for (TZrSize index = 0; index < request->resourceCount; index++) {
        mz_uint level = request->resources[index].compress ? MZ_DEFAULT_COMPRESSION : MZ_NO_COMPRESSION;
        if (!mz_zip_writer_add_mem(&zip,
                                   resourceEntryNames[index],
                                   resourceBytes[index],
                                   resourceByteCounts[index],
                                   level)) {
            zrm_set_error(errorBuffer,
                          errorBufferSize,
                          "zrm failed to write resource '%s'",
                          request->resources[index].logicalName);
            goto cleanup;
        }
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to finalize archive");
        goto cleanup;
    }

    ok = ZR_TRUE;

cleanup:
    if (writerInitialized) {
        mz_zip_writer_end(&zip);
    }
    if (manifestText != ZR_NULL) {
        cJSON_free(manifestText);
    }
    if (manifest != ZR_NULL) {
        cJSON_Delete(manifest);
    }
    if (assembly != ZR_NULL) {
        cJSON_Delete(assembly);
    }
    if (modulesJson != ZR_NULL) {
        cJSON_Delete(modulesJson);
    }
    if (resourcesJson != ZR_NULL) {
        cJSON_Delete(resourcesJson);
    }
    if (moduleEntryNames != ZR_NULL) {
        for (TZrSize index = 0; index < request->moduleCount; index++) {
            free(moduleEntryNames[index]);
        }
        free(moduleEntryNames);
    }
    if (resourceEntryNames != ZR_NULL) {
        for (TZrSize index = 0; index < request->resourceCount; index++) {
            free(resourceEntryNames[index]);
        }
        free(resourceEntryNames);
    }
    if (moduleBytes != ZR_NULL) {
        for (TZrSize index = 0; index < request->moduleCount; index++) {
            free(moduleBytes[index]);
        }
        free(moduleBytes);
    }
    if (resourceBytes != ZR_NULL) {
        for (TZrSize index = 0; index < request->resourceCount; index++) {
            free(resourceBytes[index]);
        }
        free(resourceBytes);
    }
    free(moduleByteCounts);
    free(resourceByteCounts);
    return ok;
}

TZrBool ZrLibrary_Zrm_Open(const TZrChar *path,
                           SZrLibrary_ZrmArchive *archive,
                           TZrChar *errorBuffer,
                           TZrSize errorBufferSize) {
    mz_zip_archive *zip;
    int manifestIndex;
    size_t manifestSize = 0;
    void *manifestBytes = ZR_NULL;
    cJSON *manifest = ZR_NULL;
    cJSON *assembly = ZR_NULL;
    const TZrChar *format;
    TZrBool ok = ZR_FALSE;

    zrm_set_error(errorBuffer, errorBufferSize, ZR_NULL);
    if (path == ZR_NULL || archive == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm open requires a path and archive");
        return ZR_FALSE;
    }

    memset(archive, 0, sizeof(*archive));
    zip = (mz_zip_archive *)calloc(1, sizeof(*zip));
    if (zip == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to allocate zip reader");
        return ZR_FALSE;
    }

    if (!mz_zip_reader_init_file(zip, path, 0)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to open '%s'", path);
        free(zip);
        return ZR_FALSE;
    }

    if (!zrm_copy_text(archive->path, sizeof(archive->path), path)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm path is too long");
        goto cleanup;
    }
    archive->zipHandle = zip;

    manifestIndex = mz_zip_reader_locate_file(zip, ZR_LIBRARY_ZRM_MANIFEST_ENTRY, ZR_NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (manifestIndex < 0) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest is missing");
        goto cleanup;
    }

    manifestBytes = mz_zip_reader_extract_to_heap(zip, (mz_uint)manifestIndex, &manifestSize, 0);
    if (manifestBytes == ZR_NULL || manifestSize == 0) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to read manifest");
        goto cleanup;
    }

    manifest = cJSON_ParseWithLength((const TZrChar *)manifestBytes, manifestSize);
    if (manifest == ZR_NULL || !cJSON_IsObject(manifest)) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest is not valid JSON");
        goto cleanup;
    }

    format = zrm_json_string(manifest, "format");
    if (format == ZR_NULL || strcmp(format, ZR_LIBRARY_ZRM_FORMAT) != 0) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest has unsupported format");
        goto cleanup;
    }

    assembly = cJSON_GetObjectItemCaseSensitive(manifest, "assembly");
    if (!cJSON_IsObject(assembly) ||
        !zrm_copy_text(archive->assemblyName, sizeof(archive->assemblyName), zrm_json_string(assembly, "name")) ||
        !zrm_copy_text(archive->assemblyVersion, sizeof(archive->assemblyVersion), zrm_json_string(assembly, "version")) ||
        !zrm_copy_text(archive->assemblyCulture, sizeof(archive->assemblyCulture), zrm_json_string(assembly, "culture")) ||
        !zrm_copy_text(archive->assemblyPublicKeyToken,
                       sizeof(archive->assemblyPublicKeyToken),
                       zrm_json_string(assembly, "publicKeyToken")) ||
        !zrm_copy_text(archive->assemblyKind, sizeof(archive->assemblyKind), zrm_json_string(assembly, "kind")) ||
        !zrm_copy_text(archive->entryModule, sizeof(archive->entryModule), zrm_json_string(manifest, "entry")) ||
        archive->assemblyName[0] == '\0' ||
        archive->assemblyVersion[0] == '\0' ||
        archive->entryModule[0] == '\0') {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm manifest has invalid assembly identity");
        goto cleanup;
    }

    if (!zrm_parse_entry_array(cJSON_GetObjectItemCaseSensitive(manifest, "modules"),
                               ZR_TRUE,
                               &archive->modules,
                               &archive->moduleCount,
                               errorBuffer,
                               errorBufferSize) ||
        !zrm_parse_entry_array(cJSON_GetObjectItemCaseSensitive(manifest, "resources"),
                               ZR_FALSE,
                               &archive->resources,
                               &archive->resourceCount,
                               errorBuffer,
                               errorBufferSize)) {
        goto cleanup;
    }

    for (TZrSize index = 0; index < archive->moduleCount; index++) {
        if (!zrm_apply_zip_stat(zip, &archive->modules[index], errorBuffer, errorBufferSize)) {
            goto cleanup;
        }
    }
    for (TZrSize index = 0; index < archive->resourceCount; index++) {
        if (!zrm_apply_zip_stat(zip, &archive->resources[index], errorBuffer, errorBufferSize)) {
            goto cleanup;
        }
    }

    ok = ZR_TRUE;

cleanup:
    if (manifest != ZR_NULL) {
        cJSON_Delete(manifest);
    }
    if (manifestBytes != ZR_NULL) {
        mz_free(manifestBytes);
    }
    if (!ok) {
        ZrLibrary_Zrm_Close(archive);
    }
    return ok;
}

void ZrLibrary_Zrm_Close(SZrLibrary_ZrmArchive *archive) {
    if (archive == ZR_NULL) {
        return;
    }

    if (archive->zipHandle != ZR_NULL) {
        mz_zip_archive *zip = (mz_zip_archive *)archive->zipHandle;
        mz_zip_reader_end(zip);
        free(zip);
    }
    free(archive->modules);
    free(archive->resources);
    memset(archive, 0, sizeof(*archive));
}

const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindModule(const SZrLibrary_ZrmArchive *archive,
                                                        const TZrChar *moduleKey) {
    if (archive == ZR_NULL || moduleKey == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < archive->moduleCount; index++) {
        if (strcmp(archive->modules[index].logicalName, moduleKey) == 0) {
            return &archive->modules[index];
        }
    }
    return ZR_NULL;
}

const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindResource(const SZrLibrary_ZrmArchive *archive,
                                                          const TZrChar *logicalName) {
    if (archive == ZR_NULL || logicalName == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < archive->resourceCount; index++) {
        if (strcmp(archive->resources[index].logicalName, logicalName) == 0) {
            return &archive->resources[index];
        }
    }
    return ZR_NULL;
}

TZrBool ZrLibrary_Zrm_ReadEntry(const SZrLibrary_ZrmArchive *archive,
                                const TZrChar *entryName,
                                TZrByte **outBytes,
                                TZrSize *outByteCount,
                                TZrChar *errorBuffer,
                                TZrSize errorBufferSize) {
    mz_zip_archive *zip;
    int fileIndex;
    size_t byteCount = 0;
    void *bytes;

    if (outBytes != ZR_NULL) {
        *outBytes = ZR_NULL;
    }
    if (outByteCount != ZR_NULL) {
        *outByteCount = 0;
    }
    zrm_set_error(errorBuffer, errorBufferSize, ZR_NULL);

    if (archive == ZR_NULL || archive->zipHandle == ZR_NULL || entryName == ZR_NULL ||
        outBytes == ZR_NULL || outByteCount == ZR_NULL) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm read entry received invalid arguments");
        return ZR_FALSE;
    }

    zip = (mz_zip_archive *)archive->zipHandle;
    fileIndex = mz_zip_reader_locate_file(zip, entryName, ZR_NULL, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex < 0) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm entry '%s' not found", entryName);
        return ZR_FALSE;
    }

    bytes = mz_zip_reader_extract_to_heap(zip, (mz_uint)fileIndex, &byteCount, 0);
    if (bytes == ZR_NULL && byteCount != 0) {
        zrm_set_error(errorBuffer, errorBufferSize, "zrm failed to extract entry '%s'", entryName);
        return ZR_FALSE;
    }

    *outBytes = (TZrByte *)bytes;
    *outByteCount = (TZrSize)byteCount;
    return ZR_TRUE;
}

void ZrLibrary_Zrm_FreeBytes(TZrByte *bytes) {
    if (bytes != ZR_NULL) {
        mz_free(bytes);
    }
}
