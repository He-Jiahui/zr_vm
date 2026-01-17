//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

// LSP 位置（行和列从0开始）
typedef struct SZrLspPosition {
    TInt32 line;                      // 行号（从0开始）
    TInt32 character;                 // 列号（从0开始）
} SZrLspPosition;

// LSP 范围
typedef struct SZrLspRange {
    SZrLspPosition start;
    SZrLspPosition end;
} SZrLspRange;

// LSP 位置（URI + 位置）
typedef struct SZrLspLocation {
    SZrString *uri;                   // 文件 URI
    SZrLspRange range;                // 范围
} SZrLspLocation;

// LSP 诊断
typedef struct SZrLspDiagnostic {
    SZrLspRange range;                // 范围
    TInt32 severity;                  // 严重程度（1=Error, 2=Warning, 3=Info, 4=Hint）
    SZrString *code;                  // 错误代码（可选）
    SZrString *message;               // 消息
    SZrArray relatedInformation;      // 相关信息（可选）
} SZrLspDiagnostic;

// LSP 补全项
typedef struct SZrLspCompletionItem {
    SZrString *label;                 // 标签
    TInt32 kind;                      // 类型（1=Text, 2=Method, 3=Function, 4=Constructor, 5=Field, 6=Variable, 7=Class, 8=Interface, 9=Module, 10=Property, 11=Unit, 12=Value, 13=Enum, 14=Keyword, 15=Snippet, 16=Color, 17=File, 18=Reference, 19=Folder, 20=EnumMember, 21=Constant, 22=Struct, 23=Event, 24=Operator, 25=TypeParameter）
    SZrString *detail;                // 详细信息（可选）
    SZrString *documentation;         // 文档（可选，markdown格式）
    SZrString *insertText;            // 插入文本（可选）
    SZrString *insertTextFormat;      // 插入文本格式（"plaintext"或"snippet"）
} SZrLspCompletionItem;

// LSP 悬停信息
typedef struct SZrLspHover {
    SZrArray contents;                // 内容数组（markdown字符串数组）
    SZrLspRange range;                // 范围（可选）
} SZrLspHover;

// LSP 接口上下文
typedef struct SZrLspContext {
    SZrState *state;
    SZrIncrementalParser *parser;
    SZrSemanticAnalyzer *analyzer;
    SZrHashSet uriToAnalyzerMap;      // URI 到分析器的映射（值为SZrSemanticAnalyzer*）
} SZrLspContext;

// LSP 接口管理函数

// 创建 LSP 上下文
ZR_LANGUAGE_SERVER_API SZrLspContext *ZrLspContextNew(SZrState *state);

// 释放 LSP 上下文
ZR_LANGUAGE_SERVER_API void ZrLspContextFree(SZrState *state, SZrLspContext *context);

// 更新文档
ZR_LANGUAGE_SERVER_API TBool ZrLspUpdateDocument(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  const TChar *content,
                                                  TZrSize contentLength,
                                                  TZrSize version);

// 获取诊断
ZR_LANGUAGE_SERVER_API TBool ZrLspGetDiagnostics(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrArray *result);

// 获取补全
ZR_LANGUAGE_SERVER_API TBool ZrLspGetCompletion(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrArray *result);

// 获取悬停信息
ZR_LANGUAGE_SERVER_API TBool ZrLspGetHover(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspPosition position,
                                           SZrLspHover **result);

// 获取定义位置
ZR_LANGUAGE_SERVER_API TBool ZrLspGetDefinition(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrArray *result);

// 查找引用
ZR_LANGUAGE_SERVER_API TBool ZrLspFindReferences(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  TBool includeDeclaration,
                                                  SZrArray *result);

// 重命名符号
ZR_LANGUAGE_SERVER_API TBool ZrLspRename(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri,
                                         SZrLspPosition position,
                                         SZrString *newName,
                                         SZrArray *result);

// 工具函数

// 转换 FileRange 到 LspRange
ZR_LANGUAGE_SERVER_API SZrLspRange ZrLspRangeFromFileRange(SZrFileRange fileRange);

// 转换 LspRange 到 FileRange
ZR_LANGUAGE_SERVER_API SZrFileRange ZrLspRangeToFileRange(SZrLspRange lspRange, SZrString *uri);

// 转换 FilePosition 到 LspPosition
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLspPositionFromFilePosition(SZrFilePosition filePosition);

// 转换 LspPosition 到 FilePosition
ZR_LANGUAGE_SERVER_API SZrFilePosition ZrLspPositionToFilePosition(SZrLspPosition lspPosition);

#endif //ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H
