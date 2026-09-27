//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_PARSER_H
#define ZR_VM_PARSER_PARSER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"

/** @brief 兼容的文本诊断回调；location 与 message 仅供本次调用借用。 */
typedef void (*TZrParserErrorCallback)(TZrPtr userData,
                                       const SZrFileRange *location,
                                       const TZrChar *message,
                                       EZrToken token);

/** @brief 结构化诊断回调；诊断对象及内部数组在回调返回后可被释放。 */
typedef void (*TZrParserStructuredErrorCallback)(TZrPtr userData,
                                                const SZrStructuredDiagnostic *diagnostic,
                                                EZrToken token);

/** @brief 持有 lexer 游标、错误标志及可选诊断接收端的单次解析状态。
 * @note Init 分配 lexer，Free 只释放该游标；源文本、源名、回调 userData 和返回的 AST
 *       均由调用方管理。回调应在解析前设置，错误文本字段可能借用临时缓冲。 */
typedef struct SZrParserState {
    SZrLexState *lexer;           // 词法分析器
    SZrState *state;              // VM 状态
    SZrFileRange currentLocation; // 当前位置
    TZrBool hasError;               // 是否有错误
    TZrBool hasFatalError;          // production parsing cannot return an AST after removed legacy syntax
    const TZrChar *errorMessage;    // 错误消息
    TZrParserErrorCallback errorCallback; // 旧错误回调（可选）
    TZrParserStructuredErrorCallback structuredErrorCallback; // 结构化错误回调（可选）
    TZrPtr errorUserData;             // 错误回调用户数据
    TZrBool suppressErrorOutput;      // 是否抑制 stderr 输出
    TZrBool enableLegacyMigrationParsing; // 仅供显式 migration frontend 捕获旧语法修复
} SZrParserState;

/** @brief 初始化状态并预读第一个 token；词法状态对象分配失败时设置 hasError。
 * @pre ps、state、source 非空，源字节在 State_Free 前有效。
 * @note 预读发生在调用方安装回调及 suppressErrorOutput 之前。 */
ZR_PARSER_API void ZrParser_State_Init(SZrParserState *ps, SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName);

/** @brief 释放 lexer 游标及缓冲，可接受空指针；不释放源文本或已返回的 AST。 */
ZR_PARSER_API void ZrParser_State_Free(SZrParserState *ps);

/** @brief 从当前 token 解析脚本，供编译器、LSP 与迁移入口复用已配置的状态。
 * @return AST 由调用方用 ZrParser_Ast_Free 释放；失败或禁止的旧语法返回空。
 * @note 普通语法错误可能被恢复并仍返回 AST，调用方还应检查诊断回调及错误状态。 */
ZR_PARSER_API SZrAstNode *ZrParser_ParseWithState(SZrParserState *ps);

/** @brief 将已初始化解析器向前推进到原源文本中的精确 token 起点。
 * @return 空白、注释、文件末尾、无效偏移或已越过目标时返回假；不会倒退。 */
ZR_PARSER_API TZrBool ZrParser_State_SeekToTokenStart(
        SZrParserState *ps,
        TZrSize sourceOffset);

/** @brief 从当前 token 解析一个顶层语句，供 LSP 局部声明重解析使用。
 * @return 成功的节点归调用方，用 ZrParser_Ast_Free 释放；错误时返回空。 */
ZR_PARSER_API SZrAstNode *ZrParser_ParseTopLevelStatementWithState(SZrParserState *ps);

/** @brief 从当前状态解析单个完整表达式，供片段解析使用。
 * @return 成功的节点归调用方，用 ZrParser_Ast_Free 释放；错误时返回空。 */
ZR_PARSER_API SZrAstNode *ZrParser_ParseExpressionWithState(SZrParserState *ps);

/** @brief 初始化临时状态、解析完整脚本并释放游标。
 * @return 返回的 AST 归调用方；函数不释放 source 或 sourceName。 */
ZR_PARSER_API SZrAstNode *ZrParser_Parse(SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName);

/** @brief 递归释放解析器构造的 AST 子树及节点数组，不释放 VM 字符串。 */
ZR_PARSER_API void ZrParser_Ast_Free(SZrState *state, SZrAstNode *node);

#endif //ZR_VM_PARSER_PARSER_H

