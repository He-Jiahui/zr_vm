#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SCAN_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_INLINE_VALUE_SCAN_H

#include <stddef.h>

/** @brief 供 inlineValue 的轻量词法扫描识别 ASCII 标识符首字符。 */
int ZrStdioInlineValue_IsIdentifierStart(char ch);
/** @brief 与首字符规则配套，识别变量名及关键字边界所需的后续字符。 */
int ZrStdioInlineValue_IsIdentifierPart(char ch);
/** @brief 仅在线内完整标识符边界匹配关键字，供声明和语句筛选共用。 */
int ZrStdioInlineValue_IsKeywordAt(const char *content,
                                   size_t lineStart,
                                   size_t lineEnd,
                                   size_t offset,
                                   const char *keyword);
/**
 * @brief 从给定行找第一段可供 inlineValue 扫描的代码，并延续跨行块注释状态。
 * @note outStart/outEnd 是 UTF-8 字节偏移；调用方须自行处理同一行剩余代码段。
 */
int ZrStdioInlineValue_FindCodeSpanOnLine(const char *content,
                                          size_t lineStart,
                                          size_t lineEnd,
                                          int *inBlockComment,
                                          size_t *outStart,
                                          size_t *outEnd);
/** @brief 判定行首是否可能是可查询语义事实的表达式语句，排除常见声明和控制关键字。 */
int ZrStdioInlineValue_IsExpressionStatementStart(const char *content,
                                                  size_t lineStart,
                                                  size_t lineEnd,
                                                  size_t contentLength,
                                                  size_t offset);
/** @brief 在括号、单/双引号和注释边界内寻找表达式语句的终止分号。 */
size_t ZrStdioInlineValue_FindExpressionStatementEnd(const char *content,
                                                     size_t start,
                                                     size_t limit);
/** @brief 为表达式范围选择语义查询位置，使运算、成员访问等事实优先于普通首 token。 */
size_t ZrStdioInlineValue_FindSemanticQueryOffset(const char *content,
                                                  size_t start,
                                                  size_t end);

#endif
