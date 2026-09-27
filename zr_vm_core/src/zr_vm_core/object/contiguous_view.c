#include "zr_vm_core/contiguous_view.h"

#include <stdint.h>
#include <string.h>

/* 统一填充可选诊断，让创建、切片和索引失败共用同一结果约定。 */
static void view_diag(SZrViewDiagnostic *d, EZrViewDiagnosticCode code,
                      TZrUInt64 expected, TZrUInt64 actual) {
    if (d != ZR_NULL) { d->code = code; d->expected = expected; d->actual = actual; }
}

/* 供复用描述符显式回到无效状态；不触碰借用的 owner。 */
void ZrCore_View_Init(SZrContiguousView *view) {
    if (view != ZR_NULL) memset(view, 0, sizeof(*view));
}

/* 偏移计算的前置保护，失败不写输出，避免溢出后再作边界判断。 */
static TZrBool checked_mul(TZrSize a, TZrSize b, TZrSize *out) {
    if (out == ZR_NULL || (b != 0u && a > SIZE_MAX / b)) return ZR_FALSE;
    *out = a * b; return ZR_TRUE;
}

/* 与乘法保护配套，防止视图终点、切片起点及索引偏移回绕。 */
static TZrBool checked_add(TZrSize a, TZrSize b, TZrSize *out) {
    if (out == ZR_NULL || b > SIZE_MAX - a) return ZR_FALSE;
    *out = a + b; return ZR_TRUE;
}

/* 只验证描述符内部一致性与调用方提供的 generation；不读取 owner 的真实存储。 */
TZrBool ZrCore_View_Validate(const SZrContiguousView *view,
                             TZrUInt64 currentGeneration,
                             SZrViewDiagnostic *diagnostic) {
    TZrSize span;
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
    /* TODO: 接入真实容器/FFI 前需确认谁校验 GC root、布局哈希和只读能力；当前只检查描述符字段。 */
    if (view == ZR_NULL || view->stride == 0u || view->elementSize == 0u ||
        (view->flags & ~(ZR_VIEW_FLAG_READ_ONLY | ZR_VIEW_FLAG_PINNED | ZR_VIEW_FLAG_INLINE_STORAGE)) != 0u ||
        (view->length != 0u && view->ownerRoot == ZR_NULL) ||
        (view->length != 0u &&
         (!checked_mul(view->length - 1u, view->stride, &span) ||
          !checked_add(view->byteOffset, span, &span) ||
          !checked_add(span, view->elementSize, &span)))) {
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

/* 在局部候选上验证后才发布 view，失败保留调用方的旧描述符。 */
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

/* 子视图继承 owner、布局和借用元数据，仅改变窗口；直接供 SSA 数组切片契约测试。 */
TZrBool ZrCore_View_Slice(const SZrContiguousView *view, TZrSize start,
                           TZrSize length, SZrContiguousView *slice,
                           SZrViewDiagnostic *diagnostic) {
    TZrSize delta, offset;
    SZrContiguousView candidate;
    if (!ZrCore_View_Validate(view, 0u, diagnostic)) return ZR_FALSE;
    if (slice == ZR_NULL) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_INVALID, 0u, 0u);
        return ZR_FALSE;
    }
    if (start > view->length || length > view->length - start) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_BOUNDS, view->length, start);
        return ZR_FALSE;
    }
    if (!checked_mul(start, view->stride, &delta) ||
        !checked_add(view->byteOffset, delta, &offset)) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_OVERFLOW, 0u, 0u);
        return ZR_FALSE;
    }
    candidate = *view;
    candidate.byteOffset = offset;
    candidate.length = length;
    if (!ZrCore_View_Validate(&candidate, 0u, diagnostic)) return ZR_FALSE;
    *slice = candidate;
    return ZR_TRUE;
}

/* Exec IR 适配器把下标交给此处作有符号边界和字节偏移检查；结果不包含裸元素指针。 */
TZrBool ZrCore_View_IndexOffset(const SZrContiguousView *view, TZrInt64 index,
                               TZrSize *offset, SZrViewDiagnostic *diagnostic) {
    TZrSize delta;
    /* TODO: 当前适配器未传真实存储代数；接入可变容器前需核对代数/借用检查的调用位置。 */
    if (!ZrCore_View_Validate(view, 0u, diagnostic)) return ZR_FALSE;
    if (offset == ZR_NULL) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_INVALID, 0u, 0u);
        return ZR_FALSE;
    }
    if (index < 0 || (TZrUInt64)index >= (TZrUInt64)view->length) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_BOUNDS,
                  view->length, (TZrUInt64)(index < 0 ? 0 : index));
        return ZR_FALSE;
    }
    if (!checked_mul((TZrSize)index, view->stride, &delta) ||
        !checked_add(view->byteOffset, delta, offset)) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_OVERFLOW, 0u, 0u);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
