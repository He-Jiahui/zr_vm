#include "zr_vm_language_server_stdio_internal.h"

void free_locations_array(SZrState *state, SZrArray *locations) {
    TZrSize index;

    if (state == ZR_NULL || locations == ZR_NULL) {
        return;
    }

    for (index = 0; index < locations->length; index++) {
        SZrLspLocation **locationPtr = (SZrLspLocation **)ZrCore_Array_Get(locations, index);
        if (locationPtr != ZR_NULL && *locationPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *locationPtr, sizeof(SZrLspLocation));
        }
    }
    ZrCore_Array_Free(state, locations);
}

void free_symbols_array(SZrState *state, SZrArray *symbols) {
    TZrSize index;

    if (state == ZR_NULL || symbols == ZR_NULL) {
        return;
    }

    for (index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr =
            (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
        }
    }
    ZrCore_Array_Free(state, symbols);
}

void free_diagnostics_array(SZrState *state, SZrArray *diagnostics) {
    TZrSize index;

    if (state == ZR_NULL || diagnostics == ZR_NULL) {
        return;
    }

    for (index = 0; index < diagnostics->length; index++) {
        SZrLspDiagnostic **diagnosticPtr = (SZrLspDiagnostic **)ZrCore_Array_Get(diagnostics, index);
        if (diagnosticPtr != ZR_NULL && *diagnosticPtr != ZR_NULL) {
            ZrCore_Array_Free(state, &(*diagnosticPtr)->relatedInformation);
            ZrCore_Memory_RawFree(state->global, *diagnosticPtr, sizeof(SZrLspDiagnostic));
        }
    }
    ZrCore_Array_Free(state, diagnostics);
}

void free_completion_items_array(SZrState *state, SZrArray *items) {
    TZrSize index;

    if (state == ZR_NULL || items == ZR_NULL) {
        return;
    }

    for (index = 0; index < items->length; index++) {
        SZrLspCompletionItem **itemPtr = (SZrLspCompletionItem **)ZrCore_Array_Get(items, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *itemPtr, sizeof(SZrLspCompletionItem));
        }
    }
    ZrCore_Array_Free(state, items);
}

void free_inlay_hints_array(SZrState *state, SZrArray *hints) {
    TZrSize index;

    if (state == ZR_NULL || hints == ZR_NULL) {
        return;
    }

    for (index = 0; index < hints->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(hints, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *hintPtr, sizeof(SZrLspInlayHint));
        }
    }

    ZrCore_Array_Free(state, hints);
}

void free_highlights_array(SZrState *state, SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL || highlights == ZR_NULL) {
        return;
    }

    for (index = 0; index < highlights->length; index++) {
        SZrLspDocumentHighlight **highlightPtr =
            (SZrLspDocumentHighlight **)ZrCore_Array_Get(highlights, index);
        if (highlightPtr != ZR_NULL && *highlightPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *highlightPtr, sizeof(SZrLspDocumentHighlight));
        }
    }
    ZrCore_Array_Free(state, highlights);
}

void free_hover(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }

    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(SZrLspHover));
}

void free_rich_hover(SZrState *state, SZrLspRichHover *hover) {
    ZrLanguageServer_Lsp_FreeRichHover(state, hover);
}

void free_signature_help(SZrState *state, SZrLspSignatureHelp *help) {
    ZrLanguageServer_LspSignatureHelp_Free(state, help);
}
