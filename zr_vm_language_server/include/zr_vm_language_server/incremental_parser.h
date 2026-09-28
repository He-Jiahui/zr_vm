//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H
#define ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"

/** @brief 解析器诊断由 LSP 诊断层定义，文件版本只持有其指针。 */
typedef struct SZrDiagnostic SZrDiagnostic;

/** @brief 被文件版本、历史槽和请求快照共同引用的不可变文本块。
 * @note refCount 维持内容有效期；更新版本只替换当前块，仍被请求读取的旧块待最后一次释放后回收。 */
typedef struct SZrFileVersionContentBlock {
    TZrChar *content;
    TZrSize contentLength;
    TZrSize contentGeneration;
    TZrSize refCount;
} SZrFileVersionContentBlock;

/** @brief 保存最近一次更新前的内容和当时的文档来源，供声明级重解析与历史语义比较。
 * @note 历史槽按新到旧排列，容量由 ZR_LSP_FILE_VERSION_HISTORICAL_CONTENT_CAPACITY 限制。 */
typedef struct SZrFileVersionHistoricalContent {
    SZrFileVersionContentBlock *contentBlock;
    TZrSize version;
    TZrBool isOpenDocument;
    TZrBool usesFallbackAst;
} SZrFileVersionHistoricalContent;

/** @brief 将一次文本变更归类为模块、声明签名或声明体影响。
 * @note 语义缓存据此决定保留的查询范围；分类属于分析结果，不保证可直接重用旧 AST。 */
enum EZrFileChangeImpact {
    ZR_FILE_CHANGE_IMPACT_NONE,
    ZR_FILE_CHANGE_IMPACT_MODULE,
    ZR_FILE_CHANGE_IMPACT_DECLARATION_SIGNATURE,
    ZR_FILE_CHANGE_IMPACT_DECLARATION_BODY,
};

/** @brief C API 使用的文件变更影响枚举别名。 */
typedef enum EZrFileChangeImpact EZrFileChangeImpact;

/** @brief 记录本次 AST 实际经历的解析路径，供项目索引与回归测试判断缓存复用范围。 */
enum EZrIncrementalParseMode {
    ZR_INCREMENTAL_PARSE_MODE_FULL_REPARSE = 0,
    ZR_INCREMENTAL_PARSE_MODE_TOKEN_EQUIVALENT,
    ZR_INCREMENTAL_PARSE_MODE_DECLARATION_REPARSE,
};

/** @brief C API 使用的解析路径枚举别名。 */
typedef enum EZrIncrementalParseMode EZrIncrementalParseMode;

/** @brief 关联旧/新范围、声明身份与词法等价性，让更新后的解析和语义缓存使用同一变更事实。
 * @note isTokenEquivalent 仅表示可复用现有 AST 的词法判定；声明影响仍需在调用链后续分类。 */
typedef struct SZrFileChangeInfo {
    SZrFileRange oldRange;
    SZrFileRange newRange;
    EZrFileChangeImpact impact;
    EZrAstNodeType declarationType;
    SZrFileRange declarationRange;
    TZrBool hasDeclaration;
    TZrBool isTokenEquivalent;
} SZrFileChangeInfo;

/** @brief URI 对应的可变文档状态，承接客户端 overlay 或工作区磁盘缓存。
 * @note parser 持有此记录及其 AST、诊断和当前/历史内容块；GetFileVersion 返回借用指针，移除文件或释放 parser 后失效。
 * @note usesFallbackAst 表示 AST 对应旧的有效语法版本；当前文本仍可能有解析错误，语义请求须据此拒绝旧结果。 */
typedef struct SZrFileVersion {
    SZrString *uri;                   // 文件 URI
    TZrSize version;                  // 版本号
    TZrBool isOpenDocument;           // 客户端 overlay，而非 workspace disk cache
    SZrFileVersionContentBlock *textBlock; // 当前内容块
    SZrFileVersionHistoricalContent
            historicalContent[ZR_LSP_FILE_VERSION_HISTORICAL_CONTENT_CAPACITY];
    TZrSize historicalContentCount;   // 按新到旧保存，最多两份
    SZrAstNode *ast;                  // 解析后的 AST
    TZrBool usesFallbackAst;            // 当前 AST 是否是旧版本保留下来的 last-good 快照
    TZrBool isDesynchronized;           // 最后一次同步通知无效，语义请求必须 fail closed
    TZrBool isDirty;                    // 是否需要重新解析
    SZrFileRange lastChangeRange;     // 最后变更的范围（用于增量解析）
    SZrFileChangeInfo lastChangeInfo; // 最后变更的旧/新范围与声明级影响
    TZrChar *lastContentHash;           // 内容哈希（用于快速比较，可选）
    TZrSize lastContentHashLength;    // 哈希长度
    TZrBool hasIncrementalInfo;         // 是否有增量信息
    EZrIncrementalParseMode lastParseMode; // actual parse path used for the current AST
    SZrArray parserDiagnostics;      // 语法诊断信息（SZrDiagnostic*）
} SZrFileVersion;

/** @brief 为解析、导航和语义请求固定一次文本内容读取。
 * @note Acquire 持有 contentBlock，故内容可跨文件更新继续读取；uri 不独立持有，仍须维持文档所属上下文有效。 */
typedef struct SZrFileVersionContentSnapshot {
    SZrString *uri;
    TZrSize version;
    TZrBool isOpenDocument;
    TZrChar *content;
    TZrSize contentLength;
    TZrSize contentGeneration;
    TZrBool usesFallbackAst;
    SZrFileVersionContentBlock *contentBlock;
} SZrFileVersionContentSnapshot;

/** @brief 管理 URI 到文件版本的映射及其 AST 生命周期，供 LSP 文档同步与项目索引共享。
 * @note retainedPreviousAstOutput 仅供一次 ParseRetainingPreviousAst 调用临时移交旧 AST，不应由普通调用方直接设置。 */
typedef struct SZrIncrementalParser {
    SZrState *state;
    SZrHashSet uriToFileMap;          // URI 到文件版本的映射（值为SZrFileVersion*）
    SZrParserState *parserState;      // 解析器状态（共享）
    TZrBool enableIncrementalParse;     // 是否启用增量解析
    TZrBool enableContentHash;          // 是否使用内容哈希优化
    SZrAstNode **retainedPreviousAstOutput; // 单次parse的旧AST所有权移交槽
} SZrIncrementalParser;

/** @brief 为 LSP 上下文创建文档版本和解析状态的所有者。
 * @return 成功返回需用 IncrementalParser_Free 释放的解析器。 */
ZR_LANGUAGE_SERVER_API SZrIncrementalParser *ZrLanguageServer_IncrementalParser_New(SZrState *state);

/** @brief 在上下文结束时释放全部文件版本、AST 与解析状态。
 * @note 此后 GetFileVersion 和 GetAST 返回的借用指针均失效。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_IncrementalParser_Free(SZrState *state, 
                                                     SZrIncrementalParser *parser);

/** @brief 将项目扫描或 provider 缓存中的磁盘文本写入版本表，供后续解析与索引读取。
 * @pre 已存在记录须提供递增版本；content 在调用期间有效。
 * @return 旧版本或已检查到的分配失败时为假；成功不代表 AST 已更新。
 * BUG: 首次 URI 的 HashSet_Add 失败时共享更新器仍返回真，且未释放新建的文件版本；调用方可能误认文本已入表。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_IncrementalParser_UpdateFile(SZrState *state,
                                                              SZrIncrementalParser *parser,
                                                              SZrString *uri,
                                                              const TZrChar *content,
                                                              TZrSize contentLength,
                                                              TZrSize version);
/** @brief 将客户端打开文档的文本作为 overlay 写入版本表，优先于磁盘缓存参与语义请求。
 * @pre 通常须提供递增版本；首次覆盖同版本的磁盘快照可接受。
 * @return 通常接受更新时为真，调用方仍需解析并检查诊断。
 * BUG: 首次 URI 的 HashSet_Add 失败时共享更新器仍返回真，且未释放新建的文件版本；调用方可能误认 overlay 已入表。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_IncrementalParser_UpdateOpenDocument(
        SZrState *state,
        SZrIncrementalParser *parser,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize version);

/** @brief 将已更新的文本转成 AST 和解析诊断，供 LSP 分析器及项目索引消费。
 * @note 会按词法等价、声明局部重解析、完整重解析的可用条件选择路径；语法失败时可能保留旧 AST 并设置 usesFallbackAst。
 * @return 有可用 AST 或解析诊断时为真；调用方不得仅凭真值认为当前文本语义有效。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_IncrementalParser_Parse(SZrState *state,
                                                        SZrIncrementalParser *parser,
                                                        SZrString *uri);
/** @brief 在替换 AST 时把旧树交给语义快照缓存，以保留上一代查询结果的比较上下文。
 * @pre retainedPreviousAst 是可写输出槽；同一 parser 不可嵌套执行此移交。
 * @note 输出非 NULL 时所有权转给调用方，必须交由快照缓存接管或用 ZrParser_Ast_Free 释放。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_IncrementalParser_ParseRetainingPreviousAst(
    SZrState *state,
    SZrIncrementalParser *parser,
    SZrString *uri,
    /* Non-null result transfers the previous AST to the caller. */
    SZrAstNode **retainedPreviousAst);

/** @brief 给项目索引等消费者取得文件 AST；必要时惰性触发解析。
 * @return 借用指针；下一次解析、移除文件或释放 parser 后可能失效。 */
ZR_LANGUAGE_SERVER_API SZrAstNode *ZrLanguageServer_IncrementalParser_GetAST(SZrIncrementalParser *parser,
                                                               SZrString *uri);

/** @brief 在项目文件驱逐或 URI 更名时移除版本记录并释放其 AST 与诊断。
 * @note 调用方须先结束对该文件版本及 AST 的借用。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_IncrementalParser_RemoveFile(SZrState *state,
                                                           SZrIncrementalParser *parser,
                                                           SZrString *uri);

/** @brief 给 LSP 处理器借用当前 URI 对应的版本记录。
 * @return 记录由 parser 持有；RemoveFile、IncrementalParser_Free 后不得再使用。 */
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrLanguageServer_IncrementalParser_GetFileVersion(SZrIncrementalParser *parser,
                                                                           SZrString *uri);

/** @brief 为首次见到的 URI 建立文本、AST 与诊断状态，供解析器版本表收纳。
 * @note uri 为借用引用；返回记录需由解析器或调用方最终使用 FileVersion_Free 释放。 */
ZR_LANGUAGE_SERVER_API SZrFileVersion *ZrLanguageServer_FileVersion_New(SZrState *state,
                                                          SZrString *uri,
                                                          const TZrChar *content,
                                                          TZrSize contentLength,
                                                          TZrSize version);

/** @brief 释放版本记录拥有的 AST、诊断和内容块；已有快照仍可保持文本块有效。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_FileVersion_Free(SZrState *state, SZrFileVersion *fileVersion);

/** @brief 用已计算的变更事实替换文本并保留有限历史，供后续解析选择复用路径。
 * @pre changeInfo 描述同一次旧/新文本变更；调用方负责版本单调性。
 * @return 成功时新内容生效并标记解析状态；失败时保留旧版本。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_FileVersion_UpdateContent(SZrState *state,
                                                         SZrFileVersion *fileVersion,
                                                         const TZrChar *content,
                                                         TZrSize contentLength,
                                                         TZrSize version,
                                                         const SZrFileChangeInfo *changeInfo);

/** @brief 为一个请求固定文件版本的当前文本，避免更新时释放读取中的内容。
 * @note 成功后必须调用 FileVersionContentSnapshot_Free；只有文本块独立保活，uri/AST 不因此延寿。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_FileVersionContentSnapshot_Acquire(
    SZrState *state,
    SZrFileVersion *fileVersion,
    SZrFileVersionContentSnapshot *outSnapshot);
/** @brief 释放一次文本快照的引用，并清空输出结构，防止继续读取已过期内容。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_FileVersionContentSnapshot_Free(
    SZrState *state,
    SZrFileVersionContentSnapshot *snapshot);
/** @brief 查询可供声明级重解析对照的历史文本数量。
 * @note 历史按新到旧排列，更新可能淘汰最旧条目。 */
ZR_LANGUAGE_SERVER_API TZrSize
ZrLanguageServer_FileVersionHistoricalContentSnapshot_Count(
    const SZrFileVersion *fileVersion);
/** @brief 固定指定历史文本，供增量语法重解析比较旧/新内容。
 * @pre historyIndex 小于 Count 的当次结果，且调用期间版本记录未被移除。
 * @note 成功后必须调用 FileVersionContentSnapshot_Free。 */
ZR_LANGUAGE_SERVER_API TZrBool
ZrLanguageServer_FileVersionHistoricalContentSnapshot_Acquire(
    SZrState *state,
    SZrFileVersion *fileVersion,
    TZrSize historyIndex,
    SZrFileVersionContentSnapshot *outSnapshot);

#endif //ZR_VM_LANGUAGE_SERVER_INCREMENTAL_PARSER_H
