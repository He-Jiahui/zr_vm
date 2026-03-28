//
// Created by HeJiahui on 2025/7/27.
//
#include "tinydir/tinydir.h"

#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"


// #if defined(ZR_VM_PLATFORM_IS_WIN)
// #define ZR_FILE_MODE_IS_FILE(BUFFER) ((BUFFER).st_mode & (S_IFREG))
// #define ZR_FILE_MODE_IS_DIRECTORY(BUFFER) ((BUFFER).st_mode & (S_IFDIR))
// #elif defined(ZR_VM_PLATFORM_IS_UNIX)
// #define ZR_FILE_MODE_IS_FILE(BUFFER) (S_ISREG((BUFFER).st_mode))
// #define ZR_FILE_MODE_IS_DIRECTORY(BUFFER) (S_ISDIR((BUFFER).st_mode))
// #endif
//

EZrLibrary_File_Exist ZrLibrary_File_Exist(TZrNativeString path) {
    tinydir_file file;
    TZrInt64 result = tinydir_file_open(&file, path);
    if (result == 0) {
        if (file.is_reg) {
            return ZR_LIBRARY_FILE_IS_FILE;
        } else if (file.is_dir) {
            return ZR_LIBRARY_FILE_IS_DIRECTORY;
        } else {
            return ZR_LIBRARY_FILE_IS_OTHER;
        }
    }
    return ZR_LIBRARY_FILE_NOT_EXIST;
}
TZrBool ZrLibrary_File_GetDirectory(TZrNativeString path, ZR_OUT TZrNativeString directory) {
    tinydir_file file;
    TZrInt64 result = tinydir_file_open(&file, path);
    if (result == 0) {
        TZrSize pathLength = ZrCore_NativeString_Length(file.path) - ZrCore_NativeString_Length(file.name);
        ZrCore_Memory_RawCopy(directory, file.path, pathLength);
        directory[pathLength] = '\0';
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

void ZrLibrary_File_PathJoin(TZrNativeString path1, TZrNativeString path2, ZR_OUT TZrNativeString result) {
    if (result == ZR_NULL) {
        return;
    }
    if (path1 == ZR_NULL || path2 == ZR_NULL) {
        result[0] = '\0';
        return;
    }
    TZrSize length1 = ZrCore_NativeString_Length(path1);
    TZrSize length2 = ZrCore_NativeString_Length(path2);

    if (length1 == 0) {
        snprintf(result, ZR_LIBRARY_MAX_PATH_LENGTH, "%s", path2);
        return;
    }
    if (length2 == 0) {
        snprintf(result, ZR_LIBRARY_MAX_PATH_LENGTH, "%s", path1);
        return;
    }

    TZrBool path1HasSeparator = path1[length1 - 1] == '/' || path1[length1 - 1] == '\\';
    TZrBool path2HasSeparator = path2[0] == '/' || path2[0] == '\\';

    if (path1HasSeparator && path2HasSeparator) {
        snprintf(result, ZR_LIBRARY_MAX_PATH_LENGTH, "%s%s", path1, path2 + 1);
        return;
    }
    if (path1HasSeparator || path2HasSeparator) {
        snprintf(result, ZR_LIBRARY_MAX_PATH_LENGTH, "%s%s", path1, path2);
        return;
    }

    snprintf(result, ZR_LIBRARY_MAX_PATH_LENGTH, "%s%c%s", path1, ZR_SEPARATOR, path2);
}

TZrNativeString ZrLibrary_File_ReadAll(SZrGlobalState *global, TZrNativeString path) {
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(global, path, ZR_FALSE);
    if (reader == ZR_NULL) {
        return ZR_NULL;
    }
    TZrNativeString buffer = ZrCore_Memory_RawMallocWithType(global, reader->size + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    if (buffer == ZR_NULL) {
        ZrLibrary_File_CloseRead(global, reader);
        return ZR_NULL;
    }

    TZrSize readSize = fread(buffer, 1, reader->size, reader->file);
    buffer[readSize] = '\0';
    if (readSize != reader->size && ferror(reader->file)) {
        ZrLibrary_File_CloseRead(global, reader);
        ZrCore_Memory_RawFreeWithType(global, buffer, reader->size + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        return ZR_NULL;
    }

    ZrLibrary_File_CloseRead(global, reader);
    return buffer;
}

SZrLibrary_File_Reader *ZrLibrary_File_OpenRead(SZrGlobalState *global, TZrNativeString path, TZrBool isBinary) {
    if (ZrLibrary_File_Exist(path) == ZR_LIBRARY_FILE_NOT_EXIST) {
        return ZR_NULL;
    }
    SZrLibrary_File_Reader *reader =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrLibrary_File_Reader), ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
    if (reader == ZR_NULL) {
        return ZR_NULL;
    }
    reader->file = fopen(path, isBinary ? "rb" : "r");
    if (reader->file == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, reader, sizeof(SZrLibrary_File_Reader), ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        return ZR_NULL;
    }
    // get file size
    fseek(reader->file, 0, SEEK_END);
    reader->size = ftell(reader->file);
    fseek(reader->file, 0, SEEK_SET);
    return reader;
}

void ZrLibrary_File_CloseRead(SZrGlobalState *global, SZrLibrary_File_Reader *reader) {
    if (reader->file == ZR_NULL) {
        return;
    }
    fclose(reader->file);
    reader->file = ZR_NULL;
    ZrCore_Memory_RawFreeWithType(global, reader, sizeof(SZrLibrary_File_Reader), ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
}

TZrBool ZrLibrary_File_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    ZR_UNUSED_PARAMETER(md5);
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(state->global, path, ZR_FALSE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    return ZR_TRUE;
}


TZrBytePtr ZrLibrary_File_SourceReadImplementation(SZrState *state, TZrPtr reader, ZR_OUT TZrSize *size) {
    SZrLibrary_File_Reader *fileReader = (SZrLibrary_File_Reader *) reader;
    TZrSize readSize = fread(fileReader->buffer, 1, ZR_LIBRARY_FILE_BUFFER_SIZE, fileReader->file);
    if (readSize == 0) {
        return ZR_NULL;
    }
    *size = readSize;
    return fileReader->buffer;
}

void ZrLibrary_File_SourceCloseImplementation(SZrState *state, TZrPtr reader) {
    SZrLibrary_File_Reader *fileReader = ZR_CAST(SZrLibrary_File_Reader *, reader);
    // if (fileReader->file == ZR_NULL) {
    //     return;
    // }
    ZrLibrary_File_CloseRead(state->global, fileReader);
    // fileReader->file = ZR_NULL;
}
