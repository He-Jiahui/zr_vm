//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_PARSER_H
#define ZR_VM_PARSER_PARSER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"

// 解析器状态
// 参考: lua/src/lparser.h (FuncState, LexState)
typedef struct SZrParserState {
    SZrLexState *lexer;           // 词法分析器
    SZrState *state;              // VM 状态
    SZrFileRange currentLocation; // 当前位置
    TZrBool hasError;               // 是否有错误
    const TZrChar *errorMessage;    // 错误消息
} SZrParserState;

// 初始化解析器状态
ZR_PARSER_API void ZrParser_State_Init(SZrParserState *ps, SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName);

// 清理解析器状态
ZR_PARSER_API void ZrParser_State_Free(SZrParserState *ps);

// 公共 API

// 解析源代码，返回 AST 根节点
ZR_PARSER_API SZrAstNode *ZrParser_Parse(SZrState *state, const TZrChar *source, TZrSize sourceLength, SZrString *sourceName);

// 释放 AST 节点
ZR_PARSER_API void ZrParser_Ast_Free(SZrState *state, SZrAstNode *node);

#endif //ZR_VM_PARSER_PARSER_H

