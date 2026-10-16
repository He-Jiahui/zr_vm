/* Lexical host paths and bounded path text. Filesystem operations live in file.c. */
#include "file_path_internal.h"
#include <errno.h>
#include <string.h>
#if defined(ZR_PLATFORM_WIN)
#include <direct.h>
#else
#include <unistd.h>
#endif
#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif
TZrBool file_is_separator(TZrChar ch) {
    return ch == '/' || ch == '\\';
}

TZrBool file_string_equals(const TZrChar *lhs, const TZrChar *rhs) {
    return lhs != ZR_NULL && rhs != ZR_NULL && strcmp(lhs, rhs) == 0;
}

TZrBool file_copy_text(TZrNativeString destination, TZrSize destinationSize, const TZrChar *source) {
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

TZrBool file_path_is_root(const TZrChar *path) {
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

TZrSize file_extract_root(const TZrChar *path, TZrChar *root, TZrSize rootSize) {
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

#if defined(ZR_PLATFORM_WIN)
    /* Resolving C:relative needs per-drive cwd state. Reject it explicitly. */
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' && !file_is_separator(path[2])) {
        errno = EINVAL;
        return ZR_FALSE;
    }
#endif
    if (ZrLibrary_File_IsAbsolutePath(path)) {
        return file_copy_text(absolutePath, absolutePathSize, path);
    }

    if (!file_get_current_directory(currentDirectory, sizeof(currentDirectory))) {
        return ZR_FALSE;
    }

    return file_join_into(currentDirectory, path, absolutePath, absolutePathSize);
}

static void file_compact_separators(TZrChar *path) {
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;
    TZrBool previousWasSeparator = ZR_FALSE;

    if (path == ZR_NULL) {
        return;
    }

#if defined(ZR_PLATFORM_WIN)
    /* A UNC root starts with two separators; compact only the remainder. */
    if (file_is_separator(path[0]) && file_is_separator(path[1])) {
        path[0] = path[1] = ZR_SEPARATOR;
        readIndex = writeIndex = 2;
        previousWasSeparator = ZR_TRUE;
    }
#endif
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

TZrBool file_fill_path_parts(const TZrChar *normalizedPath, SZrLibrary_File_Info *outInfo) {
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

const TZrChar *file_basename_ptr(const TZrChar *path) {
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

TZrBool file_join_into(const TZrChar *path1,
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
    {
        int length;
        if (path1HasSeparator && path2HasSeparator) {
            length = snprintf(result, resultSize, "%s%s", path1, path2 + 1);
        } else if (path1HasSeparator || path2HasSeparator) {
            length = snprintf(result, resultSize, "%s%s", path1, path2);
        } else {
            length = snprintf(result, resultSize, "%s%c%s", path1, ZR_SEPARATOR, path2);
        }
        if (length < 0 || (TZrSize)length >= resultSize) {
            result[0] = '\0';
            errno = length < 0 ? EINVAL : ENAMETOOLONG;
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }
}

TZrBool ZrLibrary_File_IsAbsolutePath(TZrNativeString path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

#if defined(ZR_PLATFORM_WIN)
    return (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
            path[1] == ':' && file_is_separator(path[2])) ||
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

void ZrLibrary_File_PathJoin(const TZrChar *path1, const TZrChar *path2, ZR_OUT TZrNativeString result) {
    if (!file_join_into(path1, path2, result, ZR_LIBRARY_MAX_PATH_LENGTH) && result != ZR_NULL) {
        result[0] = '\0';
    }
}
