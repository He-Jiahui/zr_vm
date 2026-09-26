//
// Tensor native callbacks.
//

#include "zr_vm_lib_math/tensor.h"

static SZrObject *zr_math_tensor_copy_array(SZrState *state, SZrObject *array) {
    /* 只复制外层数组：构造、reshape 和 toArray 的调用方不会共享可变数组容器。 */
    /* TODO: PushValue 结果未检查；需用分配失败注入验证副本是否可能静默截短。 */
    SZrObject *copy = ZrLib_Array_New(state); TZrSize i;
    for (i = 0; copy != ZR_NULL && i < ZrLib_Array_Length(array); i++) ZrLib_Array_PushValue(state, copy, ZrLib_Array_Get(state, array, i));
    return copy;
}

TZrBool ZrMath_Tensor_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 当前公开入口要求 shape 与 data 两个数组；descriptor 的签名文字另需同步。 */
    SZrObject *shape = ZR_NULL;
    SZrObject *data = ZR_NULL;
    SZrObject *tensor;
    SZrObject *shapeCopy;
    SZrObject *dataCopy;
    if (!ZrLib_CallContext_ReadArray(context, 0, &shape) || !ZrLib_CallContext_ReadArray(context, 1, &data)) return ZR_FALSE;
    tensor = ZrMath_ResolveConstructTarget(context);
    shapeCopy = zr_math_tensor_copy_array(context->state, shape);
    dataCopy = zr_math_tensor_copy_array(context->state, data);
    if (tensor == ZR_NULL || shapeCopy == ZR_NULL || dataCopy == ZR_NULL ||
        !ZrMath_TensorPopulate(context->state, tensor, shapeCopy, dataCopy)) return ZR_FALSE;
    return ZrMath_FinishConstructObject(context, result, tensor);
}

TZrBool ZrMath_Tensor_Clone(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; SZrObject *tensor;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage)) return ZR_FALSE;
    tensor = ZrMath_TensorMake(context->state, zr_math_tensor_copy_array(context->state, storage.shape), zr_math_tensor_copy_array(context->state, storage.data));
    if (tensor == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, tensor, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Reshape(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 总元素数保持不变，但两份数组均复制；调用方拿到独立的 Tensor 实例。 */
    ZrMathTensorStorage storage; SZrObject *shape = ZR_NULL; SZrObject *tensor;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || !ZrLib_CallContext_ReadArray(context, 0, &shape)) return ZR_FALSE;
    if (ZrMath_TensorTotalSize(context->state, shape) != storage.size) return ZR_FALSE;
    tensor = ZrMath_TensorMake(context->state, zr_math_tensor_copy_array(context->state, shape), zr_math_tensor_copy_array(context->state, storage.data));
    if (tensor == ZR_NULL) return ZR_FALSE;
    ZrLib_Value_SetObject(context->state, result, tensor, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Fill(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; TZrFloat64 value = 0.0; TZrSize i;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || !ZrLib_CallContext_ReadFloat(context, 0, &value)) return ZR_FALSE;
    for (i = 0; i < ZrLib_Array_Length(storage.data); i++) { SZrTypeValue v; ZrLib_Value_SetFloat(context->state, &v, value); ZrMath_ArraySetValue(context->state, storage.data, i, &v); }
    ZrLib_Value_SetObject(context->state, result, ZrMath_SelfObject(context), ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Get(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; SZrObject *indices = ZR_NULL; TZrSize offset = 0; const SZrTypeValue *value; TZrFloat64 number = 0.0;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || !ZrLib_CallContext_ReadArray(context, 0, &indices) ||
        !ZrMath_TensorComputeOffset(context->state, storage.shape, indices, &offset)) return ZR_FALSE;
    value = ZrLib_Array_Get(context->state, storage.data, offset);
    if (!ZrMath_NumberFromValue(value, &number)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, result, number);
    return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Set(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; SZrObject *indices = ZR_NULL; TZrSize offset = 0; TZrFloat64 value = 0.0; SZrTypeValue slot;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || !ZrLib_CallContext_ReadArray(context, 0, &indices) ||
        !ZrMath_TensorComputeOffset(context->state, storage.shape, indices, &offset) || !ZrLib_CallContext_ReadFloat(context, 1, &value)) return ZR_FALSE;
    ZrLib_Value_SetFloat(context->state, &slot, value); ZrMath_ArraySetValue(context->state, storage.data, offset, &slot);
    ZrLib_Value_SetObject(context->state, result, ZrMath_SelfObject(context), ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Sum(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 聚合依赖构造时 data 全部可转为数值；当前构造器并未保证这一点，见 Populate 的 BUG。 */
    ZrMathTensorStorage storage; TZrSize i; TZrFloat64 sum = 0.0; TZrFloat64 value;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage)) return ZR_FALSE;
    for (i = 0; i < ZrLib_Array_Length(storage.data); i++) if (ZrMath_ArrayReadFloat(context->state, storage.data, i, &value)) sum += value;
    ZrLib_Value_SetFloat(context->state, result, sum); return ZR_TRUE;
}
TZrBool ZrMath_Tensor_Mean(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; TZrSize i; TZrFloat64 sum = 0.0; TZrFloat64 value;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || storage.size <= 0) return ZR_FALSE;
    for (i = 0; i < ZrLib_Array_Length(storage.data); i++) if (ZrMath_ArrayReadFloat(context->state, storage.data, i, &value)) sum += value;
    ZrLib_Value_SetFloat(context->state, result, sum / (TZrFloat64)storage.size); return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Transpose2D(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 将 row-major 的二维坐标交换后写入新数组，供脚本保留原 Tensor。 */
    ZrMathTensorStorage storage; TZrInt64 rows = 0, cols = 0, r, c; SZrObject *shape; SZrObject *data; SZrObject *tensor;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || storage.rank != 2 ||
        !ZrMath_ArrayReadInt(context->state, storage.shape, 0, &rows) || !ZrMath_ArrayReadInt(context->state, storage.shape, 1, &cols)) return ZR_FALSE;
    shape = ZrLib_Array_New(context->state); data = ZrLib_Array_New(context->state); if (shape == ZR_NULL || data == ZR_NULL) return ZR_FALSE;
    { SZrTypeValue v; ZrLib_Value_SetInt(context->state, &v, cols); ZrLib_Array_PushValue(context->state, shape, &v); ZrLib_Value_SetInt(context->state, &v, rows); ZrLib_Array_PushValue(context->state, shape, &v); }
    for (c = 0; c < cols; c++) for (r = 0; r < rows; r++) { TZrFloat64 value = 0.0; ZrMath_ArrayReadFloat(context->state, storage.data, (TZrSize)(r * cols + c), &value); { SZrTypeValue v; ZrLib_Value_SetFloat(context->state, &v, value); ZrLib_Array_PushValue(context->state, data, &v); } }
    tensor = ZrMath_TensorMake(context->state, shape, data); if (tensor == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, tensor, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}

TZrBool ZrMath_Tensor_Matmul(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 仅支持严格二维的矩阵乘积；不做广播，输出形状为左行数 × 右列数。 */
    ZrMathTensorStorage lhs; ZrMathTensorStorage rhs; SZrObject *other = ZR_NULL; TZrInt64 lr = 0, lc = 0, rr = 0, rc = 0, r, c, k; SZrObject *shape; SZrObject *data; SZrObject *tensor;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_TensorGetStorage(context->state, other, &rhs) || lhs.rank != 2 || rhs.rank != 2 ||
        !ZrMath_ArrayReadInt(context->state, lhs.shape, 0, &lr) || !ZrMath_ArrayReadInt(context->state, lhs.shape, 1, &lc) ||
        !ZrMath_ArrayReadInt(context->state, rhs.shape, 0, &rr) || !ZrMath_ArrayReadInt(context->state, rhs.shape, 1, &rc) || lc != rr) return ZR_FALSE;
    shape = ZrLib_Array_New(context->state); data = ZrLib_Array_New(context->state); if (shape == ZR_NULL || data == ZR_NULL) return ZR_FALSE;
    { SZrTypeValue v; ZrLib_Value_SetInt(context->state, &v, lr); ZrLib_Array_PushValue(context->state, shape, &v); ZrLib_Value_SetInt(context->state, &v, rc); ZrLib_Array_PushValue(context->state, shape, &v); }
    for (r = 0; r < lr; r++) for (c = 0; c < rc; c++) { TZrFloat64 sum = 0.0; for (k = 0; k < lc; k++) { TZrFloat64 a = 0.0, b = 0.0; ZrMath_ArrayReadFloat(context->state, lhs.data, (TZrSize)(r * lc + k), &a); ZrMath_ArrayReadFloat(context->state, rhs.data, (TZrSize)(k * rc + c), &b); sum += a * b; } { SZrTypeValue v; ZrLib_Value_SetFloat(context->state, &v, sum); ZrLib_Array_PushValue(context->state, data, &v); } }
    tensor = ZrMath_TensorMake(context->state, shape, data); if (tensor == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, tensor, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}

static TZrBool zr_math_tensor_binary(ZrLibCallContext *context, SZrTypeValue *result, TZrFloat64 sign) {
    /* add/sub 共用逐项运算，只接受逐维相等的形状；结果持有 shape 的副本。 */
    ZrMathTensorStorage lhs; ZrMathTensorStorage rhs; SZrObject *other = ZR_NULL; SZrObject *tensor; SZrObject *data; TZrSize i;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &lhs) || !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_TensorGetStorage(context->state, other, &rhs) || !ZrMath_TensorShapeEquals(context->state, lhs.shape, rhs.shape)) return ZR_FALSE;
    data = ZrLib_Array_New(context->state); if (data == ZR_NULL) return ZR_FALSE;
    for (i = 0; i < ZrLib_Array_Length(lhs.data); i++) { TZrFloat64 a = 0.0, b = 0.0; SZrTypeValue v; ZrMath_ArrayReadFloat(context->state, lhs.data, i, &a); ZrMath_ArrayReadFloat(context->state, rhs.data, i, &b); ZrLib_Value_SetFloat(context->state, &v, a + sign * b); ZrLib_Array_PushValue(context->state, data, &v); }
    tensor = ZrMath_TensorMake(context->state, zr_math_tensor_copy_array(context->state, lhs.shape), data); if (tensor == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, tensor, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Tensor_Add(ZrLibCallContext *context, SZrTypeValue *result) { return zr_math_tensor_binary(context, result, 1.0); }
TZrBool ZrMath_Tensor_Sub(ZrLibCallContext *context, SZrTypeValue *result) { return zr_math_tensor_binary(context, result, -1.0); }
TZrBool ZrMath_Tensor_MulScalar(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; TZrFloat64 factor = 0.0; SZrObject *data; SZrObject *tensor; TZrSize i;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage) || !ZrLib_CallContext_ReadFloat(context, 0, &factor)) return ZR_FALSE;
    data = ZrLib_Array_New(context->state); if (data == ZR_NULL) return ZR_FALSE;
    for (i = 0; i < ZrLib_Array_Length(storage.data); i++) { TZrFloat64 value = 0.0; SZrTypeValue v; ZrMath_ArrayReadFloat(context->state, storage.data, i, &value); ZrLib_Value_SetFloat(context->state, &v, value * factor); ZrLib_Array_PushValue(context->state, data, &v); }
    tensor = ZrMath_TensorMake(context->state, zr_math_tensor_copy_array(context->state, storage.shape), data); if (tensor == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, tensor, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
TZrBool ZrMath_Tensor_ToArray(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; SZrObject *copy;
    if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage)) return ZR_FALSE;
    copy = zr_math_tensor_copy_array(context->state, storage.data); if (copy == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, copy, ZR_VALUE_TYPE_ARRAY); return ZR_TRUE;
}
TZrBool ZrMath_Tensor_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrMathTensorStorage storage; if (!ZrMath_TensorGetStorage(context->state, ZrMath_SelfObject(context), &storage)) return ZR_FALSE;
    return ZrMath_MakeStringResult(context->state, result, "Tensor(rank=%lu, size=%lld)", (unsigned long)storage.rank, (long long)storage.size);
}
