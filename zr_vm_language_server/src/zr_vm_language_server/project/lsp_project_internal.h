#ifndef ZR_VM_LANGUAGE_SERVER_LSP_PROJECT_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_PROJECT_INTERNAL_H

#include "interface/lsp_interface_internal.h"

/** @brief AST 导入绑定是导航、引用和诊断的共同视图；名称借用 VM，绑定由 CollectImportBindings 创建并由 FreeImportBindings 释放。 */
typedef struct SZrLspImportBinding {
    SZrString *aliasName; /**< 文件内用来引用模块的本地名字。 */
    SZrString *moduleName; /**< 项目解析器使用的目标模块键。 */
    SZrFileRange aliasLocation; /**< rename/references 中本地名字的编辑范围。 */
    SZrFileRange modulePathLocation; /**< import 目标字面量的编辑范围。 */
} SZrLspImportBinding;

/** @brief 保留 import 成员命中的模块、接收者和成员位置，供跨模块导航组合使用。 */
typedef struct SZrLspImportedMemberHit {
    SZrString *moduleName; /**< 命中的导入目标模块。 */
    SZrString *memberName; /**< 在该模块上查找的成员名。 */
    SZrFileRange receiverLocation; /**< import 别名作为接收者的位置。 */
    SZrFileRange location; /**< 成员名本身的导航或引用位置。 */
} SZrLspImportedMemberHit;

/** @brief 编辑前保存模块名和公开契约摘要；模块名是 VM 管理的字符串，比较期间须保持 VM state 有效。 */
typedef struct SZrLspProjectPublicContractSnapshot {
    SZrString *moduleName;
    TZrUInt64 hash;
    TZrSize exportCount;
    TZrBool hasHash;
} SZrLspProjectPublicContractSnapshot;

/** @brief 按公开契约是否可比较及是否变化决定反向依赖重分析范围。 */
typedef enum EZrLspProjectPublicContractChange {
    ZR_LSP_PROJECT_PUBLIC_CONTRACT_UNAVAILABLE = 0,
    ZR_LSP_PROJECT_PUBLIC_CONTRACT_MATCH,
    ZR_LSP_PROJECT_PUBLIC_CONTRACT_CHANGE
} EZrLspProjectPublicContractChange;

/** @brief 按 URI 等价关系查找现有文件记录；返回借用指针，索引重建后失效。 */
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByUri(SZrLspProjectIndex *projectIndex,
                                                                     SZrString *uri);
/** @brief 按规范化模块键查重；返回项目索引持有的记录。 */
SZrLspProjectFileRecord *ZrLanguageServer_LspProject_FindRecordByModuleName(SZrLspProjectIndex *projectIndex,
                                                                            SZrString *moduleName);
/** @brief 为源码模块入口导航取得当前文档可呈现的范围。 */
SZrFileRange ZrLanguageServer_LspProject_GetSourceModuleEntryRange(
        SZrLspContext *context,
        const SZrLspProjectFileRecord *record);
/** @brief 按 .zrp URI 定位索引，供虚拟文档作用域及移除事件使用；可返回数组下标。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectByProjectUri(SZrLspContext *context,
                                                                        SZrString *uri,
                                                                        TZrSize *outIndex);
/** @brief 为真实 URI 选择最深项目根，虚拟 URI 使用显式项目作用域。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_FindProjectForUri(SZrLspContext *context, SZrString *uri);
/** @brief 按文档 URI 发现项目并建立索引；语义入口加载由调用方按需执行。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_GetOrCreateForUri(SZrState *state,
                                                                  SZrLspContext *context,
                                                                  SZrString *uri);
/** @brief 按明确 .zrp URI 建立索引，供项目级查询使用。 */
SZrLspProjectIndex *ZrLanguageServer_LspProject_GetOrCreateByProjectUri(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrString *projectUri);
/** @brief 元数据解析器按项目语义装载模块，可能递归读取依赖源文件。 */
TZrBool ZrLanguageServer_LspProject_EnsureModuleLoadedByName(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *moduleName);
/** @brief 为诊断与跨快照引用建立轻量源码图；失败时调用方不得使用部分记录。 */
TZrBool ZrLanguageServer_LspProject_EnsureScannedSourceGraph(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex);
/** @brief 收集项目图与打开文档的去重 URI 并集，供 stdio/WASM 批量诊断；数组由调用方持有。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_CollectDiagnosticDocumentUris(
    SZrState *state,
    SZrLspContext *context,
    SZrArray *outUris);
/** @brief 项目删除事件移除索引及不再由其他项目共享的分析状态。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                                                     SZrLspContext *context,
                                                                                     SZrString *uri);
/** @brief 工作区移除项目时保留未保存的打开文档覆盖层。 */
TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUriPreservingOpenDocuments(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri);
/** @brief 删除一份项目文件记录及其分析缓存；多个项目记录需重复调用。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                                                 SZrLspContext *context,
                                                                                 SZrString *uri);
/** @brief didRenameFiles 后迁移记录 URI 并失效旧解析缓存；调用方随后按新 URI 刷新模块身份。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_PrepareSourceRename(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri);
/** @brief 收集源码移动所需的 import 位置修改，供调用方组装 WorkspaceEdit。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_CollectSourceRenameEdits(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri,
        SZrString **outNewModuleName,
        SZrArray *outLocations);
/** @brief 一并收集 import 编辑位置和文档快照，以便重命名前校验。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_CollectSourceRenameEditPlan(
        SZrState *state,
        SZrLspContext *context,
        SZrString *oldUri,
        SZrString *newUri,
        SZrString **outNewModuleName,
        SZrArray *outLocations,
        SZrArray *outDocumentSnapshots);
/** @brief 验证重命名编辑计划中的文档快照仍匹配当前内容。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_ValidateSourceRenameEditPlan(
        SZrState *state,
        SZrLspContext *context,
        const SZrArray *documentSnapshots);
/** @brief 从重命名计划中返回指定 URI 的借用快照。 */
ZR_LANGUAGE_SERVER_API const SZrLspSourceRenameDocumentSnapshot *
ZrLanguageServer_LspProject_FindSourceRenameDocumentSnapshot(
        const SZrArray *documentSnapshots,
        SZrString *uri);
/** @brief 元数据或插件监听事件失效 VM 缓存并按磁盘 .zrp 重建所属项目。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                                            SZrLspContext *context,
                                                                                            SZrString *uri);
/** @brief 在语义分析完成后写入文件记录的公开契约摘要。 */
void ZrLanguageServer_LspProject_UpdatePublicContractRecord(
        SZrLspProjectFileRecord *record,
        const SZrSemanticAnalyzer *analyzer);
/** @brief 编辑前借用模块名并复制契约摘要，供变更判定。 */
void ZrLanguageServer_LspProject_CapturePublicContract(
        SZrLspProjectIndex *projectIndex,
        SZrString *uri,
        SZrLspProjectPublicContractSnapshot *outSnapshot);
/** @brief 比较更新前后契约；摘要不可用时保守传播反向依赖失效。 */
EZrLspProjectPublicContractChange ZrLanguageServer_LspProject_ClassifyPublicContractChange(
        SZrLspProjectIndex *projectIndex,
        const SZrLspProjectPublicContractSnapshot *previous,
        const SZrLspProjectFileRecord *current);

/** @brief 释放 CollectImportBindings 创建的绑定对象和数组，不释放 VM 字符串。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspProject_FreeImportBindings(SZrState *state, SZrArray *bindings);
/** @brief 从 AST 收集真实导入绑定，供导航、诊断和依赖刷新共用。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspProject_CollectImportBindings(SZrState *state,
                                                                              SZrAstNode *node,
                                                                              SZrArray *bindings);
/** @brief 优先按编辑覆盖层收集 URI 依赖，无 AST 时回退源码文本。 */
TZrBool ZrLanguageServer_LspProject_CollectImportModuleNamesForUri(SZrState *state,
                                                                   SZrLspContext *context,
                                                                   SZrString *uri,
                                                                   SZrArray *moduleNames);
/** @brief 按源码别名关联导入模块，供成员导航和补全。 */
SZrLspImportBinding *ZrLanguageServer_LspProject_FindImportBindingByAlias(SZrArray *bindings, SZrString *aliasName);
/** @brief 判断光标是否命中 AST 导入绑定，以供语义查询分流。 */
TZrBool ZrLanguageServer_LspProject_FindImportBindingHit(SZrAstNode *node,
                                                         SZrArray *bindings,
                                                         SZrFileRange position,
                                                         SZrLspImportBinding **outBinding,
                                                         SZrFileRange *outLocation);
/** @brief 将指定模块成员的项目内引用追加到调用方结果。 */
TZrBool ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(SZrState *state,
                                                                          SZrLspContext *context,
                                                                          SZrString *uri,
                                                                          SZrAstNode *node,
                                                                          SZrArray *bindings,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrArray *result);
/** @brief 将目标模块成员访问中的别名接收者和成员位置追加到模块级引用结果。 */
TZrBool ZrLanguageServer_LspProject_AppendMatchingImportedModuleLocations(SZrState *state,
                                                                          SZrLspContext *context,
                                                                          SZrString *uri,
                                                                          SZrAstNode *node,
                                                                          SZrArray *bindings,
                                                                          SZrString *moduleName,
                                                                          SZrArray *result);
/** @brief 将模块在当前文件绑定的本地别名位置追加到结果。 */
TZrBool ZrLanguageServer_LspProject_AppendMatchingImportBindingLocations(SZrState *state,
                                                                         SZrLspContext *context,
                                                                         SZrString *uri,
                                                                         SZrArray *bindings,
                                                                         SZrString *moduleName,
                                                                         SZrArray *result);
/** @brief 将 import 目标文字位置追加到源码重命名结果。 */
TZrBool ZrLanguageServer_LspProject_AppendMatchingImportTargetLocations(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrString *uri,
                                                                        SZrArray *bindings,
                                                                        SZrString *moduleName,
                                                                        SZrArray *result);
/** @brief 遍历项目文件图追加目标模块的 import 引用。 */
TZrBool ZrLanguageServer_LspProject_AppendProjectImportTargetReferences(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrString *fallbackUri,
        SZrString *moduleName,
        SZrArray *result);
/** @brief 将项目导入可解析性转为当前文档的 LSP 诊断。 */
TZrBool ZrLanguageServer_LspProject_CollectImportDiagnostics(SZrState *state,
                                                             SZrLspContext *context,
                                                             SZrLspProjectIndex *projectIndex,
                                                             SZrString *uri,
                                                             SZrArray *result);
/** @brief 从二进制产物路径反推模块键，供导航和监听失效使用。 */
TZrBool ZrLanguageServer_LspProject_DeriveBinaryModuleNameFromPath(SZrLspProjectIndex *projectIndex,
                                                                   const TZrChar *path,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize);

#endif
