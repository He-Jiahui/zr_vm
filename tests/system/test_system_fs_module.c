//
// zr.system.fs object model and FileStream handle_id wrapper tests.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library.h"
#include "zr_vm_parser.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

typedef struct SZrTestTimer {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

void setUp(void) {
}

void tearDown(void) {
}

static void test_panic_handler(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
}

static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_State_Create(test_panic_handler);
    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        ZrVmLibSystem_Register(state->global);
        ZrVmLibFfi_Register(state->global);
    }
    return state;
}

static void destroy_test_state(SZrState *state) {
    ZrTests_State_Destroy(state);
}

static SZrObjectModule *import_module(SZrState *state, const TZrChar *moduleName) {
    SZrString *modulePath;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    modulePath = ZrCore_String_CreateFromNative(state, (TZrNativeString)moduleName);
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_ImportByPath(state, modulePath);
}

static SZrFunction *compile_source(SZrState *state, const TZrChar *source, const TZrChar *sourceNameText) {
    SZrAstNode *ast;
    SZrString *sourceName;
    SZrFunction *compiled;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)sourceNameText);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        if (state->hasCurrentException) {
            ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        }
        return ZR_NULL;
    }

    compiled = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    if (compiled == ZR_NULL && state->hasCurrentException) {
        ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
    }

    return compiled;
}

static const TZrChar *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

static TZrBool string_equals_cstring(SZrState *state, const SZrTypeValue *value, const TZrChar *text) {
    const TZrChar *nativeText = string_value_native(state, value);
    return nativeText != ZR_NULL && text != ZR_NULL && strcmp(nativeText, text) == 0;
}

static const SZrTypeValue *object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static const SZrTypeValue *get_module_export_value(SZrState *state,
                                                   SZrObjectModule *module,
                                                   const TZrChar *exportName) {
    SZrString *exportString;

    if (state == ZR_NULL || module == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    exportString = ZrCore_String_CreateFromNative(state, (TZrNativeString)exportName);
    if (exportString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportString);
}

static SZrObjectPrototype *get_module_exported_prototype(SZrState *state,
                                                         SZrObjectModule *module,
                                                         const TZrChar *typeName) {
    const SZrTypeValue *value = get_module_export_value(state, module, typeName);
    SZrObject *object;

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

static SZrObject *find_named_entry_in_array(SZrState *state,
                                            SZrObject *array,
                                            const TZrChar *fieldName,
                                            const TZrChar *expectedName) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || fieldName == ZR_NULL || expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < ZrLib_Array_Length(array); index++) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(state, array, index);
        SZrObject *entryObject;
        const SZrTypeValue *nameValue;

        if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
            continue;
        }

        entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
        if (entryObject == ZR_NULL) {
            continue;
        }

        nameValue = object_field(state, entryObject, fieldName);
        if (string_equals_cstring(state, nameValue, expectedName)) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

static TZrBool string_array_contains(SZrState *state, const SZrTypeValue *arrayValue, const TZrChar *expectedText) {
    SZrObject *array;
    TZrSize index;

    if (state == ZR_NULL || arrayValue == ZR_NULL || expectedText == ZR_NULL || arrayValue->type != ZR_VALUE_TYPE_ARRAY ||
        arrayValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    array = ZR_CAST_OBJECT(state, arrayValue->value.object);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < ZrLib_Array_Length(array); index++) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(state, array, index);
        if (string_equals_cstring(state, entryValue, expectedText)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void escape_for_zr_string_literal(char *destination, size_t destinationSize, const char *source) {
    size_t writeIndex = 0;
    size_t readIndex = 0;

    if (destination == NULL || destinationSize == 0) {
        return;
    }

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }

    while (source[readIndex] != '\0' && writeIndex + 1 < destinationSize) {
        if (source[readIndex] == '\\' || source[readIndex] == '"') {
            if (writeIndex + 2 >= destinationSize) {
                break;
            }
            destination[writeIndex++] = '\\';
        }
        destination[writeIndex++] = source[readIndex++];
    }

    destination[writeIndex] = '\0';
}

static TZrBool make_unique_test_root(const TZrChar *stem, TZrChar *buffer, TZrSize bufferSize) {
    static unsigned long long uniqueCounter = 0;
    TZrChar baseName[128];
    TZrChar artifactPath[ZR_TESTS_PATH_MAX];
    TZrSize extensionIndex = 0;

    if (stem == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    snprintf(baseName,
             sizeof(baseName),
             "%s_%lld_%llu_%llu",
             stem,
             (long long)time(ZR_NULL),
             (unsigned long long)clock(),
             uniqueCounter++);
    if (!ZrTests_Path_GetGeneratedArtifact("system_fs", "runtime", baseName, ".tmp", artifactPath, sizeof(artifactPath))) {
        return ZR_FALSE;
    }

    extensionIndex = strlen(artifactPath);
    while (extensionIndex > 0 && artifactPath[extensionIndex - 1] != '.') {
        extensionIndex--;
    }
    if (extensionIndex == 0 || extensionIndex > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, artifactPath, extensionIndex - 1);
    buffer[extensionIndex - 1] = '\0';
    if (ZrLibrary_File_Exist(buffer) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete(buffer, ZR_TRUE)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static void test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields(void) {
    SZrTestTimer timer = {0};
    SZrState *state;
    SZrObjectModule *fsModule;
    SZrObjectModule *exceptionModule;
    const SZrTypeValue *moduleInfoValue;
    const SZrTypeValue *typesValue;
    SZrObject *moduleInfo;
    SZrObject *typesArray;
    SZrObject *fileEntry;
    SZrObject *folderEntry;
    SZrObject *streamEntry;
    SZrObject *readerEntry;
    SZrObject *writerEntry;
    SZrObject *ioExceptionEntry;
    SZrObjectPrototype *streamPrototype;

    ZR_TEST_START("zr.system.fs metadata exposes object surface and wrapper fields");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    fsModule = import_module(state, "zr.system.fs");
    TEST_ASSERT_NOT_NULL(fsModule);

    moduleInfoValue = get_module_export_value(state, fsModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    TEST_ASSERT_NOT_NULL(moduleInfoValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);
    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);

    typesValue = object_field(state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
    typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
    TEST_ASSERT_NOT_NULL(typesArray);

    fileEntry = find_named_entry_in_array(state, typesArray, "name", "File");
    folderEntry = find_named_entry_in_array(state, typesArray, "name", "Folder");
    streamEntry = find_named_entry_in_array(state, typesArray, "name", "FileStream");
    readerEntry = find_named_entry_in_array(state, typesArray, "name", "IStreamReader");
    writerEntry = find_named_entry_in_array(state, typesArray, "name", "IStreamWriter");
    TEST_ASSERT_NOT_NULL(fileEntry);
    TEST_ASSERT_NOT_NULL(folderEntry);
    TEST_ASSERT_NOT_NULL(streamEntry);
    TEST_ASSERT_NOT_NULL(readerEntry);
    TEST_ASSERT_NOT_NULL(writerEntry);

    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, fileEntry, "extendsTypeName"), "FileSystemEntry"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, folderEntry, "extendsTypeName"), "FileSystemEntry"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiLoweringKind"), "handle_id"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiUnderlyingTypeName"), "i32"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiOwnerMode"), "owned"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiReleaseHook"), "close"));
    TEST_ASSERT_TRUE(string_array_contains(state, object_field(state, streamEntry, "implements"), "IStreamReader"));
    TEST_ASSERT_TRUE(string_array_contains(state, object_field(state, streamEntry, "implements"), "IStreamWriter"));

    streamPrototype = get_module_exported_prototype(state, fsModule, "FileStream");
    TEST_ASSERT_NOT_NULL(streamPrototype);
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiLoweringKind"),
                                           "handle_id"));
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiUnderlyingTypeName"),
                                           "i32"));
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiOwnerMode"),
                                           "owned"));
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiReleaseHook"),
                                           "close"));

    exceptionModule = import_module(state, "zr.system.exception");
    TEST_ASSERT_NOT_NULL(exceptionModule);
    moduleInfoValue = get_module_export_value(state, exceptionModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    TEST_ASSERT_NOT_NULL(moduleInfoValue);
    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);
    typesValue = object_field(state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
    TEST_ASSERT_NOT_NULL(typesArray);
    ioExceptionEntry = find_named_entry_in_array(state, typesArray, "name", "IOException");
    TEST_ASSERT_NOT_NULL(ioExceptionEntry);
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, ioExceptionEntry, "extendsTypeName"), "RuntimeError"));

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs metadata exposes object surface and wrapper fields");
    ZR_TEST_DIVIDER();
}

static void test_system_fs_source_runtime_supports_path_objects_and_directory_operations(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = %%import(\"zr.system.fs\");\n"
            "var root = new fs.Folder(\"%s\");\n"
            "var nested = new fs.Folder(\"%s\");\n"
            "var file = new fs.File(\"%s\");\n"
            "var bytesFile = new fs.File(\"%s\");\n"
            "var copyTarget = \"%s\";\n"
            "var movedTarget = \"%s\";\n"
            "var copiedFolderTarget = \"%s\";\n"
            "var movedFolderTarget = \"%s\";\n"
            "root.create(true);\n"
            "nested.create(true);\n"
            "file.create(true);\n"
            "if (!file.exists()) { return -1; }\n"
            "if (file.name != \"example.data.txt\") { return -2; }\n"
            "if (file.extension != \".txt\") { return -3; }\n"
            "if (file.parent == null || file.parent.fullPath != nested.fullPath) { return -4; }\n"
            "if (file.writeText(\"alpha\") != 5) { return -5; }\n"
            "if (file.readText() != \"alpha\") { return -6; }\n"
            "bytesFile.writeBytes([65, 66, 67]);\n"
            "bytesFile.appendBytes([68]);\n"
            "var bytes = bytesFile.readBytes();\n"
            "if (bytes.length != 4 || bytes[0] != 65 || bytes[3] != 68) { return -7; }\n"
            "var copy = file.copyTo(copyTarget, true);\n"
            "if (!copy.exists() || copy.readText() != \"alpha\") { return -8; }\n"
            "var moved = copy.moveTo(movedTarget, true);\n"
            "if (!moved.exists() || new fs.File(copyTarget).exists()) { return -9; }\n"
            "var entries = root.entries();\n"
            "var files = root.files();\n"
            "var folders = root.folders();\n"
            "var globbed = root.glob(\"*.txt\", true);\n"
            "if (entries.length != 3 || files.length != 2 || folders.length != 1 || globbed.length != 2) { return -10; }\n"
            "if (files[0].name != \"bytes.bin\" || files[1].name != \"moved.txt\") { return -11; }\n"
            "if (folders[0].name != \"nested.dir\") { return -12; }\n"
            "var info = file.refresh();\n"
            "if (file.fileInfo == null) { return -131; }\n"
            "if (info == null) { return -130; }\n"
            "if (!info.exists || !info.isFile || info.size != 5) { return -13; }\n"
            "if (file.fileInfo.name != \"example.data.txt\") { return -141; }\n"
            "if (file.fileInfo.extension != \".txt\") { return -142; }\n"
            "if (info.name != \"example.data.txt\" || info.extension != \".txt\") { return -14; }\n"
            "if (info.parentPath != nested.fullPath) { return -15; }\n"
            "var copiedFolder = nested.copyTo(copiedFolderTarget, true);\n"
            "if (!new fs.File(copiedFolderTarget + \"/example.data.txt\").exists()) { return -16; }\n"
            "var movedFolder = copiedFolder.moveTo(movedFolderTarget, true);\n"
            "if (!new fs.File(movedFolderTarget + \"/example.data.txt\").exists()) { return -17; }\n"
            "movedFolder.delete(true);\n"
            "if (movedFolder.exists()) { return -18; }\n"
            "moved.delete();\n"
            "if (moved.exists()) { return -19; }\n"
            "root.delete(true);\n"
            "if (root.exists()) { return -20; }\n"
            "return 1;\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar nestedPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    TZrChar bytesPath[ZR_TESTS_PATH_MAX];
    TZrChar copyPath[ZR_TESTS_PATH_MAX];
    TZrChar movedPath[ZR_TESTS_PATH_MAX];
    TZrChar copiedFolderPath[ZR_TESTS_PATH_MAX];
    TZrChar movedFolderPath[ZR_TESTS_PATH_MAX];
    char source[16384];
    char escapedRoot[ZR_TESTS_PATH_MAX * 2];
    char escapedNested[ZR_TESTS_PATH_MAX * 2];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    char escapedBytes[ZR_TESTS_PATH_MAX * 2];
    char escapedCopy[ZR_TESTS_PATH_MAX * 2];
    char escapedMoved[ZR_TESTS_PATH_MAX * 2];
    char escapedCopiedFolder[ZR_TESTS_PATH_MAX * 2];
    char escapedMovedFolder[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs runtime supports path objects and directory operations");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("path_objects", rootPath, sizeof(rootPath)));
    snprintf(nestedPath, sizeof(nestedPath), "%s/nested.dir", rootPath);
    snprintf(filePath, sizeof(filePath), "%s/example.data.txt", nestedPath);
    snprintf(bytesPath, sizeof(bytesPath), "%s/bytes.bin", rootPath);
    snprintf(copyPath, sizeof(copyPath), "%s/copy.txt", rootPath);
    snprintf(movedPath, sizeof(movedPath), "%s/moved.txt", rootPath);
    snprintf(copiedFolderPath, sizeof(copiedFolderPath), "%s/nested-copy", rootPath);
    snprintf(movedFolderPath, sizeof(movedFolderPath), "%s/nested-moved", rootPath);

    escape_for_zr_string_literal(escapedRoot, sizeof(escapedRoot), rootPath);
    escape_for_zr_string_literal(escapedNested, sizeof(escapedNested), nestedPath);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    escape_for_zr_string_literal(escapedBytes, sizeof(escapedBytes), bytesPath);
    escape_for_zr_string_literal(escapedCopy, sizeof(escapedCopy), copyPath);
    escape_for_zr_string_literal(escapedMoved, sizeof(escapedMoved), movedPath);
    escape_for_zr_string_literal(escapedCopiedFolder, sizeof(escapedCopiedFolder), copiedFolderPath);
    escape_for_zr_string_literal(escapedMovedFolder, sizeof(escapedMovedFolder), movedFolderPath);

    snprintf(source,
             sizeof(source),
             kSourceTemplate,
             escapedRoot,
             escapedNested,
             escapedFile,
             escapedBytes,
             escapedCopy,
             escapedMoved,
             escapedCopiedFolder,
             escapedMovedFolder);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_path_objects_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs runtime supports path objects and directory operations");
    ZR_TEST_DIVIDER();
}

static void test_system_fs_source_runtime_supports_stream_modes_using_and_handle_id_lowering(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_tell_fd\")# tellFd(fd:i32): i32;\n"
            "}\n"
            "var fs = %%import(\"zr.system.fs\");\n"
            "var exception = %%import(\"zr.system.exception\");\n"
            "func takeReader(reader: fs.IStreamReader): string {\n"
            "  return reader.readText(1);\n"
            "}\n"
            "func takeWriter(writer: fs.IStreamWriter): int {\n"
            "  return writer.writeText(\"!\");\n"
            "}\n"
            "func readWithUsing(target: fs.File): string {\n"
            "  var usingStream = target.open(\"r\");\n"
            "  %using usingStream;\n"
            "  return usingStream.readText(-1);\n"
            "}\n"
            "var file = new fs.File(\"%s\");\n"
            "var exclusiveFile = new fs.File(\"%s\");\n"
            "file.parent.create(true);\n"
            "var stream = file.open(\"w+\");\n"
            "if (stream.closed || stream.length != 0 || stream.position != 0) { return -1; }\n"
            "if (stream.mode != \"w+\") { return -2; }\n"
            "if (stream.path != file.fullPath) { return -3; }\n"
            "if (stream.writeText(\"abc\") != 3) { return -4; }\n"
            "if (stream.position != 3 || stream.length != 3) { return -5; }\n"
            "if (tellFd(stream) != 3) { return -6; }\n"
            "if (stream.seek(1, \"begin\") != 1) { return -7; }\n"
            "if (tellFd(stream) != 1) { return -8; }\n"
            "if (stream.writeText(\"Z\") != 1) { return -9; }\n"
            "if (stream.seek(0, \"begin\") != 0) { return -10; }\n"
            "if (takeReader(stream) != \"a\") { return -11; }\n"
            "if (stream.seek(0, \"begin\") != 0) { return -12; }\n"
            "if (stream.readText(-1) != \"aZc\") { return -13; }\n"
            "stream.setLength(2);\n"
            "if (stream.length != 2) { return -14; }\n"
            "stream.close();\n"
            "stream.close();\n"
            "if (!stream.closed) { return -15; }\n"
            "var append = file.open(\"a+\");\n"
            "if (takeWriter(append) != 1) { return -16; }\n"
            "append.seek(0, \"begin\");\n"
            "if (append.readText(-1) != \"aZ!\") { return -17; }\n"
            "append.close();\n"
            "var firstOpen = exclusiveFile.open(\"x+\");\n"
            "firstOpen.writeBytes([1, 2, 3]);\n"
            "firstOpen.seek(0, \"begin\");\n"
            "var roundtrip = firstOpen.readBytes(-1);\n"
            "if (roundtrip.length != 3 || roundtrip[0] != 1 || roundtrip[2] != 3) { return -18; }\n"
            "firstOpen.close();\n"
            "var duplicateFailed = 0;\n"
            "try {\n"
            "  exclusiveFile.open(\"x\");\n"
            "} catch (e: exception.IOException) {\n"
            "  duplicateFailed = 1;\n"
            "}\n"
            "if (duplicateFailed != 1) { return -19; }\n"
            "if (readWithUsing(file) != \"aZ!\") { return -20; }\n"
            "return 1;\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    TZrChar exclusivePath[ZR_TESTS_PATH_MAX];
    char source[16384];
    char escapedFixture[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    char escapedExclusive[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs runtime supports stream modes, using, and handle_id lowering");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("stream_modes", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/stream.txt", rootPath);
    snprintf(exclusivePath, sizeof(exclusivePath), "%s/exclusive.bin", rootPath);

    escape_for_zr_string_literal(escapedFixture, sizeof(escapedFixture), ZR_VM_FFI_FIXTURE_PATH);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    escape_for_zr_string_literal(escapedExclusive, sizeof(escapedExclusive), exclusivePath);

    snprintf(source, sizeof(source), kSourceTemplate, escapedFixture, escapedFile, escapedExclusive);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_stream_modes_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs runtime supports stream modes, using, and handle_id lowering");
    ZR_TEST_DIVIDER();
}

static void test_system_fs_source_runtime_raises_io_exception_for_missing_file(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = %%import(\"zr.system.fs\");\n"
            "var exception = %%import(\"zr.system.exception\");\n"
            "try {\n"
            "  new fs.File(\"%s\").open(\"r\");\n"
            "  return 0;\n"
            "} catch (e: exception.IOException) {\n"
            "  return 1;\n"
            "}\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar missingPath[ZR_TESTS_PATH_MAX];
    char source[4096];
    char escapedMissing[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs runtime raises IOException for missing file");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("io_exception", rootPath, sizeof(rootPath)));
    snprintf(missingPath, sizeof(missingPath), "%s/missing.txt", rootPath);
    escape_for_zr_string_literal(escapedMissing, sizeof(escapedMissing), missingPath);
    snprintf(source, sizeof(source), kSourceTemplate, escapedMissing);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_io_exception_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs runtime raises IOException for missing file");
    ZR_TEST_DIVIDER();
}

static void test_system_fs_handle_id_lowering_rejects_closed_stream(void) {
    static const TZrChar *kSourceTemplate =
            "%%extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_tell_fd\")# tellFd(fd:i32): i32;\n"
            "}\n"
            "var fs = %%import(\"zr.system.fs\");\n"
            "var file = new fs.File(\"%s\");\n"
            "var stream = file.open(\"w+\");\n"
            "stream.close();\n"
            "return tellFd(stream);\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    char source[8192];
    char escapedFixture[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.system.fs handle_id lowering rejects closed stream");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("closed_stream", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/closed.txt", rootPath);
    escape_for_zr_string_literal(escapedFixture, sizeof(escapedFixture), ZR_VM_FFI_FIXTURE_PATH);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    snprintf(source, sizeof(source), kSourceTemplate, escapedFixture, escapedFile);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_closed_stream_ffi_lowering.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, entryFunction, &result));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs handle_id lowering rejects closed stream");
    ZR_TEST_DIVIDER();
}

static void test_system_fs_handle_id_lowering_does_not_apply_to_ordinary_calls(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = %%import(\"zr.system.fs\");\n"
            "func acceptFd(fd:i32): i32 {\n"
            "  return fd;\n"
            "}\n"
            "var file = new fs.File(\"%s\");\n"
            "var stream = file.open(\"w+\");\n"
            "return acceptFd(stream);\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    char source[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;

    ZR_TEST_START("zr.system.fs handle_id lowering does not apply to ordinary calls");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("ordinary_call", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/ordinary.txt", rootPath);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    snprintf(source, sizeof(source), kSourceTemplate, escapedFile);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_ordinary_call_no_lowering.zr");
    TEST_ASSERT_NULL(entryFunction);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs handle_id lowering does not apply to ordinary calls");
    ZR_TEST_DIVIDER();
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields);
    RUN_TEST(test_system_fs_source_runtime_supports_path_objects_and_directory_operations);
    RUN_TEST(test_system_fs_source_runtime_supports_stream_modes_using_and_handle_id_lowering);
    RUN_TEST(test_system_fs_source_runtime_raises_io_exception_for_missing_file);
    RUN_TEST(test_system_fs_handle_id_lowering_rejects_closed_stream);
    RUN_TEST(test_system_fs_handle_id_lowering_does_not_apply_to_ordinary_calls);

    return UNITY_END();
}
