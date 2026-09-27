#ifndef ZR_VM_TESTS_PARSER_AOT_SOURCE_CONTRACT_MATCH_H
#define ZR_VM_TESTS_PARSER_AOT_SOURCE_CONTRACT_MATCH_H

#include <string.h>

/* 源码契约只折叠 ASCII 空白，避免生成器换行和缩进变化误伤片段断言。 */
static int zr_test_aot_source_contract_is_ascii_whitespace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\f' || value == '\v';
}

/* AOT 合约测试先查原样片段，再允许模式与生成源码之间插入 ASCII 空白。 */
static int zr_test_aot_source_contract_contains(const char *text, const char *needle) {
    const char *start;

    if (text == NULL || needle == NULL) {
        return 0;
    }
    if (strstr(text, needle) != NULL) {
        return 1;
    }
    while (zr_test_aot_source_contract_is_ascii_whitespace(*needle)) {
        needle++;
    }
    if (*needle == '\0') {
        return 1;
    }

    for (start = text; *start != '\0'; start++) {
        const char *textCursor = start;
        const char *needleCursor = needle;

        if (zr_test_aot_source_contract_is_ascii_whitespace(*start)) {
            continue;
        }
        for (;;) {
            while (zr_test_aot_source_contract_is_ascii_whitespace(*textCursor)) {
                textCursor++;
            }
            while (zr_test_aot_source_contract_is_ascii_whitespace(*needleCursor)) {
                needleCursor++;
            }
            if (*needleCursor == '\0') {
                return 1;
            }
            if (*textCursor == '\0' || *textCursor != *needleCursor) {
                break;
            }
            textCursor++;
            needleCursor++;
        }
    }
    return 0;
}

#endif
