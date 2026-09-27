#ifndef ZR_VM_TESTS_AOT_C_TYPED_CALL_CONTRACT_SUPPORT_H
#define ZR_VM_TESTS_AOT_C_TYPED_CALL_CONTRACT_SUPPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#ifndef ARRAY_COUNT
/* 供固定数组合同针脚求元素数；指针参数不适用。 */
#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))
#endif

/* 读取完整源码文本，成功返回的缓冲区交调用方释放。 */
static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

/* 优先由 __FILE__ 定位仓库根目录；路径标记缺失时按当前目录读取。 */
static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == NULL) {
        return NULL;
    }

    marker = strstr(sourceFile, "tests/parser/");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

/* 逐项核对生成器源码针脚，失败时指出缺失片段。 */
static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            /* BUG: Unity 失败跳转会跳过调用方末尾的源码缓冲区释放。 */
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

#endif
