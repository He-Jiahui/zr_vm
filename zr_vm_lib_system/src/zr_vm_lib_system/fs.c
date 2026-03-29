//
// zr.system.fs callbacks.
//

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "zr_vm_lib_system/fs.h"

#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <direct.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define ZR_SYSTEM_FS_PATH_MAX 4096

static void system_fs_write_int_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void system_fs_write_bool_field(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static void system_fs_write_string_field(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const TZrChar *value) {
    SZrTypeValue fieldValue;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

static TZrBool system_fs_read_path_argument(const ZrLibCallContext *context, TZrSize index, const TZrChar **outPath) {
    SZrString *pathString = ZR_NULL;

    if (context == ZR_NULL || outPath == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadString(context, index, &pathString) || pathString == ZR_NULL) {
        return ZR_FALSE;
    }

    *outPath = ZrCore_String_GetNativeString(pathString);
    return *outPath != ZR_NULL;
}

static TZrBool system_fs_get_current_directory(TZrChar *buffer, TZrSize bufferSize) {
#if defined(ZR_PLATFORM_WIN)
    return _getcwd(buffer, (int)bufferSize) != ZR_NULL;
#else
    return getcwd(buffer, bufferSize) != ZR_NULL;
#endif
}

static int system_fs_change_directory_native(const TZrChar *path) {
#if defined(ZR_PLATFORM_WIN)
    return _chdir(path);
#else
    return chdir(path);
#endif
}

static int system_fs_create_directory_native(const TZrChar *path) {
#if defined(ZR_PLATFORM_WIN)
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

static TZrBool system_fs_create_directories_native(const TZrChar *path) {
    TZrChar buffer[ZR_SYSTEM_FS_PATH_MAX];
    TZrSize length;
    TZrSize index;

    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    length = strlen(path);
    if (length >= sizeof(buffer)) {
        return ZR_FALSE;
    }

    memcpy(buffer, path, length + 1);
    for (index = 1; index < length; index++) {
        TZrBool isSeparator = buffer[index] == '/' || buffer[index] == '\\';
        TZrBool isWindowsDriveSeparator = (index == 2 && buffer[1] == ':');

        if (!isSeparator || isWindowsDriveSeparator) {
            continue;
        }

        {
            TZrChar saved = buffer[index];
            buffer[index] = '\0';
            if (buffer[0] != '\0' &&
                system_fs_create_directory_native(buffer) != 0 &&
                errno != EEXIST) {
                return ZR_FALSE;
            }
            buffer[index] = saved;
        }
    }

    return system_fs_create_directory_native(buffer) == 0 || errno == EEXIST;
}

static TZrBool system_fs_read_all_text(const TZrChar *path, TZrChar **outBuffer) {
    FILE *file;
    long fileSize;
    TZrChar *buffer;

    if (path == ZR_NULL || outBuffer == ZR_NULL) {
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
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }

    buffer = (TZrChar *)malloc((size_t)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_FALSE;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    *outBuffer = buffer;
    return ZR_TRUE;
}

static TZrBool system_fs_write_text_file(const TZrChar *path, const TZrChar *text, const TZrChar *mode) {
    FILE *file;
    size_t length = text != ZR_NULL ? strlen(text) : 0;

    if (path == ZR_NULL || mode == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(path, mode);
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    if (length > 0 && fwrite(text, 1, length, file) != length) {
        fclose(file);
        return ZR_FALSE;
    }

    fclose(file);
    return ZR_TRUE;
}

static TZrBool system_fs_query_stat(const TZrChar *path, TZrInt64 *outSize, TZrInt64 *outModifiedMilliseconds) {
    if (path == ZR_NULL) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        struct _stat64 info;
        if (_stat64(path, &info) != 0) {
            return ZR_FALSE;
        }
        if (outSize != ZR_NULL) {
            *outSize = (TZrInt64)info.st_size;
        }
        if (outModifiedMilliseconds != ZR_NULL) {
            *outModifiedMilliseconds = (TZrInt64)info.st_mtime * 1000;
        }
        return ZR_TRUE;
    }
#else
    {
        struct stat info;
        if (stat(path, &info) != 0) {
            return ZR_FALSE;
        }
        if (outSize != ZR_NULL) {
            *outSize = (TZrInt64)info.st_size;
        }
        if (outModifiedMilliseconds != ZR_NULL) {
            *outModifiedMilliseconds = (TZrInt64)info.st_mtime * 1000;
        }
        return ZR_TRUE;
    }
#endif
}

static SZrObject *system_fs_make_file_info(SZrState *state, const TZrChar *path) {
    SZrObject *object;
    EZrLibrary_File_Exist existence;
    TZrInt64 size = 0;
    TZrInt64 modifiedMilliseconds = 0;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "SystemFileInfo");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }

    existence = path != ZR_NULL ? ZrLibrary_File_Exist(path) : ZR_LIBRARY_FILE_NOT_EXIST;
    if (path != ZR_NULL) {
        system_fs_write_string_field(state, object, "path", path);
        system_fs_query_stat(path, &size, &modifiedMilliseconds);
    }

    system_fs_write_int_field(state, object, "size", size);
    system_fs_write_bool_field(state, object, "isFile", existence == ZR_LIBRARY_FILE_IS_FILE);
    system_fs_write_bool_field(state, object, "isDirectory", existence == ZR_LIBRARY_FILE_IS_DIRECTORY);
    system_fs_write_int_field(state, object, "modifiedMilliseconds", modifiedMilliseconds);
    return object;
}

TZrBool ZrSystem_Fs_CurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrChar buffer[ZR_SYSTEM_FS_PATH_MAX];

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_get_current_directory(buffer, sizeof(buffer))) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(context->state, result, buffer);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ChangeCurrentDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, system_fs_change_directory_native(path) == 0);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_PathExists(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, ZrLibrary_File_Exist(path) != ZR_LIBRARY_FILE_NOT_EXIST);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_IsFile(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, ZrLibrary_File_Exist(path) == ZR_LIBRARY_FILE_IS_FILE);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_IsDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, ZrLibrary_File_Exist(path) == ZR_LIBRARY_FILE_IS_DIRECTORY);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_CreateDirectory(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state,
                        result,
                        system_fs_create_directory_native(path) == 0 || errno == EEXIST);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_CreateDirectories(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, system_fs_create_directories_native(path));
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_RemovePath(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, remove(path) == 0);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ReadText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    TZrChar *buffer = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path) || !system_fs_read_all_text(path, &buffer)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetString(context->state, result, buffer);
    free(buffer);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_WriteText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    const TZrChar *text = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path) ||
        !system_fs_read_path_argument(context, 1, &text)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, system_fs_write_text_file(path, text, "wb"));
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_AppendText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    const TZrChar *text = ZR_NULL;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path) ||
        !system_fs_read_path_argument(context, 1, &text)) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetBool(context->state, result, system_fs_write_text_file(path, text, "ab"));
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_GetInfo(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *path = ZR_NULL;
    SZrObject *fileInfo;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!system_fs_read_path_argument(context, 0, &path)) {
        return ZR_FALSE;
    }

    fileInfo = system_fs_make_file_info(context->state, path);
    if (fileInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, fileInfo, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}
