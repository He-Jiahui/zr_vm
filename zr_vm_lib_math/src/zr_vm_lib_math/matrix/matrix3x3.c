//
// Matrix3x3 native callbacks.
//

#include "zr_vm_lib_math/matrix3x3.h"

/** @file
 * ## Summary
 * 实现 Matrix3x3 的构造器、算术方法和 meta callback，并把数值快照交给共用对象/字符串工厂。
 * 本文件的数组索引保持 row-major；矩阵乘向量按列向量定义。
 *
 * ## Constraints and lifecycle
 * native callback 在 dispatcher 的同步调用帧内读取借用对象，将中间值放在栈上，成功后把 VM
 * 管理的值写入结果槽。局部数组不得逃逸；对象字段写入经共用 setter，setter 的软失败须按下方
 * 窄 BUG 说明理解。
 *
 * ## References
 * `matrix3x3_registry.c` 发布这些 callback；`common.c` 提供 Matrix3x3 工厂；
 * `docs/wiki/03-modules/math-api.md` 记录数值约定和已证明风险。
 */

/**
 * ## Summary
 * 将单位矩阵写入调用者提供的九元素数组，供默认构造和静态 identity 方法复用。
 *
 * ## Parameters
 * @param[out] m 至少含九个 `TZrFloat64` 槽位的可写缓冲区。
 *
 * ## Returns
 * 无返回值；写入对角线为 1、其余分量为 0 的 row-major 矩阵。
 *
 * ## Constraints and lifecycle
 * 不分配内存、不保留指针；缓冲区长度由调用者保证为九。
 *
 * ## References
 * `ZrMath_Matrix3x3_Construct` 与 `ZrMath_Matrix3x3_Identity` 在 callback 栈数组上调用。
 */
static void zr_math_matrix3_identity(TZrFloat64 *m) {
    TZrSize i;
    for (i = 0; i < 9; i++) {
        m[i] = 0.0;
    }
    m[0] = 1.0;
    m[4] = 1.0;
    m[8] = 1.0;
}

/**
 * ## Summary
 * 计算两个 row-major 3×3 矩阵的乘积，供实例方法和 `*` 分支共用。
 *
 * ## Parameters
 * @param[in] lhs 左操作数的九元素快照。
 * @param[in] rhs 右操作数的九元素快照。
 * @param[out] out 接收九个乘积元素的可写缓冲区。
 *
 * ## Returns
 * 无返回值；乘积写入 `out`。
 *
 * ## Constraints and lifecycle
 * 三个缓冲区均须至少有九个 double；`out` 不得与 `lhs` 或 `rhs` 重叠，因为每个输出项先清零。
 * 不执行有限值检查，浮点溢出按当前 double 运算传播。所有缓冲区由同步调用者持有。
 *
 * ## References
 * `ZrMath_Matrix3x3_MulMatrix` 传入两个独立输入数组和一个独立输出数组。
 */
static void zr_math_matrix3_mul(const TZrFloat64 *lhs, const TZrFloat64 *rhs, TZrFloat64 *out) {
    /* 注册的 mulMatrix 与 `*` 共用左乘语义；索引按 row-major 对应列向量。 */
    TZrSize row;
    TZrSize column;
    TZrSize k;
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            out[row * 3 + column] = 0.0;
            for (k = 0; k < 3; k++) {
                out[row * 3 + column] += lhs[row * 3 + k] * rhs[k * 3 + column];
            }
        }
    }
}

/**
 * ## Summary
 * 用 cofactor 展开计算 row-major 3×3 矩阵的行列式。
 *
 * ## Parameters
 * @param[in] m 至少含九个 double 的矩阵快照。
 *
 * ## Returns
 * @return 返回按直接乘减公式得到的 double 行列式；有限输入的中间运算也可能产生无穷或 NaN。
 *
 * ## Constraints and lifecycle
 * 不缩放输入或检测中间溢出；调用者持有快照且本函数不保留它。
 *
 * ## References
 * `ZrMath_Matrix3x3_Determinant` 与 `zr_math_matrix3_inverse` 共用此公式。
 */
static TZrFloat64 zr_math_matrix3_det(const TZrFloat64 *m) {
    return m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
}

/**
 * ## Summary
 * 以固定绝对阈值判断矩阵能否求逆，并将伴随矩阵结果写入输出缓冲区。
 *
 * ## Parameters
 * @param[in] m 至少含九个 double 的输入快照。
 * @param[out] out 至少含九个 double 的输出缓冲区；仅成功时会被写入。
 *
 * ## Returns
 * @return 行列式绝对值不大于 `ZR_MATH_EPSILON` 时返回 `ZR_FALSE` 且保持 `out` 原样；其余情况写入当前公式结果并返回 `ZR_TRUE`，结果可能非有限，不能据此保证得到实际逆矩阵。
 *
 * ## Constraints and lifecycle
 * `m` 与 `out` 必须是互不重叠的九元素缓冲区。判定是固定绝对阈值而非尺度相关条件；NaN 与阈值比较为假，
 * 因此会继续计算。BUG: 有限输入 `[[A,A,0],[A,A,0],[0,0,1]]`（A=1e200）令两个外层乘积分别溢出为 Inf，随后 Inf-Inf，
 * 令 NaN 绕过阈值并输出 NaN 项；数学行列式实际为零。
 *
 * ## References
 * `ZrMath_Matrix3x3_Inverse` 将 false 传给 native dispatcher，并在 true 时创建结果对象。
 */
static TZrBool zr_math_matrix3_inverse(const TZrFloat64 *m, TZrFloat64 *out) {
    TZrFloat64 determinant = zr_math_matrix3_det(m);
    if (fabs(determinant) <= ZR_MATH_EPSILON) {
        return ZR_FALSE;
    }

    out[0] = (m[4] * m[8] - m[5] * m[7]) / determinant;
    out[1] = (m[2] * m[7] - m[1] * m[8]) / determinant;
    out[2] = (m[1] * m[5] - m[2] * m[4]) / determinant;
    out[3] = (m[5] * m[6] - m[3] * m[8]) / determinant;
    out[4] = (m[0] * m[8] - m[2] * m[6]) / determinant;
    out[5] = (m[2] * m[3] - m[0] * m[5]) / determinant;
    out[6] = (m[3] * m[7] - m[4] * m[6]) / determinant;
    out[7] = (m[1] * m[6] - m[0] * m[7]) / determinant;
    out[8] = (m[0] * m[4] - m[1] * m[3]) / determinant;
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 字段名是静态只读表；values 是本次调用的栈快照，由下面的同步 helper 复制。 */
    static const TZrChar *const kFields[] = {"m00","m01","m02","m10","m11","m12","m20","m21","m22"};
    TZrFloat64 values[9];
    TZrSize index;

    if (ZrLib_CallContext_ArgumentCount(context) == 0) {
        zr_math_matrix3_identity(values);
    } else {
        if (ZrLib_CallContext_ArgumentCount(context) != 9) {
            /* BUG: registry 允许 0..9，callback 只接受 0 或 9；1..8 会被拒绝，
             * 但 RaiseArityError 仍报告 0..9，使诊断范围包含实际拒绝的数量。 */
            ZrLib_CallContext_RaiseArityError(context, 0, 9);
        }
        for (index = 0; index < 9; index++) {
            if (!ZrLib_CallContext_ReadFloat(context, index, &values[index])) {
                return ZR_FALSE;
            }
        }
    }

    /* BUG: 合法九参数构造可经 parser 的 CREATE_OBJECT/TO_STRUCT hidden call 复用兼容 seed；
     * 9 参数超过 fast lane inline 容量后仍由 generic dispatcher pin receiver。对象字段表初始容量 4，
     * 第四字段 m10（或随后扩容点 m20）触发 HASH_BUCKET rehash。若本次唯一失败是该分配，
     * 且其余 receiver/key pin 与分配成功，受支持的 realloc allocator 保留旧 bucket：void setter
     * 仅记录错误，构造 helper 继续并返回成功，结果会缺少该字段。内置 `ZrLibrary_CommonState_BuiltinAllocator`
     * 和默认 harness allocator 均用 realloc；自定义 FZrAllocator 的失败保留语义由 Identity 块 TODO 指向。
     */
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}

TZrBool ZrMath_Matrix3x3_Identity(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 values[9];
    SZrObject *object;
    zr_math_matrix3_identity(values);
    /* BUG: 新对象 map 达到三项阈值后，第四字段需要 HASH_BUCKET 扩容；若仅该次分配失败且
     * 其他字段值、key/object pin 和分配成功，内置 realloc allocator 保留旧 bucket，void setter 只记录错误，工厂仍返回对象。
     * TODO: 对自定义 FZrAllocator 核实失败时是否保留旧块；下一步检查 memory.h 与注册 allocator 实现。 */
    object = ZrMath_MakeMatrix3x3(context->state, values);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    /* BUG: 若 receiver pin 成功占用 ignored-root 最后一个槽，尚未登记的字段 key 随后需要 ARRAY 扩容，
     * 且该次 RawMalloc 单独软失败，setter 会跳过字段而工厂仍成功发布对象；此路径与 HASH_BUCKET 独立。 */
    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Transpose(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    TZrFloat64 transpose[9];
    TZrSize row;
    TZrSize column;
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix)) {
        return ZR_FALSE;
    }

    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            transpose[row * 3 + column] = matrix[column * 3 + row];
        }
    }

    object = ZrMath_MakeMatrix3x3(context->state, transpose);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Determinant(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix)) {
        return ZR_FALSE;
    }
    /* BUG: 有限的大分量可令两个外层乘积分别溢出为 Inf，随后 Inf-Inf 产生 NaN；例如
     * [[1e200,1e200,0],[1e200,1e200,0],[0,0,1]] 的精确行列式为 0。 */
    ZrLib_Value_SetFloat(context->state, result, zr_math_matrix3_det(matrix));
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_Inverse(ZrLibCallContext *context, SZrTypeValue *result) {
    /* TODO: 近零行列式使回调返回 ZR_FALSE，dispatcher 在无异常时交付 null；下一步对照
     * GetType.kMethods 的 Matrix3x3 返回描述与 native false-result 契约，确认调用方是否预期 null。 */
    TZrFloat64 matrix[9];
    TZrFloat64 inverse[9];
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix) ||
        !zr_math_matrix3_inverse(matrix, inverse)) {
        return ZR_FALSE;
    }

    object = ZrMath_MakeMatrix3x3(context->state, inverse);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_MulVector(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    ZrMathVector3 vector;
    SZrObject *other = ZR_NULL;
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadVector3Object(context->state, other, &vector)) {
        return ZR_FALSE;
    }

    /* BUG: 有限 matrix 首行 [A,A,0] 与向量 [A,-A,0]（A=1e200）使首分量计算 Inf-Inf=NaN，
     * 而实数结果为 0；当前乘加未缩放或检测该中间溢出。 */
    object = ZrMath_MakeVector3(context->state,
                                matrix[0] * vector.x + matrix[1] * vector.y + matrix[2] * vector.z,
                                matrix[3] * vector.x + matrix[4] * vector.y + matrix[5] * vector.z,
                                matrix[6] * vector.x + matrix[7] * vector.y + matrix[8] * vector.z);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 lhs[9];
    TZrFloat64 rhs[9];
    TZrFloat64 product[9];
    SZrObject *other = ZR_NULL;
    SZrObject *object;

    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), lhs) ||
        !ZrLib_CallContext_ReadObject(context, 0, &other) ||
        !ZrMath_ReadMatrix3Object(context->state, other, rhs)) {
        return ZR_FALSE;
    }

    zr_math_matrix3_mul(lhs, rhs, product);
    /* BUG: 左矩阵首行为 [A,A,0]、右矩阵首列为 [A,-A,0] 且 A=1e200 时，有限输入的
     * 两项乘积变为 Inf 与 -Inf，首项相加为 NaN，而精确矩阵乘积该项为 0。 */
    object = ZrMath_MakeMatrix3x3(context->state, product);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(context->state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrMath_Matrix3x3_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) {
    /* `*` 只按精确的 Vector3 原型名分流；其他对象继续按九字段矩阵读取，不是通用协议派发。 */
    SZrObject *other = ZR_NULL;
    if (!ZrLib_CallContext_ReadObject(context, 0, &other)) {
        return ZR_FALSE;
    }
    if (ZrMath_ObjectTypeEquals(context->state, other, "Vector3")) {
        return ZrMath_Matrix3x3_MulVector(context, result);
    }
    return ZrMath_Matrix3x3_MulMatrix(context, result);
}

TZrBool ZrMath_Matrix3x3_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 matrix[9];
    if (!ZrMath_ReadMatrix3Object(context->state, ZrMath_SelfObject(context), matrix)) {
        return ZR_FALSE;
    }
    /* BUG: 全零矩阵的格式文本短于短字符串上限；若该字符串未驻留且缓存 HASH_BUCKET 扩容的
     * Memory_Allocate 失败，字符串 setter 保持预置 null，MakeStringResult 仍返回 true，dispatcher 交付 null。
     * 触发要求本次缓存确需扩桶；此处是原始桶分配返回 NULL 的软失败，没有进入 VM 抛错路径。 */
    return ZrMath_MakeStringResult(context->state,
                                   result,
                                   "Matrix3x3([%g,%g,%g],[%g,%g,%g],[%g,%g,%g])",
                                   matrix[0],
                                   matrix[1],
                                   matrix[2],
                                   matrix[3],
                                   matrix[4],
                                   matrix[5],
                                   matrix[6],
                                   matrix[7],
                                   matrix[8]);
}
