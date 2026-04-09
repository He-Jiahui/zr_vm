//
// Created by HeJiahui on 2025/7/27.
//

#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "tinydir.h"

#include "zr_vm_library/conf.h"
#include "zr_vm_library/file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZR_PLATFORM_WIN)
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if defined(ZR_PLATFORM_WIN)
#define ZR_FILE_NATIVE_OPEN _open
#else
#define ZR_FILE_NATIVE_OPEN open
#endif

static TZrBool file_is_separator(TZrChar ch);
static TZrBool file_string_equals(const TZrChar *lhs, const TZrChar *rhs);
static TZrBool file_copy_text(TZrNativeString destination, TZrSize destinationSize, const TZrChar *source);
static TZrBool file_get_current_directory(TZrChar *buffer, TZrSize bufferSize);
static int file_make_directory_native(const TZrChar *path);
static int file_remove_directory_native(const TZrChar *path);
static int file_unlink_native(const TZrChar *path);
static TZrBool file_handle_readable_chunk_size(TZrSize requestedSize, unsigned int *outSize);
static TZrBool file_is_dot_entry(const TZrChar *name);
static TZrBool file_path_is_root(const TZrChar *path);
static TZrSize file_extract_root(const TZrChar *path, TZrChar *root, TZrSize rootSize);
static TZrBool file_build_absolute_path(const TZrChar *path, TZrChar *absolutePath, TZrSize absolutePathSize);
static void file_compact_separators(TZrChar *path);
static void file_trim_trailing_separator(TZrChar *path, TZrSize rootLength);
static TZrBool file_fill_path_parts(const TZrChar *normalizedPath, SZrLibrary_File_Info *outInfo);
static EZrLibrary_File_Exist file_stat_exist(const TZrChar *path,
                                             TZrInt64 *outSize,
                                             TZrInt64 *outModifiedMilliseconds,
                                             TZrInt64 *outCreatedMilliseconds,
                                             TZrInt64 *outAccessedMilliseconds);
static TZrBool file_list_reserve(SZrLibrary_File_List *list, TZrSize requiredCount);
static TZrBool file_list_push(SZrLibrary_File_List *list, const TZrChar *path, EZrLibrary_File_Exist existence);
static int file_list_entry_compare(const void *lhs, const void *rhs);
static TZrBool file_list_directory_recursive(const TZrChar *path,
                                             TZrBool recursively,
                                             SZrLibrary_File_List *outList);
static TZrBool file_wildcard_match(const TZrChar *pattern, const TZrChar *text);
static const TZrChar *file_basename_ptr(const TZrChar *path);
static TZrBool file_join_into(const TZrChar *path1,
                              const TZrChar *path2,
                              TZrChar *result,
                              TZrSize resultSize);
static TZrBool file_copy_file_contents(const TZrChar *sourcePath,
                                       const TZrChar *targetPath,
                                       TZrBool overwrite);
static TZrBool file_copy_directory_recursive(const TZrChar *sourcePath,
                                             const TZrChar *targetPath,
                                             TZrBool overwrite);
static TZrBool file_delete_directory_recursive(const TZrChar *path);
static TZrBool file_parse_mode(const TZrChar *mode,
                               int *outFlags,
                               TZrBool *outReadable,
                               TZrBool *outWritable,
                               TZrBool *outAppend,
                               TZrChar *normalizedMode,
                               TZrSize normalizedModeSize);

static TZrBool file_is_separator(TZrChar ch) {
    return ch == '/' || ch == '\\';
}

static TZrBool file_string_equals(const TZrChar *lhs, const TZrChar *rhs) {
    return lhs != ZR_NULL && rhs != ZR_NULL && strcmp(lhs, rhs) == 0;
}

static TZrBool file_copy_text(TZrNativeString destination, TZrSize destinationSize, const TZrChar *source) {
    TZrSize length;

    if (destination == ZR_NULL || destinationSize == 0) {
        return ZR_FALSE;
    }

    destination[0] = '\0';
    if (source == ZR_NULL) {
        return ZR_TRUE;
    }

    length = strlen(source);
    if (length + 1 > destinationSize) {
        errno = ENAMETOOLONG;
        return ZR_FALSE;
    }

    memcpy(destination, source, length + 1);
    return ZR_TRUE;
}

static TZrBool file_get_current_directory(TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0) {
        errno = EINVAL;
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    return _getcwd(buffer, (int)bufferSize) != ZR_NULL;
#else
    return getcwd(buffer, bufferSize) != ZR_NULL;
#endif
}

static int file_make_directory_native(const TZrChar *path) {
#if defined(ZR_PLATFORM_WIN)
    return _mkdir(path);
#else
    return mkdir(path, ZR_VM_POSIX_DIRECTORY_CREATE_MODE);
#endif
}

static int file_remove_directory_native(const TZrChar *path) {
#if defined(ZR_PLATFORM_WIN)
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

static int file_unlink_native(const TZrChar *path) {
#if defined(ZR_PLATFORM_WIN)
    return _unlink(path);
#else
    return unlink(path);
#endif
}

static TZrBool file_handle_readable_chunk_size(TZrSize requestedSize, unsigned int *outSize) {
    if (outSize == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (requestedSize > (TZrSize)0x7fffffffU) {
        *outSize = 0x7fffffffU;
    } else {
        *outSize = (unsigned int)requestedSize;
    }
    return ZR_TRUE;
}

static TZrBool file_is_dot_entry(const TZrChar *name) {
    return file_string_equals(name, ".") || file_string_equals(name, "..");
}

static TZrBool file_path_is_root(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' &&
        path[2] != '\0' &&
        file_is_separator(path[2]) &&
        path[3] == '\0') {
        return ZR_TRUE;
    }
    if (file_is_separator(path[0]) && file_is_separator(path[1]) && path[2] == '\0') {
        return ZR_TRUE;
    }
#endif

    return file_is_separator(path[0]) && path[1] == '\0';
}

static TZrSize file_extract_root(const TZrChar *path, TZrChar *root, TZrSize rootSize) {
    if (root != ZR_NULL && rootSize > 0) {
        root[0] = '\0';
    }

    if (path == ZR_NULL || path[0] == '\0') {
        return 0;
    }

#if defined(ZR_PLATFORM_WIN)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
        TZrChar buffer[4];
        buffer[0] = path[0];
        buffer[1] = ':';
        if (file_is_separator(path[2])) {
            buffer[2] = ZR_SEPARATOR;
            buffer[3] = '\0';
            file_copy_text(root, rootSize, buffer);
            return 3;
        }
        buffer[2] = '\0';
        file_copy_text(root, rootSize, buffer);
        return 2;
    }

    if (file_is_separator(path[0]) && file_is_separator(path[1])) {
        TZrSize cursor = 2;
        TZrSize componentCount = 0;
        while (path[cursor] != '\0' && componentCount < 2) {
            while (file_is_separator(path[cursor])) {
                cursor++;
            }
            while (path[cursor] != '\0' && !file_is_separator(path[cursor])) {
                cursor++;
            }
            componentCount++;
        }
        if (root != ZR_NULL && rootSize > 0) {
            TZrSize length = cursor;
            if (length + 1 > rootSize) {
                errno = ENAMETOOLONG;
                return 0;
            }
            memcpy(root, path, length);
            root[length] = '\0';
        }
        return cursor;
    }
#endif

    if (file_is_separator(path[0])) {
        if (root != ZR_NULL && rootSize > 1) {
            root[0] = ZR_SEPARATOR;
            root[1] = '\0';
        }
        return 1;
    }

    return 0;
}

static TZrBool file_build_absolute_path(const TZrChar *path,
                                        TZrChar *absolutePath,
                                        TZrSize absolutePathSize) {
    TZrChar currentDirectory[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (path == ZR_NULL || absolutePath == ZR_NULL || absolutePathSize == 0) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (ZrLibrary_File_IsAbsolutePath(path)) {
        return file_copy_text(absolutePath, absolutePathSize, path);
    }

    if (!file_get_current_directory(currentDirectory, sizeof(currentDirectory))) {
        return ZR_FALSE;
    }

    ZrLibrary_File_PathJoin(currentDirectory, path, absolutePath);
    return absolutePath[0] != '\0';
}

static void file_compact_separators(TZrChar *path) {
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;
    TZrBool previousWasSeparator = ZR_FALSE;

    if (path == ZR_NULL) {
        return;
    }

    while (path[readIndex] != '\0') {
        TZrChar ch = path[readIndex++];
        if (file_is_separator(ch)) {
            if (previousWasSeparator) {
                continue;
            }
            ch = ZR_SEPARATOR;
            previousWasSeparator = ZR_TRUE;
        } else {
            previousWasSeparator = ZR_FALSE;
        }
        path[writeIndex++] = ch;
    }
    path[writeIndex] = '\0';
}

static void file_trim_trailing_separator(TZrChar *path, TZrSize rootLength) {
    TZrSize length;

    if (path == ZR_NULL) {
        return;
    }

    length = strlen(path);
    while (length > rootLength && file_is_separator(path[length - 1])) {
        path[--length] = '\0';
    }
}

static TZrBool file_fill_path_parts(const TZrChar *normalizedPath, SZrLibrary_File_Info *outInfo) {
    TZrChar root[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;
    const TZrChar *lastSeparator = ZR_NULL;
    const TZrChar *name = normalizedPath;
    const TZrChar *lastDot = ZR_NULL;
    TZrSize index;

    if (normalizedPath == ZR_NULL || outInfo == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    rootLength = file_extract_root(normalizedPath, root, sizeof(root));
    if (file_path_is_root(normalizedPath)) {
        if (!file_copy_text(outInfo->parentPath, sizeof(outInfo->parentPath), "")) {
            return ZR_FALSE;
        }
        if (!file_copy_text(outInfo->name, sizeof(outInfo->name), normalizedPath)) {
            return ZR_FALSE;
        }
        return file_copy_text(outInfo->extension, sizeof(outInfo->extension), "");
    }

    for (index = strlen(normalizedPath); index > rootLength; index--) {
        if (file_is_separator(normalizedPath[index - 1])) {
            lastSeparator = normalizedPath + index - 1;
            break;
        }
    }

    if (lastSeparator != ZR_NULL) {
        name = lastSeparator + 1;
        if (lastSeparator == normalizedPath && rootLength == 1) {
            if (!file_copy_text(outInfo->parentPath, sizeof(outInfo->parentPath), root)) {
                return ZR_FALSE;
            }
        } else {
            TZrSize parentLength = (TZrSize)(lastSeparator - normalizedPath);
            if (parentLength == 0 && rootLength > 0) {
                parentLength = rootLength;
            }
            if (parentLength + 1 > sizeof(outInfo->parentPath)) {
                errno = ENAMETOOLONG;
                return ZR_FALSE;
            }
            memcpy(outInfo->parentPath, normalizedPath, parentLength);
            outInfo->parentPath[parentLength] = '\0';
        }
    } else {
        if (!file_copy_text(outInfo->parentPath, sizeof(outInfo->parentPath), rootLength > 0 ? root : "")) {
            return ZR_FALSE;
        }
    }

    if (!file_copy_text(outInfo->name, sizeof(outInfo->name), name)) {
        return ZR_FALSE;
    }

    for (lastDot = name + strlen(name); lastDot > name; lastDot--) {
        if (lastDot[-1] == '.') {
            if (lastDot - 1 != name) {
                return file_copy_text(outInfo->extension, sizeof(outInfo->extension), lastDot - 1);
            }
            break;
        }
    }

    return file_copy_text(outInfo->extension, sizeof(outInfo->extension), "");
}

static EZrLibrary_File_Exist file_stat_exist(const TZrChar *path,
                                             TZrInt64 *outSize,
                                             TZrInt64 *outModifiedMilliseconds,
                                             TZrInt64 *outCreatedMilliseconds,
                                             TZrInt64 *outAccessedMilliseconds) {
    if (path == ZR_NULL || path[0] == '\0') {
        errno = EINVAL;
        return ZR_LIBRARY_FILE_NOT_EXIST;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        struct _stat64 info;
        if (_stat64(path, &info) != 0) {
            return ZR_LIBRARY_FILE_NOT_EXIST;
        }

        if (outSize != ZR_NULL) {
            *outSize = (TZrInt64)info.st_size;
        }
        if (outModifiedMilliseconds != ZR_NULL) {
            *outModifiedMilliseconds = (TZrInt64)info.st_mtime * 1000;
        }
        if (outCreatedMilliseconds != ZR_NULL) {
            *outCreatedMilliseconds = (TZrInt64)info.st_ctime * 1000;
        }
        if (outAccessedMilliseconds != ZR_NULL) {
            *outAccessedMilliseconds = (TZrInt64)info.st_atime * 1000;
        }

        if ((info.st_mode & _S_IFREG) != 0) {
            return ZR_LIBRARY_FILE_IS_FILE;
        }
        if ((info.st_mode & _S_IFDIR) != 0) {
            return ZR_LIBRARY_FILE_IS_DIRECTORY;
        }
        return ZR_LIBRARY_FILE_IS_OTHER;
    }
#else
    {
        struct stat info;
        if (stat(path, &info) != 0) {
            return ZR_LIBRARY_FILE_NOT_EXIST;
        }

        if (outSize != ZR_NULL) {
            *outSize = (TZrInt64)info.st_size;
        }
        if (outModifiedMilliseconds != ZR_NULL) {
            *outModifiedMilliseconds = (TZrInt64)info.st_mtime * 1000;
        }
        if (outCreatedMilliseconds != ZR_NULL) {
            *outCreatedMilliseconds = (TZrInt64)info.st_ctime * 1000;
        }
        if (outAccessedMilliseconds != ZR_NULL) {
            *outAccessedMilliseconds = (TZrInt64)info.st_atime * 1000;
        }

        if (S_ISREG(info.st_mode)) {
            return ZR_LIBRARY_FILE_IS_FILE;
        }
        if (S_ISDIR(info.st_mode)) {
            return ZR_LIBRARY_FILE_IS_DIRECTORY;
        }
        return ZR_LIBRARY_FILE_IS_OTHER;
    }
#endif
}
static TZrBool file_list_reserve(SZrLibrary_File_List *list, TZrSize requiredCount) {
    SZrLibrary_File_ListEntry *newEntries;
    TZrSize newCapacity;

    if (list == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (requiredCount <= list->capacity) {
        return ZR_TRUE;
    }

    newCapacity = list->capacity > 0 ? list->capacity : 8U;
    while (newCapacity < requiredCount) {
        newCapacity *= 2U;
    }

    newEntries = (SZrLibrary_File_ListEntry *)realloc(list->entries, newCapacity * sizeof(SZrLibrary_File_ListEntry));
    if (newEntries == ZR_NULL) {
        return ZR_FALSE;
    }

    list->entries = newEntries;
    list->capacity = newCapacity;
    return ZR_TRUE;
}

static TZrBool file_list_push(SZrLibrary_File_List *list,
                              const TZrChar *path,
                              EZrLibrary_File_Exist existence) {
    SZrLibrary_File_ListEntry *entry;

    if (list == ZR_NULL || path == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (!file_list_reserve(list, list->count + 1)) {
        return ZR_FALSE;
    }

    entry = &list->entries[list->count];
    memset(entry, 0, sizeof(*entry));
    if (!file_copy_text(entry->path, sizeof(entry->path), path)) {
        return ZR_FALSE;
    }
    entry->existence = existence;
    list->count++;
    return ZR_TRUE;
}

static int file_list_entry_compare(const void *lhs, const void *rhs) {
    const SZrLibrary_File_ListEntry *left = (const SZrLibrary_File_ListEntry *)lhs;
    const SZrLibrary_File_ListEntry *right = (const SZrLibrary_File_ListEntry *)rhs;
    return strcmp(left->path, right->path);
}

static TZrBool file_list_directory_recursive(const TZrChar *path,
                                             TZrBool recursively,
                                             SZrLibrary_File_List *outList) {
    tinydir_dir dir;
    TZrSize index;
    TZrBool success = ZR_FALSE;

    if (path == ZR_NULL || outList == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(&dir, 0, sizeof(dir));
    if (tinydir_open_sorted(&dir, path) != 0) {
        return ZR_FALSE;
    }

    success = ZR_TRUE;
    for (index = 0; index < dir.n_files; index++) {
        tinydir_file file;
        TZrChar normalized[ZR_LIBRARY_MAX_PATH_LENGTH];
        EZrLibrary_File_Exist existence;

        memset(&file, 0, sizeof(file));
        if (tinydir_readfile_n(&dir, &file, index) != 0) {
            success = ZR_FALSE;
            break;
        }
        if (file_is_dot_entry(file.name)) {
            continue;
        }

        if (!ZrLibrary_File_NormalizePath(file.path, normalized, sizeof(normalized))) {
            success = ZR_FALSE;
            break;
        }

        existence = file.is_reg ? ZR_LIBRARY_FILE_IS_FILE
                                : (file.is_dir ? ZR_LIBRARY_FILE_IS_DIRECTORY : ZR_LIBRARY_FILE_IS_OTHER);
        if (!file_list_push(outList, normalized, existence)) {
            success = ZR_FALSE;
            break;
        }

        if (recursively && existence == ZR_LIBRARY_FILE_IS_DIRECTORY &&
            !file_list_directory_recursive(normalized, ZR_TRUE, outList)) {
            success = ZR_FALSE;
            break;
        }
    }

    tinydir_close(&dir);
    return success;
}

static TZrBool file_wildcard_match(const TZrChar *pattern, const TZrChar *text) {
    if (pattern == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    while (*pattern != '\0') {
        if (*pattern == '*') {
            pattern++;
            if (*pattern == '\0') {
                return ZR_TRUE;
            }
            while (*text != '\0') {
                if (file_wildcard_match(pattern, text)) {
                    return ZR_TRUE;
                }
                text++;
            }
            return file_wildcard_match(pattern, text);
        }
        if (*pattern == '?') {
            if (*text == '\0') {
                return ZR_FALSE;
            }
            pattern++;
            text++;
            continue;
        }
        if (*pattern != *text) {
            return ZR_FALSE;
        }
        pattern++;
        text++;
    }

    return *text == '\0';
}

static const TZrChar *file_basename_ptr(const TZrChar *path) {
    const TZrChar *cursor;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    cursor = path + strlen(path);
    while (cursor > path) {
        if (file_is_separator(cursor[-1])) {
            break;
        }
        cursor--;
    }
    return cursor;
}

static TZrBool file_join_into(const TZrChar *path1,
                              const TZrChar *path2,
                              TZrChar *result,
                              TZrSize resultSize) {
    TZrSize length1;
    TZrSize length2;
    TZrBool path1HasSeparator;
    TZrBool path2HasSeparator;

    if (result == ZR_NULL || resultSize == 0) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    result[0] = '\0';
    if (path1 == ZR_NULL || path2 == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLibrary_File_IsAbsolutePath(path2)) {
        return file_copy_text(result, resultSize, path2);
    }

    length1 = strlen(path1);
    length2 = strlen(path2);
    if (length1 == 0) {
        return file_copy_text(result, resultSize, path2);
    }
    if (length2 == 0) {
        return file_copy_text(result, resultSize, path1);
    }

    path1HasSeparator = file_is_separator(path1[length1 - 1]);
    path2HasSeparator = file_is_separator(path2[0]);
    if (path1HasSeparator && path2HasSeparator) {
        return snprintf(result, resultSize, "%s%s", path1, path2 + 1) >= 0;
    }
    if (path1HasSeparator || path2HasSeparator) {
        return snprintf(result, resultSize, "%s%s", path1, path2) >= 0;
    }
    return snprintf(result, resultSize, "%s%c%s", path1, ZR_SEPARATOR, path2) >= 0;
}

static TZrBool file_parse_mode(const TZrChar *mode,
                               int *outFlags,
                               TZrBool *outReadable,
                               TZrBool *outWritable,
                               TZrBool *outAppend,
                               TZrChar *normalizedMode,
                               TZrSize normalizedModeSize) {
    TZrChar buffer[ZR_LIBRARY_FILE_STREAM_MODE_MAX];
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;
    TZrBool plus = ZR_FALSE;
    TZrChar kind;
    int flags = O_BINARY;
    TZrBool readable = ZR_FALSE;
    TZrBool writable = ZR_FALSE;
    TZrBool append = ZR_FALSE;

    if (mode == ZR_NULL || mode[0] == '\0' || outFlags == ZR_NULL || outReadable == ZR_NULL ||
        outWritable == ZR_NULL || outAppend == ZR_NULL || normalizedMode == ZR_NULL || normalizedModeSize == 0) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    while (mode[readIndex] != '\0' && writeIndex + 1 < sizeof(buffer)) {
        if (mode[readIndex] != 'b') {
            buffer[writeIndex++] = mode[readIndex];
        }
        readIndex++;
    }
    buffer[writeIndex] = '\0';
    if (buffer[0] == '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }

    kind = buffer[0];
    if (buffer[1] == '+') {
        plus = ZR_TRUE;
        if (buffer[2] != '\0') {
            errno = EINVAL;
            return ZR_FALSE;
        }
    } else if (buffer[1] != '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }

    switch (kind) {
        case 'r':
            readable = ZR_TRUE;
            writable = plus;
            flags |= plus ? O_RDWR : O_RDONLY;
            break;
        case 'w':
            readable = plus;
            writable = ZR_TRUE;
            flags |= (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
            break;
        case 'a':
            readable = plus;
            writable = ZR_TRUE;
            append = ZR_TRUE;
            flags |= (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
            break;
        case 'x':
            readable = plus;
            writable = ZR_TRUE;
            flags |= (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_EXCL;
            break;
        default:
            errno = EINVAL;
            return ZR_FALSE;
    }

    if (snprintf(normalizedMode, normalizedModeSize, "%c%s", kind, plus ? "+" : "") < 0) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    *outFlags = flags;
    *outReadable = readable;
    *outWritable = writable;
    *outAppend = append;
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

static TZrBool file_delete_directory_recursive(const TZrChar *path) {
    SZrLibrary_File_List children;
    TZrSize index;
    TZrBool success = ZR_TRUE;

    memset(&children, 0, sizeof(children));
    if (!ZrLibrary_File_ListDirectory((TZrNativeString)path, ZR_FALSE, &children)) {
        return ZR_FALSE;
    }

    for (index = 0; index < children.count; index++) {
        if (!ZrLibrary_File_Delete(children.entries[index].path, ZR_TRUE)) {
            success = ZR_FALSE;
            break;
        }
    }

    ZrLibrary_File_List_Free(&children);
    if (!success) {
        return ZR_FALSE;
    }
    return file_remove_directory_native(path) == 0;
}

EZrLibrary_File_Exist ZrLibrary_File_Exist(TZrNativeString path) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (path == ZR_NULL || path[0] == '\0' ||
        !ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath))) {
        return ZR_LIBRARY_FILE_NOT_EXIST;
    }

    return file_stat_exist(normalizedPath, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL);
}

TZrBool ZrLibrary_File_IsAbsolutePath(TZrNativeString path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    return (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') ||
           (file_is_separator(path[0]) && file_is_separator(path[1]));
#else
    return file_is_separator(path[0]);
#endif
}

TZrBool ZrLibrary_File_NormalizePath(TZrNativeString path,
                                     ZR_OUT TZrNativeString normalizedPath,
                                     TZrSize normalizedPathSize) {
    TZrChar absolutePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar root[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *segments[ZR_LIBRARY_MAX_PATH_LENGTH / 2U];
    TZrSize segmentLengths[ZR_ARRAY_COUNT(segments)];
    TZrSize segmentCount = 0;
    TZrSize rootLength;
    TZrSize cursor;
    TZrSize writeIndex = 0;

    if (normalizedPath == ZR_NULL || normalizedPathSize == 0 || path == ZR_NULL || path[0] == '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (!file_build_absolute_path(path, absolutePath, sizeof(absolutePath))) {
        return ZR_FALSE;
    }

    file_compact_separators(absolutePath);
    rootLength = file_extract_root(absolutePath, root, sizeof(root));
    cursor = rootLength;

    while (absolutePath[cursor] != '\0') {
        TZrChar *segmentStart;
        TZrSize segmentLength;

        while (file_is_separator(absolutePath[cursor])) {
            cursor++;
        }
        if (absolutePath[cursor] == '\0') {
            break;
        }

        segmentStart = &absolutePath[cursor];
        while (absolutePath[cursor] != '\0' && !file_is_separator(absolutePath[cursor])) {
            cursor++;
        }
        segmentLength = (TZrSize)(&absolutePath[cursor] - segmentStart);
        if (absolutePath[cursor] != '\0') {
            absolutePath[cursor++] = '\0';
        }

        if (file_string_equals(segmentStart, ".") || segmentLength == 0) {
            continue;
        }
        if (file_string_equals(segmentStart, "..")) {
            if (segmentCount > 0) {
                segmentCount--;
            }
            continue;
        }

        if (segmentCount >= ZR_ARRAY_COUNT(segments)) {
            errno = ENAMETOOLONG;
            return ZR_FALSE;
        }
        segments[segmentCount] = segmentStart;
        segmentLengths[segmentCount] = segmentLength;
        segmentCount++;
    }

    normalizedPath[0] = '\0';
    if (root[0] != '\0') {
        if (!file_copy_text(normalizedPath, normalizedPathSize, root)) {
            return ZR_FALSE;
        }
        writeIndex = strlen(normalizedPath);
    }

    for (cursor = 0; cursor < segmentCount; cursor++) {
        if (writeIndex > 0 && !file_is_separator(normalizedPath[writeIndex - 1])) {
            if (writeIndex + 1 >= normalizedPathSize) {
                errno = ENAMETOOLONG;
                return ZR_FALSE;
            }
            normalizedPath[writeIndex++] = ZR_SEPARATOR;
            normalizedPath[writeIndex] = '\0';
        }
        if (writeIndex + segmentLengths[cursor] + 1 > normalizedPathSize) {
            errno = ENAMETOOLONG;
            return ZR_FALSE;
        }
        memcpy(normalizedPath + writeIndex, segments[cursor], segmentLengths[cursor]);
        writeIndex += segmentLengths[cursor];
        normalizedPath[writeIndex] = '\0';
    }

    if (normalizedPath[0] == '\0') {
        if (root[0] != '\0') {
            return file_copy_text(normalizedPath, normalizedPathSize, root);
        }
        return file_copy_text(normalizedPath, normalizedPathSize, ".");
    }

    file_trim_trailing_separator(normalizedPath, rootLength > 0 ? rootLength : 0);
    return ZR_TRUE;
}

TZrBool ZrLibrary_File_GetDirectory(TZrNativeString path, ZR_OUT TZrNativeString directory) {
    SZrLibrary_File_Info info;

    if (directory == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(&info, 0, sizeof(info));
    if (!ZrLibrary_File_QueryInfo(path, &info)) {
        return ZR_FALSE;
    }

    return file_copy_text(directory, ZR_LIBRARY_MAX_PATH_LENGTH, info.parentPath);
}

TZrBool ZrLibrary_File_QueryInfo(TZrNativeString path, ZR_OUT SZrLibrary_File_Info *outInfo) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (path == ZR_NULL || outInfo == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(outInfo, 0, sizeof(*outInfo));
    if (!ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath)) ||
        !file_copy_text(outInfo->path, sizeof(outInfo->path), normalizedPath) ||
        !file_fill_path_parts(normalizedPath, outInfo)) {
        return ZR_FALSE;
    }

    outInfo->existence = file_stat_exist(normalizedPath,
                                         &outInfo->size,
                                         &outInfo->modifiedMilliseconds,
                                         &outInfo->createdMilliseconds,
                                         &outInfo->accessedMilliseconds);
    outInfo->exists = outInfo->existence != ZR_LIBRARY_FILE_NOT_EXIST;
    return ZR_TRUE;
}

void ZrLibrary_File_PathJoin(const TZrChar *path1, const TZrChar *path2, ZR_OUT TZrNativeString result) {
    if (!file_join_into(path1, path2, result, ZR_LIBRARY_MAX_PATH_LENGTH) && result != ZR_NULL) {
        result[0] = '\0';
    }
}

TZrBool ZrLibrary_File_CreateDirectorySingle(TZrNativeString path) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;

    if (path == ZR_NULL || path[0] == '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (!ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath))) {
        return ZR_FALSE;
    }

    existence = file_stat_exist(normalizedPath, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL);
    if (existence == ZR_LIBRARY_FILE_IS_DIRECTORY) {
        return ZR_TRUE;
    }
    if (existence != ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = EEXIST;
        return ZR_FALSE;
    }

    return file_make_directory_native(normalizedPath) == 0;
}

TZrBool ZrLibrary_File_CreateDirectories(TZrNativeString path) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar buffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize rootLength;
    TZrSize index;

    if (path == ZR_NULL || path[0] == '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }

    if (!ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath)) ||
        !file_copy_text(buffer, sizeof(buffer), normalizedPath)) {
        return ZR_FALSE;
    }

    if (file_stat_exist(normalizedPath, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL) == ZR_LIBRARY_FILE_IS_DIRECTORY) {
        return ZR_TRUE;
    }

    rootLength = file_extract_root(buffer, ZR_NULL, 0);
    for (index = rootLength; buffer[index] != '\0'; index++) {
        if (!file_is_separator(buffer[index])) {
            continue;
        }
        {
            TZrChar saved = buffer[index];
            EZrLibrary_File_Exist existence;
            buffer[index] = '\0';
            existence = file_stat_exist(buffer, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL);
            if (buffer[0] != '\0' && existence == ZR_LIBRARY_FILE_NOT_EXIST &&
                file_make_directory_native(buffer) != 0 && errno != EEXIST) {
                buffer[index] = saved;
                return ZR_FALSE;
            }
            if (existence != ZR_LIBRARY_FILE_NOT_EXIST && existence != ZR_LIBRARY_FILE_IS_DIRECTORY) {
                buffer[index] = saved;
                errno = EEXIST;
                return ZR_FALSE;
            }
            buffer[index] = saved;
        }
    }

    return ZrLibrary_File_CreateDirectorySingle(buffer);
}

TZrBool ZrLibrary_File_CreateEmpty(TZrNativeString path, TZrBool recursively) {
    SZrLibrary_File_Info info;
    TZrLibrary_File_Handle handle;

    if (path == ZR_NULL || path[0] == '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(&info, 0, sizeof(info));
    if (!ZrLibrary_File_QueryInfo(path, &info)) {
        return ZR_FALSE;
    }
    if (info.exists) {
        if (info.existence == ZR_LIBRARY_FILE_IS_FILE) {
            return ZR_TRUE;
        }
        errno = EEXIST;
        return ZR_FALSE;
    }

    if (recursively && info.parentPath[0] != '\0' &&
        ZrLibrary_File_Exist(info.parentPath) == ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_CreateDirectories(info.parentPath)) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    handle = ZR_FILE_NATIVE_OPEN(info.path, O_WRONLY | O_CREAT | O_BINARY, _S_IREAD | _S_IWRITE);
#else
    handle = ZR_FILE_NATIVE_OPEN(info.path, O_WRONLY | O_CREAT | O_BINARY, 0666);
#endif
    if (handle == ZR_LIBRARY_FILE_INVALID_HANDLE) {
        return ZR_FALSE;
    }

    return ZrLibrary_File_CloseHandle(handle);
}

TZrBool ZrLibrary_File_Delete(TZrNativeString path, TZrBool recursively) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_File_Exist existence;

    if (path == ZR_NULL || path[0] == '\0') {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath))) {
        return ZR_FALSE;
    }

    existence = file_stat_exist(normalizedPath, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL);
    if (existence == ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = ENOENT;
        return ZR_FALSE;
    }
    if (existence == ZR_LIBRARY_FILE_IS_DIRECTORY) {
        return recursively ? file_delete_directory_recursive(normalizedPath)
                           : (file_remove_directory_native(normalizedPath) == 0);
    }
    return file_unlink_native(normalizedPath) == 0;
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

    existence = file_stat_exist(normalizedSource, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL);
    if (existence == ZR_LIBRARY_FILE_NOT_EXIST) {
        errno = ENOENT;
        return ZR_FALSE;
    }
    if (file_string_equals(normalizedSource, normalizedTarget)) {
        return ZR_TRUE;
    }

    return existence == ZR_LIBRARY_FILE_IS_DIRECTORY
                   ? file_copy_directory_recursive(normalizedSource, normalizedTarget, overwrite)
                   : file_copy_file_contents(normalizedSource, normalizedTarget, overwrite);
}

TZrBool ZrLibrary_File_Move(TZrNativeString sourcePath, TZrNativeString targetPath, TZrBool overwrite) {
    TZrChar normalizedSource[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedTarget[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (sourcePath == ZR_NULL || targetPath == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_NormalizePath(sourcePath, normalizedSource, sizeof(normalizedSource)) ||
        !ZrLibrary_File_NormalizePath(targetPath, normalizedTarget, sizeof(normalizedTarget))) {
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

TZrBool ZrLibrary_File_ListDirectory(TZrNativeString path,
                                     TZrBool recursively,
                                     ZR_OUT SZrLibrary_File_List *outList) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (path == ZR_NULL || outList == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(outList, 0, sizeof(*outList));
    if (!ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath))) {
        return ZR_FALSE;
    }
    if (file_stat_exist(normalizedPath, ZR_NULL, ZR_NULL, ZR_NULL, ZR_NULL) != ZR_LIBRARY_FILE_IS_DIRECTORY) {
        errno = ENOTDIR;
        return ZR_FALSE;
    }
    if (!file_list_directory_recursive(normalizedPath, recursively, outList)) {
        ZrLibrary_File_List_Free(outList);
        return ZR_FALSE;
    }

    qsort(outList->entries, outList->count, sizeof(outList->entries[0]), file_list_entry_compare);
    return ZR_TRUE;
}

TZrBool ZrLibrary_File_Glob(TZrNativeString path,
                            TZrNativeString pattern,
                            TZrBool recursively,
                            ZR_OUT SZrLibrary_File_List *outList) {
    SZrLibrary_File_List allEntries;
    TZrChar normalizedBase[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize baseLength;
    TZrSize index;

    if (path == ZR_NULL || pattern == ZR_NULL || outList == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(&allEntries, 0, sizeof(allEntries));
    memset(outList, 0, sizeof(*outList));
    if (!ZrLibrary_File_NormalizePath(path, normalizedBase, sizeof(normalizedBase)) ||
        !ZrLibrary_File_ListDirectory(path, recursively, &allEntries)) {
        return ZR_FALSE;
    }

    baseLength = strlen(normalizedBase);
    for (index = 0; index < allEntries.count; index++) {
        const TZrChar *candidate = file_basename_ptr(allEntries.entries[index].path);
        const TZrChar *relativeCandidate = candidate;
        TZrBool usesRelativePattern = strchr(pattern, '/') != ZR_NULL || strchr(pattern, '\\') != ZR_NULL;

        if (usesRelativePattern && strncmp(allEntries.entries[index].path, normalizedBase, baseLength) == 0) {
            relativeCandidate = allEntries.entries[index].path + baseLength;
            while (*relativeCandidate != '\0' && file_is_separator(*relativeCandidate)) {
                relativeCandidate++;
            }
        }

        if (file_wildcard_match(pattern, usesRelativePattern ? relativeCandidate : candidate) &&
            !file_list_push(outList, allEntries.entries[index].path, allEntries.entries[index].existence)) {
            ZrLibrary_File_List_Free(&allEntries);
            ZrLibrary_File_List_Free(outList);
            return ZR_FALSE;
        }
    }

    ZrLibrary_File_List_Free(&allEntries);
    qsort(outList->entries, outList->count, sizeof(outList->entries[0]), file_list_entry_compare);
    return ZR_TRUE;
}

void ZrLibrary_File_List_Free(ZR_OUT SZrLibrary_File_List *list) {
    if (list == ZR_NULL) {
        return;
    }

    free(list->entries);
    list->entries = ZR_NULL;
    list->count = 0;
    list->capacity = 0;
}

TZrBool ZrLibrary_File_OpenHandle(TZrNativeString path,
                                  TZrNativeString mode,
                                  ZR_OUT SZrLibrary_File_StreamOpenResult *outResult) {
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    int flags = 0;

    if (path == ZR_NULL || mode == ZR_NULL || outResult == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    memset(outResult, 0, sizeof(*outResult));
    outResult->handle = ZR_LIBRARY_FILE_INVALID_HANDLE;
    if (!ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath)) ||
        !file_parse_mode(mode,
                         &flags,
                         &outResult->readable,
                         &outResult->writable,
                         &outResult->append,
                         outResult->normalizedMode,
                         sizeof(outResult->normalizedMode))) {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    outResult->handle = ZR_FILE_NATIVE_OPEN(normalizedPath, flags, _S_IREAD | _S_IWRITE);
#else
    outResult->handle = ZR_FILE_NATIVE_OPEN(normalizedPath, flags, 0666);
#endif
    if (outResult->handle == ZR_LIBRARY_FILE_INVALID_HANDLE) {
        return ZR_FALSE;
    }

    if (outResult->append) {
        TZrInt64 ignoredPosition = 0;
        if (!ZrLibrary_File_SeekHandle(outResult->handle, 0, SEEK_END, &ignoredPosition)) {
            ZrLibrary_File_CloseHandle(outResult->handle);
            outResult->handle = ZR_LIBRARY_FILE_INVALID_HANDLE;
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLibrary_File_CloseHandle(TZrLibrary_File_Handle handle) {
    if (handle == ZR_LIBRARY_FILE_INVALID_HANDLE) {
        errno = EBADF;
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    return _close(handle) == 0;
#else
    return close(handle) == 0;
#endif
}

TZrBool ZrLibrary_File_ReadHandle(TZrLibrary_File_Handle handle,
                                  void *buffer,
                                  TZrSize requestedSize,
                                  ZR_OUT TZrSize *outReadSize) {
    unsigned int chunkSize = 0;

    if (buffer == ZR_NULL || outReadSize == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    *outReadSize = 0;
    if (!file_handle_readable_chunk_size(requestedSize, &chunkSize)) {
        return ZR_FALSE;
    }
    if (chunkSize == 0) {
        return ZR_TRUE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        int readSize = _read(handle, buffer, (unsigned int)chunkSize);
        if (readSize < 0) {
            return ZR_FALSE;
        }
        *outReadSize = (TZrSize)readSize;
        return ZR_TRUE;
    }
#else
    {
        ssize_t readSize = read(handle, buffer, (size_t)chunkSize);
        if (readSize < 0) {
            return ZR_FALSE;
        }
        *outReadSize = (TZrSize)readSize;
        return ZR_TRUE;
    }
#endif
}

TZrBool ZrLibrary_File_WriteHandle(TZrLibrary_File_Handle handle,
                                   const void *buffer,
                                   TZrSize requestedSize,
                                   ZR_OUT TZrSize *outWrittenSize) {
    unsigned int chunkSize = 0;

    if (buffer == ZR_NULL || outWrittenSize == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    *outWrittenSize = 0;
    if (!file_handle_readable_chunk_size(requestedSize, &chunkSize)) {
        return ZR_FALSE;
    }
    if (chunkSize == 0) {
        return ZR_TRUE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        int writtenSize = _write(handle, buffer, (unsigned int)chunkSize);
        if (writtenSize < 0) {
            return ZR_FALSE;
        }
        *outWrittenSize = (TZrSize)writtenSize;
        return ZR_TRUE;
    }
#else
    {
        ssize_t writtenSize = write(handle, buffer, (size_t)chunkSize);
        if (writtenSize < 0) {
            return ZR_FALSE;
        }
        *outWrittenSize = (TZrSize)writtenSize;
        return ZR_TRUE;
    }
#endif
}

TZrBool ZrLibrary_File_SeekHandle(TZrLibrary_File_Handle handle,
                                  TZrInt64 offset,
                                  int origin,
                                  ZR_OUT TZrInt64 *outPosition) {
    if (outPosition == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        TZrInt64 position = _lseeki64(handle, offset, origin);
        if (position < 0) {
            return ZR_FALSE;
        }
        *outPosition = position;
        return ZR_TRUE;
    }
#else
    {
        off_t position = lseek(handle, (off_t)offset, origin);
        if (position < 0) {
            return ZR_FALSE;
        }
        *outPosition = (TZrInt64)position;
        return ZR_TRUE;
    }
#endif
}

TZrBool ZrLibrary_File_GetHandlePosition(TZrLibrary_File_Handle handle, ZR_OUT TZrInt64 *outPosition) {
    return ZrLibrary_File_SeekHandle(handle, 0, SEEK_CUR, outPosition);
}

TZrBool ZrLibrary_File_GetHandleLength(TZrLibrary_File_Handle handle, ZR_OUT TZrInt64 *outLength) {
    if (outLength == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    {
        struct _stat64 info;
        if (_fstat64(handle, &info) != 0) {
            return ZR_FALSE;
        }
        *outLength = (TZrInt64)info.st_size;
        return ZR_TRUE;
    }
#else
    {
        struct stat info;
        if (fstat(handle, &info) != 0) {
            return ZR_FALSE;
        }
        *outLength = (TZrInt64)info.st_size;
        return ZR_TRUE;
    }
#endif
}

TZrBool ZrLibrary_File_SetHandleLength(TZrLibrary_File_Handle handle, TZrInt64 length) {
    if (length < 0) {
        errno = EINVAL;
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    return _chsize_s(handle, (__int64)length) == 0;
#else
    return ftruncate(handle, (off_t)length) == 0;
#endif
}

TZrBool ZrLibrary_File_FlushHandle(TZrLibrary_File_Handle handle) {
#if defined(ZR_PLATFORM_WIN)
    return _commit(handle) == 0;
#else
    return fsync(handle) == 0;
#endif
}

TZrNativeString ZrLibrary_File_ReadAll(SZrGlobalState *global, TZrNativeString path) {
    SZrLibrary_File_Reader *reader = ZrLibrary_File_OpenRead(global, path, ZR_FALSE);
    TZrNativeString buffer;
    TZrSize readSize;

    if (reader == ZR_NULL) {
        return ZR_NULL;
    }

    buffer = ZrCore_Memory_RawMallocWithType(global, reader->size + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    if (buffer == ZR_NULL) {
        ZrLibrary_File_CloseRead(global, reader);
        return ZR_NULL;
    }

    readSize = fread(buffer, 1, reader->size, reader->file);
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
    SZrLibrary_File_Reader *reader;
    TZrChar normalizedPath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (path == ZR_NULL || !ZrLibrary_File_NormalizePath(path, normalizedPath, sizeof(normalizedPath)) ||
        ZrLibrary_File_Exist(normalizedPath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_NULL;
    }

    reader = ZrCore_Memory_RawMallocWithType(global, sizeof(SZrLibrary_File_Reader), ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
    if (reader == ZR_NULL) {
        return ZR_NULL;
    }

    reader->file = fopen(normalizedPath, isBinary ? "rb" : "r");
    if (reader->file == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, reader, sizeof(SZrLibrary_File_Reader), ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
        return ZR_NULL;
    }

    file_copy_text(reader->normalizedPath, sizeof(reader->normalizedPath), normalizedPath);
    fseek(reader->file, 0, SEEK_END);
    reader->size = (TZrSize)ftell(reader->file);
    fseek(reader->file, 0, SEEK_SET);
    return reader;
}

void ZrLibrary_File_CloseRead(SZrGlobalState *global, SZrLibrary_File_Reader *reader) {
    if (reader == ZR_NULL) {
        return;
    }

    if (reader->file != ZR_NULL) {
        fclose(reader->file);
        reader->file = ZR_NULL;
    }

    ZrCore_Memory_RawFreeWithType(global, reader, sizeof(SZrLibrary_File_Reader), ZR_MEMORY_NATIVE_TYPE_FILE_BUFFER);
}

TZrBool ZrLibrary_File_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5, SZrIo *io) {
    SZrLibrary_File_Reader *reader;

    ZR_UNUSED_PARAMETER(md5);
    reader = ZrLibrary_File_OpenRead(state->global, path, ZR_FALSE);
    if (reader == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Io_Init(state, io, ZrLibrary_File_SourceReadImplementation, ZrLibrary_File_SourceCloseImplementation, reader);
    return ZR_TRUE;
}

TZrBytePtr ZrLibrary_File_SourceReadImplementation(SZrState *state, TZrPtr reader, ZR_OUT TZrSize *size) {
    SZrLibrary_File_Reader *fileReader = (SZrLibrary_File_Reader *)reader;
    TZrSize readSize;

    ZR_UNUSED_PARAMETER(state);
    if (fileReader == ZR_NULL || size == ZR_NULL) {
        return ZR_NULL;
    }

    readSize = fread(fileReader->buffer, 1, ZR_LIBRARY_FILE_BUFFER_SIZE, fileReader->file);
    if (readSize == 0) {
        return ZR_NULL;
    }

    *size = readSize;
    return (TZrBytePtr)fileReader->buffer;
}

void ZrLibrary_File_SourceCloseImplementation(SZrState *state, TZrPtr reader) {
    if (state == ZR_NULL || reader == ZR_NULL) {
        return;
    }

    ZrLibrary_File_CloseRead(state->global, (SZrLibrary_File_Reader *)reader);
}
