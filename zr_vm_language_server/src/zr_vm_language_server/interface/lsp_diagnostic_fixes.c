#include "interface/lsp_diagnostic_fixes.h"

#include <string.h>

void ZrLanguageServer_Lsp_CopyDiagnosticFixes(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              const SZrDiagnostic *diagnostic,
                                              SZrLspDiagnostic *lspDiagnostic) {
    TZrSize capacity;

    if (state == ZR_NULL || diagnostic == ZR_NULL || lspDiagnostic == ZR_NULL) {
        return;
    }

    capacity = diagnostic->fixes.isValid ? diagnostic->fixes.length : 0;
    ZrCore_Array_Init(state, &lspDiagnostic->fixes, sizeof(SZrLspDiagnosticFix), capacity);
    lspDiagnostic->descriptorId = diagnostic->descriptorId;
    if (!diagnostic->fixes.isValid) {
        return;
    }

    for (TZrSize index = 0; index < diagnostic->fixes.length; index++) {
        const SZrDiagnosticFix *fix =
            (const SZrDiagnosticFix *)ZrCore_Array_Get((SZrArray *)&diagnostic->fixes, index);
        SZrLspDiagnosticFix lspFix;

        if (fix == ZR_NULL) {
            continue;
        }

        memset(&lspFix, 0, sizeof(lspFix));
        lspFix.title = fix->title;
        lspFix.editRange = ZrLanguageServer_Lsp_RangeFromFileRangeForDocument(context, uri, fix->editRange);
        lspFix.editText = fix->editText;
        lspFix.applicability = fix->applicability;
        ZrCore_Array_Push(state, &lspDiagnostic->fixes, &lspFix);
    }
}
