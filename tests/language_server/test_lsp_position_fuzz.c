//
// Focused LSP position codec fuzz and malformed UTF-8 robustness checks.
//

#include <stdio.h>
#include <string.h>

#include "zr_vm_language_server.h"

/* 各独立位置用例累积失败数，供 main 的 CTest 退出码使用。 */
static int g_failures = 0;

/* 从 LSP UTF-16 行列回到 parser 字节偏移时，同时核对内部 1 基行列。 */
static void check_file_position(const TZrChar *summary,
                                const TZrChar *content,
                                TZrSize contentLength,
                                TZrInt32 line,
                                TZrInt32 character,
                                TZrSize expectedOffset,
                                TZrInt32 expectedFileLine,
                                TZrInt32 expectedFileColumn) {
    SZrLspPosition lspPosition;
    SZrFilePosition filePosition;

    lspPosition.line = line;
    lspPosition.character = character;

    filePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(lspPosition,
                                                                          content,
                                                                          contentLength);
    if (filePosition.offset != expectedOffset ||
        filePosition.line != expectedFileLine ||
        filePosition.column != expectedFileColumn) {
        printf("FAIL: %s expected file position %llu:%d:%d but got %llu:%d:%d\n",
               summary,
               (unsigned long long)expectedOffset,
               expectedFileLine,
               expectedFileColumn,
               (unsigned long long)filePosition.offset,
               filePosition.line,
               filePosition.column);
        g_failures++;
        return;
    }

    printf("PASS: %s\n", summary);
}

/* 从源码字节边界返回 LSP 位置，验证反向转换不吞掉换行。 */
static void check_lsp_position_from_offset(const TZrChar *summary,
                                           const TZrChar *content,
                                           TZrSize contentLength,
                                           TZrSize offset,
                                           TZrInt32 expectedLine,
                                           TZrInt32 expectedCharacter) {
    SZrFilePosition filePosition;
    SZrLspPosition lspPosition;

    filePosition = ZrParser_FilePosition_Create(offset, 1, 1);
    lspPosition = ZrLanguageServer_LspPosition_FromFilePositionWithContent(filePosition,
                                                                           content,
                                                                           contentLength);
    if (lspPosition.line != expectedLine || lspPosition.character != expectedCharacter) {
        printf("FAIL: %s expected LSP position %d:%d but got %d:%d\n",
               summary,
               expectedLine,
               expectedCharacter,
               lspPosition.line,
               lspPosition.character);
        g_failures++;
        return;
    }

    printf("PASS: %s\n", summary);
}

/* 固定种子生成可复现序列，失败时可用相同字节流重新定位。 */
static TZrUInt32 fuzz_next(TZrUInt32 *state) {
    TZrUInt32 value = *state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

/* 仅为 choose_valid_codepoint 的合法标量编码；调用方预留四字节空间。 */
static TZrSize append_utf8_codepoint(TZrChar *buffer, TZrSize offset, TZrUInt32 codepoint) {
    if (codepoint <= 0x7Fu) {
        buffer[offset++] = (TZrChar)codepoint;
    } else if (codepoint <= 0x7FFu) {
        buffer[offset++] = (TZrChar)(0xC0u | (codepoint >> 6));
        buffer[offset++] = (TZrChar)(0x80u | (codepoint & 0x3Fu));
    } else if (codepoint <= 0xFFFFu) {
        buffer[offset++] = (TZrChar)(0xE0u | (codepoint >> 12));
        buffer[offset++] = (TZrChar)(0x80u | ((codepoint >> 6) & 0x3Fu));
        buffer[offset++] = (TZrChar)(0x80u | (codepoint & 0x3Fu));
    } else {
        buffer[offset++] = (TZrChar)(0xF0u | (codepoint >> 18));
        buffer[offset++] = (TZrChar)(0x80u | ((codepoint >> 12) & 0x3Fu));
        buffer[offset++] = (TZrChar)(0x80u | ((codepoint >> 6) & 0x3Fu));
        buffer[offset++] = (TZrChar)(0x80u | (codepoint & 0x3Fu));
    }

    return offset;
}

/* 混合 ASCII、换行、BMP 与非 BMP 字符，让编码边界在固定样本中都出现。 */
static TZrUInt32 choose_valid_codepoint(TZrUInt32 *state) {
    TZrUInt32 value = fuzz_next(state);

    switch (value % 6u) {
        case 0:
            return (TZrUInt32)('a' + (value % 26u));
        case 1:
            return (TZrUInt32)('0' + (value % 10u));
        case 2:
            return '\n';
        case 3:
            return 0x03BBu;
        case 4:
            return 0x4E2Du;
        default:
            return 0x1F600u + (value % 16u);
    }
}

/* 非法双字节前导之后的 LF 仍应成为下一行起点，正反方向都要保留。 */
static void test_malformed_utf8_lead_byte_does_not_swallow_newline(void) {
    const TZrChar content[] = {
        'a',
        (TZrChar)0xC2,
        '\n',
        'b',
        '\0'
    };
    const TZrSize contentLength = sizeof(content) - 1;

    check_file_position("Malformed two-byte lead does not swallow following newline",
                        content,
                        contentLength,
                        1,
                        0,
                        3,
                        2,
                        1);
    check_lsp_position_from_offset("Malformed two-byte lead preserves newline in reverse mapping",
                                   content,
                                   contentLength,
                                   3,
                                   1,
                                   0);
}

/* 截断四字节序列之后的 LF 不能被错误当作续字节吞掉。 */
static void test_truncated_four_byte_sequence_does_not_swallow_newline(void) {
    const TZrChar content[] = {
        'x',
        (TZrChar)0xF0,
        (TZrChar)0x9F,
        '\n',
        'y',
        '\0'
    };
    const TZrSize contentLength = sizeof(content) - 1;

    check_file_position("Truncated four-byte sequence does not swallow following newline",
                        content,
                        contentLength,
                        1,
                        0,
                        4,
                        2,
                        1);
    check_lsp_position_from_offset("Truncated four-byte sequence preserves newline in reverse mapping",
                                   content,
                                   contentLength,
                                   4,
                                   1,
                                   0);
}

/* 只在完整码点边界要求 offset 往返恒等；UTF-16 代理对内部不是源码字节边界。 */
static void test_deterministic_valid_utf8_roundtrip_fuzz(void) {
    TZrUInt32 seed = 0x5EED1234u;
    TZrChar content[512];
    TZrSize boundaries[160];
    TZrSize contentLength = 0;
    TZrSize boundaryCount = 0;
    TZrSize index;

    boundaries[boundaryCount++] = 0;
    for (index = 0; index < 128; index++) {
        TZrUInt32 codepoint = choose_valid_codepoint(&seed);
        if (contentLength + 4 >= sizeof(content)) {
            break;
        }

        contentLength = append_utf8_codepoint(content, contentLength, codepoint);
        boundaries[boundaryCount++] = contentLength;
    }
    content[contentLength] = '\0';

    for (index = 0; index < boundaryCount; index++) {
        SZrFilePosition originalFilePosition;
        SZrLspPosition lspPosition;
        SZrFilePosition mappedFilePosition;
        TZrSize offset = boundaries[index];

        originalFilePosition = ZrParser_FilePosition_Create(offset, 1, 1);
        lspPosition = ZrLanguageServer_LspPosition_FromFilePositionWithContent(originalFilePosition,
                                                                               content,
                                                                               contentLength);
        mappedFilePosition = ZrLanguageServer_LspPosition_ToFilePositionWithContent(lspPosition,
                                                                                    content,
                                                                                    contentLength);
        if (mappedFilePosition.offset != offset) {
            printf("FAIL: Deterministic valid UTF-8 fuzz roundtrip boundary %llu expected offset %llu but got %llu via %d:%d\n",
                   (unsigned long long)index,
                   (unsigned long long)offset,
                   (unsigned long long)mappedFilePosition.offset,
                   lspPosition.line,
                   lspPosition.character);
            g_failures++;
            return;
        }
    }

    printf("PASS: Deterministic valid UTF-8 fuzz roundtrip\n");
}

/* 独立 CTest 目标先覆盖畸形序列，再覆盖固定有效 UTF-8 序列的所有边界。 */
int main(void) {
    printf("==========\n");
    printf("Language Server - LSP Position Fuzz Tests\n");
    printf("==========\n\n");

    test_malformed_utf8_lead_byte_does_not_swallow_newline();
    test_truncated_four_byte_sequence_does_not_swallow_newline();
    test_deterministic_valid_utf8_roundtrip_fuzz();

    printf("\n==========\n");
    if (g_failures == 0) {
        printf("All LSP Position Fuzz Tests Completed\n");
        printf("==========\n");
        return 0;
    }

    printf("%d LSP Position Fuzz Test(s) Failed\n", g_failures);
    printf("==========\n");
    return 1;
}
