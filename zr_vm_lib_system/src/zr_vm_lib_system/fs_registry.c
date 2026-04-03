//
// zr.system.fs descriptor registry.
//

#include "zr_vm_lib_system/fs_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_FsRegistry_GetModule(void) {
    static const ZrLibFieldDescriptor kFileInfoFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("path", "string", "Full path that was queried."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("size", "int", "File size in bytes when available."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("isFile", "bool", "Whether the path currently resolves to a file."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("isDirectory", "bool",
                                         "Whether the path currently resolves to a directory."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("modifiedMilliseconds", "int",
                                         "Last modification timestamp in milliseconds when available."),
    };
    static const ZrLibTypeDescriptor kTypes[] = {
            ZR_LIB_TYPE_DESCRIPTOR_INIT("SystemFileInfo", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kFileInfoFields,
                                        ZR_ARRAY_COUNT(kFileInfoFields), ZR_NULL, 0, ZR_NULL, 0,
                                        "Filesystem metadata snapshot.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                        ZR_TRUE, ZR_TRUE, "SystemFileInfo()", ZR_NULL, 0),
    };
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"currentDirectory", 0, 0, ZrSystem_Fs_CurrentDirectory, "string", "Return the current working directory.", ZR_NULL, 0},
            {"changeCurrentDirectory", 1, 1, ZrSystem_Fs_ChangeCurrentDirectory, "bool", "Change the current working directory.", ZR_NULL, 0},
            {"pathExists", 1, 1, ZrSystem_Fs_PathExists, "bool", "Check whether a path exists.", ZR_NULL, 0},
            {"isFile", 1, 1, ZrSystem_Fs_IsFile, "bool", "Check whether a path is a file.", ZR_NULL, 0},
            {"isDirectory", 1, 1, ZrSystem_Fs_IsDirectory, "bool", "Check whether a path is a directory.", ZR_NULL, 0},
            {"createDirectory", 1, 1, ZrSystem_Fs_CreateDirectory, "bool", "Create a single directory.", ZR_NULL, 0},
            {"createDirectories", 1, 1, ZrSystem_Fs_CreateDirectories, "bool", "Create a directory tree.", ZR_NULL, 0},
            {"removePath", 1, 1, ZrSystem_Fs_RemovePath, "bool", "Remove a file or empty directory.", ZR_NULL, 0},
            {"readText", 1, 1, ZrSystem_Fs_ReadText, "string", "Read a UTF-8 text file.", ZR_NULL, 0},
            {"writeText", 2, 2, ZrSystem_Fs_WriteText, "bool", "Write a UTF-8 text file.", ZR_NULL, 0},
            {"appendText", 2, 2, ZrSystem_Fs_AppendText, "bool", "Append UTF-8 text to a file.", ZR_NULL, 0},
            {"getInfo", 1, 1, ZrSystem_Fs_GetInfo, "SystemFileInfo", "Query filesystem metadata for a path.", ZR_NULL, 0},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"currentDirectory", "function", "currentDirectory(): string", "Return the current working directory."},
            {"changeCurrentDirectory", "function", "changeCurrentDirectory(path: string): bool", "Change the current working directory."},
            {"pathExists", "function", "pathExists(path: string): bool", "Check whether a path exists."},
            {"isFile", "function", "isFile(path: string): bool", "Check whether a path is a file."},
            {"isDirectory", "function", "isDirectory(path: string): bool", "Check whether a path is a directory."},
            {"createDirectory", "function", "createDirectory(path: string): bool", "Create a single directory."},
            {"createDirectories", "function", "createDirectories(path: string): bool", "Create a directory tree."},
            {"removePath", "function", "removePath(path: string): bool", "Remove a file or empty directory."},
            {"readText", "function", "readText(path: string): string", "Read a UTF-8 text file."},
            {"writeText", "function", "writeText(path: string, text: string): bool", "Write a UTF-8 text file."},
            {"appendText", "function", "appendText(path: string, text: string): bool", "Append UTF-8 text to a file."},
            {"getInfo", "function", "getInfo(path: string): SystemFileInfo", "Query filesystem metadata for a path."},
            {"SystemFileInfo", "type", "struct SystemFileInfo { path, size, isFile, isDirectory, modifiedMilliseconds }", "Filesystem metadata snapshot."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.fs\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.fs",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            kTypes,
            ZR_ARRAY_COUNT(kTypes),
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Filesystem helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
