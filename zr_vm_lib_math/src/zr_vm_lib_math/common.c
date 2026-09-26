//
// Shared helpers for zr.math split implementation.
//

#include "zr_vm_lib_math/math_common.h"

#include <stdio.h>

/* 在编译期选择可用指令集；调用方仍只依赖点积及余数处理这一数值契约。 */
#if defined(__AVX2__)
#define ZR_VM_LIB_MATH_USE_AVX2 1
#include <immintrin.h>
#elif defined(__SSE4_1__) || defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define ZR_VM_LIB_MATH_USE_SSE2 1
#include <immintrin.h>
#elif defined(__ARM_NEON) && defined(__aarch64__)
#define ZR_VM_LIB_MATH_USE_NEON 1
#include <arm_neon.h>
#endif

/* 命名方法和 meta 运算需要创建新对象，字段顺序与各 type descriptor 保持一致。 */
static SZrObject *zr_math_make_object_with_fields(SZrState *state,
                                                  const TZrChar *typeName,
                                                  const TZrChar *const *fieldNames,
                                                  const TZrFloat64 *fieldValues,
                                                  TZrSize fieldCount) {
    SZrObject *object = ZrLib_Type_NewInstance(state, typeName);
    TZrSize index;
    if (object == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < fieldCount; index++) {
        ZrMath_WriteFloatField(state, object, fieldNames[index], fieldValues[index]);
    }
    return object;
}

TZrFloat64 ZrMath_AbsFloat(TZrFloat64 value) {
    return value < 0.0 ? -value : value;
}

TZrBool ZrMath_AlmostEqual(TZrFloat64 lhs, TZrFloat64 rhs, TZrFloat64 epsilon) {
    return ZrMath_AbsFloat(lhs - rhs) <= epsilon;
}

TZrFloat64 ZrMath_Dot(const TZrFloat64 *lhs, const TZrFloat64 *rhs, TZrSize count) {
#if defined(ZR_VM_LIB_MATH_USE_AVX2)
    TZrSize index = 0;
    __m256d sum = _mm256_setzero_pd();
    TZrFloat64 partial[4];
    for (; index + 4 <= count; index += 4) {
        __m256d a = _mm256_loadu_pd(lhs + index);
        __m256d b = _mm256_loadu_pd(rhs + index);
        sum = _mm256_add_pd(sum, _mm256_mul_pd(a, b));
    }
    _mm256_storeu_pd(partial, sum);
    {
        TZrFloat64 result = partial[0] + partial[1] + partial[2] + partial[3];
        for (; index < count; index++) {
            result += lhs[index] * rhs[index];
        }
        return result;
    }
#elif defined(ZR_VM_LIB_MATH_USE_SSE2)
    TZrSize index = 0;
    __m128d sum = _mm_setzero_pd();
    TZrFloat64 partial[2];
    for (; index + 2 <= count; index += 2) {
        __m128d a = _mm_loadu_pd(lhs + index);
        __m128d b = _mm_loadu_pd(rhs + index);
        sum = _mm_add_pd(sum, _mm_mul_pd(a, b));
    }
    _mm_storeu_pd(partial, sum);
    {
        TZrFloat64 result = partial[0] + partial[1];
        for (; index < count; index++) {
            result += lhs[index] * rhs[index];
        }
        return result;
    }
#elif defined(ZR_VM_LIB_MATH_USE_NEON)
    TZrSize index = 0;
    float64x2_t sum = vdupq_n_f64(0.0);
    for (; index + 2 <= count; index += 2) {
        float64x2_t a = vld1q_f64(lhs + index);
        float64x2_t b = vld1q_f64(rhs + index);
        sum = vaddq_f64(sum, vmulq_f64(a, b));
    }
    {
        TZrFloat64 result = vgetq_lane_f64(sum, 0) + vgetq_lane_f64(sum, 1);
        for (; index < count; index++) {
            result += lhs[index] * rhs[index];
        }
        return result;
    }
#else
    TZrFloat64 result = 0.0;
    TZrSize index;
    for (index = 0; index < count; index++) {
        result += lhs[index] * rhs[index];
    }
    return result;
#endif
}

TZrBool ZrMath_NumberFromValue(const SZrTypeValue *value, TZrFloat64 *outValue) {
    /* VM 数值标签统一转换为计算用 double；不接受对象等隐式强制转换。 */
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (value->type) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            *outValue = (TZrFloat64)value->value.nativeObject.nativeInt64;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            *outValue = (TZrFloat64)value->value.nativeObject.nativeUInt64;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            *outValue = value->value.nativeObject.nativeDouble;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrMath_IntFromValue(const SZrTypeValue *value, TZrInt64 *outValue) {
    /* TODO: Tensor shape/index 会走此处；核查非有限、越界浮点和 uint64 大值转换为
     * int64 的平台语义，避免非法维度在转换后参与长度计算。 */
    if (value == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (value->type) {
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            *outValue = value->value.nativeObject.nativeInt64;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            *outValue = (TZrInt64)value->value.nativeObject.nativeDouble;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrMath_ReadFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 *outValue) {
    return ZrMath_NumberFromValue(ZrLib_Object_GetFieldCString(state, object, fieldName), outValue);
}

void ZrMath_WriteFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetFloat(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

void ZrMath_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

void ZrMath_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    /* TODO: 全仓只找到此定义及声明，确认布尔字段 helper 是否仍属当前 math 契约。 */
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

SZrObject *ZrMath_SelfObject(ZrLibCallContext *context) {
    SZrTypeValue *selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || (selfValue->type != ZR_VALUE_TYPE_OBJECT && selfValue->type != ZR_VALUE_TYPE_ARRAY)) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

TZrBool ZrMath_ObjectTypeEquals(SZrState *state, SZrObject *object, const TZrChar *typeName) {
    ZR_UNUSED_PARAMETER(state);
    return object != ZR_NULL && object->prototype != ZR_NULL && object->prototype->name != ZR_NULL &&
           typeName != ZR_NULL && strcmp(ZrCore_String_GetNativeString(object->prototype->name), typeName) == 0;
}

SZrObject *ZrMath_ResolveConstructTarget(ZrLibCallContext *context) {
    SZrObject *self;
    SZrObjectPrototype *ownerPrototype;
    SZrObjectPrototype *targetPrototype;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    self = ZrMath_SelfObject(context);
    ownerPrototype = ZrLib_CallContext_OwnerPrototype(context);
    if (self != ZR_NULL && ownerPrototype != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(self, ownerPrototype)) {
        return self;
    }

    targetPrototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    if (targetPrototype == ZR_NULL) {
        targetPrototype = ownerPrototype;
    }
    return ZrLib_Type_NewInstanceWithPrototype(context->state, targetPrototype);
}

TZrBool ZrMath_FinishConstructObject(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object) {
    if (context == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_ConstructFloatObject(ZrLibCallContext *context,
                                    SZrTypeValue *result,
                                    const TZrChar *const *fieldNames,
                                    const TZrFloat64 *fieldValues,
                                    TZrSize fieldCount) {
    SZrObject *object;
    TZrSize index;

    if (context == ZR_NULL || result == ZR_NULL || fieldNames == ZR_NULL || fieldValues == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZrMath_ResolveConstructTarget(context);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < fieldCount; index++) {
        ZrMath_WriteFloatField(context->state, object, fieldNames[index], fieldValues[index]);
    }

    return ZrMath_FinishConstructObject(context, result, object);
}

SZrObject *ZrMath_MakeVector2(SZrState *state, TZrFloat64 x, TZrFloat64 y) {
    static const TZrChar *const kFields[] = {"x", "y"};
    const TZrFloat64 values[] = {x, y};
    return zr_math_make_object_with_fields(state, "Vector2", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeVector3(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z) {
    static const TZrChar *const kFields[] = {"x", "y", "z"};
    const TZrFloat64 values[] = {x, y, z};
    return zr_math_make_object_with_fields(state, "Vector3", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeVector4(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w) {
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"};
    const TZrFloat64 values[] = {x, y, z, w};
    return zr_math_make_object_with_fields(state, "Vector4", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeQuaternion(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w) {
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"};
    const TZrFloat64 values[] = {x, y, z, w};
    return zr_math_make_object_with_fields(state, "Quaternion", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeComplex(SZrState *state, TZrFloat64 real, TZrFloat64 imag) {
    static const TZrChar *const kFields[] = {"real", "imag"};
    const TZrFloat64 values[] = {real, imag};
    return zr_math_make_object_with_fields(state, "Complex", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeMatrix3x3(SZrState *state, const TZrFloat64 *values) {
    static const TZrChar *const kFields[] = {"m00","m01","m02","m10","m11","m12","m20","m21","m22"};
    return zr_math_make_object_with_fields(state, "Matrix3x3", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeMatrix4x4(SZrState *state, const TZrFloat64 *values) {
    static const TZrChar *const kFields[] = {
            "m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"
    };
    return zr_math_make_object_with_fields(state, "Matrix4x4", kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_ReadVector2Object(SZrState *state, SZrObject *object, ZrMathVector2 *outValue) {
    return outValue != ZR_NULL && ZrMath_ReadFloatField(state, object, "x", &outValue->x) &&
           ZrMath_ReadFloatField(state, object, "y", &outValue->y);
}

TZrBool ZrMath_ReadVector3Object(SZrState *state, SZrObject *object, ZrMathVector3 *outValue) {
    return outValue != ZR_NULL && ZrMath_ReadFloatField(state, object, "x", &outValue->x) &&
           ZrMath_ReadFloatField(state, object, "y", &outValue->y) &&
           ZrMath_ReadFloatField(state, object, "z", &outValue->z);
}

TZrBool ZrMath_ReadVector4Object(SZrState *state, SZrObject *object, ZrMathVector4 *outValue) {
    return outValue != ZR_NULL && ZrMath_ReadFloatField(state, object, "x", &outValue->x) &&
           ZrMath_ReadFloatField(state, object, "y", &outValue->y) &&
           ZrMath_ReadFloatField(state, object, "z", &outValue->z) &&
           ZrMath_ReadFloatField(state, object, "w", &outValue->w);
}

TZrBool ZrMath_ReadQuaternionObject(SZrState *state, SZrObject *object, ZrMathQuaternion *outValue) {
    return outValue != ZR_NULL && ZrMath_ReadFloatField(state, object, "x", &outValue->x) &&
           ZrMath_ReadFloatField(state, object, "y", &outValue->y) &&
           ZrMath_ReadFloatField(state, object, "z", &outValue->z) &&
           ZrMath_ReadFloatField(state, object, "w", &outValue->w);
}

TZrBool ZrMath_ReadComplexObject(SZrState *state, SZrObject *object, ZrMathComplex *outValue) {
    return outValue != ZR_NULL && ZrMath_ReadFloatField(state, object, "real", &outValue->real) &&
           ZrMath_ReadFloatField(state, object, "imag", &outValue->imag);
}

TZrBool ZrMath_ReadMatrix3Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues) {
    static const TZrChar *const kFields[] = {"m00","m01","m02","m10","m11","m12","m20","m21","m22"};
    TZrSize index;
    for (index = 0; outValues != ZR_NULL && index < ZR_ARRAY_COUNT(kFields); index++) {
        if (!ZrMath_ReadFloatField(state, object, kFields[index], &outValues[index])) {
            return ZR_FALSE;
        }
    }
    return outValues != ZR_NULL;
}

TZrBool ZrMath_ReadMatrix4Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues) {
    static const TZrChar *const kFields[] = {
            "m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"
    };
    TZrSize index;
    for (index = 0; outValues != ZR_NULL && index < ZR_ARRAY_COUNT(kFields); index++) {
        if (!ZrMath_ReadFloatField(state, object, kFields[index], &outValues[index])) {
            return ZR_FALSE;
        }
    }
    return outValues != ZR_NULL;
}

TZrBool ZrMath_ArrayReadFloat(SZrState *state, SZrObject *array, TZrSize index, TZrFloat64 *outValue) {
    return ZrMath_NumberFromValue(ZrLib_Array_Get(state, array, index), outValue);
}

TZrBool ZrMath_ArrayReadInt(SZrState *state, SZrObject *array, TZrSize index, TZrInt64 *outValue) {
    return ZrMath_IntFromValue(ZrLib_Array_Get(state, array, index), outValue);
}

TZrBool ZrMath_ArraySetValue(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value) {
    /* TODO: Fill/Set 把本 helper 的真值当作写入成功；底层 SetValue 无返回值，
     * 需用分配失败或不可写数组场景确认错误是否可由 VM 状态传出。 */
    SZrTypeValue key;
    ZrLib_Value_SetInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

TZrInt64 ZrMath_TensorTotalSize(SZrState *state, SZrObject *shapeArray) {
    /* BUG: 构造器允许 shape 数组进入此路径；正维度如 [INT64_MAX, 2] 在乘法处发生
     * 有符号溢出，后续 data 长度校验无法可靠拒绝该输入。见 Tensor_Construct/Populate。 */
    TZrInt64 total = 1;
    TZrSize index;
    for (index = 0; index < ZrLib_Array_Length(shapeArray); index++) {
        TZrInt64 dimension = 0;
        if (!ZrMath_ArrayReadInt(state, shapeArray, index, &dimension) || dimension <= 0) {
            return -1;
        }
        /* BUG: ReadInt 会把 2.9 截断为 2；Tensor([2.9], [a,b]) 因而通过长度校验，
         * 但对外 shape 字段仍为 2.9，维度契约与实际布局不一致。 */
        total *= dimension;
    }
    return total;
}

SZrObject *ZrMath_TensorMakeZeroData(SZrState *state, TZrInt64 size) {
    /* TODO: 当前没有调用方；核对它是否原本用于 metadata 宣称的 fillValue 构造形式。 */
    SZrObject *data = ZrLib_Array_New(state);
    TZrInt64 index;
    for (index = 0; data != ZR_NULL && index < size; index++) {
        SZrTypeValue value;
        ZrLib_Value_SetFloat(state, &value, 0.0);
        ZrLib_Array_PushValue(state, data, &value);
    }
    return data;
}

TZrBool ZrMath_TensorGetStorage(SZrState *state, SZrObject *tensor, ZrMathTensorStorage *outStorage) {
    /* TODO: 这里只做字段类型检查；公开的 shape/rank/size 若被脚本修改，运算仍信任缓存值。
     * 需检查字段可写性和修改后 get/matmul 的实际行为，再决定是否重验存储不变量。 */
    const SZrTypeValue *shapeValue;
    const SZrTypeValue *dataValue;
    const SZrTypeValue *rankValue;
    const SZrTypeValue *sizeValue;
    TZrInt64 rank = 0;
    if (tensor == ZR_NULL || outStorage == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(outStorage, 0, sizeof(*outStorage));
    shapeValue = ZrLib_Object_GetFieldCString(state, tensor, "shape");
    dataValue = ZrLib_Object_GetFieldCString(state, tensor, "__data");
    rankValue = ZrLib_Object_GetFieldCString(state, tensor, "rank");
    sizeValue = ZrLib_Object_GetFieldCString(state, tensor, "size");
    if (shapeValue == ZR_NULL || dataValue == ZR_NULL || rankValue == ZR_NULL || sizeValue == ZR_NULL ||
        shapeValue->type != ZR_VALUE_TYPE_ARRAY || dataValue->type != ZR_VALUE_TYPE_ARRAY ||
        !ZrMath_IntFromValue(rankValue, &rank) || !ZrMath_IntFromValue(sizeValue, &outStorage->size)) {
        return ZR_FALSE;
    }
    outStorage->shape = ZR_CAST_OBJECT(state, shapeValue->value.object);
    outStorage->data = ZR_CAST_OBJECT(state, dataValue->value.object);
    outStorage->rank = (TZrSize)rank;
    return ZR_TRUE;
}

TZrBool ZrMath_TensorPopulate(SZrState *state, SZrObject *tensor, SZrObject *shapeArray, SZrObject *dataArray) {
    /* 构造器和计算回调把数组交给 Tensor 持有；此处建立 shape、data、rank、size
     * 的共同基线，后续 get/set 和数值运算依赖这四个字段仍然一致。 */
    /* BUG: 这里只检查 data 长度。Tensor([1], ["x"]) 可构造；sum/mean 忽略
     * 该元素并返回 0，其他计算路径把读取失败的元素当作零值使用。 */
    TZrInt64 totalSize;

    if (state == ZR_NULL || tensor == ZR_NULL || shapeArray == ZR_NULL || dataArray == ZR_NULL) {
        return ZR_FALSE;
    }

    totalSize = ZrMath_TensorTotalSize(state, shapeArray);
    if (totalSize < 0 || (TZrSize)totalSize != ZrLib_Array_Length(dataArray)) {
        return ZR_FALSE;
    }

    {
        SZrTypeValue shapeValue;
        SZrTypeValue dataValue;
        ZrLib_Value_SetObject(state, &shapeValue, shapeArray, ZR_VALUE_TYPE_ARRAY);
        ZrLib_Value_SetObject(state, &dataValue, dataArray, ZR_VALUE_TYPE_ARRAY);
        ZrLib_Object_SetFieldCString(state, tensor, "shape", &shapeValue);
        ZrLib_Object_SetFieldCString(state, tensor, "__data", &dataValue);
    }

    ZrMath_WriteIntField(state, tensor, "rank", (TZrInt64)ZrLib_Array_Length(shapeArray));
    ZrMath_WriteIntField(state, tensor, "size", totalSize);
    return ZR_TRUE;
}

SZrObject *ZrMath_TensorMake(SZrState *state, SZrObject *shapeArray, SZrObject *dataArray) {
    SZrObject *tensor = ZrLib_Type_NewInstance(state, "Tensor");
    if (tensor == ZR_NULL) {
        return ZR_NULL;
    }

    if (!ZrMath_TensorPopulate(state, tensor, shapeArray, dataArray)) {
        return ZR_NULL;
    }

    return tensor;
}

TZrBool ZrMath_TensorShapeEquals(SZrState *state, SZrObject *lhsShape, SZrObject *rhsShape) {
    TZrSize index;
    TZrSize count = ZrLib_Array_Length(lhsShape);
    if (count != ZrLib_Array_Length(rhsShape)) {
        return ZR_FALSE;
    }
    for (index = 0; index < count; index++) {
        TZrInt64 lhs = 0;
        TZrInt64 rhs = 0;
        if (!ZrMath_ArrayReadInt(state, lhsShape, index, &lhs) ||
            !ZrMath_ArrayReadInt(state, rhsShape, index, &rhs) ||
            lhs != rhs) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrMath_TensorComputeOffset(SZrState *state, SZrObject *shape, SZrObject *indices, TZrSize *outOffset) {
    /* 逆序累积 stride，保证 get/set 与 matmul 使用相同的 row-major 布局。 */
    /* BUG: ArrayReadInt 对浮点索引截断；get([0.9]) 会访问第 0 项而非拒绝非整数索引。 */
    TZrSize rank = ZrLib_Array_Length(shape);
    TZrSize stride = 1;
    TZrSize offset = 0;
    TZrSize index;
    if (outOffset == ZR_NULL || rank != ZrLib_Array_Length(indices)) {
        return ZR_FALSE;
    }
    for (index = rank; index > 0; index--) {
        TZrInt64 dimension = 0;
        TZrInt64 position = 0;
        if (!ZrMath_ArrayReadInt(state, shape, index - 1, &dimension) ||
            !ZrMath_ArrayReadInt(state, indices, index - 1, &position) ||
            position < 0 || position >= dimension) {
            return ZR_FALSE;
        }
        offset += (TZrSize)position * stride;
        stride *= (TZrSize)dimension;
    }
    *outOffset = offset;
    return ZR_TRUE;
}

TZrBool ZrMath_MakeStringResult(SZrState *state, SZrTypeValue *result, const TZrChar *format, ...) {
    TZrChar buffer[ZR_MATH_FORMAT_BUFFER_LENGTH];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    ZrLib_Value_SetString(state, result, buffer);
    return ZR_TRUE;
}
