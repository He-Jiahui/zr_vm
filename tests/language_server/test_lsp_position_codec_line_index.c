/* U5a: the request-scoped line index must give exactly the same UTF-16 positions as the
 * full-scan converter for every byte offset, including CRLF, lone CR, astral and CJK
 * characters, malformed or truncated UTF-8 and offsets past the end. */
#include <stdio.h>
#include <string.h>

#include "interface/lsp_position_codec.h"
#include "runtime_support.h"

static int check(SZrState *state, const char *label, const char *content, size_t length) {
    SZrLspLineIndex index;
    int failures = 0;
    if (!ZrLanguageServer_LspLineIndex_Build(state, &index, content, length)) {
        printf("FAIL: %s: build\n", label);
        return 1;
    }
    for (size_t offset = 0; offset <= length + 2; ++offset) {
        SZrLspPosition expected = ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(content, length, offset);
        SZrLspPosition actual = ZrLanguageServer_LspLineIndex_ByteOffsetToUtf16Position(&index, content, length, offset);
        if (expected.line != actual.line || expected.character != actual.character) {
            if (failures++ < 5) {
                printf("FAIL: %s offset %zu expected %d:%d got %d:%d\n", label, offset,
                       expected.line, expected.character, actual.line, actual.character);
            }
        }
    }
    /* A different content pointer must fall back to the full scan, not reuse the index. */
    {
        char copy[256];
        size_t n = length < sizeof(copy) ? length : sizeof(copy);
        memcpy(copy, content, n);
        for (size_t offset = 0; offset <= n; ++offset) {
            SZrLspPosition expected = ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(copy, n, offset);
            SZrLspPosition actual = ZrLanguageServer_LspLineIndex_ByteOffsetToUtf16Position(&index, copy, n, offset);
            if (expected.line != actual.line || expected.character != actual.character) failures++;
        }
    }
    ZrLanguageServer_LspLineIndex_Free(state, &index);
    printf("%s: %s (%zu bytes)\n", failures == 0 ? "PASS" : "FAIL", label, length);
    return failures != 0;
}

int main(void) {
    static const char ascii[] = "fn add(a: int): int {\n    return a;\n}\nlet x = add(1);";
    static const char crlf[] = "let a = 1;\r\nlet b = 2;\r\n\r\nlet c = a + b;\r\n";
    static const char lonecr[] = "a\rb\r\rc\nd\r";
    static const char astral[] = "/* \xF0\x9F\x98\x80\xF0\x9F\x98\x80 */ let s = \"\xF0\x9F\x98\x80\";\n\xF0\x9F\x98\x80x\n";
    static const char cjk[] = "let \xE4\xB8\xAD\xE6\x96\x87 = 1;\n// \xE4\xB8\xAD\r\n\xE6\x96\x87";
    static const char malformed[] = "a\xFF" "b\n\xC3\n\xE4\xB8\n\xF0\x9F\x98\nz\x80\x80\n";
    static const char truncated[] = "x\n\xF0\x9F";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    int failures = 0;
    SZrLspLineIndex empty;
    if (state == ZR_NULL) return 1;
    failures += check(state, "ascii without trailing newline", ascii, sizeof(ascii) - 1);
    failures += check(state, "crlf", crlf, sizeof(crlf) - 1);
    failures += check(state, "lone cr", lonecr, sizeof(lonecr) - 1);
    failures += check(state, "astral (surrogate pairs)", astral, sizeof(astral) - 1);
    failures += check(state, "cjk mixed line endings", cjk, sizeof(cjk) - 1);
    failures += check(state, "malformed utf-8", malformed, sizeof(malformed) - 1);
    failures += check(state, "truncated utf-8 at end", truncated, sizeof(truncated) - 1);
    failures += check(state, "empty", "", 0);
    /* A NULL index (e.g. allocation failure) also falls back to the full scan. */
    {
        SZrLspPosition expected = ZrLanguageServer_LspPositionCodec_ByteOffsetToUtf16Position(crlf, sizeof(crlf) - 1, 20);
        SZrLspPosition actual = ZrLanguageServer_LspLineIndex_ByteOffsetToUtf16Position(ZR_NULL, crlf, sizeof(crlf) - 1, 20);
        int ok = expected.line == actual.line && expected.character == actual.character;
        printf("%s: null index falls back\n", ok ? "PASS" : "FAIL");
        failures += !ok;
    }
    memset(&empty, 0, sizeof(empty));
    ZrLanguageServer_LspLineIndex_Free(state, &empty);
    ZrTests_Runtime_State_Destroy(state);
    return failures == 0 ? 0 : 1;
}
