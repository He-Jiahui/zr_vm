#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPORT_CHAIN_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_IMPORT_CHAIN_H

#include "metadata/lsp_metadata_provider.h"
#include "project/lsp_project_internal.h"

/** @brief 光标命中的导入别名或其成员；供语义查询、悬停和声明导航共享同一条元数据链。
 *  @note 字符串及声明信息借用 analyzer、项目索引或上下文；调用方须在这些对象有效期内消费。
 *  memberName 为空表示命中导入别名，resolvedMember 仅在命中成员时有意义。 */
typedef struct SZrLspSemanticImportChainHit {
    SZrString *moduleName;
    SZrString *memberName;
    SZrFileRange location;
    SZrLspResolvedMetadataMember resolvedMember;
} SZrLspSemanticImportChainHit;

/** @brief 从模块成员元数据取得再导出目标；面向需要逐段追踪导入链的调用方。
 *  @pre state、context、analyzer、moduleName、memberName 与 outResolvedMember 有效。
 *  @note outNextModuleName 可为空；成员不是模块时，成功解析也会把它置空。当前仓内无运行时调用方。 */
TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveLinkedMember(
    SZrState *state,
    SZrLspContext *context,
    SZrSemanticAnalyzer *analyzer,
    SZrLspProjectIndex *projectIndex,
    SZrString *moduleName,
    SZrString *memberName,
    SZrLspResolvedMetadataMember *outResolvedMember,
    SZrString **outNextModuleName);
/** @brief 将光标前以点号结束的导入别名链定位到末端模块，供模块成员补全查询。
 *  @pre content 指向至少 contentLength 字节；cursorOffset 不超过长度并位于点号之后。
 *  @note 只处理点号前的简单标识符链；返回的模块借用当前语义查询和项目索引状态。 */
TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveCompletionModuleAtOffset(
    SZrState *state,
    SZrLspContext *context,
    SZrLspProjectIndex *projectIndex,
    SZrSemanticAnalyzer *analyzer,
    SZrArray *bindings,
    const TZrChar *content,
    TZrSize contentLength,
    TZrSize cursorOffset,
    SZrLspResolvedImportedModule *outResolvedModule);
/** @brief 在当前 AST 中把光标范围映射到导入链成员，供规范符号查询和接收者类型判定。
 *  @pre bindings 来自同一 analyzer 的导入声明；queryRange 使用该文档的源坐标。
 *  @note 只返回首个命中；outHit 中的引用不得越过 analyzer 或项目元数据生命周期。 */
TZrBool ZrLanguageServer_LspSemanticImportChain_ResolveAtRange(SZrState *state,
                                                               SZrLspContext *context,
                                                               SZrLspProjectIndex *projectIndex,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrArray *bindings,
                                                               SZrFileRange queryRange,
                                                               SZrLspSemanticImportChainHit *outHit);
/** @brief 在指定 URI 的导入链中收集目标模块成员的引用位置，供项目级引用导航回调使用。
 *  @pre result 是可追加的位置数组；uri 对应可分析的项目源文件或打开文档。
 *  @return 当前实现的布尔值表示至少追加一处匹配，而非扫描成功；调用方须区别空结果。 */
TZrBool ZrLanguageServer_LspSemanticImportChain_AppendMatchingLocationsForUri(SZrState *state,
                                                                              SZrLspContext *context,
                                                                              SZrLspProjectIndex *projectIndex,
                                                                              SZrString *uri,
                                                                              SZrString *moduleName,
                                                                              SZrString *memberName,
                                                                              SZrArray *result);

#endif
