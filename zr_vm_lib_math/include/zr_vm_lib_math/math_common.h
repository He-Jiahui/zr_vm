//
// Shared helpers for zr.math split implementation.
//

#ifndef ZR_VM_LIB_MATH_COMMON_H
#define ZR_VM_LIB_MATH_COMMON_H

#include "zr_vm_lib_math/module.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

/** @brief 从 VM 字段读出的临时二维数值；回调使用完后不保留其地址。 */
typedef struct ZrMathVector2 {
    TZrFloat64 x;
    TZrFloat64 y;
} ZrMathVector2;

/** @brief 与 `Vector3` 字段顺序对应的临时分量快照。 */
typedef struct ZrMathVector3 {
    TZrFloat64 x;
    TZrFloat64 y;
    TZrFloat64 z;
} ZrMathVector3;

/** @brief 与 `Vector4` 字段顺序对应的临时分量快照。 */
typedef struct ZrMathVector4 {
    TZrFloat64 x;
    TZrFloat64 y;
    TZrFloat64 z;
    TZrFloat64 w;
} ZrMathVector4;

/** @brief 与 `Quaternion` 字段顺序对应的临时分量快照。 */
typedef struct ZrMathQuaternion {
    TZrFloat64 x;
    TZrFloat64 y;
    TZrFloat64 z;
    TZrFloat64 w;
} ZrMathQuaternion;

/** @brief 与 `Complex` 的实部和虚部对应的临时分量快照。 */
typedef struct ZrMathComplex {
    TZrFloat64 real;
    TZrFloat64 imag;
} ZrMathComplex;

/* TODO: 向量和四元数回调把上述结构体视为连续 TZrFloat64 数组传给 ZrMath_Dot；
 * 核对成员填充及跨成员指针运算的 C 对象模型约束，再决定是否保留这种调用约定。 */

/** @brief 从 Tensor 对象借用的形状和数据数组视图。
 *  `rank` 对应 shape 长度，`size` 对应数据元素数；调用方不得把这些对象指针保存到本次
 *  native 回调之外，也不能假设仅凭字段类型检查即可证明维度与数据仍一致。
 */
typedef struct ZrMathTensorStorage {
    SZrObject *shape;
    SZrObject *data;
    TZrSize rank;
    TZrInt64 size;
} ZrMathTensorStorage;

TZrFloat64 ZrMath_AbsFloat(TZrFloat64 value);
/** @brief 用绝对误差比较两个标量；供公开的 almostEqual 回调复用。
 *  @note 调用方负责选择非负 epsilon；NaN 输入不会得到相等结果。
 */
TZrBool ZrMath_AlmostEqual(TZrFloat64 lhs, TZrFloat64 rhs, TZrFloat64 epsilon);
/** @brief 计算已验证长度的连续数值分量点积，供向量和四元数回调复用。
 *  @pre `lhs`、`rhs` 均指向至少 `count` 个可读 `TZrFloat64`。
 */
TZrFloat64 ZrMath_Dot(const TZrFloat64 *lhs, const TZrFloat64 *rhs, TZrSize count);

TZrBool ZrMath_NumberFromValue(const SZrTypeValue *value, TZrFloat64 *outValue);
TZrBool ZrMath_IntFromValue(const SZrTypeValue *value, TZrInt64 *outValue);
TZrBool ZrMath_ReadFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 *outValue);
void ZrMath_WriteFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 value);
void ZrMath_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value);
void ZrMath_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value);

SZrObject *ZrMath_SelfObject(ZrLibCallContext *context);
TZrBool ZrMath_ObjectTypeEquals(SZrState *state, SZrObject *object, const TZrChar *typeName);
/** @brief 复用 VM 传入且属于目标原型的构造 receiver，否则按目标原型创建实例。
 *  @note 子类型构造不能退回按静态类型名分配，否则会丢失目标原型。
 */
SZrObject *ZrMath_ResolveConstructTarget(ZrLibCallContext *context);
TZrBool ZrMath_FinishConstructObject(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object);
TZrBool ZrMath_ConstructFloatObject(ZrLibCallContext *context,
                                    SZrTypeValue *result,
                                    const TZrChar *const *fieldNames,
                                    const TZrFloat64 *fieldValues,
                                    TZrSize fieldCount);

SZrObject *ZrMath_MakeVector2(SZrState *state, TZrFloat64 x, TZrFloat64 y);
SZrObject *ZrMath_MakeVector3(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z);
SZrObject *ZrMath_MakeVector4(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w);
SZrObject *ZrMath_MakeQuaternion(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w);
SZrObject *ZrMath_MakeComplex(SZrState *state, TZrFloat64 real, TZrFloat64 imag);
SZrObject *ZrMath_MakeMatrix3x3(SZrState *state, const TZrFloat64 *values);
SZrObject *ZrMath_MakeMatrix4x4(SZrState *state, const TZrFloat64 *values);

TZrBool ZrMath_ReadVector2Object(SZrState *state, SZrObject *object, ZrMathVector2 *outValue);
TZrBool ZrMath_ReadVector3Object(SZrState *state, SZrObject *object, ZrMathVector3 *outValue);
TZrBool ZrMath_ReadVector4Object(SZrState *state, SZrObject *object, ZrMathVector4 *outValue);
TZrBool ZrMath_ReadQuaternionObject(SZrState *state, SZrObject *object, ZrMathQuaternion *outValue);
TZrBool ZrMath_ReadComplexObject(SZrState *state, SZrObject *object, ZrMathComplex *outValue);
TZrBool ZrMath_ReadMatrix3Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues);
TZrBool ZrMath_ReadMatrix4Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues);

TZrBool ZrMath_ArrayReadFloat(SZrState *state, SZrObject *array, TZrSize index, TZrFloat64 *outValue);
TZrBool ZrMath_ArrayReadInt(SZrState *state, SZrObject *array, TZrSize index, TZrInt64 *outValue);
TZrBool ZrMath_ArraySetValue(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value);

/** @brief 借用 Tensor 的 shape、隐藏 data 与缓存的 rank/size 字段供 native 运算使用。
 *  @return 字段不存在或类型不符时失败；不复制数组，也不验证全部维度值。
 */
TZrBool ZrMath_TensorGetStorage(SZrState *state, SZrObject *tensor, ZrMathTensorStorage *outStorage);
TZrBool ZrMath_TensorPopulate(SZrState *state, SZrObject *tensor, SZrObject *shapeArray, SZrObject *dataArray);
SZrObject *ZrMath_TensorMake(SZrState *state, SZrObject *shapeArray, SZrObject *dataArray);
TZrBool ZrMath_TensorShapeEquals(SZrState *state, SZrObject *lhsShape, SZrObject *rhsShape);
/** @brief 按 row-major 规则把完整维度索引映射为数据数组偏移。
 *  @note 依赖存储 shape 未被改写；Get/Set 当前仅检查字段类型，不重验维度不变量。
 *  @return 索引数量或范围不符、数值转换失败时返回失败；浮点维度/索引会被截断。
 */
TZrBool ZrMath_TensorComputeOffset(SZrState *state, SZrObject *shape, SZrObject *indices, TZrSize *outOffset);
TZrInt64 ZrMath_TensorTotalSize(SZrState *state, SZrObject *shapeArray);
SZrObject *ZrMath_TensorMakeZeroData(SZrState *state, TZrInt64 size);

TZrBool ZrMath_MakeStringResult(SZrState *state, SZrTypeValue *result, const TZrChar *format, ...);

#endif // ZR_VM_LIB_MATH_COMMON_H
