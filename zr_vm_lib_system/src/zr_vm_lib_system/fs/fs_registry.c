//
// zr.system.fs descriptor registry.
//

#include "zr_vm_lib_system/fs_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibParameterDescriptor g_path_parameter[] = {
        {"path", "string", "Filesystem path."},
};

static const ZrLibParameterDescriptor g_text_parameter[] = {
        {"text", "string", "UTF-8 text payload."},
};

static const ZrLibParameterDescriptor g_path_and_text_parameters[] = {
        {"path", "string", "Filesystem path."},
        {"text", "string", "UTF-8 text payload."},
};

static const ZrLibParameterDescriptor g_bytes_parameter[] = {
        {"bytes", "array", "Byte array represented as integers 0..255."},
};

static const ZrLibParameterDescriptor g_create_recursive_parameter[] = {
        {"recursively", "bool", "Whether to create parent directories recursively."},
};

static const ZrLibParameterDescriptor g_delete_recursive_parameter[] = {
        {"recursively", "bool", "Whether to delete child entries recursively."},
};

static const ZrLibParameterDescriptor g_copy_parameters[] = {
        {"targetPath", "string", "Destination filesystem path."},
        {"overwrite", "bool", "Whether to overwrite an existing destination."},
};

static const ZrLibParameterDescriptor g_open_parameters[] = {
        {"mode", "string", "Open mode: r/r+/w/w+/a/a+/x/x+ with optional b alias."},
};

static const ZrLibParameterDescriptor g_read_count_parameters[] = {
        {"count", "int", "Maximum byte count, or -1 for the remaining payload."},
};

static const ZrLibParameterDescriptor g_seek_parameters[] = {
        {"offset", "int", "Signed seek offset."},
        {"origin", "string", "Seek origin: begin/current/end."},
};

static const ZrLibParameterDescriptor g_set_length_parameters[] = {
        {"length", "int", "New stream length in bytes."},
};

static const ZrLibParameterDescriptor g_glob_parameters[] = {
        {"pattern", "string", "Wildcard pattern using * and ?."},
        {"recursively", "bool", "Whether to recurse into subfolders."},
};

static const ZrLibMetaMethodDescriptor g_entry_constructors[] = {
        {ZR_META_CONSTRUCTOR, 1, 1, ZrSystem_Fs_Entry_Constructor, "null",
         "Initialize a path wrapper object from the supplied path.", g_path_parameter,
         ZR_ARRAY_COUNT(g_path_parameter), ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_stream_meta_methods[] = {
        {ZR_META_CLOSE, 1, 1, ZrSystem_Fs_Stream_Close, "null",
         "Close this FileStream. Registered for using/%using auto-close.", ZR_NULL, 0, ZR_NULL, 0},
};

static const ZrLibFieldDescriptor g_file_info_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("path", "string", "Normalized absolute path that was queried."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("size", "int", "File size in bytes when available."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("isFile", "bool", "Whether the path currently resolves to a file."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("isDirectory", "bool", "Whether the path currently resolves to a directory."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("modifiedMilliseconds", "int",
                                     "Last modification timestamp in milliseconds when available."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("exists", "bool", "Whether the path currently exists."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("name", "string", "Final path segment."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("extension", "string", "Trailing extension, including the leading dot when present."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("parentPath", "string", "Normalized absolute parent directory path."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("createdMilliseconds", "int",
                                     "Creation or best-effort birth timestamp in milliseconds."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("accessedMilliseconds", "int",
                                     "Last access timestamp in milliseconds when available."),
};

static const ZrLibFieldDescriptor g_entry_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("path", "string", "Original constructor path."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("fullPath", "string", "Normalized absolute path."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("name", "string", "Final path segment."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("extension", "string", "Trailing extension, including the leading dot when present."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("parent", "Folder", "Normalized parent folder object, or null at a filesystem root."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("fileInfo", "SystemFileInfo", "Latest metadata snapshot for this path wrapper."),
};

static const ZrLibFieldDescriptor g_stream_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("path", "string", "Normalized absolute path bound to this stream."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("mode", "string", "Canonical open mode without the optional b alias."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("position", "int", "Current file position."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("length", "int", "Current file length in bytes."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("closed", "bool", "Whether this stream has been closed."),
};

static const ZrLibMethodDescriptor g_entry_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("exists", 0, 0, ZrSystem_Fs_Entry_Exists, "bool",
                                      "Refresh and return whether the wrapped path exists.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("refresh", 0, 0, ZrSystem_Fs_Entry_Refresh, "SystemFileInfo",
                                      "Refresh the fileInfo snapshot and return it.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_file_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("open", 0, 1, ZrSystem_Fs_File_Open, "FileStream",
                                      "Open this file and return a FileStream wrapper.", ZR_FALSE,
                                      g_open_parameters, ZR_ARRAY_COUNT(g_open_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("create", 0, 1, ZrSystem_Fs_File_Create, "null",
                                      "Create the file if needed.", ZR_FALSE,
                                      g_create_recursive_parameter, ZR_ARRAY_COUNT(g_create_recursive_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("readText", 0, 0, ZrSystem_Fs_File_ReadText, "string",
                                      "Read the whole file as UTF-8 text.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("writeText", 1, 1, ZrSystem_Fs_File_WriteText, "int",
                                      "Replace file contents with UTF-8 text.", ZR_FALSE,
                                      g_text_parameter, ZR_ARRAY_COUNT(g_text_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("appendText", 1, 1, ZrSystem_Fs_File_AppendText, "int",
                                      "Append UTF-8 text to the file.", ZR_FALSE,
                                      g_text_parameter, ZR_ARRAY_COUNT(g_text_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("readBytes", 0, 0, ZrSystem_Fs_File_ReadBytes, "array",
                                      "Read the whole file as byte integers.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("writeBytes", 1, 1, ZrSystem_Fs_File_WriteBytes, "int",
                                      "Replace file contents with byte integers.", ZR_FALSE,
                                      g_bytes_parameter, ZR_ARRAY_COUNT(g_bytes_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("appendBytes", 1, 1, ZrSystem_Fs_File_AppendBytes, "int",
                                      "Append byte integers to the file.", ZR_FALSE,
                                      g_bytes_parameter, ZR_ARRAY_COUNT(g_bytes_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("copyTo", 1, 2, ZrSystem_Fs_File_CopyTo, "File",
                                      "Copy this file to a new path.", ZR_FALSE,
                                      g_copy_parameters, ZR_ARRAY_COUNT(g_copy_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("moveTo", 1, 2, ZrSystem_Fs_File_MoveTo, "File",
                                      "Move this file to a new path.", ZR_FALSE,
                                      g_copy_parameters, ZR_ARRAY_COUNT(g_copy_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("delete", 0, 0, ZrSystem_Fs_File_Delete, "null",
                                      "Delete this file.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_folder_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("create", 0, 1, ZrSystem_Fs_Folder_Create, "null",
                                      "Create the folder if needed.", ZR_FALSE,
                                      g_create_recursive_parameter, ZR_ARRAY_COUNT(g_create_recursive_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("entries", 0, 0, ZrSystem_Fs_Folder_Entries, "array",
                                      "List direct child entries sorted by fullPath.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("files", 0, 0, ZrSystem_Fs_Folder_Files, "array",
                                      "List direct child files sorted by fullPath.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("folders", 0, 0, ZrSystem_Fs_Folder_Folders, "array",
                                      "List direct child folders sorted by fullPath.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("glob", 1, 2, ZrSystem_Fs_Folder_Glob, "array",
                                      "List wildcard matches beneath this folder.", ZR_FALSE,
                                      g_glob_parameters, ZR_ARRAY_COUNT(g_glob_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("copyTo", 1, 2, ZrSystem_Fs_Folder_CopyTo, "Folder",
                                      "Copy this folder tree to a new path.", ZR_FALSE,
                                      g_copy_parameters, ZR_ARRAY_COUNT(g_copy_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("moveTo", 1, 2, ZrSystem_Fs_Folder_MoveTo, "Folder",
                                      "Move this folder tree to a new path.", ZR_FALSE,
                                      g_copy_parameters, ZR_ARRAY_COUNT(g_copy_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("delete", 0, 1, ZrSystem_Fs_Folder_Delete, "null",
                                      "Delete this folder tree.", ZR_FALSE,
                                      g_delete_recursive_parameter, ZR_ARRAY_COUNT(g_delete_recursive_parameter)),
};

static const ZrLibMethodDescriptor g_stream_reader_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("readBytes", 0, 1, ZrSystem_Fs_Stream_ReadBytes, "array",
                                      "Read bytes from the current position.", ZR_FALSE,
                                      g_read_count_parameters, ZR_ARRAY_COUNT(g_read_count_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("readText", 0, 1, ZrSystem_Fs_Stream_ReadText, "string",
                                      "Read UTF-8 text from the current position.", ZR_FALSE,
                                      g_read_count_parameters, ZR_ARRAY_COUNT(g_read_count_parameters)),
};

static const ZrLibMethodDescriptor g_stream_writer_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("writeBytes", 1, 1, ZrSystem_Fs_Stream_WriteBytes, "int",
                                      "Write byte integers at the current position.", ZR_FALSE,
                                      g_bytes_parameter, ZR_ARRAY_COUNT(g_bytes_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("writeText", 1, 1, ZrSystem_Fs_Stream_WriteText, "int",
                                      "Write UTF-8 text at the current position.", ZR_FALSE,
                                      g_text_parameter, ZR_ARRAY_COUNT(g_text_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("flush", 0, 0, ZrSystem_Fs_Stream_Flush, "null",
                                      "Flush buffered stream state to the OS.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_file_stream_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("readBytes", 0, 1, ZrSystem_Fs_Stream_ReadBytes, "array",
                                      "Read bytes from the current position.", ZR_FALSE,
                                      g_read_count_parameters, ZR_ARRAY_COUNT(g_read_count_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("readText", 0, 1, ZrSystem_Fs_Stream_ReadText, "string",
                                      "Read UTF-8 text from the current position.", ZR_FALSE,
                                      g_read_count_parameters, ZR_ARRAY_COUNT(g_read_count_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("writeBytes", 1, 1, ZrSystem_Fs_Stream_WriteBytes, "int",
                                      "Write byte integers at the current position.", ZR_FALSE,
                                      g_bytes_parameter, ZR_ARRAY_COUNT(g_bytes_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("writeText", 1, 1, ZrSystem_Fs_Stream_WriteText, "int",
                                      "Write UTF-8 text at the current position.", ZR_FALSE,
                                      g_text_parameter, ZR_ARRAY_COUNT(g_text_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("flush", 0, 0, ZrSystem_Fs_Stream_Flush, "null",
                                      "Flush buffered stream state to the OS.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("seek", 1, 2, ZrSystem_Fs_Stream_Seek, "int",
                                      "Seek to a new file position.", ZR_FALSE,
                                      g_seek_parameters, ZR_ARRAY_COUNT(g_seek_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("setLength", 1, 1, ZrSystem_Fs_Stream_SetLength, "null",
                                      "Resize the underlying file.", ZR_FALSE,
                                      g_set_length_parameters, ZR_ARRAY_COUNT(g_set_length_parameters)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrSystem_Fs_Stream_Close, "null",
                                      "Close this FileStream.", ZR_FALSE, ZR_NULL, 0),
};

static const TZrChar *g_file_stream_implements[] = {"IStreamReader", "IStreamWriter"};

const ZrLibModuleDescriptor *ZrSystem_FsRegistry_GetModule(void) {
    static const ZrLibTypeDescriptor kTypes[] = {
            ZR_LIB_TYPE_DESCRIPTOR_INIT("SystemFileInfo", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, g_file_info_fields,
                                        ZR_ARRAY_COUNT(g_file_info_fields), ZR_NULL, 0, ZR_NULL, 0,
                                        "Filesystem metadata snapshot.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                        ZR_TRUE, ZR_TRUE, "SystemFileInfo()", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("FileSystemEntry", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, g_entry_fields,
                                        ZR_ARRAY_COUNT(g_entry_fields), g_entry_methods,
                                        ZR_ARRAY_COUNT(g_entry_methods), g_entry_constructors,
                                        ZR_ARRAY_COUNT(g_entry_constructors),
                                        "Normalized path wrapper with refreshable metadata.", ZR_NULL, ZR_NULL, 0,
                                        ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "FileSystemEntry(path: string)",
                                        ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("File", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, g_entry_fields,
                                        ZR_ARRAY_COUNT(g_entry_fields), g_file_methods,
                                        ZR_ARRAY_COUNT(g_file_methods), g_entry_constructors,
                                        ZR_ARRAY_COUNT(g_entry_constructors),
                                        "Object-oriented file path wrapper.", "FileSystemEntry", ZR_NULL, 0, ZR_NULL,
                                        0, ZR_NULL, ZR_TRUE, ZR_TRUE, "File(path: string)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("Folder", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, g_entry_fields,
                                        ZR_ARRAY_COUNT(g_entry_fields), g_folder_methods,
                                        ZR_ARRAY_COUNT(g_folder_methods), g_entry_constructors,
                                        ZR_ARRAY_COUNT(g_entry_constructors),
                                        "Object-oriented folder path wrapper.", "FileSystemEntry", ZR_NULL, 0, ZR_NULL,
                                        0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Folder(path: string)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("IStreamReader", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                        g_stream_reader_methods, ZR_ARRAY_COUNT(g_stream_reader_methods), ZR_NULL, 0,
                                        "Readable stream interface.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                        ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("IStreamWriter", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                        g_stream_writer_methods, ZR_ARRAY_COUNT(g_stream_writer_methods), ZR_NULL, 0,
                                        "Writable stream interface.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                        ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_FFI_INIT("FileStream", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, g_stream_fields,
                                            ZR_ARRAY_COUNT(g_stream_fields), g_file_stream_methods,
                                            ZR_ARRAY_COUNT(g_file_stream_methods), g_stream_meta_methods,
                                            ZR_ARRAY_COUNT(g_stream_meta_methods),
                                            "Owned native file descriptor wrapper.", ZR_NULL,
                                            g_file_stream_implements, ZR_ARRAY_COUNT(g_file_stream_implements), ZR_NULL,
                                            0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                            "handle_id", ZR_NULL, "i32", "owned", "close"),
    };
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"currentDirectory", 0, 0, ZrSystem_Fs_CurrentDirectory, "string",
             "Return the current working directory.", ZR_NULL, 0},
            {"changeCurrentDirectory", 1, 1, ZrSystem_Fs_ChangeCurrentDirectory, "bool",
             "Change the current working directory.", g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"pathExists", 1, 1, ZrSystem_Fs_PathExists, "bool", "Check whether a path exists.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"isFile", 1, 1, ZrSystem_Fs_IsFile, "bool", "Check whether a path is a file.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"isDirectory", 1, 1, ZrSystem_Fs_IsDirectory, "bool", "Check whether a path is a directory.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"createDirectory", 1, 1, ZrSystem_Fs_CreateDirectory, "bool", "Create a single directory.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"createDirectories", 1, 1, ZrSystem_Fs_CreateDirectories, "bool", "Create a directory tree.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"removePath", 1, 1, ZrSystem_Fs_RemovePath, "bool", "Remove a file or empty directory.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"readText", 1, 1, ZrSystem_Fs_ReadText, "string", "Read a UTF-8 text file.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
            {"writeText", 2, 2, ZrSystem_Fs_WriteText, "bool", "Write a UTF-8 text file.",
             g_path_and_text_parameters, ZR_ARRAY_COUNT(g_path_and_text_parameters)},
            {"appendText", 2, 2, ZrSystem_Fs_AppendText, "bool", "Append UTF-8 text to a file.",
             g_path_and_text_parameters, ZR_ARRAY_COUNT(g_path_and_text_parameters)},
            {"getInfo", 1, 1, ZrSystem_Fs_GetInfo, "SystemFileInfo", "Query filesystem metadata for a path.",
             g_path_parameter, ZR_ARRAY_COUNT(g_path_parameter)},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"currentDirectory", "function", "currentDirectory(): string", "Return the current working directory."},
            {"changeCurrentDirectory", "function", "changeCurrentDirectory(path: string): bool",
             "Change the current working directory."},
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
            {"SystemFileInfo", "type", "struct SystemFileInfo", "Filesystem metadata snapshot."},
            {"FileSystemEntry", "type", "class FileSystemEntry", "Normalized path wrapper with refreshable metadata."},
            {"File", "type", "class File extends FileSystemEntry", "Object-oriented file path wrapper."},
            {"Folder", "type", "class Folder extends FileSystemEntry", "Object-oriented folder path wrapper."},
            {"IStreamReader", "type", "interface IStreamReader", "Readable stream interface."},
            {"IStreamWriter", "type", "interface IStreamWriter", "Writable stream interface."},
            {"FileStream", "type", "class FileStream implements IStreamReader, IStreamWriter",
             "Owned native file descriptor wrapper."},
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
            "Filesystem object model, stream wrappers, and compatibility helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
