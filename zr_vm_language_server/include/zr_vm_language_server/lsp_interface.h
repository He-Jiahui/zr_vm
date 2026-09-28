/**
 * @file
 * @brief 将增量解析、语义分析和工程索引投影到 LSP 请求的公共 C 契约。
 * @note 请求坐标使用零基行和 UTF-16 列；除特别说明外，查询结果中的 SZrString 由同一 VM 状态管理。
 */

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_language_server/lsp_semantic_snapshot.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

/** @brief 编辑器请求和响应共用的位置；行号从零开始，character 以 UTF-16 码元计。 */
typedef struct SZrLspPosition {
    TZrInt32 line;                      // 行号（从0开始）
    TZrInt32 character;                 // 列号（从0开始）
} SZrLspPosition;

/** @brief 编辑器可选择或展示的范围；坐标使用 SZrLspPosition 的编码契约。 */
typedef struct SZrLspRange {
    SZrLspPosition start;
    SZrLspPosition end;
} SZrLspRange;

/** @brief 仅借用优化 remark 的语义层类型，避免公共 LSP 接口依赖其完整定义。 */
struct SZrOptimizationRemark;

/** @brief 导航目标；URI 字符串与拥有此位置的查询结果使用同一 VM 生命周期。 */
typedef struct SZrLspLocation {
    SZrString *uri;                   // 文件 URI
    SZrLspRange range;                // 范围
} SZrLspLocation;

/** @brief 为诊断补充另一个源码位置和解释，随所属诊断一同释放容器。 */
typedef struct SZrLspDiagnosticRelatedInformation {
    SZrLspLocation location;
    SZrString *message;
} SZrLspDiagnosticRelatedInformation;

/** @brief 可供 code action 投影的诊断修复候选；适用性由诊断产生方决定。 */
typedef struct SZrLspDiagnosticFix {
    SZrString *title;
    SZrLspRange editRange;
    SZrString *editText;
    EZrDiagnosticFixApplicability applicability;
} SZrLspDiagnosticFix;

/**
 * @brief 解析、语义和导入错误的统一展示对象，供发布器及 code action 消费。
 * @note relatedInformation 与 fixes 是诊断内的值数组；调用方用 FreeDiagnostics 归还外层结果。
 */
typedef struct SZrLspDiagnostic {
    SZrLspRange range;                // 范围
    TZrInt32 severity;                  // 严重程度（1=Error, 2=Warning, 3=Info, 4=Hint）
    SZrString *code;                  // 错误代码（可选）
    SZrString *message;               // 消息
    SZrArray relatedInformation;      // SZrLspDiagnosticRelatedInformation
    SZrArray fixes;                   // SZrLspDiagnosticFix
    TZrUInt32 descriptorId;
    SZrString *codeDescriptionHref;
    EZrDiagnosticNoFixReason noFixReason;
} SZrLspDiagnostic;

/** @brief 把语义候选与插入行为交给编辑器；可选文本由 VM 字符串管理。 */
typedef struct SZrLspCompletionItem {
    SZrString *label;                 // 标签
    TZrInt32 kind;                      // 类型（1=Text, 2=Method, 3=Function, 4=Constructor, 5=Field, 6=Variable, 7=Class, 8=Interface, 9=Module, 10=Property, 11=Unit, 12=Value, 13=Enum, 14=Keyword, 15=Snippet, 16=Color, 17=File, 18=Reference, 19=Folder, 20=EnumMember, 21=Constant, 22=Struct, 23=Event, 24=Operator, 25=TypeParameter）
    SZrString *detail;                // 详细信息（可选）
    SZrString *documentation;         // 文档（可选，markdown格式）
    SZrString *insertText;            // 插入文本（可选）
    SZrString *insertTextFormat;      // 插入文本格式（"plaintext"或"snippet"）
} SZrLspCompletionItem;

/** @brief 常规 hover 的 Markdown 内容与可选命中范围；由查询方按对象结果释放。 */
typedef struct SZrLspHover {
    SZrArray contents;                // 内容数组（markdown字符串数组）
    SZrLspRange range;                // 范围（可选）
} SZrLspHover;

/** @brief 扩展 hover 的单个带角色片段，供客户端按语义字段分组展示。 */
typedef struct SZrLspRichHoverSection {
    SZrString *role;                  // 语义角色（name/signature/access/...）
    SZrString *label;                 // 字段标签
    SZrString *value;                 // 字段值
} SZrLspRichHoverSection;

/** @brief 扩展 hover 的分段响应；与普通 hover 的内容数组及释放路径分开。 */
typedef struct SZrLspRichHover {
    SZrArray sections;                // SZrLspRichHoverSection*
    SZrLspRange range;                // 范围（可选）
} SZrLspRichHover;

/** @brief 签名参数的标签及可选 Markdown 文档，生命周期由所属签名帮助控制。 */
typedef struct SZrLspParameterInformation {
    SZrString *label;                 // 参数标签
    SZrString *documentation;         // 文档（可选，markdown格式）
} SZrLspParameterInformation;

/** @brief 一种候选调用签名；parameters 保存参数对象指针并由签名帮助释放。 */
typedef struct SZrLspSignatureInformation {
    SZrString *label;                 // 签名标签
    SZrString *documentation;         // 文档（可选，markdown格式）
    SZrArray parameters;              // 参数数组（SZrLspParameterInformation*）
} SZrLspSignatureInformation;

/** @brief 调用点的候选签名及当前激活项；使用 LspSignatureHelp_Free 归还。 */
typedef struct SZrLspSignatureHelp {
    SZrArray signatures;              // 签名数组（SZrLspSignatureInformation*）
    TZrInt32 activeSignature;         // 当前激活的签名
    TZrInt32 activeParameter;         // 当前激活的参数
} SZrLspSignatureHelp;

/** @brief 文档或工作区符号的导航摘要，容器名和位置由索引或当前分析器提供。 */
typedef struct SZrLspSymbolInformation {
    SZrString *name;                  // 符号名称
    TZrInt32 kind;                    // SymbolKind
    SZrString *containerName;         // 容器名称（可选）
    SZrLspLocation location;          // 符号位置
} SZrLspSymbolInformation;

/** @brief 项目浏览视图的模块摘要，导航 URI 和范围可指向源码或虚拟声明。 */
typedef struct SZrLspProjectModuleSummary {
    TZrInt32 sourceKind;
    TZrBool isEntry;
    SZrString *moduleName;
    SZrString *displayName;
    SZrString *description;
    SZrString *navigationUri;
    SZrLspRange range;
} SZrLspProjectModuleSummary;

/** @brief 文档内联提示；生成时以请求范围筛选，返回数组由 FreeInlayHints 释放。 */
typedef struct SZrLspInlayHint {
    SZrLspPosition position;
    SZrString *label;
    TZrInt32 kind;
    TZrBool paddingLeft;
    TZrBool paddingRight;
} SZrLspInlayHint;

/** @brief 当前文档内同一语义目标的局部高亮位置。 */
typedef struct SZrLspDocumentHighlight {
    SZrLspRange range;                // 高亮范围
    TZrInt32 kind;                    // DocumentHighlightKind
} SZrLspDocumentHighlight;

/** @brief 源码编辑提案；调用方须在相同文档版本上应用坐标。 */
typedef struct SZrLspTextEdit {
    SZrLspRange range;
    SZrString *newText;
} SZrLspTextEdit;

/** @brief 与诊断或重构建议关联的一组文本编辑；FreeCodeActions 释放嵌套 edits。 */
typedef struct SZrLspCodeAction {
    SZrString *title;
    SZrString *kind;
    SZrArray edits;                   // SZrLspTextEdit*
    TZrBool isPreferred;
} SZrLspCodeAction;

/** @brief 编辑器折叠段，起止位置已投影到 LSP 坐标。 */
typedef struct SZrLspFoldingRange {
    TZrInt32 startLine;
    TZrInt32 startCharacter;
    TZrInt32 endLine;
    TZrInt32 endCharacter;
    SZrString *kind;
} SZrLspFoldingRange;

/** @brief 选择范围及可选的父、祖父包围范围；标志位决定后续范围是否有效。 */
typedef struct SZrLspSelectionRange {
    SZrLspRange range;
    TZrBool hasParent;
    SZrLspRange parentRange;
    TZrBool hasGrandParent;
    SZrLspRange grandParentRange;
} SZrLspSelectionRange;

/** @brief 文档中的可点击目标；target 可指向普通源码或只读虚拟声明 URI。 */
typedef struct SZrLspDocumentLink {
    SZrLspRange range;
    SZrString *target;
    SZrString *tooltip;
} SZrLspDocumentLink;

/** @brief 放在源码范围上的命令入口；hasPositionArgument 决定附带坐标是否有效。 */
typedef struct SZrLspCodeLens {
    SZrLspRange range;
    SZrString *commandTitle;
    SZrString *command;
    SZrString *argument;
    TZrBool hasPositionArgument;
    SZrLspPosition positionArgument;
} SZrLspCodeLens;

/**
 * @brief 调用/类型层级查询的导航节点，兼容普通位置和快照语义身份。
 * @note 有语义身份时，semanticVersion 应与准备阶段的文档版本相配；否则后续查询可能找不到目标。
 */
typedef struct SZrLspHierarchyItem {
    SZrString *name;
    SZrString *detail;
    TZrInt32 kind;
    SZrString *uri;
    SZrLspRange range;
    SZrLspRange selectionRange;
    TZrBool hasSemanticIdentity;
    TZrSymbolId semanticId;
    TZrTypeId semanticTypeId;
    TZrSize semanticVersion;
} SZrLspHierarchyItem;

/** @brief 一条层级调用边；item 与 fromRanges 随 FreeHierarchyCalls 一并释放。 */
typedef struct SZrLspHierarchyCall {
    SZrLspHierarchyItem *item;
    SZrArray fromRanges;              // SZrLspRange
} SZrLspHierarchyCall;

/** @brief 同步请求的可选取消探针；userData 由注册方持有，执行期必须有效。 */
typedef TZrBool (*FZrLspRequestCancellationCheck)(void *userData);

/** @brief 工作区文件夹状态由上下文持有；公共头文件只暴露指针以隔离实现。 */
struct SZrLspWorkspace;

/**
 * @brief 一个语言服务会话共享的解析器、URI 分析器、工程索引和语义缓存。
 * @note New/Free 管理成员所有权；请求层可短暂设置取消探针，不能跨请求保留 userData。
 */
typedef struct SZrLspContext {
    SZrState *state;
    SZrIncrementalParser *parser;
    SZrSemanticAnalyzer *analyzer;
    SZrHashSet uriToAnalyzerMap;      // URI 到分析器的映射（值为SZrSemanticAnalyzer*）
    struct SZrLspSemanticSnapshotCache *semanticSnapshotCache;
    struct SZrLspSemanticCacheLru *semanticCacheLru;
    struct SZrLspWorkspace *workspace; /* Canonical workspace folders, internal use only. */
    SZrArray projectIndexes;          // 已打开项目索引（SZrLspProjectIndex*，内部使用）
    TZrChar *clientSelectedZrpNativePath; /*!< IDE 选中的 .zrp 绝对路径（原生路径，可为 ZR_NULL） */
    FZrLspRequestCancellationCheck requestCancellationCheck;
    void *requestCancellationUserData;
    TZrUInt64 semanticSnapshotProviderGeneration;
    SZrLspSemanticSnapshot *activeSemanticSnapshot;
} SZrLspContext;

/**
 * @brief 最近历史版本的语义身份及借用分析器。
 * @note analyzer 由历史缓存持有，条目逐出、URI 移除或上下文释放后失效；缓存上限调整可清理其内部查询存储。
 */
typedef struct SZrLspHistoricalSemanticSnapshot {
    TZrSize version;
    TZrSize contentGeneration;
    const SZrSemanticAnalyzer *analyzer;
} SZrLspHistoricalSemanticSnapshot;

/** @brief 语义缓存当前上限、占用峰值和逐出统计的只读快照。 */
typedef struct SZrLspSemanticCacheStorageInfo {
    TZrSize limitBytes;
    TZrSize storageBytes;
    TZrSize peakStorageBytes;
    TZrSize evictionCount;
    TZrSize releasedBytes;
} SZrLspSemanticCacheStorageInfo;

/**
 * @brief 为一个语言服务会话建立解析器、工程工作区和语义缓存。
 * @return 成功时返回由调用方持有的上下文；失败时返回 ZR_NULL。
 */
ZR_LANGUAGE_SERVER_API SZrLspContext *ZrLanguageServer_LspContext_New(SZrState *state);

/** @brief 结束会话并释放其工程索引、分析器和缓存；state 应与创建时相同。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspContext_Free(SZrState *state, SZrLspContext *context);
/**
 * @brief 将当前同步请求的取消探针临时交给遍历型查询。
 * @note 回调与 userData 不转移所有权；请求结束时应以空回调清除。
 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspContext_SetRequestCancellationCheck(
        SZrLspContext *context,
        FZrLspRequestCancellationCheck check,
        void *userData);
/** @brief 让查询及响应发送路径检查当前请求的取消状态；未注册探针时返回假。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspContext_IsRequestCancellationRequested(
        const SZrLspContext *context);
/**
 * @brief 按历史索引借出 URI 的旧版语义分析器及版本身份，供快照回归和比较使用。
 * @note 索引 0 是最近一次更新前的版本；返回分析器随缓存逐出或上下文释放失效。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_Lsp_GetHistoricalSemanticSnapshot(
        const SZrLspContext *context,
        const SZrString *uri,
        TZrSize historyIndex,
        SZrLspHistoricalSemanticSnapshot *outSnapshot);
/**
 * @brief 调整会话语义缓存上限并立即执行逐出。
 * @note limitBytes 为零会释放可逐出的查询存储；不能跨此调用保留分析器的内部缓存指针。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_Lsp_SetSemanticCacheStorageLimit(
        SZrState *state,
        SZrLspContext *context,
        TZrSize limitBytes);
/** @brief 读取当前缓存占用和累计逐出统计；outInfo 为调用方提供的值对象。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_Lsp_GetSemanticCacheStorageInfo(
        const SZrLspContext *context,
        SZrLspSemanticCacheStorageInfo *outInfo);

/**
 * @brief 保存 IDE 选中的 .zrp URI，使多工程目录下的项目发现以该工程为准。
 * @note 传入 ZR_NULL 清除选择；上下文持有转换后的原生路径，不借用输入 URI 缓冲区。
 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspContext_SetClientSelectedZrpUri(SZrState *state,
                                                                                SZrLspContext *context,
                                                                                SZrString *zrpFileUri);

/**
 * @brief 接收编辑器文档版本，更新解析与语义状态并刷新所属工程的依赖索引。
 * @note content 按 contentLength 指定的字节长度读取；更新失败后不得继续信任旧分析器。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_UpdateDocument(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  const TZrChar *content,
                                                  TZrSize contentLength,
                                                  TZrSize version);

/**
 * @brief 汇总当前文档的解析、语义和导入诊断，供发布与代码动作使用。
 * @note result 是可追加的 SZrLspDiagnostic* 数组；无论查询成功与否，已有项最终由 FreeDiagnostics 归还。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDiagnostics(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrArray *result);

/**
 * @brief 将光标处的语义候选投影为编辑器补全项；非代码区域可正常返回空列表。
 * @note result 保存原生 SZrLspCompletionItem*；序列化后释放项和数组，文本由 VM 管理。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCompletion(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrArray *result);

/**
 * @brief 按语义目标与调用签名构建普通 Markdown 悬停内容。
 * @note 成功产生对象时，调用方须释放 contents 数组和 hover 对象；内容字符串属于 VM 状态。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetHover(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspPosition position,
                                           SZrLspHover **result);

/**
 * @brief 将普通悬停拆为带角色片段，供扩展客户端逐字段展示。
 * @note 返回对象使用 FreeRichHover，不能按普通 hover 的容器格式释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetRichHover(SZrState *state,
                                                                 SZrLspContext *context,
                                                                 SZrString *uri,
                                                                 SZrLspPosition position,
                                                                 SZrLspRichHover **result);

/**
 * @brief 为调用点提供候选签名、参数说明和当前激活参数。
 * @note 返回的嵌套对象由 LspSignatureHelp_Free 统一释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetSignatureHelp(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri,
                                                                     SZrLspPosition position,
                                                                     SZrLspSignatureHelp **result);

/**
 * @brief 统一解析源码与原生虚拟声明页的定义目标。
 * @note result 中的 SZrLspLocation* 由调用方在消费后逐项释放；URI 字符串由 VM 管理。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDefinition(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrArray *result);

/**
 * @brief 按语义身份收集跨文件引用；includeDeclaration 决定是否包含声明位置。
 * @note 可被请求取消；失败时 result 中已追加的位置仍需调用方清理。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_FindReferences(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  TZrBool includeDeclaration,
                                                  SZrArray *result);

/**
 * @brief 收集可编辑目标的位置，供协议层构造带版本校验的 WorkspaceEdit。
 * @note newName 当前只作非空检查；调用方不得把此函数视为标识符合法性或冲突验证。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_Rename(SZrState *state,
                                         SZrLspContext *context,
                                         SZrString *uri,
                                         SZrLspPosition position,
                                         SZrString *newName,
                                         SZrArray *result);

/** @brief 生成当前文档的大纲符号；结果为需逐项释放的 SZrLspSymbolInformation* 数组。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDocumentSymbols(SZrState *state,
                                                      SZrLspContext *context,
                                                      SZrString *uri,
                                                      SZrArray *result);

/** @brief 合并打开文档与项目索引中的匹配符号；query 为 VM 字符串，结果按文档符号方式释放。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetWorkspaceSymbols(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *query,
                                                       SZrArray *result);

/**
 * @brief 为原生模块的虚拟声明 URI 生成只读文档，供跳转目标与客户端取文。
 * @note 仅接受已识别的虚拟声明 URI；outText 是由 VM 管理的字符串，调用方无需原生释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetNativeDeclarationDocument(SZrState *state,
                                                                                 SZrLspContext *context,
                                                                                 SZrString *uri,
                                                                                 SZrString **outText);

/**
 * @brief 汇总项目图中的源码、FFI、二进制和原生模块，供客户端浏览树展示。
 * @note result 应先零初始化或由同一 state 初始化；失败后也可交给 FreeProjectModules 清理已追加项。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetProjectModules(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *projectUri,
                                                                     SZrArray *result);

/** @brief 归还 GetProjectModules 的摘要项和数组；其中的 VM 字符串不单独释放。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeProjectModules(SZrState *state, SZrArray *result);
/** @brief 归还扩展悬停的所有角色片段及容器。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeRichHover(SZrState *state, SZrLspRichHover *result);

/** @brief 在给定文档范围内收集类型等内联提示；结果对象与容器由 FreeInlayHints 归还。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetInlayHints(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrLspRange range,
                                                                  SZrArray *result);

/** @brief 归还 GetInlayHints 的原生提示项；label 字符串仍归 VM 所有。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeInlayHints(SZrState *state, SZrArray *result);

/** @brief 找出当前文档中同一语义目标的出现位置，供编辑器局部高亮。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDocumentHighlights(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri,
                                                          SZrLspPosition position,
                                                          SZrArray *result);

/**
 * @brief 生成 LSP semanticTokens/full 的整数 data 数组。
 * @note result 元素是 TZrUInt32 而非原生对象指针；消费后直接释放数组缓冲区。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetSemanticTokens(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrArray *result);
/** @brief 返回语义 token 图例中的类型数量，供 initialize 能力和请求结果共用。 */
ZR_LANGUAGE_SERVER_API TZrSize ZrLanguageServer_Lsp_SemanticTokenTypeCount(void);
/** @brief 按图例索引读取静态类型名；返回文本不归调用方所有。 */
ZR_LANGUAGE_SERVER_API const TZrChar *ZrLanguageServer_Lsp_SemanticTokenTypeName(TZrSize index);

/**
 * @brief 为整份文档提出格式化编辑；调用方负责在相同文档版本上应用。
 * @note result 是 SZrLspTextEdit* 数组，完成后使用 FreeTextEdits。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetFormatting(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri,
                                                                  SZrArray *result);

/** @brief 只为客户端选择的范围提出格式化编辑；结果仍按 FreeTextEdits 释放。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetRangeFormatting(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri,
                                                                       SZrLspRange range,
                                                                       SZrArray *result);

/**
 * @brief 结合给定范围及诊断修复生成可执行的编辑动作。
 * @note result 中每个动作可能持有嵌套文本编辑；统一使用 FreeCodeActions。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCodeActions(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrLspRange range,
                                                                   SZrArray *result);

/** @brief 将语法结构投影为文档折叠段；返回数组用 FreeFoldingRanges 归还。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetFoldingRanges(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri,
                                                                     SZrArray *result);

/**
 * @brief 为 positions 中的每个光标构造逐层包围的选择范围。
 * @note positions 在调用期间借用；result 中的原生项由 FreeSelectionRanges 释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetSelectionRanges(SZrState *state,
                                                                       SZrLspContext *context,
                                                                       SZrString *uri,
                                                                       const SZrLspPosition *positions,
                                                                       TZrSize positionCount,
                                                                       SZrArray *result);

/** @brief 查找文档可导航链接，包括虚拟声明目标；返回数组用 FreeDocumentLinks 归还。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDocumentLinks(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri,
                                                                     SZrArray *result);

/** @brief 在文档源码上生成可执行命令标记；返回数组用 FreeCodeLens 归还。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCodeLens(SZrState *state,
                                                                SZrLspContext *context,
                                                                SZrString *uri,
                                                                SZrArray *result);

/**
 * @brief 提供独立的声明导航入口，当前实现复用定义查询和位置结果契约。
 * TODO: 仓内仅见此声明和实现、未见协议调用；确认未来需不需要区分声明与定义目标。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetDeclaration(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrLspPosition position,
                                                                   SZrArray *result);

/**
 * @brief 提供类型定义导航入口，当前实现复用定义查询和位置结果契约。
 * TODO: 仓内仅见此声明和实现、未见协议调用；确认类型定义是否需要独立语义解析。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetTypeDefinition(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result);

/** @brief 取得接口或抽象目标的实现位置；结果是调用方负责释放的位置对象数组。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetImplementation(SZrState *state,
                                                                      SZrLspContext *context,
                                                                      SZrString *uri,
                                                                      SZrLspPosition position,
                                                                      SZrArray *result);

/**
 * @brief 在光标处准备调用层级节点，并保存后续查询需要的语义身份与文档版本。
 * @note result 中的 SZrLspHierarchyItem* 由 FreeHierarchyItems 归还。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_PrepareCallHierarchy(SZrState *state,
                                                                         SZrLspContext *context,
                                                                         SZrString *uri,
                                                                         SZrLspPosition position,
                                                                         SZrArray *result);

/**
 * @brief 根据已准备节点列出调用它的目标与来源范围。
 * @note item 的语义版本须仍匹配当前文档；返回边及嵌套节点由 FreeHierarchyCalls 释放。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(SZrState *state,
                                                                                  SZrLspContext *context,
                                                                                  const SZrLspHierarchyItem *item,
                                                                                  SZrArray *result);

/** @brief 根据已准备节点列出它调用的目标；版本约束和释放方式与 incoming 相同。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(SZrState *state,
                                                                                  SZrLspContext *context,
                                                                                  const SZrLspHierarchyItem *item,
                                                                                  SZrArray *result);

/** @brief 在光标处准备类型层级节点，供后续父类与子类查询复用语义身份。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_PrepareTypeHierarchy(SZrState *state,
                                                                         SZrLspContext *context,
                                                                         SZrString *uri,
                                                                         SZrLspPosition position,
                                                                         SZrArray *result);

/** @brief 列出已准备类型的直接父类型；item 的文档版本须有效，结果用 FreeHierarchyItems 释放。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(SZrState *state,
                                                                               SZrLspContext *context,
                                                                               const SZrLspHierarchyItem *item,
                                                                               SZrArray *result);

/** @brief 列出已准备类型的直接子类型；item 的文档版本须有效，结果用 FreeHierarchyItems 释放。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(SZrState *state,
                                                                             SZrLspContext *context,
                                                                             const SZrLspHierarchyItem *item,
                                                                             SZrArray *result);

/** @brief 归还格式化或独立文本编辑结果的原生项与数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeTextEdits(SZrState *state, SZrArray *result);
/** @brief 归还诊断项及其相关信息和修复子数组；诊断文本仍由 VM 管理。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeDiagnostics(SZrState *state, SZrArray *result);
/** @brief 将可说明的“无自动修复”原因映射为协议字符串；未指定原因返回空指针。 */
ZR_LANGUAGE_SERVER_API const TZrChar *ZrLanguageServer_Lsp_DiagnosticNoFixReasonName(
        EZrDiagnosticNoFixReason reason);
/** @brief 归还动作及每个动作携带的文本编辑数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeCodeActions(SZrState *state, SZrArray *result);
/** @brief 归还折叠段结果中的原生项与数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeFoldingRanges(SZrState *state, SZrArray *result);
/** @brief 归还选择范围结果中的原生项与数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeSelectionRanges(SZrState *state, SZrArray *result);
/** @brief 归还文档链接结果中的原生项与数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeDocumentLinks(SZrState *state, SZrArray *result);
/** @brief 归还源码命令标记结果中的原生项与数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeCodeLens(SZrState *state, SZrArray *result);
/** @brief 归还准备调用或类型层级时生成的节点数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeHierarchyItems(SZrState *state, SZrArray *result);
/** @brief 归还调用层级边、嵌套目标节点及来源范围数组。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_Lsp_FreeHierarchyCalls(SZrState *state, SZrArray *result);

/**
 * @brief 预检查光标目标能否重命名，并给出标识符范围与展示占位文本。
 * @note 返回的占位文本由 VM 管理；真正 Rename 仍会重新解析并获取当前文档快照。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_PrepareRename(SZrState *state,
                                                 SZrLspContext *context,
                                                 SZrString *uri,
                                                 SZrLspPosition position,
                                                 SZrLspRange *outRange,
                                                 SZrString **outPlaceholder);

/** @brief 归还签名、参数及其数组容器；标签与文档字符串仍由 VM 管理。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspSignatureHelp_Free(SZrState *state,
                                                                   SZrLspSignatureHelp *help);

/**
 * @brief 用产生文件范围的同一份内容把解析器字节偏移投影到 LSP UTF-16 范围。
 * @pre content 与 fileRange 的偏移来自同一文档版本。
 */
ZR_LANGUAGE_SERVER_API SZrLspRange ZrLanguageServer_LspRange_FromFileRangeWithContent(SZrFileRange fileRange,
                                                                                       const TZrChar *content,
                                                                                       TZrSize contentLength);

/**
 * @brief 将客户端范围定位到解析器字节偏移并关联来源 URI。
 * @note 返回范围借用 uri；content 与客户端坐标必须对应同一文档版本。
 */
ZR_LANGUAGE_SERVER_API SZrFileRange ZrLanguageServer_LspRange_ToFileRangeWithContent(SZrLspRange lspRange,
                                                                                     SZrString *uri,
                                                                                     const TZrChar *content,
                                                                                     TZrSize contentLength);

/**
 * @brief 在没有源文本可重算偏移时，将描述符的一基结构坐标投影到 LSP 零基范围。
 * @note 只接受正坐标和正序范围；此入口不做 UTF-16 字节换算。
 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_Lsp_TryRangeFromDescriptorMetadataCoordinates(
        SZrFileRange range,
        SZrLspRange *outRange);

/**
 * @brief 将优化 remark 的规范字节范围按对应内容投影到 LSP UTF-16 范围。
 * @note 交由语义适配器完成，不重新分析源码；content 须是 remark 对应版本的文本。
 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_Lsp_ProjectOptimizationRemarkRange(
        const struct SZrOptimizationRemark *remark,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspRange *outRange);

/** @brief 用同版本内容把解析器字节偏移转成客户端零基行、UTF-16 列。 */
ZR_LANGUAGE_SERVER_API SZrLspPosition ZrLanguageServer_LspPosition_FromFilePositionWithContent(
        SZrFilePosition filePosition,
        const TZrChar *content,
        TZrSize contentLength);

/** @brief 把客户端零基行、UTF-16 列转成解析器偏移及一基文件坐标。 */
ZR_LANGUAGE_SERVER_API SZrFilePosition ZrLanguageServer_LspPosition_ToFilePositionWithContent(SZrLspPosition lspPosition,
                                                                                 const TZrChar *content, TZrSize contentLength);

#endif //ZR_VM_LANGUAGE_SERVER_LSP_INTERFACE_H
