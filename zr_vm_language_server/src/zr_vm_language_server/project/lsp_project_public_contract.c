#include "project/lsp_project_internal.h"

#include <string.h>

#include "zr_vm_parser/semantic_query.h"

/* 仅在分析器具备完整语义状态时缓存导出契约；未知状态保留 hasHash=false，
 * 后续刷新必须保守地重析逆向依赖，不能把缺失哈希当成“无变化”。 */
void ZrLanguageServer_LspProject_UpdatePublicContractRecord(
        SZrLspProjectFileRecord *record,
        const SZrSemanticAnalyzer *analyzer) {
    SZrParserSemanticPublicContractQuery query;

    if (record == ZR_NULL) {
        return;
    }
    record->publicContractHash = 0U;
    record->publicContractExportCount = 0U;
    record->hasPublicContractHash = ZR_FALSE;
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || analyzer->ast == ZR_NULL) {
        return;
    }
    if (ZrParser_SemanticQuery_PublicContract(
                analyzer->semanticContext,
                analyzer->compilerState->typeEnv,
                analyzer->ast,
                &query)) {
        record->publicContractHash = query.hash;
        record->publicContractExportCount = query.exportCount;
        record->hasPublicContractHash = ZR_TRUE;
    }
}

/* 在替换当前文档分析结果之前抓取旧导出摘要，作为刷新后是否重析 importer 的比较基线；
 * moduleName 是 VM 管理的字符串，不由快照持有；分类期间须保持该 VM state 有效。 */
void ZrLanguageServer_LspProject_CapturePublicContract(
        SZrLspProjectIndex *projectIndex,
        SZrString *uri,
        SZrLspProjectPublicContractSnapshot *outSnapshot) {
    SZrLspProjectFileRecord *record;

    if (outSnapshot == ZR_NULL) {
        return;
    }
    memset(outSnapshot, 0, sizeof(*outSnapshot));
    record = ZrLanguageServer_LspProject_FindRecordByUri(projectIndex, uri);
    if (record == ZR_NULL) {
        return;
    }
    outSnapshot->moduleName = record->moduleName;
    outSnapshot->hash = record->publicContractHash;
    outSnapshot->exportCount = record->publicContractExportCount;
    outSnapshot->hasHash = record->hasPublicContractHash;
}

/* 模块键、摘要和导出数都一致才保留逆向依赖；其他可比较变化触发重析，
 * 任一侧语义摘要缺失则返回 unavailable，由刷新路径保守处理。 */
EZrLspProjectPublicContractChange ZrLanguageServer_LspProject_ClassifyPublicContractChange(
        SZrLspProjectIndex *projectIndex,
        const SZrLspProjectPublicContractSnapshot *previous,
        const SZrLspProjectFileRecord *current) {
    if (projectIndex == ZR_NULL || previous == ZR_NULL || current == ZR_NULL ||
        previous->moduleName == ZR_NULL || current->moduleName == ZR_NULL ||
        !previous->hasHash || !current->hasPublicContractHash) {
        if (projectIndex != ZR_NULL) {
            projectIndex->publicContractHashUnavailableCount++;
        }
        return ZR_LSP_PROJECT_PUBLIC_CONTRACT_UNAVAILABLE;
    }
    if (ZrLanguageServer_Lsp_StringsEqual(previous->moduleName, current->moduleName) &&
        previous->hash == current->publicContractHash &&
        previous->exportCount == current->publicContractExportCount) {
        projectIndex->publicContractHashMatchCount++;
        return ZR_LSP_PROJECT_PUBLIC_CONTRACT_MATCH;
    }
    projectIndex->publicContractHashChangeCount++;
    return ZR_LSP_PROJECT_PUBLIC_CONTRACT_CHANGE;
}
