#include "semantic_query_published_diagnostics.h"

#include "zr_vm_parser/diagnostic_builder.h"

TZrBool ZrParser_SemanticQueryPublished_AppendDiagnostic(
        SZrSemanticContext *context,
        const SZrSemanticDiagnosticFact *fact) {
    SZrStructuredDiagnostic diagnostic;

    if (context == ZR_NULL || fact == ZR_NULL ||
        !context->queryDiagnostics.isValid ||
        !ZrParser_StructuredDiagnostic_Copy(
                context->state, &diagnostic, &fact->diagnostic)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->queryDiagnostics, &diagnostic);
    return ZR_TRUE;
}
