/* Copy/move validate source and lexical component overlap before changing targets. */
#include "file_path_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#if defined(ZR_PLATFORM_WIN)
#include <io.h>
#include <sys/stat.h>
#define ZR_FILE_NATIVE_OPEN _open
#else
#include <sys/stat.h>
#include <unistd.h>
#define ZR_FILE_NATIVE_OPEN open
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

static TZrBool file_path_is_strict_ancestor(const TZrChar *parent, const TZrChar *child) {
    TZrSize length = strlen(parent);
    return length > 0 && strlen(child) > length && strncmp(parent, child, length) == 0 &&
           (file_is_separator(parent[length - 1]) || file_is_separator(child[length]));
}

static TZrBool file_copy_paths_are_disjoint(const TZrChar *source, const TZrChar *target,
                                           EZrLibrary_File_Exist sourceType) {
    if (file_path_is_strict_ancestor(target, source) ||
        (sourceType == ZR_LIBRARY_FILE_IS_DIRECTORY && file_path_is_strict_ancestor(source, target))) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
static TZrBool file_copy_file_contents(const TZrChar *sourcePath,
                                       const TZrChar *targetPath,
                                       TZrBool overwrite) {
    TZrLibrary_File_Handle sourceHandle = ZR_LIBRARY_FILE_INVALID_HANDLE;
    TZrLibrary_File_Handle targetHandle = ZR_LIBRARY_FILE_INVALID_HANDLE;
    char buffer[65536];
    TZrBool success = ZR_FALSE;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (!overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }
    if (overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete((TZrNativeString)targetPath, ZR_TRUE)) {
        return ZR_FALSE;
    }

    {
        SZrLibrary_File_StreamOpenResult sourceOpen;
        TZrChar parentPath[ZR_LIBRARY_MAX_PATH_LENGTH];
        memset(&sourceOpen, 0, sizeof(sourceOpen));
        if (!ZrLibrary_File_OpenHandle((TZrNativeString)sourcePath, "rb", &sourceOpen)) {
            return ZR_FALSE;
        }
        sourceHandle = sourceOpen.handle;

        if (ZrLibrary_File_GetDirectory((TZrNativeString)targetPath, parentPath) &&
            parentPath[0] != '\0' &&
            ZrLibrary_File_Exist(parentPath) == ZR_LIBRARY_FILE_NOT_EXIST &&
            !ZrLibrary_File_CreateDirectories(parentPath)) {
            goto cleanup;
        }

#if defined(ZR_PLATFORM_WIN)
        targetHandle = ZR_FILE_NATIVE_OPEN(targetPath,
                                           (overwrite ? (O_WRONLY | O_CREAT | O_TRUNC | O_BINARY)
                                                      : (O_WRONLY | O_CREAT | O_EXCL | O_BINARY)),
                                           _S_IREAD | _S_IWRITE);
#else
        targetHandle = ZR_FILE_NATIVE_OPEN(targetPath,
                                           (overwrite ? (O_WRONLY | O_CREAT | O_TRUNC | O_BINARY)
                                                      : (O_WRONLY | O_CREAT | O_EXCL | O_BINARY)),
                                           0666);
#endif
    }
    if (targetHandle == ZR_LIBRARY_FILE_INVALID_HANDLE) {
        goto cleanup;
    }

    for (;;) {
        TZrSize readSize = 0;
        TZrSize writtenSize = 0;
        if (!ZrLibrary_File_ReadHandle(sourceHandle, buffer, sizeof(buffer), &readSize)) {
            goto cleanup;
        }
        if (readSize == 0) {
            break;
        }
        if (!ZrLibrary_File_WriteHandle(targetHandle, buffer, readSize, &writtenSize) || writtenSize != readSize) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (sourceHandle != ZR_LIBRARY_FILE_INVALID_HANDLE) {
        ZrLibrary_File_CloseHandle(sourceHandle);
    }
    if (targetHandle != ZR_LIBRARY_FILE_INVALID_HANDLE) {
        ZrLibrary_File_CloseHandle(targetHandle);
    }
    return success;
}

static TZrBool file_copy_directory_recursive(const TZrChar *sourcePath,
                                             const TZrChar *targetPath,
                                             TZrBool overwrite) {
    SZrLibrary_File_List children;
    TZrSize index;
    TZrBool success = ZR_FALSE;

    memset(&children, 0, sizeof(children));
    if (!overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }
    if (overwrite && ZrLibrary_File_Exist((TZrNativeString)targetPath) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete((TZrNativeString)targetPath, ZR_TRUE)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_CreateDirectories((TZrNativeString)targetPath) ||
        !ZrLibrary_File_ListDirectory((TZrNativeString)sourcePath, ZR_FALSE, &children)) {
        return ZR_FALSE;
    }

    success = ZR_TRUE;
    for (index = 0; index < children.count; index++) {
        const TZrChar *childName = file_basename_ptr(children.entries[index].path);
        TZrChar targetChildPath[ZR_LIBRARY_MAX_PATH_LENGTH];
        if (!file_join_into(targetPath, childName, targetChildPath, sizeof(targetChildPath))) {
            success = ZR_FALSE;
            break;
        }

        if (children.entries[index].existence == ZR_LIBRARY_FILE_IS_DIRECTORY) {
            if (!file_copy_directory_recursive(children.entries[index].path, targetChildPath, overwrite)) {
                success = ZR_FALSE;
                break;
            }
        } else {
            if (!file_copy_file_contents(children.entries[index].path, targetChildPath, overwrite)) {
                success = ZR_FALSE;
                break;
            }
        }
    }

    ZrLibrary_File_List_Free(&children);
    return success;
}

TZrBool ZrLibrary_File_Copy(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite) {
    TZrChar normalizedSource[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTarget[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(sourcePath, normalizedSource, sizeof(normalizedSource)) ||
        !ZrLibrary_File_NormalizePath(targetPath, normalizedTarget, sizeof(normalizedTarget))) {
        return ZR_FALSE;
    }

    existence = ZrLibrary_File_Exist(normalizedSource);
    if (existence == ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = ENOENT;
        return ZR_FALSE;
    }
    if (file_string_equals(normalizedSource, normalizedTarget)) {
        return ZR_TRUE;
    }
    if (!file_copy_paths_are_disjoint(normalizedSource, normalizedTarget, existence)) {
        return ZR_FALSE;
    }

    return existence == ZR_LIBRARY_FILE_IS_DIRECTORY
                   ? file_copy_directory_recursive(normalizedSource, normalizedTarget, overwrite)
                   : file_copy_file_contents(normalizedSource, normalizedTarget, overwrite);
}

TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite) {
    TZrChar normalizedSource[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTarget[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(sourcePath, normalizedSource, sizeof(normalizedSource)) ||
        !ZrLibrary_File_NormalizePath(targetPath, normalizedTarget, sizeof(normalizedTarget))) {
        return ZR_FALSE;
    }

    existence = ZrLibrary_File_Exist(normalizedSource);
    if (existence == ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = ENOENT;
        return ZR_FALSE;
    }
    if (file_string_equals(normalizedSource, normalizedTarget)) {
        return ZR_TRUE;
    }
    if (!file_copy_paths_are_disjoint(normalizedSource, normalizedTarget, existence)) {
        return ZR_FALSE;
    }

    if (!overwrite && ZrLibrary_File_Exist(normalizedTarget) != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }
    if (overwrite && ZrLibrary_File_Exist(normalizedTarget) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete(normalizedTarget, ZR_TRUE)) {
        return ZR_FALSE;
    }

    if (rename(normalizedSource, normalizedTarget) == 0) {
        return ZR_TRUE;
    }

    return ZrLibrary_File_Copy(normalizedSource, normalizedTarget, ZR_TRUE) &&
           ZrLibrary_File_Delete(normalizedSource, ZR_TRUE);
}
