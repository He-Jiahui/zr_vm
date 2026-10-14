//
// Matrix4x4 native callbacks.
//

#include "zr_vm_lib_math/matrix4x4.h"

/** @file
 * ## Summary
 * Matrix4x4 原生回调及固定大小数值 helper。回调经静态 descriptor 函数指针进入，字段读取/写入与对象创建由 math common 辅助层完成。
 * ## Constraints and lifecycle
 * 数值缓冲只在单次 callback 栈帧内有效；此实现不保留跨调用状态。固定主元阈值、double 中间溢出、对象字段发布失败和字符串返回失败分别见对应函数注释。
 * ## References
 * matrix4x4_registry.c 的 kMethods/kMeta；ZrMath_MakeMatrix4x4、ZrMath_ReadMatrix4Object、ZrMath_ConstructFloatObject。
 */
/**
 * ## Summary
 * 将至少 16 个 TZrFloat64 槽清零并写入单位矩阵主对角线，供构造和变换工厂初始化。
 *
 * ## Parameters
 * @param[out] m 调用方提供的 16 元素输出缓冲；函数逐槽覆盖。
 *
 * ## Returns
 * 无返回值；结果写入 m。
 *
 * ## Constraints and lifecycle
 * 不分配或保存缓冲指针；调用方必须保证 m 至少含 16 个元素且在调用期间可写。
 *
 * ## References
 * ZrMath_Matrix4x4_Construct、Translation、Scale、RotationX、RotationY、RotationZ。
 */

static void zr_math_matrix4_identity(TZrFloat64 *m) {
    TZrSize i; for (i = 0; i < 16; i++) m[i] = 0.0; m[0] = m[5] = m[10] = m[15] = 1.0;
}
/**
 * ## Summary
 * 以行主序计算 a × b，供 Matrix4x4.mulMatrix callback 使用。
 *
 * ## Parameters
 * @param[in] a 左侧 16 分量矩阵。
 * @param[in] b 右侧 16 分量矩阵。
 * @param[out] out 接收 16 个乘积；存储不得与 a 或 b 重叠。
 *
 * ## Returns
 * 无返回值；结果逐项写入 out。
 *
 * ## Constraints and lifecycle
 * 直接进行 double 乘加，不检查有限值或中间溢出。重叠输出会破坏后续读取；当前 callback 使用三个独立局部数组。
 *
 * ## References
 * ZrMath_Matrix4x4_MulMatrix；row-major 与 column-vector 约定见 matrix4x4.h。
 */
static void zr_math_matrix4_mul(const TZrFloat64 *a, const TZrFloat64 *b, TZrFloat64 *out) {
    /* 行主序、左矩阵乘右矩阵；translation/rotation 工厂与列向量乘法均依赖此约定。 */
    TZrSize r, c, k; for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) { out[r * 4 + c] = 0.0; for (k = 0; k < 4; k++) out[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c]; }
}
/**
 * ## Summary
 * 在输入副本上以部分选主元消元求 4×4 determinant，不修改调用方数组。
 *
 * ## Parameters
 * @param[in] m 指向 16 个 row-major 分量；实现先复制到局部数组。
 *
 * ## Returns
 * @return 遇到绝对值小于等于 epsilon 的主元时返回 0；否则返回累计乘积，double 累加可能产生 Inf/NaN。
 *
 * ## Constraints and lifecycle
 * 绝对主元阈值不是尺度不变量。BUG: diag(1e-10,1e10,1,1) 精确 determinant 为 1，但首主元被判为 0。另 BUG: diag(1.5e154,1.5e154,2e-9,2e-9) 每个 pivot 都大于 1e-9，精确 determinant 为 9e290；累积前两个 pivot 时中间 double 先溢出为 Inf。
 *
 * ## References
 * ZR_MATH_EPSILON、ZrMath_Matrix4x4_Determinant、matrix4x4_registry.c determinant descriptor。
 */
static TZrFloat64 zr_math_matrix4_det(TZrFloat64 *m) {
    /* BUG: 绝对主元阈值错误地把可逆矩阵 diag(1e-10, 1e10, 1, 1)
     * 的 determinant 判为 0；该矩阵实际行列式为 1，可经命名回调直接观察。 */
    TZrFloat64 a[16]; TZrFloat64 det = 1.0; TZrSize i, j, k; memcpy(a, m, sizeof(a));
    for (i = 0; i < 4; i++) {
        TZrSize pivot = i; for (j = i + 1; j < 4; j++) if (fabs(a[j * 4 + i]) > fabs(a[pivot * 4 + i])) pivot = j;
        if (fabs(a[pivot * 4 + i]) <= ZR_MATH_EPSILON) return 0.0;
        if (pivot != i) { for (k = 0; k < 4; k++) { TZrFloat64 t = a[i * 4 + k]; a[i * 4 + k] = a[pivot * 4 + k]; a[pivot * 4 + k] = t; } det = -det; }
        det *= a[i * 4 + i];
        for (j = i + 1; j < 4; j++) { TZrFloat64 f = a[j * 4 + i] / a[i * 4 + i]; for (k = i; k < 4; k++) a[j * 4 + k] -= f * a[i * 4 + k]; }
    }
    return det;
}
/**
 * ## Summary
 * 以 Gauss-Jordan 消元求 4×4 inverse；只有全部主元通过检查后才复制结果。
 *
 * ## Parameters
 * @param[in] m 指向 16 个 row-major 输入分量，不被改写。
 * @param[out] out 成功时写入 16 个逆矩阵分量；失败时保持原内容不变。
 *
 * ## Returns
 * @return 若任一主元比较命中 fabs(pivot) <= ZR_MATH_EPSILON 则返回 ZR_FALSE 且不写 out；未命中该分支时返回 ZR_TRUE 并复制计算结果，NaN 比较会绕过拒绝分支，且不保证有限或准确。
 *
 * ## Constraints and lifecycle
 * 固定绝对阈值使 diag(1e-10,1e-10,1e-10,1e-10)（条件数为 1，数学 inverse 为 1e10·I）返回 ZR_FALSE；callback 保持结果槽未写，dispatcher 在 VM 无异常时将 false 归一化为 null。TODO: 核实该拒绝边界是否符合公开数值政策；下一步确认 descriptor/脚本层失败结果约定并添加直接数值断言。
 *
 * ## References
 * ZR_MATH_EPSILON、ZrMath_Matrix4x4_Inverse、matrix4x4_registry.c inverse descriptor。
 */
static TZrBool zr_math_matrix4_inverse(const TZrFloat64 *m, TZrFloat64 *out) {
    /* 与 determinant 使用相同的固定主元阈值；失败沿回调返回 ZR_FALSE。 */
    TZrFloat64 a[16], inv[16]; TZrSize i, j, k; memcpy(a, m, sizeof(a)); zr_math_matrix4_identity(inv);
    for (i = 0; i < 4; i++) {
        TZrSize pivot = i; for (j = i + 1; j < 4; j++) if (fabs(a[j * 4 + i]) > fabs(a[pivot * 4 + i])) pivot = j;
        if (fabs(a[pivot * 4 + i]) <= ZR_MATH_EPSILON) return ZR_FALSE;
        if (pivot != i) for (k = 0; k < 4; k++) { TZrFloat64 t = a[i * 4 + k]; a[i * 4 + k] = a[pivot * 4 + k]; a[pivot * 4 + k] = t; t = inv[i * 4 + k]; inv[i * 4 + k] = inv[pivot * 4 + k]; inv[pivot * 4 + k] = t; }
        { TZrFloat64 d = a[i * 4 + i]; for (k = 0; k < 4; k++) { a[i * 4 + k] /= d; inv[i * 4 + k] /= d; } }
        for (j = 0; j < 4; j++) if (j != i) { TZrFloat64 f = a[j * 4 + i]; for (k = 0; k < 4; k++) { a[j * 4 + k] -= f * a[i * 4 + k]; inv[j * 4 + k] -= f * inv[i * 4 + k]; } }
    }
    memcpy(out, inv, sizeof(inv)); return ZR_TRUE;
}
TZrBool ZrMath_Matrix4x4_Construct(ZrLibCallContext *context, SZrTypeValue *result) {
    /* 零参数构造单位变换；完整十六分量构造自定义变换。 */
    /* 字段名按 row-major 顺序与 registry 的 kFields 及 ZrMath_ReadMatrix4Object 一一对应。 */
    static const TZrChar *const kFields[] = {
            "m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"
    };
    TZrFloat64 values[16];
    TZrSize i;

    if (ZrLib_CallContext_ArgumentCount(context) == 0) {
        zr_math_matrix4_identity(values);
    } else {
        if (ZrLib_CallContext_ArgumentCount(context) != 16) {
            /* BUG: registry 的 kMeta.constructor 声明 min=0/max=16，dispatcher 因而放行中间 arity；本 callback 实际只接受 0 或 16，并以 RaiseArityError(0,16) 报出看似允许中间值的范围。 */
            ZrLib_CallContext_RaiseArityError(context, 0, 16);
        }
        for (i = 0; i < 16; i++) {
            if (!ZrLib_CallContext_ReadFloat(context, i, &values[i])) {
                return ZR_FALSE;
            }
        }
    }
    return ZrMath_ConstructFloatObject(context, result, kFields, values, ZR_ARRAY_COUNT(kFields));
}
/* BUG: Constructor 与新对象输出回调逐字段调用 void setter。nodeMap 初始容量 4、resizeThreshold 3；无故障时需扩桶的成功字段序号为第 4/7/13 项（m03/m12/m30），阈值为 3/6/12。默认 ZrLibrary_CommonState_BuiltinAllocator 将有效旧块扩容转交 realloc；若仅其中一次 HASH_BUCKET 分配返回 NULL 且此前/后续 object/key/value pin、键创建和其它分配成功，realloc 保留旧桶，setter 跳过该字段但循环继续并仍发布对象。失败项不增加 count，后续触发序号会顺延；本批没有故障注入。TODO: 自定义 allocator 对扩容失败旧块的保留契约尚待核实，入口为 zr_vm_core/include/zr_vm_core/memory.h 中的 FZrAllocator contract。链路：ZrMath_MakeMatrix4x4 / ZrMath_ConstructFloatObject → ZrMath_WriteFloatField → ZrLib_Object_SetFieldCString → object_set_value_core → ZrCore_HashSet_Add / Rehash。 */
TZrBool ZrMath_Matrix4x4_Identity(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16]; SZrObject *o; zr_math_matrix4_identity(m); o = ZrMath_MakeMatrix4x4(context->state, m); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Transpose(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16], t[16]; TZrSize r, c; SZrObject *o; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m)) return ZR_FALSE; for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) t[r * 4 + c] = m[c * 4 + r]; o = ZrMath_MakeMatrix4x4(context->state, t); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Determinant(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16]; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m)) return ZR_FALSE; ZrLib_Value_SetFloat(context->state, result, zr_math_matrix4_det(m)); return ZR_TRUE; }
/* TODO: inverse 失败时 native dispatcher 在无异常状态下给出 null，但 registry 声明
 * Matrix4x4 返回类型；核对脚本层是否允许这一失败结果，再决定契约如何表述。 */
TZrBool ZrMath_Matrix4x4_Inverse(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16], inv[16]; SZrObject *o; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m) || !zr_math_matrix4_inverse(m, inv)) return ZR_FALSE; o = ZrMath_MakeMatrix4x4(context->state, inv); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
/* BUG: 有限输入仍可使乘加失去精确抵消：矩阵首行 [1e200,1e200,0,0] 与向量 [1e200,-1e200,0,0] 的数学 x 结果为 0；double 中两个乘积分别溢出为 +Inf/-Inf，当前表达式得 NaN 并直接交付结果。静态路径证明，本批未运行数值探针。 */
TZrBool ZrMath_Matrix4x4_MulVector(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 m[16]; ZrMathVector4 v; SZrObject *other = ZR_NULL; SZrObject *o; TZrFloat64 r[4];
    if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m) || !ZrLib_CallContext_ReadObject(context, 0, &other) || !ZrMath_ReadVector4Object(context->state, other, &v)) return ZR_FALSE;
    r[0] = m[0]*v.x + m[1]*v.y + m[2]*v.z + m[3]*v.w; r[1] = m[4]*v.x + m[5]*v.y + m[6]*v.z + m[7]*v.w; r[2] = m[8]*v.x + m[9]*v.y + m[10]*v.z + m[11]*v.w; r[3] = m[12]*v.x + m[13]*v.y + m[14]*v.z + m[15]*v.w;
    o = ZrMath_MakeVector4(context->state, r[0], r[1], r[2], r[3]); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
/* BUG: 有限矩阵输入也可让乘积项溢出后相消失败：左矩阵首行 [1e200,1e200,0,0]、右矩阵首列 [1e200,-1e200,0,0] 的精确左上元素为 0，当前 double 累加产生 Inf + (-Inf) = NaN 并写入输出。静态路径证明，本批未运行数值探针。 */
TZrBool ZrMath_Matrix4x4_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrFloat64 a[16], b[16], out[16]; SZrObject *other = ZR_NULL; SZrObject *o;
    if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), a) || !ZrLib_CallContext_ReadObject(context, 0, &other) || !ZrMath_ReadMatrix4Object(context->state, other, b)) return ZR_FALSE;
    zr_math_matrix4_mul(a, b, out); o = ZrMath_MakeMatrix4x4(context->state, out); if (o == ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state, result, o, ZR_VALUE_TYPE_OBJECT); return ZR_TRUE;
}
/* 变换工厂分别构造平移、缩放和弧度旋转，不读取 self，供类型静态方法调用。 */
TZrBool ZrMath_Matrix4x4_Translation(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 x = 0.0, y = 0.0, z = 0.0, m[16]; SZrObject *o; if (!ZrLib_CallContext_ReadFloat(context,0,&x) || !ZrLib_CallContext_ReadFloat(context,1,&y) || !ZrLib_CallContext_ReadFloat(context,2,&z)) return ZR_FALSE; zr_math_matrix4_identity(m); m[3]=x; m[7]=y; m[11]=z; o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_Scale(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 x = 1.0, y = 1.0, z = 1.0, m[16]; SZrObject *o; if (!ZrLib_CallContext_ReadFloat(context,0,&x) || !ZrLib_CallContext_ReadFloat(context,1,&y) || !ZrLib_CallContext_ReadFloat(context,2,&z)) return ZR_FALSE; zr_math_matrix4_identity(m); m[0]=x; m[5]=y; m[10]=z; o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_RotationX(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 a=0.0,m[16]; SZrObject *o; if(!ZrLib_CallContext_ReadFloat(context,0,&a)) return ZR_FALSE; zr_math_matrix4_identity(m); m[5]=cos(a); m[6]=-sin(a); m[9]=sin(a); m[10]=cos(a); o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_RotationY(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 a=0.0,m[16]; SZrObject *o; if(!ZrLib_CallContext_ReadFloat(context,0,&a)) return ZR_FALSE; zr_math_matrix4_identity(m); m[0]=cos(a); m[2]=sin(a); m[8]=-sin(a); m[10]=cos(a); o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
TZrBool ZrMath_Matrix4x4_RotationZ(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 a=0.0,m[16]; SZrObject *o; if(!ZrLib_CallContext_ReadFloat(context,0,&a)) return ZR_FALSE; zr_math_matrix4_identity(m); m[0]=cos(a); m[1]=-sin(a); m[4]=sin(a); m[5]=cos(a); o=ZrMath_MakeMatrix4x4(context->state,m); if(o==ZR_NULL) return ZR_FALSE; ZrLib_Value_SetObject(context->state,result,o,ZR_VALUE_TYPE_OBJECT); return ZR_TRUE; }
/* 右值仅按 prototype name 精确匹配 Vector4；其他 OBJECT/ARRAY 会进入 Matrix4x4 字段读取路径，不能把任意对象视为乘法兼容。ReadObject 类别错误抛 VM 异常，后续字段不匹配普通返回 false。 */
TZrBool ZrMath_Matrix4x4_MetaMul(ZrLibCallContext *context, SZrTypeValue *result) { SZrObject *other = ZR_NULL; if (!ZrLib_CallContext_ReadObject(context, 0, &other)) return ZR_FALSE; return ZrMath_ObjectTypeEquals(context->state, other, "Vector4") ? ZrMath_Matrix4x4_MulVector(context, result) : ZrMath_Matrix4x4_MulMatrix(context, result); }
/* BUG: 对短格式结果，若 API-cache miss 后短串 intern 表恰在 HASH_BUCKET 扩容处 Memory_Allocate 返回 NULL，String_Create 返回 NULL；SetStringObject 对 NULL 不写 result，MakeStringResult 仍返回 true。这里是桶分配软失败，不是 RawMalloc/VM 异常路径。TODO: 输出超过 ZR_VM_SHORT_STRING_MAX 时仍走 native_binding_create_string/CreateTryHitCache；栈局部缓冲区地址决定桶，缓存比较通过 ZR_CAST_STRING_TO_NATIVE 按短串内联布局取字节。下一步结合 conversion.h:73-76、string.c:958-974 核对长串/短串共桶及同地址复用；当前不据此认定悬空引用。 */
TZrBool ZrMath_Matrix4x4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result) { TZrFloat64 m[16]; if (!ZrMath_ReadMatrix4Object(context->state, ZrMath_SelfObject(context), m)) return ZR_FALSE; return ZrMath_MakeStringResult(context->state, result, "Matrix4x4([%g,%g,%g,%g],[%g,%g,%g,%g],[%g,%g,%g,%g],[%g,%g,%g,%g])", m[0],m[1],m[2],m[3],m[4],m[5],m[6],m[7],m[8],m[9],m[10],m[11],m[12],m[13],m[14],m[15]); }
