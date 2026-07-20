#include "project/lsp_project_internal.h"

#include <string.h>

#include "zr_vm_parser/semantic_query.h"

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
