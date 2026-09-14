#include "zr_vm_core/contiguous_view.h"

#include <stdint.h>
#include <string.h>

static void view_diag(SZrViewDiagnostic *d, EZrViewDiagnosticCode code,
                      TZrUInt64 expected, TZrUInt64 actual) {
    if (d != ZR_NULL) { d->code = code; d->expected = expected; d->actual = actual; }
}

void ZrCore_View_Init(SZrContiguousView *view) {
    if (view != ZR_NULL) memset(view, 0, sizeof(*view));
}

static TZrBool checked_mul(TZrSize a, TZrSize b, TZrSize *out) {
    if (out == ZR_NULL || (b != 0u && a > SIZE_MAX / b)) return ZR_FALSE;
    *out = a * b; return ZR_TRUE;
}

static TZrBool checked_add(TZrSize a, TZrSize b, TZrSize *out) {
    if (out == ZR_NULL || b > SIZE_MAX - a) return ZR_FALSE;
    *out = a + b; return ZR_TRUE;
}

TZrBool ZrCore_View_Validate(const SZrContiguousView *view,
                             TZrUInt64 currentGeneration,
                             SZrViewDiagnostic *diagnostic) {
    TZrSize span;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    if (view == ZR_NULL || view->stride == 0u || view->elementSize == 0u ||
        (view->flags & ~(ZR_VIEW_FLAG_READ_ONLY | ZR_VIEW_FLAG_PINNED | ZR_VIEW_FLAG_INLINE_STORAGE)) != 0u ||
        (view->length != 0u && view->ownerRoot == ZR_NULL) ||
        !checked_mul(view->length - (view->length != 0u ? 1u : 0u), view->stride, &span) ||
        !checked_add(view->byteOffset, span, &span) ||
        !checked_add(span, view->elementSize, &span)) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_INVALID, 0u, 0u); return ZR_FALSE;
    }
    if (currentGeneration != 0u && view->storageGeneration != currentGeneration) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_GENERATION, currentGeneration, view->storageGeneration); return ZR_FALSE;
    }
    if ((view->flags & ZR_VIEW_FLAG_PINNED) != 0u && view->lifetimeRegion == 0u) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_LIFETIME, 1u, 0u); return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_View_Create(const SZrContiguousViewRequest *request,
                           SZrContiguousView *view,
                           SZrViewDiagnostic *diagnostic) {
    SZrContiguousView candidate;
    if (request == ZR_NULL || view == ZR_NULL) { view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_INVALID, 0u, 0u); return ZR_FALSE; }
    candidate.ownerRoot = request->ownerRoot; candidate.byteOffset = request->byteOffset;
    candidate.length = request->length; candidate.stride = request->stride;
    candidate.elementSize = request->elementSize; candidate.elementLayoutHash = request->elementLayoutHash;
    candidate.storageGeneration = request->storageGeneration; candidate.lifetimeRegion = request->lifetimeRegion;
    candidate.flags = request->flags;
    if (!ZrCore_View_Validate(&candidate, 0u, diagnostic)) return ZR_FALSE;
    *view = candidate; return ZR_TRUE;
}

TZrBool ZrCore_View_Slice(const SZrContiguousView *view, TZrSize start,
                          TZrSize length, SZrContiguousView *slice,
                          SZrViewDiagnostic *diagnostic) {
    TZrSize delta, offset;
    if (!ZrCore_View_Validate(view, 0u, diagnostic)) return ZR_FALSE;
    if (slice == ZR_NULL || start > view->length || length > view->length - start ||
        !checked_mul(start, view->stride, &delta) || !checked_add(view->byteOffset, delta, &offset)) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_BOUNDS, view != ZR_NULL ? view->length : 0u, start); return ZR_FALSE;
    }
    *slice = *view; slice->byteOffset = offset; slice->length = length; return ZR_TRUE;
}

TZrBool ZrCore_View_IndexOffset(const SZrContiguousView *view, TZrInt64 index,
                               TZrSize *offset, SZrViewDiagnostic *diagnostic) {
    TZrSize delta;
    if (!ZrCore_View_Validate(view, 0u, diagnostic)) return ZR_FALSE;
    if (offset == ZR_NULL || index < 0 ||
        (TZrUInt64)index >= (TZrUInt64)view->length ||
        !checked_mul((TZrSize)index, view->stride, &delta) || !checked_add(view->byteOffset, delta, offset)) {
        view_diag(diagnostic, index < 0 ? ZR_VIEW_DIAGNOSTIC_BOUNDS : ZR_VIEW_DIAGNOSTIC_OVERFLOW,
                  view != ZR_NULL ? view->length : 0u, (TZrUInt64)(index < 0 ? 0 : index)); return ZR_FALSE;
    }
    return ZR_TRUE;
}
