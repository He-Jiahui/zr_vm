/** @file
 * ## Summary
 * `zr.math` 各类型的 native callback 共用的数值转换、字段访问、对象构造和 Tensor 数组操作层。
 * 它把 VM 值转成短期 C 快照，或把结果交回 VM；业务回调仍负责参数语义和运算。
 *
 * ## Constraints and lifecycle
 * 所有对象、数组和 `SZrState` 均属于当前 VM 调用；本头文件中的视图和栈缓冲区不转移所有权，
 * 不能保存到回调之外或跨 VM 线程使用。写字段和字符串设置的底层接口可能不提供失败状态，
 * 具体软失败边界见对应 helper 与批次说明。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 聚合各注册表；Vector、Quaternion、Complex、Matrix
 * 与 Tensor callback 在各自实现中调用这些 helper。
 */

/**
 * ## Summary
 * 防止 Math shared-helper 声明在同一翻译单元内重复展开。
 *
 * ## Constraints and lifecycle
 * 仅供预处理器使用；不产生运行时状态或 ABI 符号。
 */
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

/**
 * ## Summary
 * Vector2 callback 使用的两个浮点分量快照；避免计算过程中重复访问 VM 字段。
 *
 * ## Fields
 * `x`、`y` 分别对应 Vector2 的同名成员，顺序也被部分连续点积调用复用。
 *
 * ## Invariants and lifecycle
 * 这是按值复制的临时 C 数据，不拥有 VM 对象；只在创建它的 callback 中使用。
 * BUG: Vector2 的 count=2 Dot 在 AVX2 的标量尾循环或纯标量分支读取 lhs[1]/rhs[1]，越出首成员 double 子对象；SSE2/NEON 单次 intrinsic load 的对象范围另待核。
 *
 * ## References
 * `ZrMath_ReadVector2Object` 填充快照，Vector2 callback 将其交给数值 helper。
 */
typedef struct ZrMathVector2 {
    TZrFloat64 x; /**< 第一分量，对应 VM 字段 `x`。 */
    TZrFloat64 y; /**< 第二分量，对应 VM 字段 `y`。 */
} ZrMathVector2;

/**
 * ## Summary
 * Vector3 callback 的临时分量快照，按字段顺序保存一次读取所得的数值。
 *
 * ## Fields
 * `x`、`y`、`z` 分别对应 VM 的同名成员；不包含对象指针或 GC 所有权。
 *
 * ## Invariants and lifecycle
 * 只在当前 native callback 中借用。BUG: `Vector3` 的 count=3 Dot 调用存在 C11 越出首成员标量对象的读取，具体分支见下方证据块。
 *
 * ## References
 * `ZrMath_ReadVector3Object` 填充该快照，Vector3 callback 读取它进行运算。
 */
typedef struct ZrMathVector3 {
    TZrFloat64 x; /**< 第一分量，对应 VM 字段 `x`。 */
    TZrFloat64 y; /**< 第二分量，对应 VM 字段 `y`。 */
    TZrFloat64 z; /**< 第三分量，对应 VM 字段 `z`。 */
} ZrMathVector3;

/**
 * ## Summary
 * Vector4 callback 的临时分量快照，保持 VM 成员的读取顺序。
 *
 * ## Fields
 * `x`、`y`、`z`、`w` 分别对应 VM 同名字段。
 *
 * ## Invariants and lifecycle
 * 按值复制，不拥有 VM 对象。BUG: Vector4 的 count=4 Dot 在 SSE2/NEON 第二轮计算 lhs+2/rhs+2，或在标量分支访问下标 1，越出首成员对象；AVX2 单次 intrinsic load 另待核。
 *
 * ## References
 * `ZrMath_ReadVector4Object` 填充该类型，Vector4 运算回调消费其成员。
 */
typedef struct ZrMathVector4 {
    TZrFloat64 x; /**< 第一分量，对应 VM 字段 `x`。 */
    TZrFloat64 y; /**< 第二分量，对应 VM 字段 `y`。 */
    TZrFloat64 z; /**< 第三分量，对应 VM 字段 `z`。 */
    TZrFloat64 w; /**< 第四分量，对应 VM 字段 `w`。 */
} ZrMathVector4;

/**
 * ## Summary
 * Quaternion callback 使用的按值分量快照。
 *
 * ## Fields
 * `x`、`y`、`z`、`w` 对应 VM Quaternion 的同名成员。
 *
 * ## Invariants and lifecycle
 * 该结构体不代表额外 VM 对象且不拥有 GC 资源；只在当前 callback 中有效。
 * 当前 Dot 的跨成员数组调用仅见于 Vector2/3/4；本模块没有把 Quaternion 快照传给 Dot 的调用点。
 *
 * ## References
 * `ZrMath_ReadQuaternionObject` 读取 VM 字段，Quaternion callback 消费此快照。
 */
typedef struct ZrMathQuaternion {
    TZrFloat64 x; /**< 虚部向量的 x 分量。 */
    TZrFloat64 y; /**< 虚部向量的 y 分量。 */
    TZrFloat64 z; /**< 虚部向量的 z 分量。 */
    TZrFloat64 w; /**< 标量分量。 */
} ZrMathQuaternion;

/**
 * ## Summary
 * Complex callback 运算时使用的实部与虚部快照。
 *
 * ## Fields
 * `real` 保存实部，`imag` 保存虚部，顺序与注册类型字段一致。
 *
 * ## Invariants and lifecycle
 * 仅为 C 按值快照，不拥有或延长源 VM 对象的生命周期；只在当前 callback 中使用。
 *
 * ## References
 * `ZrMath_ReadComplexObject` 填充快照，Complex callback 读取其中两个分量。
 */
typedef struct ZrMathComplex {
    TZrFloat64 real; /**< 复数实部。 */
    TZrFloat64 imag; /**< 复数虚部。 */
} ZrMathComplex;

/* BUG: Vector3 count=3 在标量及 AVX2/SSE2/NEON 尾循环读取 lhs[2]/rhs[2]；Vector2 count=2 在 AVX2/标量尾循环读取下标 1；Vector4 count=4 在 SSE2/NEON 第二轮计算 lhs+2/rhs+2，纯标量也越界，违反 C11 N1570 §6.5.6p7–8。
 * 各 callback 都从首成员 double 对象的指针跨成员访问；TODO: Vector2 SSE2/NEON 与 Vector4 AVX2 的单次 intrinsic load 范围另核，不授予这些分支已执行或已证 BUG 的结论。 */

/**
 * ## Summary
 * Tensor callback 从 VM 实例字段取出的临时存储视图，避免重复查找 `shape`、`__data`、`rank`、`size`。
 *
 * ## Fields
 * `shape` 与 `data` 是 Tensor 持有的 VM 数组借用指针；`rank` 与 `size` 是由实例字段转换来的 C 快照。
 *
 * ## Invariants and lifecycle
 * 这些指针不转移数组所有权，只能在本次 callback 且 Tensor/数组仍由 VM 根保持时使用；不得释放或保存。
 * helper 仅检查字段存在及标签，不证明 shape 长度、rank、size 与 data 长度持续一致；用户修改公开字段后
 * 的一致性策略仍是 TODO。
 *
 * ## References
 * `ZrMath_TensorGetStorage` 填充该结构，Tensor get/set、数值运算和格式化回调读取它。
 */
typedef struct ZrMathTensorStorage {
    SZrObject *shape; /**< Tensor 当前持有的 shape 数组借用视图。 */
    SZrObject *data;  /**< Tensor 当前持有的隐藏数据数组借用视图。 */
    TZrSize rank;     /**< 从 `rank` 字段转换的维数快照。 */
    TZrInt64 size;    /**< 从 `size` 字段转换的数据元素数快照。 */
} ZrMathTensorStorage;

/**
 * ## Summary
 * 返回标量按当前实现计算的绝对值，供 `math.abs` callback 复用。
 *
 * ## Parameters
 * @param[in] value 输入浮点数；NaN 与无穷值按 C 浮点比较规则流过。
 *
 * ## Returns
 * @return 负数取相反数，其他值原样返回；`-0.0` 当前会保留，语义是否应等同 `fabs` 仍待核实。
 *
 * ## Constraints and lifecycle
 * 纯值运算，不访问 VM 状态或分配资源；负零疑点见实现中的 TODO。
 *
 * ## References
 * `ZrMath_Scalar_Abs` 将该 helper 用于脚本 `math.abs`。
 */
TZrFloat64 ZrMath_AbsFloat(TZrFloat64 value);
/**
 * ## Summary
 * 用调用方指定的绝对误差比较两个浮点值，供 `math.almostEqual` callback 使用。
 *
 * ## Parameters
 * @param[in] lhs 左侧标量。
 * @param[in] rhs 右侧标量。
 * @param[in] epsilon 非负绝对误差阈值；helper 不验证其符号。
 *
 * ## Returns
 * @return `abs(lhs-rhs) <= epsilon` 时为 `ZR_TRUE`，否则为 `ZR_FALSE`；NaN 比较为假。
 *
 * ## Constraints and lifecycle
 * 只比较绝对误差，不按数值量级缩放；`lhs-rhs` 可发生浮点溢出，相关策略见 Scalar callback TODO。
 *
 * ## References
 * `ZrMath_Scalar_AlmostEqual` 传入脚本参数；实现复用 `ZrMath_AbsFloat`。
 */
TZrBool ZrMath_AlmostEqual(TZrFloat64 lhs, TZrFloat64 rhs, TZrFloat64 epsilon);
/**
 * ## Summary
 * 对两段分量执行点积，供 Vector2/3/4 回调共享；按构建目标选择 SIMD 或标量路径。
 *
 * ## Parameters
 * @param[in] lhs 左分量缓冲区，至少含 `count` 个可读 `TZrFloat64`。
 * @param[in] rhs 右分量缓冲区，至少含 `count` 个可读 `TZrFloat64`。
 * @param[in] count 参与计算的分量数；为零时结果为零。
 *
 * ## Returns
 * @return 对应分量乘积累加所得浮点数。
 *
 * ## Constraints and lifecycle
 * 不检查空指针、缓冲区长度或重叠；调用方须保证两段在函数执行期间有效。AVX2/SSE2/NEON 与标量分支的
 * 加法顺序可能不同，不承诺跨平台逐位相同；SIMD 选择与尾项处理见 `common.c` 配置块。
 *
 * ## References
 * Vector2/3/4 的点积、长度与比较回调传入快照；结构体跨成员访问的具体 count/backend BUG 与未闭合 intrinsic TODO 见布局证据块。
 */
TZrFloat64 ZrMath_Dot(const TZrFloat64 *lhs, const TZrFloat64 *rhs, TZrSize count);

/**
 * ## Summary
 * 将 VM 数值标签显式转换成内部计算用 `double`，用于字段和数组读取。
 *
 * ## Parameters
 * @param[in] value 待转换的 VM 值；必须是受支持的整数或浮点数标签。
 * @param[out] outValue 转换结果槽；仅成功转换时写入。
 *
 * ## Returns
 * @return 参数非空且标签为数值时返回 `ZR_TRUE`，空指针或其他标签返回 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 不执行对象到数值的隐式转换；整数转 `double` 可能丢失大整数低位精度。值指针只在本次读取中借用。
 *
 * ## References
 * `ZrMath_ReadFloatField`、`ZrMath_ArrayReadFloat` 及 Tensor 数值操作复用该转换。
 */
TZrBool ZrMath_NumberFromValue(const SZrTypeValue *value, TZrFloat64 *outValue);
/**
 * ## Summary
 * 将 VM 数值转换为 Tensor shape/index 及缓存 rank/size 使用的 TZrInt64。
 *
 * ## Parameters
 * @param[in] value 待转换的 VM 数值。
 * @param[out] outValue 整数结果槽；只在成功时写入，失败保留原值。
 *
 * ## Returns
 * @return 对有效数值转换返回 `ZR_TRUE`；空指针、非数值或浮点超范围/非有限值返回 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 浮点值先检查有限值和 [-2^63, 2^63) 范围，再按 C 转换截去小数部分。
 * 超出 `TZrInt64` 范围的 `uint64` 转换由 C 实现定义，见本函数 TODO；成功布尔值不验证范围。
 *
 * ## References
 * ZrMath_ArrayReadInt 供 shape/index 消费者使用；ZrMath_TensorGetStorage 还直接转换公开 rank/size 字段，字段写入范围策略见其 TODO。
 */
TZrBool ZrMath_IntFromValue(const SZrTypeValue *value, TZrInt64 *outValue);
/**
 * ## Summary
 * 按字段名读取对象值并转成浮点数，供 Math callback 建立临时分量快照。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态，属于正在执行的 callback。
 * @param[in] object 被查找字段的对象或数组；函数不验证 prototype 类型。
 * @param[in] fieldName 调用期间有效的 NUL 结尾字段名。
 * @param[out] outValue 字段存在且为可转换数值时写入的结果槽。
 *
 * ## Returns
 * @return 字段可读取并通过 `ZrMath_NumberFromValue` 转换时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * VM 字段值只在本次同步调用中借用；读取失败时 outValue 不保证清零。
 *
 * ## References
 * `ZrMath_ReadVector*Object`、`ZrMath_ReadMatrix*Object`、`ZrMath_ReadComplexObject` 与 `ZrMath_ReadQuaternionObject` 按固定布局调用。
 */
TZrBool ZrMath_ReadFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 *outValue);
/**
 * ## Summary
 * 将浮点值封装为 VM 数值并尝试写入对象字段，供固定分量构造器与七个对象工厂复用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in,out] object 接收字段写入的 VM 对象；所有权仍归 VM。
 * @param[in] fieldName 调用期间有效的 NUL 结尾字段名。
 * @param[in] value 要写入的浮点值。
 *
 * ## Returns
 * 无返回值；字段设置结果不通过本 helper 返回。
 *
 * ## Constraints and lifecycle
 * 底层 setter 不提供可检查的失败状态；特定软失败会使字段缺失而上层仍继续，见共享字段发布 BUG。
 * 只在拥有 `state` 的 VM 调用流程中同步使用，不转移对象所有权。
 *
 * ## References
 * 直接调用者是 zr_math_make_object_with_fields 和 ZrMath_ConstructFloatObject；前者由 Make wrapper 使用，后者由构造 callback 使用。
 */
void ZrMath_WriteFloatField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrFloat64 value);
/**
 * ## Summary
 * 将整数封装为 VM 数值并尝试写入对象字段。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in,out] object 接收字段写入的 VM 对象；对象由 VM 管理。
 * @param[in] fieldName 调用期间有效的 NUL 结尾字段名。
 * @param[in] value 要写入的有符号整数。
 *
 * ## Returns
 * 无返回值；字段设置结果不通过本 helper 返回。
 *
 * ## Constraints and lifecycle
 * BUG: 新 Tensor 的第四个 size 字段触发 HASH_BUCKET 扩容且仅该分配返回 NULL 时，setter 不传播失败，TensorPopulate 仍返回 true；其余创建、pin、写入均须成功。仅同步借用对象。
 *
 * ## References
 * `ZrMath_TensorPopulate` 写入 `rank`、`size`；浮点对象工厂调用的是 `ZrMath_WriteFloatField`。
 */
void ZrMath_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value);
/**
 * ## Summary
 * 将布尔值封装为 VM 值并尝试写入对象字段；当前仓内未发现调用方。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in,out] object 接收字段写入的 VM 对象；对象由 VM 管理。
 * @param[in] fieldName 调用期间有效的 NUL 结尾字段名。
 * @param[in] value 要写入的布尔值。
 *
 * ## Returns
 * 无返回值；字段设置结果不通过本 helper 返回。
 *
 * ## Constraints and lifecycle
 * 与其他 `ZrMath_Write*Field` 相同，底层软失败不会由该签名报告；仓内仅找到声明和定义。
 * 是否仍是受支持 helper 需继续核对预期入口。
 *
 * ## References
 * `ZrMath_WriteFloatField`、`ZrMath_WriteIntField` 具有相同字段发布限制。
 */
void ZrMath_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value);

/**
 * ## Summary
 * 从 native callback context 取得 receiver 对象，供实例方法读取 `self`。
 *
 * ## Parameters
 * @param[in] context 有效 callback context；实现会直接读取 self 与 state。
 *
 * ## Returns
 * @return receiver 为对象或数组时返回对应 VM 对象借用指针；self 类型不符合时返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * context 必须有效，helper 不接受空 context；返回对象仅在当前 VM 调用中借用，不释放、不跨 callback 保存。
 * 不校验具体 Math prototype，字段结构检查由对应读取 helper 承担。
 *
 * ## References
 * Vector、Quaternion、Complex、Matrix 与 Tensor 的实例 callback 从本函数取得 receiver。
 */
SZrObject *ZrMath_SelfObject(ZrLibCallContext *context);
/**
 * ## Summary
 * 判断对象的 prototype 名称是否与指定 Math 类型名完全相同。
 *
 * ## Parameters
 * @param[in] state 为统一 helper 签名保留；当前实现未读取该参数。
 * @param[in] object 待判断对象；空对象或缺 prototype/name 时不匹配。
 * @param[in] typeName NUL 结尾的类型名，在比较期间保持有效。
 *
 * ## Returns
 * @return prototype 名称与 `typeName` 相同时为 `ZR_TRUE`，空链或不等时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 只比较 prototype 名称，不验证字段结构、继承关系或对象值；不持有输入指针。
 *
 * ## References
 * Matrix3x3/Matrix4x4 的 `MetaMul` callback 使用它选择操作数分支。
 */
TZrBool ZrMath_ObjectTypeEquals(SZrState *state, SZrObject *object, const TZrChar *typeName);
/**
 * ## Summary
 * 为 meta constructor 解析目标：兼容 receiver 可复用，否则按构造目标 prototype 创建实例。
 *
 * ## Parameters
 * @param[in] context 当前构造 callback context；owner、construct target 与 state 决定目标类型。
 *
 * ## Returns
 * @return 兼容 receiver 或新建的 GC 管理对象；输入无效或实例分配失败时返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 只有 receiver 属于 owner prototype 时才复用；否则优先用 VM 的 construct-target prototype，缺省回退 owner。
 * 子类型构造必须保留 VM 给出的目标 prototype。返回对象供同一次构造写字段，不转移 GC 所有权。
 *
 * ## References
 * `ZrMath_ConstructFloatObject` 与 `ZrMath_Tensor_Construct` 在构造阶段调用；新建分支使用 `ZrLib_Type_NewInstanceWithPrototype`。
 */
SZrObject *ZrMath_ResolveConstructTarget(ZrLibCallContext *context);
/**
 * ## Summary
 * 将已初始化的构造目标写入 native callback result 槽，结束 Math 对象构造。
 *
 * ## Parameters
 * @param[in] context 当前 callback context，需含有效 VM state。
 * @param[out] result VM callback 结果槽。
 * @param[in] object 已解析并初始化的 VM 对象；helper 不创建或释放它。
 *
 * ## Returns
 * @return 三个指针均非空并写入对象结果时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 成功时结果继续引用 VM 管理对象；不复核字段完整性或 prototype，也不检查 field setter 状态。
 *
 * ## References
 * `ZrMath_ConstructFloatObject` 与 `ZrMath_Tensor_Construct` 用它交付构造目标。
 */
TZrBool ZrMath_FinishConstructObject(ZrLibCallContext *context, SZrTypeValue *result, SZrObject *object);
/**
 * ## Summary
 * 共享浮点字段构造循环：解析兼容 receiver 或新实例，按给定顺序写字段，再交付 callback result。
 *
 * ## Parameters
 * @param[in] context 当前 meta constructor callback context。
 * @param[out] result VM callback 结果槽。
 * @param[in] fieldNames 至少 `fieldCount` 个在调用期间有效的 NUL 结尾字段名。
 * @param[in] fieldValues 至少 `fieldCount` 个浮点值；函数逐项同步读取，不保留缓冲区。
 * @param[in] fieldCount 要尝试写入的字段数；数组长度由调用方保证。
 *
 * ## Returns
 * @return 参数无效或目标解析失败时为 `ZR_FALSE`；完成循环并交付对象时为 `ZR_TRUE`。
 *
 * ## Constraints and lifecycle
 * compatible receiver 会原位复用；新目标由 VM 管理。字段写 helper 返回 `void`，所以本函数无法发现某次软失败，
 * `ZR_TRUE` 不保证所有字段均已发布；调用方仍须提供非空 names/values。
 *
 * ## References
 * 各对象 meta constructor 传入静态字段名表和 callback 栈上的 values；内部调用 `ZrMath_ResolveConstructTarget`、
 * `ZrMath_WriteFloatField` 与 `ZrMath_FinishConstructObject`。
 */
TZrBool ZrMath_ConstructFloatObject(ZrLibCallContext *context,
                                    SZrTypeValue *result,
                                    const TZrChar *const *fieldNames,
                                    const TZrFloat64 *fieldValues,
                                    TZrSize fieldCount);

/**
 * ## Summary
 * 新建并初始化 Vector2 数值对象，供 Vector2 实例和运算 callback 交付结果。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] x 写入字段 `x` 的分量。
 * @param[in] y 写入字段 `y` 的分量。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；实例分配失败时返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 字段表顺序为 `x`,`y`。setter 软失败不由 wrapper 报告，返回非空不保证字段均发布；不复用 callback receiver。
 *
 * ## References
 * Vector2 运算 callback 调用；实现委托给 `zr_math_make_object_with_fields`。
 */
SZrObject *ZrMath_MakeVector2(SZrState *state, TZrFloat64 x, TZrFloat64 y);
/**
 * ## Summary
 * 新建并初始化 Vector3 数值对象，供 Vector3 运算结果构造。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] x 写入 `x` 字段的第一分量。
 * @param[in] y 写入 `y` 字段的第二分量。
 * @param[in] z 写入 `z` 字段的第三分量。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 字段顺序为 `x`,`y`,`z`；字段写入软失败不由 wrapper 返回，结果可能缺字段。helper 始终新建实例。
 *
 * ## References
 * Vector3 运算 callback 使用；实现委托给 `zr_math_make_object_with_fields`。
 */
SZrObject *ZrMath_MakeVector3(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z);
/**
 * ## Summary
 * 新建并初始化 Vector4 数值对象，供 Vector4 callback 交付四分量结果。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] x 写入 `x` 字段的第一分量。
 * @param[in] y 写入 `y` 字段的第二分量。
 * @param[in] z 写入 `z` 字段的第三分量。
 * @param[in] w 写入 `w` 字段的第四分量。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 字段顺序为 `x`,`y`,`z`,`w`；void setter 软失败可能留下缺字段对象。本 factory 不复用 receiver。
 *
 * ## References
 * Vector4 callback 使用；实现委托给 `zr_math_make_object_with_fields`。
 */
SZrObject *ZrMath_MakeVector4(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w);
/**
 * ## Summary
 * 新建并初始化 Quaternion 对象，按 VM 的 `x/y/z/w` 字段布局保存四个分量。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] x 虚部向量 x 分量。
 * @param[in] y 虚部向量 y 分量。
 * @param[in] z 虚部向量 z 分量。
 * @param[in] w 标量分量。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；实例分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 字段按 `x`,`y`,`z`,`w` 发布；setter 软失败可能使字段缺失，且本 helper 不校验单位范数。
 *
 * ## References
 * Quaternion 运算 callback 使用；实现委托给 `zr_math_make_object_with_fields`。
 */
SZrObject *ZrMath_MakeQuaternion(SZrState *state, TZrFloat64 x, TZrFloat64 y, TZrFloat64 z, TZrFloat64 w);
/**
 * ## Summary
 * 新建并初始化 Complex 对象，保存实部与虚部。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] real 要写入的实部。
 * @param[in] imag 要写入的虚部。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；实例分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 按 `real`,`imag` 字段写值；setter 软失败不会由 wrapper 传播，调用方需考虑缺字段路径。
 *
 * ## References
 * Complex callback 调用；实现委托给 `zr_math_make_object_with_fields`。
 */
SZrObject *ZrMath_MakeComplex(SZrState *state, TZrFloat64 real, TZrFloat64 imag);
/**
 * ## Summary
 * 新建并初始化 Matrix3x3 对象，按 row-major 命名字段写入九个数值。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] values 至少九个元素，顺序为 `m00,m01,m02,m10,m11,m12,m20,m21,m22`。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；实例分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * `values` 必须覆盖九项且在同步调用中有效；helper 不检查长度。字段软失败可能导致矩阵对象缺项。
 *
 * ## References
 * Matrix3x3 的 Identity、Transpose、Inverse、MulMatrix 与命名工厂调用该 helper。
 */
SZrObject *ZrMath_MakeMatrix3x3(SZrState *state, const TZrFloat64 *values);
/**
 * ## Summary
 * 新建并初始化 Matrix4x4 对象，按 row-major 命名字段写入十六个数值。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] values 至少十六个元素，按 `m00` 到 `m33` 的行优先顺序排列。
 *
 * ## Returns
 * @return 新创建的 VM 管理对象；实例分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * `values` 必须覆盖十六项且在同步调用中有效；helper 不校验长度。字段发布软失败不反映在返回值中。
 *
 * ## References
 * Matrix4x4 的实例/静态工厂及乘法 callback 通过此 helper 构造结果。
 */
SZrObject *ZrMath_MakeMatrix4x4(SZrState *state, const TZrFloat64 *values);

/**
 * ## Summary
 * 从 VM 对象的 `x`,`y` 字段读取 Vector2 分量，建立本地计算快照。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；只按字段名读数值，不要求特定 prototype。
 * @param[out] outValue 接收 x/y 的调用方存储，必须非空。
 *
 * ## Returns
 * @return 两个字段均存在且可转成浮点时为 `ZR_TRUE`；参数/字段失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 逐字段写入；后续字段失败时先前已写成员不回滚。快照不持有 VM 引用，仅在当前 callback 计算中使用。
 *
 * ## References
 * `ZrMath_ReadFloatField` 执行字段访问，Vector2 实例回调消费该快照。
 */
TZrBool ZrMath_ReadVector2Object(SZrState *state, SZrObject *object, ZrMathVector2 *outValue);
/**
 * ## Summary
 * 按 `x`,`y`,`z` 顺序读取 Vector3 的数值字段到 C 快照。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；不核对 prototype。
 * @param[out] outValue 接收三个分量的非空调用方存储。
 *
 * ## Returns
 * @return 所有分量字段均存在且为可转换数值时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 后续字段失败不会回滚已写分量；快照按值复制且不延长对象生命周期，只用于本次 callback。
 *
 * ## References
 * `ZrMath_ReadFloatField` 负责单字段转换，Vector3 callback 消费此类型。
 */
TZrBool ZrMath_ReadVector3Object(SZrState *state, SZrObject *object, ZrMathVector3 *outValue);
/**
 * ## Summary
 * 按 `x`,`y`,`z`,`w` 顺序读取 Vector4 的四个数值字段。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；不核对 prototype。
 * @param[out] outValue 接收四个分量的非空调用方存储。
 *
 * ## Returns
 * @return 四字段均存在且可转换为浮点时为 `ZR_TRUE`，任一失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 输出逐项填写，失败可能留下部分更新；返回结构体不拥有 VM 对象，只在 callback 中使用。
 *
 * ## References
 * `ZrMath_ReadFloatField` 处理各成员，Vector4 callback 使用该快照进行运算。
 */
TZrBool ZrMath_ReadVector4Object(SZrState *state, SZrObject *object, ZrMathVector4 *outValue);
/**
 * ## Summary
 * 按 `x`,`y`,`z`,`w` 顺序读取 Quaternion 分量到本地快照。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；只核字段值，不验证 Quaternion prototype 或单位范数。
 * @param[out] outValue 接收四分量的非空调用方存储。
 *
 * ## Returns
 * @return 四字段均存在且可转浮点时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 字段读取失败时此前已读成员不会复原；结构体是临时数值快照，不延长对象生命周期。
 *
 * ## References
 * `ZrMath_ReadFloatField` 执行读取，Quaternion callbacks 复用本函数。
 */
TZrBool ZrMath_ReadQuaternionObject(SZrState *state, SZrObject *object, ZrMathQuaternion *outValue);
/**
 * ## Summary
 * 读取 Complex 的 `real` 与 `imag` 字段，供复数 callback 建立运算快照。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；不验证具体 prototype。
 * @param[out] outValue 接收实部/虚部的非空调用方存储。
 *
 * ## Returns
 * @return 两个字段均存在且数值可转换时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 逐字段填写，失败时可能保留部分写入；输出是本次 callback 使用的 C 快照，不拥有 VM 对象。
 *
 * ## References
 * `ZrMath_ReadFloatField` 读取成员，Complex arithmetic callbacks 使用本 helper。
 */
TZrBool ZrMath_ReadComplexObject(SZrState *state, SZrObject *object, ZrMathComplex *outValue);
/**
 * ## Summary
 * 按 row-major 的 `m00` 到 `m22` 字段顺序读取 Matrix3x3。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；只按名称读取，不校验 prototype。
 * @param[out] outValues 至少九个 `TZrFloat64` 的调用方缓冲区。
 *
 * ## Returns
 * @return 所有九项均可读取时为 `ZR_TRUE`；空缓冲区或任一字段失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 不检查缓冲区长度；字段按顺序写入，失败时已有元素不回滚。数组由调用方管理，只在同步读取期间使用。
 *
 * ## References
 * Matrix3x3 callbacks 读取矩阵时调用；单项转换由 `ZrMath_ReadFloatField` 完成。
 */
TZrBool ZrMath_ReadMatrix3Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues);
/**
 * ## Summary
 * 按 row-major 的 `m00` 到 `m33` 字段顺序读取 Matrix4x4。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] object 字段来源；不校验 prototype。
 * @param[out] outValues 至少十六个 `TZrFloat64` 的调用方缓冲区。
 *
 * ## Returns
 * @return 所有十六项均可读取时为 `ZR_TRUE`；空缓冲区或字段失败时为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 不检查输出长度；按顺序写入且失败不回滚。输出数组由调用方管理，仅在同步读取期间使用。
 *
 * ## References
 * Matrix4x4 callbacks 读取矩阵时调用；每个字段通过 `ZrMath_ReadFloatField` 转换。
 */
TZrBool ZrMath_ReadMatrix4Object(SZrState *state, SZrObject *object, TZrFloat64 *outValues);

/**
 * ## Summary
 * 从 VM 数组读取一项并转换为计算用浮点值，供 Tensor 算法复用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] array 数组对象，由 VM 管理。
 * @param[in] index 零基元素位置；范围由 VM 数组读取接口决定。
 * @param[out] outValue 接收数值的调用方槽，必须非空。
 *
 * ## Returns
 * @return 元素存在且其标签可由 `ZrMath_NumberFromValue` 转换时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 不复制数组或保留元素指针；越界/非数值只返回失败，outValue 不保证清零。
 *
 * ## References
 * Tensor sum/mean、矩阵化与逐元素运算通过本 helper 读取数据。
 */
TZrBool ZrMath_ArrayReadFloat(SZrState *state, SZrObject *array, TZrSize index, TZrFloat64 *outValue);
/**
 * ## Summary
 * 从 VM 数组读取一项并转换为有符号整数，用于 Tensor 维度和索引。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] array 数组对象，由 VM 管理。
 * @param[in] index 零基元素位置。
 * @param[out] outValue 接收整数结果的非空槽。
 *
 * ## Returns
 * @return 元素存在且 `ZrMath_IntFromValue` 接受其标签时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 浮点元素截去小数部分；超范围/非有限浮点返回 false；超范围 uint64 的实现定义行为见 `ZrMath_IntFromValue`。
 * 越界与非数值标签不写出可靠结果。
 *
 * ## References
 * `ZrMath_TensorTotalSize`、`ZrMath_TensorShapeEquals`、`ZrMath_TensorComputeOffset` 通过它读取 shape/index。
 */
TZrBool ZrMath_ArrayReadInt(SZrState *state, SZrObject *array, TZrSize index, TZrInt64 *outValue);
/**
 * ## Summary
 * 将给定 VM 值写入数组下标，供 Tensor fill/set 路径复用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in,out] array 被修改的 VM 数组；所有权不转移。
 * @param[in] index 要写入的零基位置，转换为整数 VM key。
 * @param[in] value 要发布的值；调用者保证指针在同步调用期间有效。
 *
 * ## Returns
 * @return 当前实现会执行写操作并返回 `ZR_TRUE`；不把底层写入失败作为返回值。
 *
 * ## Constraints and lifecycle
 * 底层 `ZrCore_Object_SetValue` 不返回状态，此 helper 也不检查 VM 错误状态；写入失败是否能由 state 观察需继续核对。
 *
 * ## References
 * `ZrMath_Tensor_Fill` 与 `ZrMath_Tensor_Set` 调用本 helper；`ZrMath_TensorPopulate` 不使用数组 setter。
 */
TZrBool ZrMath_ArraySetValue(SZrState *state, SZrObject *array, TZrSize index, const SZrTypeValue *value);

/**
 * ## Summary
 * 从 Tensor 实例字段建立 native 存储视图，供 get/set、运算和格式化 callback 复用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] tensor Tensor 实例；字段数组和数值由其 VM 对象持有。
 * @param[out] outStorage 接收 shape/data 借用指针及 rank/size 数值快照的非空结构体。
 *
 * ## Returns
 * @return 必需字段存在、shape/data 为数组且 rank/size 可转换为整数时为 `ZR_TRUE`；否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 返回的数组指针是借用视图，不复制、不释放、不应保存到 callback 之外。这里只检查字段类型和值的可转换性，
 * 不重新验证 rank 对 shape 长度、size 对 data 长度/元素类型的一致性；公开字段被脚本改写后的策略是 TODO。
 *
 * ## References
 * Tensor callbacks 调用本函数，结构布局见 `ZrMathTensorStorage`；字段基线由 `ZrMath_TensorPopulate` 建立。
 */
TZrBool ZrMath_TensorGetStorage(SZrState *state, SZrObject *tensor, ZrMathTensorStorage *outStorage);
/**
 * ## Summary
 * 校验 Tensor shape 的乘积与 data 长度相等，再将两数组及 rank/size 保存到对象。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in,out] tensor 接收存储字段的 VM 实例。
 * @param[in] shapeArray 维度数组；维度当前经整数 helper 读取。
 * @param[in] dataArray 要由 Tensor 持有的元素数组；本函数不复制数组。
 *
 * ## Returns
 * @return 参数无效、shape 无效或 shape 总元素数与 data 长度不同返回 `ZR_FALSE`；通过长度检查并写字段后返回 `ZR_TRUE`。
 *
 * ## Constraints and lifecycle
 * 写入字段前校验 shape 正尺寸乘积、data 数量与每个元素的数值标签；输入数组引用由 Tensor 持有，所有权仍归 VM。BUG: 新对象的第四个 size 字段桶扩容软失败会静默漏字段，
 * TensorPopulate 仍返回 true，构造/工厂可交付随后 TensorGetStorage 无法读取的实例；此证据限定单次 HASH_BUCKET 软失败及其他步骤成功。
 *
 * ## References
 * `ZrMath_Tensor_Construct`、`ZrMath_TensorMake` 建立实例；读取侧由 `ZrMath_TensorGetStorage` 取回字段。
 */
TZrBool ZrMath_TensorPopulate(SZrState *state, SZrObject *tensor, SZrObject *shapeArray, SZrObject *dataArray);
/**
 * ## Summary
 * 分配 Tensor 实例并调用 `ZrMath_TensorPopulate` 发布传入的 shape/data 数组。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] shapeArray 传入的维度数组；不会由本函数复制。
 * @param[in] dataArray 传入的数据数组；不会由本函数复制。
 *
 * ## Returns
 * @return 初始化完成的 VM 管理 Tensor；实例分配或 Populate 失败时返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 两数组继续由 Tensor 字段引用，调用方不能假设传入数组被隔离或复制。Populate 的长度检查、元素类型遗漏及字段软失败均适用。
 *
 * ## References
 * Tensor clone/reshape/算术 callback 通过该 helper 创建结果。
 */
SZrObject *ZrMath_TensorMake(SZrState *state, SZrObject *shapeArray, SZrObject *dataArray);
/**
 * ## Summary
 * 比较两份 shape 的维数和各维度整数转换结果，供 Tensor 加减预检使用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] lhsShape 左侧 shape 数组。
 * @param[in] rhsShape 右侧 shape 数组。
 *
 * ## Returns
 * @return 长度相同且每一维转换结果相等时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 不核对原始 VM 数值是否为整数，浮点维度会截断后比较；存储 shape 保留原值与操作使用整数值之间的策略需继续核实。
 *
 * ## References
 * `zr_math_tensor_binary` 使用该比较决定逐元素 Add/Sub 是否可执行。
 */
TZrBool ZrMath_TensorShapeEquals(SZrState *state, SZrObject *lhsShape, SZrObject *rhsShape);
/**
 * ## Summary
 * 按 row-major 规则把完整多维索引映射为 data 数组的线性偏移。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] shape shape 数组；调用方保证它对应当前 Tensor 存储。
 * @param[in] indices 完整索引数组，长度须等于 shape 长度。
 * @param[out] outOffset 接收偏移的非空输出槽；只在所有维度校验通过后写入。
 *
 * ## Returns
 * @return 长度匹配、每个索引在维度范围内且转换成功时为 `ZR_TRUE`，否则为 `ZR_FALSE`。
 *
 * ## Constraints and lifecycle
 * 使用逆序 stride 累积实现 row-major；浮点 shape/index 经整数 helper 截断。shape 可公开修改而 Get/Set 不重验总存储不变量；
 * 截断索引是否应被语言 API 拒绝仍需从 Tensor Get/Set 的整数参数契约核实。
 *
 * ## References
 * `ZrMath_Tensor_Get` 与 `ZrMath_Tensor_Set` 调用本函数；Get 通过 `ZrLib_Array_Get`/`ZrMath_NumberFromValue` 取值，Set 通过 `ZrMath_ArraySetValue` 写值。
 */
TZrBool ZrMath_TensorComputeOffset(SZrState *state, SZrObject *shape, SZrObject *indices, TZrSize *outOffset);
/**
 * ## Summary
 * 将 shape 各正维度相乘，供 Tensor 构造和 reshape 检查元素个数。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] shapeArray 维度数组；空数组按当前实现表示零维标量。
 *
 * ## Returns
 * @return 所有维度为正且读取成功时返回乘积；无效维度/读取失败返回 `-1`。
 *
 * ## Constraints and lifecycle
 * 浮点维度经 `ZrMath_ArrayReadInt` 截断；正维度乘积超出 int64 范围时返回 -1，乘法前检查上界。
 * shape 保留小数而运算按截断维度的兼容策略仍需核实。
 *
 * ## References
 * `ZrMath_TensorPopulate` 与 Tensor reshape callback 使用结果校验 data 元素数。
 */
TZrInt64 ZrMath_TensorTotalSize(SZrState *state, SZrObject *shapeArray);
/**
 * ## Summary
 * 创建由指定数量零浮点值组成的新 VM 数组；当前仓内未发现调用者。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[in] size 要尝试追加的零元素个数；非正值当前会得到空数组。
 *
 * ## Returns
 * @return 新分配的 VM 数组，初始分配失败返回 `ZR_NULL`。
 *
 * ## Constraints and lifecycle
 * 返回对象归 VM 管理；逐项 PushValue 的失败状态当前不检查。未发现仓内调用方，是否保留此 helper 需核查 metadata/原设计入口。
 *
 * ## References
 * `ZrMath_TensorMake` 使用调用方提供的 data，并未调用本函数；本函数仅有声明与定义。
 */
SZrObject *ZrMath_TensorMakeZeroData(SZrState *state, TZrInt64 size);

/**
 * ## Summary
 * 按 `printf` 风格格式化 Math callback 的字符串结果，供各类型 `toString` 回调复用。
 *
 * ## Parameters
 * @param[in] state 当前 VM 状态。
 * @param[out] result VM callback 结果槽。
 * @param[in] format NUL 结尾的 printf 格式串；后续变参必须与格式说明符匹配。
 * @param[in] ... 与格式串对应的值；只在本次同步格式化中读取。
 *
 * ## Returns
 * @return 当前实现调用字符串 setter 后总返回 `ZR_TRUE`；格式化/字符串发布失败不通过该状态报告。
 *
 * ## Constraints and lifecycle
 * 使用固定大小栈缓冲区；过长文本被截断且不检查 `vsnprintf`。短串 HASH_BUCKET 扩容软失败可使 result 未写而本函数仍成功（BUG）。
 * TODO: 核对长格式结果经 `ZrLib_Value_SetString`/`ZrCore_String_CreateTryHitCache` 是否按短串 inline-byte 布局比较，入口为 `zr_vm_core/src/zr_vm_core/string/string.c:958-974` 与 `zr_vm_core/include/zr_vm_core/conversion.h:73-76`；长文本仍由 VM 字符串对象管理。
 *
 * ## References
 * Vector、Quaternion、Complex、Matrix 与 Tensor 的 `MetaToString` callback 调用；实现交给 `ZrLib_Value_SetString`。
 */
TZrBool ZrMath_MakeStringResult(SZrState *state, SZrTypeValue *result, const TZrChar *format, ...);

#endif // ZR_VM_LIB_MATH_COMMON_H
