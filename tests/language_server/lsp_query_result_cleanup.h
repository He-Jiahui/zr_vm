#ifndef ZR_TESTS_LSP_QUERY_RESULT_CLEANUP_H
#define ZR_TESTS_LSP_QUERY_RESULT_CLEANUP_H

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_language_server.h"

static void free_local_reference_projection_results(
        SZrState *state,
        SZrArray *locations,
        SZrArray *highlights) {
    TZrSize index;

    if (state == ZR_NULL) {
        return;
    }
    if (locations != ZR_NULL && locations->isValid) {
        for (index = 0U; index < locations->length; index++) {
            SZrLspLocation **slot =
                    (SZrLspLocation **)ZrCore_Array_Get(locations, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global, *slot, sizeof(SZrLspLocation));
            }
        }
        ZrCore_Array_Free(state, locations);
    }
    if (highlights != ZR_NULL && highlights->isValid) {
        for (index = 0U; index < highlights->length; index++) {
            SZrLspDocumentHighlight **slot =
                    (SZrLspDocumentHighlight **)ZrCore_Array_Get(
                            highlights, index);
            if (slot != ZR_NULL && *slot != ZR_NULL) {
                ZrCore_Memory_RawFree(
                        state->global,
                        *slot,
                        sizeof(SZrLspDocumentHighlight));
            }
        }
        ZrCore_Array_Free(state, highlights);
    }
}

#endif
