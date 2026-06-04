#ifndef ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_QUERY_H
#define ZR_VM_LANGUAGE_SERVER_LSP_LOCAL_SEMANTIC_QUERY_H

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_parser/semantic_facts.h"

typedef enum EZrLspLocalSemanticQueryStatus {
    ZR_LSP_LOCAL_SEMANTIC_QUERY_UNKNOWN = 0,
    ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT,
    ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE
} EZrLspLocalSemanticQueryStatus;

typedef struct SZrLspLocalSemanticQueryResult {
    EZrLspLocalSemanticQueryStatus status;
    SZrFileRange queryRange;
    SZrDiagnostic *diagnostic;
    const SZrSemanticExpressionFact *expressionFact;
    const SZrSemanticNumericFact *numericFact;
    const SZrSemanticReferenceFact *referenceFact;
    const SZrSemanticReachabilityFact *reachabilityFact;
    const SZrSemanticLogicalFact *logicalFact;
    const SZrSemanticOwnershipFact *ownershipFact;
} SZrLspLocalSemanticQueryResult;

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLocalSemanticQuery_Init(
    SZrLspLocalSemanticQueryResult *result);

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLocalSemanticQuery_Clear(
    SZrLspLocalSemanticQueryResult *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspLocalSemanticQueryResult *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt(
    SZrState *state,
    SZrLspContext *context,
    SZrString *uri,
    SZrLspPosition position,
    SZrLspLocalSemanticQueryResult *result);

ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspLocalSemanticQuery_BuildHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover **result);

ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover(
    SZrState *state,
    const SZrLspLocalSemanticQueryResult *query,
    SZrLspHover *hover);

#endif
