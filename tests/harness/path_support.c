#include "path_support.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <direct.h>
#define zr_tests_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#define zr_tests_mkdir(path) mkdir(path, 0755)
#endif

#ifndef ZR_VM_TESTS_SOURCE_DIR
#define ZR_VM_TESTS_SOURCE_DIR "tests"
#endif

#ifndef ZR_VM_TESTS_BINARY_DIR
#define ZR_VM_TESTS_BINARY_DIR "tests_generated"
#endif

static void zr_tests_normalize_separators(TZrChar *path) {
    if (path == ZR_NULL) {
        return;
    }

    for (; *path != '\0'; path++) {
        if (*path == '\\') {
            *path = '/';
        }
    }
}

static TZrBool zr_tests_format_path(TZrChar *outPath, TZrSize maxLen, const TZrChar *format, ...) {
    va_list args;
    int written;

    if (outPath == ZR_NULL || maxLen == 0 || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(outPath, maxLen, format, args);
    va_end(args);

    if (written < 0 || (TZrSize) written >= maxLen) {
        if (maxLen > 0) {
            outPath[0] = '\0';
        }
        return ZR_FALSE;
    }

    zr_tests_normalize_separators(outPath);
    return ZR_TRUE;
}

static TZrBool zr_tests_make_directory(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }

    if (zr_tests_mkdir(path) == 0) {
        return ZR_TRUE;
    }

    return errno == EEXIST ? ZR_TRUE : ZR_FALSE;
}

TZrBool ZrTests_Path_EnsureParentDirectory(const TZrChar *filePath) {
    TZrChar working[ZR_TESTS_PATH_MAX];
    TZrSize startIndex = 0;
    TZrSize index;
    TZrSize length;

    if (filePath == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_tests_format_path(working, sizeof(working), "%s", filePath)) {
        return ZR_FALSE;
    }

    length = strlen(working);
    while (length > 0 && working[length - 1] != '/') {
        working[--length] = '\0';
    }

    if (length == 0) {
        return ZR_TRUE;
    }

    if (length >= 3 && working[1] == ':' && working[2] == '/') {
        startIndex = 3;
    } else if (working[0] == '/') {
        startIndex = 1;
    }

    for (index = startIndex; working[index] != '\0'; index++) {
        if (working[index] != '/') {
            continue;
        }

        working[index] = '\0';
        if (working[0] != '\0' && !zr_tests_make_directory(working)) {
            return ZR_FALSE;
        }
        working[index] = '/';
    }

    return zr_tests_make_directory(working);
}

TZrBool ZrTests_Path_GetFixture(const TZrChar *group,
                                const TZrChar *fileName,
                                TZrChar *outPath,
                                TZrSize maxLen) {
    if (group == ZR_NULL || fileName == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_tests_format_path(outPath, maxLen, "%s/fixtures/%s/%s", ZR_VM_TESTS_SOURCE_DIR, group, fileName);
}

TZrBool ZrTests_Path_GetParserFixture(const TZrChar *fileName, TZrChar *outPath, TZrSize maxLen) {
    return ZrTests_Path_GetFixture("parser", fileName, outPath, maxLen);
}

TZrBool ZrTests_Path_GetProjectFile(const TZrChar *projectName,
                                    const TZrChar *relativePath,
                                    TZrChar *outPath,
                                    TZrSize maxLen) {
    if (projectName == ZR_NULL || relativePath == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_tests_format_path(outPath,
                                maxLen,
                                "%s/fixtures/projects/%s/%s",
                                ZR_VM_TESTS_SOURCE_DIR,
                                projectName,
                                relativePath);
}

TZrBool ZrTests_Path_GetRepoDoc(const TZrChar *relativePath, TZrChar *outPath, TZrSize maxLen) {
    if (relativePath == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_tests_format_path(outPath, maxLen, "%s/../docs/%s", ZR_VM_TESTS_SOURCE_DIR, relativePath);
}

TZrBool ZrTests_Path_GetGoldenArtifact(const TZrChar *subDir,
                                       const TZrChar *baseName,
                                       const TZrChar *extension,
                                       TZrChar *outPath,
                                       TZrSize maxLen) {
    if (subDir == ZR_NULL || baseName == ZR_NULL || extension == ZR_NULL) {
        return ZR_FALSE;
    }

    return zr_tests_format_path(outPath,
                                maxLen,
                                "%s/golden/%s/%s%s",
                                ZR_VM_TESTS_SOURCE_DIR,
                                subDir,
                                baseName,
                                extension);
}

TZrBool ZrTests_Path_GetGeneratedArtifact(const TZrChar *suiteName,
                                          const TZrChar *subDir,
                                          const TZrChar *baseName,
                                          const TZrChar *extension,
                                          TZrChar *outPath,
                                          TZrSize maxLen) {
    if (suiteName == ZR_NULL || subDir == ZR_NULL || baseName == ZR_NULL || extension == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!zr_tests_format_path(outPath,
                              maxLen,
                              "%s/%s/%s/%s%s",
                              ZR_VM_TESTS_BINARY_DIR,
                              suiteName,
                              subDir,
                              baseName,
                              extension)) {
        return ZR_FALSE;
    }

    return ZrTests_Path_EnsureParentDirectory(outPath);
}

TZrBool ZrTests_File_Exists(const TZrChar *path) {
    FILE *file;

    if (path == ZR_NULL) {
        return ZR_FALSE;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }

    fclose(file);
    return ZR_TRUE;
}

TZrChar *ZrTests_ReadTextFile(const TZrChar *path, TZrSize *outLength) {
    TZrBytePtr buffer = ZR_NULL;
    TZrSize length = 0;

    if (!ZrTests_ReadFileBytes(path, &buffer, &length)) {
        return ZR_NULL;
    }

    if (outLength != ZR_NULL) {
        *outLength = length;
    }

    return (TZrChar *) buffer;
}

TZrBool ZrTests_ReadFileBytes(const TZrChar *path, TZrBytePtr *outBuffer, TZrSize *outLength) {
    FILE *file;
    TZrBytePtr buffer;
    long fileSize;
    TZrSize readSize;

    if (path == ZR_NULL || outBuffer == ZR_NULL || outLength == ZR_NULL) {
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

    buffer = (TZrBytePtr) malloc((TZrSize) fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }

    readSize = (TZrSize) fread(buffer, 1, (TZrSize) fileSize, file);
    fclose(file);
    if (readSize != (TZrSize) fileSize) {
        free(buffer);
        return ZR_FALSE;
    }

    buffer[readSize] = '\0';
    *outBuffer = buffer;
    *outLength = readSize;
    return ZR_TRUE;
}
