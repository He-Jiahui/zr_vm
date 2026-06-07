#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SCAN_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SCAN_H

#include <stddef.h>

int ZrStdioInlineValue_IsIdentifierStart(char ch);
int ZrStdioInlineValue_IsIdentifierPart(char ch);
int ZrStdioInlineValue_IsKeywordAt(const char *content,
                                   size_t lineStart,
                                   size_t lineEnd,
                                   size_t offset,
                                   const char *keyword);
int ZrStdioInlineValue_FindCodeSpanOnLine(const char *content,
                                          size_t lineStart,
                                          size_t lineEnd,
                                          int *inBlockComment,
                                          size_t *outStart,
                                          size_t *outEnd);
int ZrStdioInlineValue_IsExpressionStatementStart(const char *content,
                                                  size_t lineStart,
                                                  size_t lineEnd,
                                                  size_t contentLength,
                                                  size_t offset);
size_t ZrStdioInlineValue_FindExpressionStatementEnd(const char *content,
                                                     size_t start,
                                                     size_t limit);
size_t ZrStdioInlineValue_FindSemanticQueryOffset(const char *content,
                                                  size_t start,
                                                  size_t end);

#endif
