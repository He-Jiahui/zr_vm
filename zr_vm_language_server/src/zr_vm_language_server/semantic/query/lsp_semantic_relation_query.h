#ifndef ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_RELATION_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_SEMANTIC_RELATION_QUERY_H

#include "metadata/lsp_metadata_provider.h"

/** 导入来源的解析结果；URI 与身份借用当前快照，metadata 目标供导航层使用。 */
typedef struct SZrLspSemanticImportOriginTarget {
    SZrString *originIdentity;
    SZrString *virtualDeclarationUri;
    SZrFileRange referenceRange;
    SZrLspResolvedImportedModule module;
    SZrLspResolvedImportedModuleEntry declaration;
} SZrLspSemanticImportOriginTarget;

/** 区分非导入目标、可导航导入和无效导入，避免失败时误退回普通符号导航。 */
typedef enum EZrLspSemanticImportOriginResolution {
    ZR_LSP_SEMANTIC_IMPORT_ORIGIN_NOT_APPLICABLE = 0,
    ZR_LSP_SEMANTIC_IMPORT_ORIGIN_RESOLVED,
    ZR_LSP_SEMANTIC_IMPORT_ORIGIN_INVALID
} EZrLspSemanticImportOriginResolution;

/** @brief 用符号与导入关系的一致身份解析外部模块声明。
 * @note outTarget 在失败时清零；解析成功后的借用字段仅在快照有效期间使用。 */
EZrLspSemanticImportOriginResolution
ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOrigin(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *symbol,
        SZrLspSemanticImportOriginTarget *outTarget);
/** @brief 为 import 字面量光标按来源范围查找外部模块声明。
 * @note 与符号入口共用 metadata 校验，但需解析器确认该位置确为导入来源。 */
EZrLspSemanticImportOriginResolution
ZrLanguageServer_LspSemanticRelationQuery_ResolveImportOriginAt(
        SZrState *state,
        SZrLspContext *context,
        SZrLspProjectIndex *projectIndex,
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position,
        SZrLspSemanticImportOriginTarget *outTarget);

#endif
