#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/variance.h"

void ZrLanguageServer_SemanticAnalyzer_ValidateInterfaceVarianceRules(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *interfaceNode) {
    TZrSize violationIndex = 0U;
    SZrVarianceViolation violation;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || interfaceNode == ZR_NULL) {
        return;
    }

    while (ZrParser_Variance_InterfaceViolationAt(
            analyzer->compilerState,
            interfaceNode,
            violationIndex,
            &violation)) {
        SZrStructuredDiagnostic diagnostic;
        SZrSemanticDiagnosticFact fact;

        ZrParser_StructuredDiagnostic_Init(&diagnostic);
        if (!ZrParser_Variance_BuildDiagnostic(
                    state, &violation, &diagnostic)) {
            return;
        }
        memset(&fact, 0, sizeof(fact));
        fact.node = violation.node;
        fact.diagnostic = diagnostic;
        if (!ZrParser_SemanticFacts_AppendDiagnostic(
                    analyzer->semanticContext, &fact)) {
            ZrParser_StructuredDiagnostic_Free(state, &diagnostic);
            return;
        }
        ZrParser_StructuredDiagnostic_Free(state, &diagnostic);
        violationIndex++;
    }
}
