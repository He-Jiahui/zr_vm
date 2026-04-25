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
    TZrInt32 line;                      // 行号（从0开始）
    TZrInt32 character;                 // 列号（从0开始）
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

typedef struct SZrLspDiagnosticRelatedInformation {
    SZrLspLocation location;
    SZrString *message;
} SZrLspDiagnosticRelatedInformation;

// LSP 诊断
typedef struct SZrLspDiagnostic {
    SZrLspRange range;                // 范围
    TZrInt32 severity;                  // 严重程度（1=Error, 2=Warning, 3=Info, 4=Hint）
    SZrString *code;                  // 错误代码（可选）
    SZrString *message;               // 消息
    SZrArray relatedInformation;      // SZrLspDiagnosticRelatedInformation
} SZrLspDiagnostic;

// LSP 补全项
typedef struct SZrLspCompletionItem {
    SZrString *label;                 // 标签
    TZrInt32 kind;                      // 类型（1=Text, 2=Method, 3=Function, 4=Constructor, 5=Field, 6=Variable, 7=Class, 8=Interface, 9=Module, 10=Property, 11=Unit, 12=Value, 13=Enum, 14=Keyword, 15=Snippet, 16=Color, 17=File, 18=Reference, 19=Folder, 20=EnumMember, 21=Constant, 22=Struct, 23=Event, 24=Operator, 25=TypeParameter）
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

typedef struct SZrLspRichHoverSection {
    SZrString *role;                  // 语义角色（name/signature/access/...）
    SZrString *label;                 // 字段标签
    SZrString *value;                 // 字段值
} SZrLspRichHoverSection;

typedef struct SZrLspRichHover {
    SZrArray sections;                // SZrLspRichHoverSection*
    SZrLspRange range;                // 范围（可选）
} SZrLspRichHover;

typedef struct SZrLspParameterInformation {
    SZrString *label;                 // 参数标签
    SZrString *documentation;         // 文档（可选，markdown格式）
} SZrLspParameterInformation;

typedef struct SZrLspSignatureInformation {
    SZrString *label;                 // 签名标签
    SZrString *documentation;         // 文档（可选，markdown格式）
    SZrArray parameters;              // 参数数组（SZrLspParameterInformation*）
} SZrLspSignatureInformation;

typedef struct SZrLspSignatureHelp {
    SZrArray signatures;              // 签名数组（SZrLspSignatureInformation*）
    TZrInt32 activeSignature;         // 当前激活的签名
    TZrInt32 activeParameter;         // 当前激活的参数
} SZrLspSignatureHelp;

// LSP 符号信息
typedef struct SZrLspSymbolInformation {
    SZrString *name;                  // 符号名称
    TZrInt32 kind;                    // SymbolKind
    SZrString *containerName;         // 容器名称（可选）
    SZrLspLocation location;          // 符号位置
} SZrLspSymbolInformation;

typedef struct SZrLspProjectModuleSummary {
    TZrInt32 sourceKind;
    TZrBool isEntry;
    SZrString *moduleName;
    SZrString *displayName;
    SZrString *description;
    SZrString *navigationUri;
    SZrLspRange range;
} SZrLspProjectModuleSummary;

typedef struct SZrLspInlayHint {
    SZrLspPosition position;
    SZrString *label;
    TZrInt32 kind;
    TZrBool paddingLeft;
    TZrBool paddingRight;
} SZrLspInlayHint;

// LSP 文档高亮
typedef struct SZrLspDocumentHighlight {
    SZrLspRange range;                // 高亮范围
    TZrInt32 kind;                    // DocumentHighlightKind
} SZrLspDocumentHighlight;

typedef struct SZrLspTextEdit {
    SZrLspRange range;
    SZrString *newText;
} SZrLspTextEdit;

typedef struct SZrLspCodeAction {
    SZrString *title;
    SZrString *kind;
    SZrArray edits;                   // SZrLspTextEdit*
    TZrBool isPreferred;
} SZrLspCodeAction;

typedef struct SZrLspFoldingRange {
    TZrInt32 startLine;
    TZrInt32 startCharacter;
    TZrInt32 endLine;
    TZrInt32 endCharacter;
    SZrString *kind;
} SZrLspFoldingRange;

typedef struct SZrLspSelectionRange {
    SZrLspRange range;
    TZrBool hasParent;
    SZrLspRange parentRange;
    TZrBool hasGrandParent;
    SZrLspRange grandParentRange;
} SZrLspSelectionRange;

typedef struct SZrLspDocumentLink {
    SZrLspRange range;
    SZrString *target;
    SZrString *tooltip;
} SZrLspDocumentLink;

typedef struct SZrLspCodeLens {
    SZrLspRange range;
    SZrString *commandTitle;
    SZrString *command;
    SZrString *argument;
    TZrBool hasPositionArgument;
    SZrLspPosition positionArgument;
} SZrLspCodeLens;

typedef struct SZrLspHierarchyItem {
    SZrString *name;
    SZrString *detail;
    TZrInt32 kind;
    SZrString *uri;
    SZrLspRange range;
    SZrLspRange selectionRange;
} SZrLspHierarchyItem;

typedef struct SZrLspHierarchyCall {
    SZrLspHierarchyItem *item;
    SZrArray fromRanges;              // SZrLspRange
} SZrLspHierarchyCall;

// LSP 接口上下文
typedef struct SZrLspContext {
    SZrState *state;
    SZrIncrementalParser *parser;
    SZrSemanticAnalyzer *analyzer;
    SZrHashSet uriToAnalyzerMap;      // URI 到分析器的映射（值为SZrSemanticAnalyzer*）
    SZrArray projectIndexes;          // 已打开项目索引（SZrLspProjectIndex*，内部使用）
    TZrChar *clientSelectedZrpNativePath; /*!< IDE 选中的 .zrp 绝对路径（原生路径，可为 ZR_NULL） */
} SZrLspContext;

// LSP 接口管理函数

// 创建 LSP 上下文
ZR_LANGUAGE_SERVER_API SZrLspContext *ZrLanguageServer_LspContext_New(SZrState *state);

// 释放 LSP 上下文
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspContext_Free(SZrState *state, SZrLspContext *context);

// 设置 IDE 选中的工程文件（.zrp）URI，用于多工程目录下消除 discover 歧义；uri 为 ZR_NULL 时清除
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspContext_SetClientSelectedZrpUri(SZrState *state,
                                                                                SZrLspContext *context,
                                                                                SZrString *zrpFileUri);

// 更新文档
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_UpdateDocument(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  const TZrChar *content,
                                                  TZrSize contentLength,
                                                  TZrSize version);

// 获取诊断
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDiagnostics(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrArray *result);

// 获取补全
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrArray *result);

// 获取悬停信息
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetHover(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspPosition position,
                                           SZrLspHover **result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetRichHover(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrString *uri,
                                                                 SZrLspPosition position,
                                                                 SZrLspRichHover **result);

// 获取签名帮助
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetSignatureHelp(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri,
                                                                     SZrLspPosition position,
                                                                     SZrLspSignatureHelp **result);

// 获取定义位置
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDefinition(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrArray *result);

// 查找引用
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_FindReferences(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  TZrBool includeDeclaration,
                                                  SZrArray *result);

// 重命名符号
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_Rename(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri,
                                         SZrLspPosition position,
                                         SZrString *newName,
                                         SZrArray *result);

// 获取文档符号
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDocumentSymbols(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrArray *result);

// 获取工作区符号
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetWorkspaceSymbols(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *query,
                                                       SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetNativeDeclarationDocument(SZrState *state,
                                                                                 SZrLspContext *context,
                                                                                 SZrString *uri,
                                                                                 SZrString **outText);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetProjectModules(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *projectUri,
                                                                     SZrArray *result);

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeProjectModules(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeRichHover(SZrState *state, SZrLspRichHover *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetInlayHints(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrLspRange range,
                                                                  SZrArray *result);

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeInlayHints(SZrState *state, SZrArray *result);

// 获取文档高亮
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDocumentHighlights(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri,
                                                          SZrLspPosition position,
                                                          SZrArray *result);

// 获取完整语义 token 数据（LSP semanticTokens/full 的 data 数组）
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetSemanticTokens(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrArray *result);
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_Lsp_SemanticTokenTypeCount(void);
ZR_LANGUAGE_SERVER_API const TZrChar *ZrLanguageServer_Lsp_SemanticTokenTypeName(TZrSize index);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetFormatting(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetRangeFormatting(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri,
                                                                       SZrLspRange range,
                                                                       SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCodeActions(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrLspRange range,
                                                                   SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetFoldingRanges(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri,
                                                                     SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetSelectionRanges(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri,
                                                                       const SZrLspPosition *positions,
                                                                       TZrSize positionCount,
                                                                       SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDocumentLinks(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri,
                                                                     SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCodeLens(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDeclaration(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrLspPosition position,
                                                                   SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetTypeDefinition(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetImplementation(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_PrepareCallHierarchy(SZrState *state,
                                                                         SZrLspContext *context,
                                                                         SZrString *uri,
                                                                         SZrLspPosition position,
                                                                         SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(SZrState *state,
                                                                                  SZrLspContext *context,
                                                                                  const SZrLspHierarchyItem *item,
                                                                                  SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(SZrState *state,
                                                                                  SZrLspContext *context,
                                                                                  const SZrLspHierarchyItem *item,
                                                                                  SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_PrepareTypeHierarchy(SZrState *state,
                                                                         SZrLspContext *context,
                                                                         SZrString *uri,
                                                                         SZrLspPosition position,
                                                                         SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(SZrState *state,
                                                                               SZrLspContext *context,
                                                                               const SZrLspHierarchyItem *item,
                                                                               SZrArray *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(SZrState *state,
                                                                             SZrLspContext *context,
                                                                             const SZrLspHierarchyItem *item,
                                                                             SZrArray *result);

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeTextEdits(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeCodeActions(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeFoldingRanges(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeSelectionRanges(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeDocumentLinks(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeCodeLens(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeHierarchyItems(SZrState *state, SZrArray *result);
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeHierarchyCalls(SZrState *state, SZrArray *result);

// 预检查是否可重命名
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_PrepareRename(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrLspRange *outRange,
                                                 SZrString **outPlaceholder);

// 释放签名帮助
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSignatureHelp_Free(SZrState *state,
                                                                   SZrLspSignatureHelp *help);

// 工具函数

// 转换 FileRange 到 LspRange
ZR_LANGUAGE_SERVER_API SZrLspRange ZrLanguageServer_LspRange_FromFileRange(SZrFileRange fileRange);

// 转换 LspRange 到 FileRange
ZR_LANGUAGE_SERVER_API SZrFileRange ZrLanguageServer_LspRange_ToFileRange(SZrLspRange lspRange, SZrString *uri);

// 转换 LspRange 到 FileRange（带文件内容）
ZR_LANGUAGE_SERVER_API SZrFileRange ZrLanguageServer_LspRange_ToFileRangeWithContent(SZrLspRange lspRange, SZrString *uri, 
                                                                       const TZrChar *content, TZrSize contentLength);

// 转换 FilePosition 到 LspPosition
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLanguageServer_LspPosition_FromFilePosition(SZrFilePosition filePosition);

// 转换 LspPosition 到 FilePosition
ZR_LANGUAGE_SERVER_API SZrFilePosition ZrLanguageServer_LspPosition_ToFilePosition(SZrLspPosition lspPosition);

// 转换 LspPosition 到 FilePosition（带文件内容）
ZR_LANGUAGE_SERVER_API SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                                                const TZrChar *content, TZrSize contentLength);

#endif //ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H
