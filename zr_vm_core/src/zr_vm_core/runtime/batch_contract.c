#include "zr_vm_core/batch_contract.h"

#include <stdint.h>
#include <string.h>

/* 将可选诊断的四个字段作为同一次失败结果写入；空指针仅省略报告。 */
static void batch_diag(SZrBatchDiagnostic *d, EZrBatchDiagnosticCode code,
                       TZrUInt32 index, TZrUInt64 expected, TZrUInt64 actual) {
    if (d != ZR_NULL) {
        d->code = code; d->index = index; d->expected = expected; d->actual = actual;
    }
}

void ZrCore_BatchDiagnostic_Clear(SZrBatchDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) memset(diagnostic, 0, sizeof(*diagnostic));
}

const TZrChar *ZrCore_BatchDiagnostic_Name(EZrBatchDiagnosticCode code) {
    switch (code) {
        case ZR_BATCH_DIAGNOSTIC_NONE: return "none";
        case ZR_BATCH_DIAGNOSTIC_INVALID_ARGUMENT: return "invalid-argument";
        case ZR_BATCH_DIAGNOSTIC_OVERFLOW: return "overflow";
        case ZR_BATCH_DIAGNOSTIC_BOUNDS: return "bounds";
        case ZR_BATCH_DIAGNOSTIC_STRIDE: return "stride";
        case ZR_BATCH_DIAGNOSTIC_SHAPE_MISMATCH: return "shape-mismatch";
        case ZR_BATCH_DIAGNOSTIC_ALIAS_UNSAFE: return "alias-unsafe";
        case ZR_BATCH_DIAGNOSTIC_CALLBACK_ERROR: return "callback-error";
        case ZR_BATCH_DIAGNOSTIC_EFFECT_UNSAFE: return "effect-unsafe";
        case ZR_BATCH_DIAGNOSTIC_UNSUPPORTED: return "unsupported";
        default: return "unknown";
    }
}

TZrBool ZrCore_BatchShape_ElementCount(SZrBatchShape shape, TZrSize *count,
                                       SZrBatchDiagnostic *diagnostic) {
    ZrCore_BatchDiagnostic_Clear(diagnostic);
    if (count == ZR_NULL) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_INVALID_ARGUMENT, 0u, 0u, 0u);
        return ZR_FALSE;
    }
    if (shape.rows != 0u && shape.columns > SIZE_MAX / shape.rows) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_OVERFLOW, 0u, SIZE_MAX,
                   (TZrUInt64)shape.columns);
        return ZR_FALSE;
    }
    *count = shape.rows * shape.columns;
    return ZR_TRUE;
}

TZrBool ZrCore_BatchBuffer_Validate(const SZrBatchBuffer *buffer, TZrSize count,
                                    SZrBatchDiagnostic *diagnostic) {
    TZrMemoryOffset last;
    TZrUInt64 magnitude;
    ZrCore_BatchDiagnostic_Clear(diagnostic);
    if (buffer == ZR_NULL || buffer->elementSize == 0u ||
        (count != 0u && buffer->data == ZR_NULL)) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_INVALID_ARGUMENT, 0u, 1u, 0u);
        return ZR_FALSE;
    }
    if (buffer->stride == 0) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_STRIDE, 0u, 1u,
                   (TZrUInt64)buffer->stride);
        return ZR_FALSE;
    }
    if (count == 0u) return ZR_TRUE;
    /* 单个元素不使用 stride。两个元素乘 INT64_MIN 仍可表示；更多元素才会溢出。
     * 32 位 ptrdiff_t 仍按 64 位界限检查，可能放过本机放不下的乘法。 */
    if (count > 1u &&
        (buffer->stride == INT64_MIN
             ? count > 2u
             : ((buffer->stride > 0 && (TZrUInt64)(count - 1u) >
                     (TZrUInt64)INT64_MAX / (TZrUInt64)buffer->stride) ||
                (buffer->stride < 0 && (TZrUInt64)(count - 1u) >
                     ((TZrUInt64)INT64_MAX + 1u) /
                         (TZrUInt64)(-(buffer->stride + 1) + 1))))) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_OVERFLOW, 0u, SIZE_MAX,
                   (TZrUInt64)count);
        return ZR_FALSE;
    }
    last = (TZrMemoryOffset)(count - 1u) * buffer->stride;
    /* 有符号最小值不能取负。先加一再取负，用无符号把少掉的 1 加回来。 */
    if (last < 0) {
        magnitude = (TZrUInt64)(-(last + 1)) + 1u;
    } else {
        magnitude = (TZrUInt64)last;
    }
    if (magnitude > (TZrUInt64)buffer->byteLength ||
        buffer->elementSize > buffer->byteLength - (TZrSize)magnitude) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_BOUNDS, 0u,
                   buffer->byteLength, magnitude + buffer->elementSize);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_BatchBuffer_Offset(const SZrBatchBuffer *buffer, TZrSize count,
                                  TZrSize index, TZrMemoryOffset *offset,
                                  SZrBatchDiagnostic *diagnostic) {
    ZrCore_BatchDiagnostic_Clear(diagnostic);
    if (!ZrCore_BatchBuffer_Validate(buffer, count, diagnostic)) return ZR_FALSE;
    if (offset == ZR_NULL || index >= count) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_BOUNDS, (TZrUInt32)index,
                   count, index);
        return ZR_FALSE;
    }
    *offset = (TZrMemoryOffset)index * buffer->stride;
    return ZR_TRUE;
}

/* 逻辑元素占用 [最低地址, 最高地址 + elementSize)。负 stride 的末端在 data 之前。 */
static TZrBool batch_element_address_range(const SZrBatchBuffer *buffer, TZrSize count,
                                           TZrUInt64 *rangeStart, TZrUInt64 *rangeEnd,
                                           SZrBatchDiagnostic *diagnostic) {
    uintptr_t base = (uintptr_t)buffer->data;
    TZrMemoryOffset lastOffset = 0;
    uintptr_t lastAddress;
    uintptr_t low;
    uintptr_t high;

    if (count > 1u) {
        lastOffset = (TZrMemoryOffset)(count - 1u) * buffer->stride;
    }
    if (lastOffset >= 0) {
        if ((uintptr_t)lastOffset > UINTPTR_MAX - base) {
            batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_OVERFLOW, 0u, UINTPTR_MAX, 0u);
            return ZR_FALSE;
        }
        lastAddress = base + (uintptr_t)lastOffset;
    } else {
        TZrUInt64 back = (TZrUInt64)(-(lastOffset + 1)) + 1u;
        if (back > (TZrUInt64)base) {
            batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_OVERFLOW, 0u, UINTPTR_MAX, back);
            return ZR_FALSE;
        }
        lastAddress = base - (uintptr_t)back;
    }

    low = base < lastAddress ? base : lastAddress;
    high = base > lastAddress ? base : lastAddress;
    if ((uintptr_t)buffer->elementSize > UINTPTR_MAX - high) {
        batch_diag(diagnostic, ZR_BATCH_DIAGNOSTIC_OVERFLOW, 0u, UINTPTR_MAX,
                   (TZrUInt64)buffer->elementSize);
        return ZR_FALSE;
    }
    *rangeStart = (TZrUInt64)low;
    *rangeEnd = (TZrUInt64)high + (TZrUInt64)buffer->elementSize;
    return ZR_TRUE;
}

TZrBool ZrCore_BatchBuffers_MayAlias(const SZrBatchBuffer *left,
                                     TZrSize leftCount,
                                     const SZrBatchBuffer *right,
                                     TZrSize rightCount, TZrBool *mayAlias,
                                     SZrBatchDiagnostic *diagnostic) {
    TZrUInt64 leftStart, rightStart, leftEnd, rightEnd;
    ZrCore_BatchDiagnostic_Clear(diagnostic);
    /* TODO: mayAlias 为空时直接失败但诊断仍为 NONE；若将此视为无效实参，
     * 需与其他校验入口的 INVALID_ARGUMENT 报告契约保持一致。 */
    if (mayAlias == ZR_NULL || !ZrCore_BatchBuffer_Validate(left, leftCount, diagnostic) ||
        !ZrCore_BatchBuffer_Validate(right, rightCount, diagnostic)) return ZR_FALSE;
    *mayAlias = ZR_FALSE;
    if (leftCount == 0u || rightCount == 0u) return ZR_TRUE;
    if (!batch_element_address_range(left, leftCount, &leftStart, &leftEnd, diagnostic) ||
        !batch_element_address_range(right, rightCount, &rightStart, &rightEnd, diagnostic)) {
        return ZR_FALSE;
    }
    *mayAlias = (TZrBool)(leftStart < rightEnd && rightStart < leftEnd);
    return ZR_TRUE;
}

TZrBool ZrCore_BatchBuffers_ExactLayout(const SZrBatchBuffer *left,
                                        const SZrBatchBuffer *right) {
    return (TZrBool)(left != ZR_NULL && right != ZR_NULL &&
                     left->data == right->data && left->byteLength == right->byteLength &&
                     left->stride == right->stride && left->elementSize == right->elementSize);
}
