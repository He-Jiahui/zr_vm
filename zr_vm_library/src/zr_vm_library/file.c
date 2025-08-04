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

EZrLibrary_File_Exist ZrLibrary_File_Exist(TNativeString path) {
    tinydir_file file;
    TInt64 result = tinydir_file_open(&file, path);
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
TBool ZrLibrary_File_GetDirectory(TNativeString path, ZR_OUT TNativeString directory) {
    tinydir_file file;
    TInt64 result = tinydir_file_open(&file, path);
    if (result == 0) {
        TZrSize pathLength = ZrNativeStringLength(file.path) - ZrNativeStringLength(file.name);
        ZrMemoryRawCopy(directory, file.path, pathLength);
        directory[pathLength] = '\0';
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

void ZrLibrary_File_PathJoin(TNativeString path1, TNativeString path2, ZR_OUT TNativeString result) {
    if (path1 == ZR_NULL || path2 == ZR_NULL) {
        return;
    }
    if (path1[0] == '\0') {
        TZrSize totalLength = ZrNativeStringLength(path2);
        snprintf(result, totalLength, "%s", path2) + 1;
        return;
    }
    if (path2[0] == '\0') {
        TZrSize totalLength = ZrNativeStringLength(path1) + 1;
        snprintf(result, totalLength, "%s", path1);
        return;
    }
    if (path1[ZrNativeStringLength(path1) - 1] == '/' && path2[0] == '/') {
        TZrSize totalLength = ZrNativeStringLength(path1) + ZrNativeStringLength(path2);
        snprintf(result, totalLength, "%s%s", path1, path2 + 1);
        return;
    }
    if (path1[ZrNativeStringLength(path1) - 1] == '/' || path2[0] == '/') {
        TZrSize totalLength = ZrNativeStringLength(path1) + ZrNativeStringLength(path2) + 1;
        snprintf(result, totalLength, "%s%s", path1, path2);
        return;
    }
    TZrSize totalLength = ZrNativeStringLength(path1) + ZrNativeStringLength(path2) + 2;
    snprintf(result, totalLength, "%s%c%s", path1, ZR_SEPARATOR, path2);
}

TNativeString ZrLibrary_File_ReadAll(SZrGlobalState *global, TNativeString path) {
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(global, path, ZR_FALSE);
    if (reader == ZR_NULL) {
        return ZR_NULL;
    }
    TNativeString buffer = ZrMemoryRawMallocWithType(global, reader->size, ZR_VALUE_TYPE_VM_MEMORY);
    fread(buffer, 1, reader->size, reader->file);
    ZrLibrary_File_CloseRead(global, reader);
    return buffer;
}

SZrLibrary_File_Reader *ZrLibrary_File_OpenRead(SZrGlobalState *global, TNativeString path, TBool isBinary) {
    if (ZrLibrary_File_Exist(path) == ZR_LIBRARY_FILE_NOT_EXIST) {
        return ZR_NULL;
    }
    SZrLibrary_File_Reader *reader =
            ZrMemoryRawMallocWithType(global, sizeof(SZrLibrary_File_Reader), ZR_VALUE_TYPE_NATIVE_DATA);
    reader->file = fopen(path, isBinary ? "rb" : "r");
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
    ZrMemoryRawFree(global, reader, sizeof(SZrLibrary_File_Reader));
}

TBool ZrLibrary_File_SourceLoadImplementation(SZrState *state, TNativeString path, TNativeString md5, SZrIo *io) {
    ZR_UNUSED_PARAMETER(md5);
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(state->global, path, ZR_FALSE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrIoInit(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    return ZR_TRUE;
}


TBytePtr ZrLibrary_File_SourceReadImplementation(SZrState *state, TZrPtr reader, ZR_OUT TZrSize *size) {
    SZrLibrary_File_Reader *fileReader = (SZrLibrary_File_Reader *) reader;
    TZrSize readSize = fread(fileReader->buffer, 1, ZR_LIBRARY_FILE_BUFFER_SIZE, fileReader->file);
    if (readSize == 0) {
        return ZR_NULL;
    }
    *size = readSize;
    return fileReader->buffer;
}

void ZrLibrary_File_SourceCloseImplementation(SZrState *state, TZrPtr reader) {
    SZrLibrary_File_Reader *fileReader = (SZrLibrary_File_Reader *) reader;
    if (fileReader->file == ZR_NULL) {
        return;
    }
    fclose(fileReader->file);
    fileReader->file = ZR_NULL;
}
