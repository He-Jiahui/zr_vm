#ifndef ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_INTERNAL_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_library/project.h"
#include "interface/lsp_position_codec.h"
#include "interface/lsp_semantic_cache_lru.h"
#include "interface/lsp_semantic_snapshot_cache.h"
#include "interface/lsp_workspace_edit_snapshot.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/** 接收者成员查询的临时输出；字段契约由元数据解析层定义，不能跨查询长期保存。 */
typedef struct SZrLspResolvedMetadataMember SZrLspResolvedMetadataMember;
/** 项目索引由 LSP 上下文持有；查询期间借用，项目移除后指针失效。 */
typedef struct SZrLspProjectIndex SZrLspProjectIndex;

/** 所有编辑器入口共用的零基 UTF-16 光标到字节偏移转换。 */
TZrSize ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrInt32 line,
                                                           TZrInt32 column);
/** 仅将 file URI 交给本地文件路径消费者；失败时调用方不得使用输出缓冲区。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_FileUriToNativePath(SZrString *uri,
                                                                        TZrChar *buffer,
                                                                        TZrSize bufferSize);

/** 比较语义对象表示的完整字符串，而非指针身份。 */
TZrBool ZrLanguageServer_Lsp_StringsEqual(SZrString *left, SZrString *right);
/** 工作区符号筛选使用的非大小写敏感包含关系。 */
TZrBool ZrLanguageServer_Lsp_StringContainsCaseInsensitive(SZrString *haystack, SZrString *needle);
/** 针对不同编码的 file URI 核对它们是否指向同一本地文件。 */
TZrBool ZrLanguageServer_Lsp_UrisResolveToSameNativePath(SZrString *left, SZrString *right);
/** 从文档哈希表查找等价 URI 的键，供打开文档覆盖层复用。 */
SZrHashKeyValuePair *ZrLanguageServer_Lsp_FindEquivalentUriKeyPair(SZrState *state,
                                                                   SZrHashSet *set,
                                                                   SZrString *uri);
/** 导航请求优先使用符号声明的可定位范围。 */
SZrFileRange ZrLanguageServer_Lsp_GetSymbolLookupRange(SZrSymbol *symbol);
/** 语义查询的轻量代码区域预筛选，不代替 lexer 或解析器。 */
TZrBool ZrLanguageServer_Lsp_IsOffsetInCodeSpan(const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize offset);
/** 为补全/签名帮助同时容许紧邻代码末端的光标。 */
TZrBool ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan(const TZrChar *content,
                                                      TZrSize contentLength,
                                                      TZrSize offset);
/** 将符号、类型和源码邻接注释汇成 hover 与补全共享的 Markdown。 */
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_Lsp_BuildSymbolMarkdownDocumentation(
    SZrState *state,
    SZrSemanticAnalyzer *analyzer,
    SZrSymbol *symbol,
    const TZrChar *content,
    TZrSize contentLength);
/** 在可用时把 FFI 来源元数据附在符号文档后。 */
SZrString *ZrLanguageServer_Lsp_AppendSymbolFfiMetadataMarkdown(SZrState *state,
                                                                SZrString *base,
                                                                SZrSymbol *symbol);
/** 将项目和原生成员元数据补入候选项，保留词法补全与语义补全的共同输出格式。 */
void ZrLanguageServer_Lsp_EnrichCompletionItemMetadata(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrCompletionItem *item,
                                                       SZrString *hoveredSymbolName,
                                                       SZrString *resolvedTypeText,
                                                       const TZrChar *content,
                                                       TZrSize contentLength);
/** 仅在语义事实可用时把常量、数值和所有权细节附入候选项。 */
void ZrLanguageServer_Lsp_EnrichCompletionItemSemanticFacts(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrSymbol *symbol,
                                                            SZrCompletionItem *item);
/** 签名帮助参数文档的语义事实附加层；返回 VM 字符串由 VM 管理，调用方仅持有引用。 */
SZrString *ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *argumentNode);
/** 从声明前的源码注释提取面向用户的文档，不从名称臆造说明。 */
SZrString *ZrLanguageServer_Lsp_ExtractLeadingCommentMarkdown(SZrState *state,
                                                              SZrSymbol *symbol,
                                                              const TZrChar *content,
                                                              TZrSize contentLength);
/** 原生接收者成员查询：解析结果只在当前项目、分析器和文档快照期间有效。 */
TZrBool ZrLanguageServer_Lsp_TryResolveReceiverNativeMember(SZrState *state,
                                                            SZrLspContext *context,
                                                            SZrLspProjectIndex *projectIndex,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrString *uri,
                                                            SZrAstNode *ast,
                                                            const TZrChar *content,
                                                            TZrSize contentLength,
                                                            TZrSize cursorOffset,
                                                            SZrLspResolvedMetadataMember *outResolved);
/** 跨项目源码接收者成员查询，供定义和补全复用同一项目归属判断。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_TryResolveReceiverProjectMember(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrSemanticAnalyzer *analyzer,
    SZrString *uri,
    SZrAstNode *ast,
    const TZrChar *content,
    TZrSize contentLength,
    TZrSize cursorOffset,
    SZrLspResolvedMetadataMember *outResolved);
/** 无文档快照时的诊断投影；调用方需接受位置精度限制。 */
void ZrLanguageServer_Lsp_AppendDiagnostic(SZrState *state, SZrArray *result, SZrDiagnostic *diag);
/** 为当前文档使用内容感知的 UTF-16 范围，并保留结构化修复。 */
void ZrLanguageServer_Lsp_AppendDiagnosticForDocument(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrArray *result,
                                                      SZrDiagnostic *diag);
/** 无上下文的符号投影；缺源文本时范围会降为零，工作区调用方需谨慎。 */
SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformation(SZrState *state,
                                                                      SZrSymbol *symbol);
/** 已打开文档的符号投影，可用文档内容精确映射 UTF-16 范围。 */
SZrLspSymbolInformation *ZrLanguageServer_Lsp_CreateSymbolInformationForDocument(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrSymbol *symbol);
/** 对 receiver 后的成员补全做项目/原生元数据解析，必要时要求调用方 fail closed。 */
TZrBool ZrLanguageServer_Lsp_TryCollectReceiverCompletions(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrLspProjectIndex *projectIndex,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrString *uri,
                                                           SZrAstNode *ast,
                                                           const TZrChar *content,
                                                           TZrSize contentLength,
                                                           TZrSize cursorOffset,
                                                           SZrArray *result,
                                                           TZrBool *outFailClosed);
/** 无法可靠识别接收者时避免回退为无关的全局候选。 */
TZrBool ZrLanguageServer_Lsp_ShouldFailClosedReceiverCompletion(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ast,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize cursorOffset);
/** 解析器语义候选不足时提供受限的 token 前缀候选。 */
TZrBool ZrLanguageServer_Lsp_TryCollectTokenPrefixCompletions(SZrState *state,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize cursorOffset,
                                                              SZrArray *result);
/** 在装饰器和元方法导航前判定是否为语言已知 token。 */
TZrBool ZrLanguageServer_Lsp_IsKnownMetaMethodToken(const TZrChar *text, TZrSize length);
/** 同时查询用法与声明位置的符号，供通用导航入口复用。 */
ZR_LANGUAGE_SERVER_API SZrSymbol *ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition(
    SZrSemanticAnalyzer *analyzer,
    SZrFileRange position);
/** 借用当前文档版本；文档更新可原位替换内部内容，文档移除或上下文释放后指针失效。 */
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrLanguageServer_Lsp_GetDocumentFileVersion(SZrLspContext *context,
                                                                                   SZrString *uri);
/** 基于该文档版本将客户端位置转换为解析器文件位置。 */
ZR_LANGUAGE_SERVER_API SZrFilePosition ZrLanguageServer_Lsp_GetDocumentFilePosition(SZrLspContext *context,
                                                                                      SZrString *uri,
                                                                                      SZrLspPosition position);
/** 为当前文档内容投影文件位置，避免直接输出字节列。 */
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
    SZrLspContext *context,
    SZrString *uri,
    SZrFilePosition position);
/** 为诊断、符号和导航统一投影指定 URI 的范围。 */
ZR_LANGUAGE_SERVER_API SZrLspRange ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(SZrLspContext *context,
                                                                                         SZrString *uri,
                                                                                         SZrFileRange range);
/** 将二进制元数据结构范围投影为编辑器坐标；有内容时按 UTF-16 转换。 */
TZrBool ZrLanguageServer_Lsp_TryRangeFromBinaryMetadataCoordinates(SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrFileRange range,
                                                                   SZrLspRange *outRange);
/** 将二进制模块虚拟文档光标转回元数据查找所需文件坐标。 */
TZrBool ZrLanguageServer_Lsp_TryFilePositionFromBinaryMetadataCoordinates(SZrLspContext *context,
                                                                          SZrString *uri,
                                                                          SZrLspPosition position,
                                                                          SZrFilePosition *outPosition);

/**
 * 项目索引持有此记录；URI、路径和模块名是共享字符串引用。
 * RemoveFileRecord、项目清除或工作区根移除后，记录指针不再有效。
 */
typedef struct SZrLspProjectFileRecord {
    SZrString *uri;
    SZrString *path;
    SZrString *moduleName;
    TZrBool isFfiWrapperSource;
    TZrUInt64 publicContractHash;
    TZrSize publicContractExportCount;
    TZrBool hasPublicContractHash;
} SZrLspProjectFileRecord;

/**
 * 文档归属与跨文件导航共用的项目状态，由 context->projectIndexes 持有。
 * 查询只借用此指针；移除项目或未被其他根覆盖的工作区会销毁索引。
 */
typedef struct SZrLspProjectIndex {
    SZrLibrary_Project *project;
    SZrString *projectFileUri;
    SZrString *projectFilePath;
    SZrString *projectRootPath;
    SZrString *sourceRootPath;
    TZrBool hasSemanticProjectLoad;
    TZrBool hasLightweightSourceGraph;
    TZrSize reverseDependencyPreservationCount;
    TZrSize reverseDependencyReanalysisCount;
    TZrSize lastReverseDependencyReanalysisCount;
    TZrSize publicContractHashMatchCount;
    TZrSize publicContractHashChangeCount;
    TZrSize publicContractHashUnavailableCount;
    SZrArray files; // SZrLspProjectFileRecord*
    SZrArray activeModuleLoads; // SZrString*
} SZrLspProjectIndex;

/** 区分导入声明的来源，以选择源码快照、二进制元数据或描述符的坐标策略。 */
typedef enum EZrLspImportedModuleSourceKind {
    ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED = 0,
    ZR_LSP_IMPORTED_MODULE_SOURCE_PROJECT_SOURCE = 1,
    ZR_LSP_IMPORTED_MODULE_SOURCE_FFI_SOURCE_WRAPPER = 2,
    ZR_LSP_IMPORTED_MODULE_SOURCE_BINARY_METADATA = 3,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN = 4,
    ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN = 5,
    ZR_LSP_IMPORTED_MODULE_SOURCE_COMPILE_TOOL = 6
} EZrLspImportedModuleSourceKind;

/** 跨模块定义/引用的临时解析结果；项目索引和字符串字段均随上下文与查询存活。 */
typedef struct SZrLspExternalMetadataDeclaration {
    SZrLspProjectIndex *projectIndex;
    SZrString *moduleName;
    SZrString *memberName;
    TZrInt32 sourceKind;
    SZrString *declarationUri;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
} SZrLspExternalMetadataDeclaration;

/** 为已纳入上下文的 URI 复用或建立语义分析器；借用指针随 URI 移除或上下文销毁失效，文档更新可替换其内部语义状态。 */
ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *ZrLanguageServer_Lsp_GetOrCreateAnalyzer(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
/** 不触发创建地查询当前语义分析器，供诊断与缓存状态检查使用。 */
ZR_LANGUAGE_SERVER_API SZrSemanticAnalyzer *ZrLanguageServer_Lsp_FindAnalyzer(SZrState *state,
                                                                              SZrLspContext *context,
                                                                              SZrString *uri);
/** 文档关闭或替换时解除分析器索引并回收其语义状态。 */
void ZrLanguageServer_Lsp_RemoveAnalyzer(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri);
/** 统一的文档更新边界；调用方决定此次变更是否可触发项目刷新。 */
TZrBool ZrLanguageServer_Lsp_UpdateDocumentCore(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                const TZrChar *content,
                                                TZrSize contentLength,
                                                TZrSize version,
                                                TZrBool allowProjectRefresh);
/** 上下文销毁时释放其持有的项目索引与文件记录。 */
void ZrLanguageServer_Lsp_ProjectIndexes_Free(SZrState *state, SZrLspContext *context);
/** 更新项目源码图，并按调用方要求选择局部传播或全量重新扫描。 */
TZrBool ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrBool rescanAllLoadedSources);
/** 将当前文档 AST 纳入项目语义图，供跨文件导航和引用查询。 */
TZrBool ZrLanguageServer_Lsp_ProjectAnalyzeDocument(SZrState *state,
                                                    SZrLspContext *context,
                                                    SZrString *uri,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *ast);
/** 解析 URI 的项目归属，必要时加载索引；返回指针由上下文持有。 */
SZrLspProjectIndex *ZrLanguageServer_Lsp_ProjectEnsureProjectForUri(SZrState *state,
                                                                    SZrLspContext *context,
                                                                    SZrString *uri);
/** 将导入模块上的光标解析为可追踪的外部元数据声明。 */
TZrBool ZrLanguageServer_LspProject_ResolveExternalMetadataDeclaration(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri,
                                                                       SZrLspPosition position,
                                                                       SZrLspExternalMetadataDeclaration *outResolved);
/** 按来源类型汇总外部声明的引用，并尊重是否包含定义的请求参数。 */
TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationReferences(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    TZrBool includeDeclaration,
    SZrArray *result);
/** 将同一文档内的外部声明命中投影为 documentHighlight。 */
TZrBool ZrLanguageServer_LspProject_AppendExternalMetadataDeclarationHighlights(
    SZrState *state,
    SZrLspContext *context,
    const SZrLspExternalMetadataDeclaration *resolved,
    SZrString *queryUri,
    SZrArray *result);
/** 查询某 URI 是否属于已索引项目，供编辑器特性决定跨文件范围。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_ProjectContainsUri(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri);
/** 只读查找现存项目索引；不触发项目加载，返回值为借用指针。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectForUri(SZrLspContext *context, SZrString *uri);
/** 从项目图借用模块记录；索引更新后不得继续保存此指针。 */
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByModuleName(SZrLspProjectIndex *projectIndex,
                                                                            SZrString *moduleName);
/** 为 workspace/symbol 汇总项目索引中的符号。 */
TZrBool ZrLanguageServer_Lsp_ProjectAppendWorkspaceSymbols(SZrState *state,
                                                           SZrLspContext *context,
                                                           SZrString *query,
                                                           SZrArray *result);
/** 为 `super` 构造器调用提供跨类定义跳转。 */
TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDefinition(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri,
                                                              SZrLspPosition position,
                                                              SZrArray *result);
/** 将 `super` 调用映射回构造器引用集合，并保持 includeDeclaration 语义。 */
TZrBool ZrLanguageServer_Lsp_TryFindSuperConstructorReferences(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrString *uri,
                                                               SZrLspPosition position,
                                                               TZrBool includeDeclaration,
                                                               SZrArray *result);
/** 为当前文档内的 `super` 构造器用法生成高亮。 */
TZrBool ZrLanguageServer_Lsp_TryGetSuperConstructorDocumentHighlights(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result);
/** 装饰器 token 的专用定义查询，避免误落入普通标识符查找。 */
TZrBool ZrLanguageServer_Lsp_TryGetDecoratorDefinition(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrLspPosition position,
                                                       SZrArray *result);
/** 装饰器 token 的专用悬停信息入口。 */
TZrBool ZrLanguageServer_Lsp_TryGetDecoratorHover(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  SZrLspHover **result);
/** 内建元方法 token 的专用悬停信息入口。 */
TZrBool ZrLanguageServer_Lsp_TryGetMetaMethodHover(SZrState *state,
                                                   SZrLspContext *context,
                                                   SZrString *uri,
                                                   SZrLspPosition position,
                                                   SZrLspHover **result);

#endif
