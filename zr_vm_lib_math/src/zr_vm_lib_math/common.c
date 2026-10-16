//
// `zr.math` shared value/field helpers, object factories, SIMD dot product and Tensor storage operations.
//

#include "zr_vm_lib_math/math_common.h"

#include <stdio.h>

/* 配置块：编译期优先选择 AVX2、SSE2、再选 AArch64 NEON；无可用宏时 Dot 回退标量循环，向量分支另处理尾项。 */
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

/* Shared by the fixed-layout Math object wrappers below. */
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
        ZrMath_WriteFloatField(state, object, fieldNames[index], fieldValues[index]); /* setter无状态返回；一次字段失败不打断循环。 */
    }
    return object;
}

TZrFloat64 ZrMath_AbsFloat(TZrFloat64 value) {
    /* TODO: -0.0 < 0.0 为假，当前会保留负零；从 math.abs 对负零的公开行为和测试入口
     * 核对是否需与 fabs 一致。 */
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
    /* TODO: uint64 大于 INT64_MAX 的转换由 C 实现定义；核对 shape/index 及公开 rank/size 的整数范围与写入约束。 */
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
        case ZR_VALUE_TYPE_DOUBLE: {
            TZrFloat64 number = value->value.nativeObject.nativeDouble;
            /* INT64_MIN is exactly representable; -lower is the exclusive +2^63
             * bound. Casting INT64_MAX to double would round up to that bound. */
            const TZrFloat64 lower = (TZrFloat64)INT64_MIN;
            if (!isfinite(number) || number < lower || number >= -lower) {
                return ZR_FALSE;
            }
            *outValue = (TZrInt64)number;
            return ZR_TRUE;
        }
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
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue); /* BUG: setter 无状态返回；软失败不阻止工厂报告成功。 */
}

void ZrMath_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue); /* BUG: 新 Tensor 第四个 size 字段扩桶软失败时此 void setter 不报告状态，Populate 仍返回 true。 */
}

void ZrMath_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    /* TODO: 全仓只有声明和定义；核对未来 callback/注册表是否仍需要布尔字段写入 helper。 */
    SZrTypeValue fieldValue;
    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue); /* 同一 void setter 失败边界。 */
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
    static const TZrChar *const kFields[] = {"x", "y"}; /* 静态字段映射配置；次序与参数 values 配对。 */
    const TZrFloat64 values[] = {x, y};
    return zr_math_make_object_with_fields(state, "Vector2", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeVector3(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z) {
    static const TZrChar *const kFields[] = {"x", "y", "z"}; /* 静态字段映射配置；次序与参数 values 配对。 */
    const TZrFloat64 values[] = {x, y, z};
    return zr_math_make_object_with_fields(state, "Vector3", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeVector4(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w) {
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"}; /* 静态字段映射配置；次序与参数 values 配对。 */
    const TZrFloat64 values[] = {x, y, z, w};
    return zr_math_make_object_with_fields(state, "Vector4", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeQuaternion(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w) {
    static const TZrChar *const kFields[] = {"x", "y", "z", "w"}; /* 静态字段映射配置；次序与参数 values 配对。 */
    const TZrFloat64 values[] = {x, y, z, w};
    return zr_math_make_object_with_fields(state, "Quaternion", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeComplex(SZrState *state, TZrFloat64 real, TZrFloat64 imag) {
    static const TZrChar *const kFields[] = {"real", "imag"}; /* 静态字段映射配置；次序与参数 values 配对。 */
    const TZrFloat64 values[] = {real, imag};
    return zr_math_make_object_with_fields(state, "Complex", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeMatrix3x3(SZrState *state, const TZrFloat64 *values) {
    static const TZrChar *const kFields[] = {"m00","m01","m02","m10","m11","m12","m20","m21","m22"}; /* row-major 静态字段映射，与 values 的九项顺序一致。 */
    return zr_math_make_object_with_fields(state, "Matrix3x3", kFields, values, ZR_ARRAY_COUNT(kFields));
}

SZrObject *ZrMath_MakeMatrix4x4(SZrState *state, const TZrFloat64 *values) {
    static const TZrChar *const kFields[] = { /* row-major 静态字段映射，与 values 的十六项顺序一致。 */
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
    static const TZrChar *const kFields[] = {"m00","m01","m02","m10","m11","m12","m20","m21","m22"}; /* row-major 读取映射；按字段顺序填入 outValues。 */
    TZrSize index;
    for (index = 0; outValues != ZR_NULL && index < ZR_ARRAY_COUNT(kFields); index++) {
        if (!ZrMath_ReadFloatField(state, object, kFields[index], &outValues[index])) {
            return ZR_FALSE;
        }
    }
    return outValues != ZR_NULL;
}

TZrBool ZrMath_ReadMatrix4Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues) {
    static const TZrChar *const kFields[] = { /* row-major 读取映射；按字段顺序填入 outValues。 */
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
    /* TODO: 本 helper 无条件返回成功，Fill/Set 也未检查写入状态；底层 SetValue 无返回值，
     * 需用分配失败或不可写数组场景确认错误是否可由 VM 状态传出。 */
    SZrTypeValue key;
    ZrLib_Value_SetInt(state, &key, (TZrInt64)index);
    ZrCore_Object_SetValue(state, array, &key, value);
    return ZR_TRUE;
}

TZrInt64 ZrMath_TensorTotalSize(SZrState *state, SZrObject *shapeArray) {
    TZrInt64 total = 1;
    TZrSize index;
    for (index = 0; index < ZrLib_Array_Length(shapeArray); index++) {
        TZrInt64 dimension = 0;
        if (!ZrMath_ArrayReadInt(state, shapeArray, index, &dimension) || dimension <= 0) {
            return -1;
        }
        /* TODO: Float shape 按 ArrayReadInt 截断而原数组仍保留小数；核实 shape 是否承诺仅整数及兼容现有构造语义。
         * 当前已定义截断行为，不能仅凭存储表示差异推定需要拒绝小数。 */
        if (total > INT64_MAX / dimension) {
            return -1;
        }
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
    TZrInt64 totalSize;

    if (state == ZR_NULL || tensor == ZR_NULL || shapeArray == ZR_NULL || dataArray == ZR_NULL) {
        return ZR_FALSE;
    }

    totalSize = ZrMath_TensorTotalSize(state, shapeArray);
    if (totalSize < 0 || (TZrSize)totalSize != ZrLib_Array_Length(dataArray)) {
        return ZR_FALSE;
    }
    /* Validate every payload before publishing any Tensor fields. Numeric
     * NaN/Inf values remain valid data; this checks tags, not numerical policy. */
    for (TZrSize index = 0; index < ZrLib_Array_Length(dataArray); ++index) {
        TZrFloat64 number;
        if (!ZrMath_ArrayReadFloat(state, dataArray, index, &number)) {
            return ZR_FALSE;
        }
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
    ZrMath_WriteIntField(state, tensor, "size", totalSize); /* BUG: 初始空 map 的第四次插入；仅此 HASH_BUCKET 扩容返回 NULL、其余步骤成功时 size 缺失却继续报告成功。 */
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
    /* TODO: 浮点维度沿 ArrayReadInt 截断后比较；例如 [1.1] 与 [1.9] 会判同形。
     * 先核查 shape 的公开整数契约与既有兼容要求，再决定是否拒绝小数。 */
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
    /* TODO: ArrayReadInt 对浮点索引截断；核实 Get/Set 接口是否应只接受整数位置，而非保留数值截断行为。 */
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
    ZrLib_Value_SetString(state, result, buffer); /* BUG: 短串缓存桶扩容软失败不写 result；本 helper 仍返回 ZR_TRUE。 */
    return ZR_TRUE;
}

/**
 * @fn static SZrObject *zr_math_make_object_with_fields(SZrState *state, const TZrChar *typeName, const TZrChar *const *fieldNames, const TZrFloat64 *fieldValues, TZrSize fieldCount)
 * ## Summary
 * 按类型名创建 Math 实例，并按并行字段表尝试写入七个 Make wrapper 提供的数值快照。
 * 它集中处理对象创建与字段发布；各 wrapper 负责确定类型、字段顺序和结果含义。
 *
 * ## Parameters
 * @param[in] state 当前 VM state；仅在同步 native callback/factory 调用期间借用。
 * @param[in] typeName 已注册 Math 类型名，供 VM 创建其实例。
 * @param[in] fieldNames 字段名数组；必须至少有 fieldCount 项，顺序须与 fieldValues 对应。
 * @param[in] fieldValues 本次调用的浮点快照；必须至少有 fieldCount 项，不转移 VM 所有权。
 * @param[in] fieldCount 要尝试写入的字段数；由每个 wrapper 的固定布局决定。
 *
 * ## Returns
 * @return 创建失败时为 ZR_NULL；否则返回 VM 管理的新对象。非空只证明实例创建成功，不能证明每个字段写入成功。
 *
 * ## Constraints and lifecycle
 * 实例创建成功后按数组顺序调用 void WriteFloatField；单次字段 setter 的软失败不会中断循环，也不会在这里传播给调用者。
 * 受支持的 allocator 若仅在 HASH_BUCKET 扩容时返回 NULL 并保留旧桶，第四、七或第十三次插入可漏掉该字段，wrapper 仍可能交付缺字段对象；数组和快照只在本次调用中有效。
 * VM 实例由 VM 管理；本 helper 不释放对象，也不提供跨线程或异步保存这些参数的保证。
 *
 * ## References
 * 七个本地 wrapper 为 ZrMath_MakeVector2/3/4、ZrMath_MakeQuaternion、ZrMath_MakeComplex、ZrMath_MakeMatrix3x3 和 ZrMath_MakeMatrix4x4；字段写入由 ZrMath_WriteFloatField 完成。
 * 分配与缺字段路径见 docs/code-review/batches/20261014-math-common-contracts.md。
 */
