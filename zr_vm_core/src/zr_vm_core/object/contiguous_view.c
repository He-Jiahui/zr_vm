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
        (view->length != 0u && view->ownerRoot == ZR_NULL)) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_INVALID, 0u, 0u); return ZR_FALSE;
    }
    if (view->length != 0u &&
        (!checked_mul(view->length - 1u, view->stride, &span) ||
         !checked_add(view->byteOffset, span, &span) ||
         !checked_add(span, view->elementSize, &span))) {
        view_diag(diagnostic, ZR_VIEW_DIAGNOSTIC_OVERFLOW, 0u, 0u);
        return ZR_FALSE;
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

/**
 * @fn void view_diag(SZrViewDiagnostic *d, EZrViewDiagnosticCode code, TZrUInt64 expected, TZrUInt64 actual)
 * ## Summary
 * 将当前校验分支的失败类别和值上下文写入可选诊断；供本文件的公开 API 共用，避免
 * 每个分支采用不同的字段更新方式。
 *
 * ## Parameters
 * @param[out] d 可选诊断槽；为空时丢弃本次信息。
 * @param[in] code 本次失败分类。
 * @param[in] expected 与该分类对应的参考值。
 * @param[in] actual 与该分类对应的实际值。
 *
 * ## Returns
 * 无返回值；d 非空时同步覆盖其三个字段，d 为空时不产生副作用；本 helper 不分配资源。
 *
 * ## Constraints and lifecycle
 * 调用方负责使 d 在同步写入期间可写；helper 只借用该输出地址且不保存，不解释 code 对
 * expected/actual 的具体语义，也不清理调用方持有的资源。
 *
 * ## References
 * 本 helper 由 Validate、Create、Slice 和 IndexOffset 调用；
 * Validate 的字段、末端、代数和 pin 分类位于本文件 `:39`、`:45`、`:49`、
 * `:52`；Create 的空输入、Slice 的空输出/越界/溢出以及 IndexOffset 的空输出/越界/溢出
 * 分别位于 `:62`、`:80`、`:84`、`:89`、`:107`、`:111`、`:117`。
 */

/**
 * @fn TZrBool checked_mul(TZrSize a, TZrSize b, TZrSize *out)
 * ## Summary
 * 在写入偏移输出前验证两个 `TZrSize` 因子的乘积可表示，供 view 末端、slice 起点和
 * 元素索引计算共用。
 *
 * ## Parameters
 * @param[in] a 第一个非负 size 因子。
 * @param[in] b 第二个非负 size 因子。
 * @param[out] out 可为空的乘积输出槽；为空时返回 `ZR_FALSE`。
 *
 * ## Returns
 * @return out 非空且乘积不超过 SIZE_MAX 时返回 `ZR_TRUE` 并写入结果；否则返回
 * `ZR_FALSE`，不写 out。
 *
 * ## Constraints and lifecycle
 * 本 helper 只判断无符号整数表示范围，不证明乘积对应真实内存或有效 backing 区间；
 * 调用方仍须执行上层边界校验。
 *
 * ## References
 * 由 `ZrCore_View_Validate`、`ZrCore_View_Slice` 和 `ZrCore_View_IndexOffset` 调用；
 * 当前乘法检查点见本文件 `:42`、`:87`、`:115`。
 */

/**
 * @fn TZrBool checked_add(TZrSize a, TZrSize b, TZrSize *out)
 * ## Summary
 * 在写入偏移输出前验证两个 `TZrSize` 加数的和不会回绕，供末元素末端、切片起点和
 * owner 相对元素偏移计算共用。
 *
 * ## Parameters
 * @param[in] a 第一个非负 size 加数。
 * @param[in] b 第二个非负 size 加数。
 * @param[out] out 可为空的和输出槽；为空时返回 `ZR_FALSE`。
 *
 * ## Returns
 * @return out 非空且 `a + b` 可由 SIZE_MAX 表示时返回 `ZR_TRUE` 并写入结果；否则返回
 * `ZR_FALSE`，不写 out。
 *
 * ## Constraints and lifecycle
 * 本 helper 只保证算术可表示，不检查 owner、真实 backing 容量、连续性或生命周期；
 * 调用方须结合相应 descriptor 边界判断解释结果。
 *
 * ## References
 * 由 `ZrCore_View_Validate`、`ZrCore_View_Slice` 和 `ZrCore_View_IndexOffset` 调用；
 * 当前加法检查点见本文件 `:43`、`:44`、`:88`、`:116`。
 */
