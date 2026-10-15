//
// Tensor native callbacks.
//

#ifndef ZR_VM_LIB_MATH_TENSOR_H
#define ZR_VM_LIB_MATH_TENSOR_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Tensor 回调由 tensor_registry 绑定给 VM。构造参数是 shape 和 data 两数组，
 *  data 长度等于维度乘积；元素预期为数值，但当前构造尚未验证其类型。
 *  运算采用 row-major 布局；普通校验失败返回 ZR_FALSE，参数类别错误由读取 helper 抛出 VM 异常。
 */
/* @brief 复制输入数组并建立 Tensor 存储，避免调用者随后修改原数组影响实例。 */
TZrBool ZrMath_Tensor_Construct(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Clone(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 以相同元素数的新 shape 创建独立 Tensor；shape 和 data 均复制。 */
TZrBool ZrMath_Tensor_Reshape(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 原地更新数据，并把当前 Tensor 作为链式调用结果返回。 */
TZrBool ZrMath_Tensor_Fill(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 按完整的多维 indices 数组读取一个数值。 */
TZrBool ZrMath_Tensor_Get(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 按完整的多维 indices 数组原地写入一个数值。 */
TZrBool ZrMath_Tensor_Set(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Sum(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Mean(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 只接受 rank=2 的 Tensor，返回转置后的新实例。 */
TZrBool ZrMath_Tensor_Transpose2D(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 两侧均须为二维且左列数等于右行数，返回矩阵积的新实例。 */
TZrBool ZrMath_Tensor_Matmul(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Add(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_Sub(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_MulScalar(ZrLibCallContext *context, SZrTypeValue *result);
/* @brief 返回数据数组的外层副本，供脚本读取而不暴露内部数组对象。 */
TZrBool ZrMath_Tensor_ToArray(ZrLibCallContext *context, SZrTypeValue *result);
TZrBool ZrMath_Tensor_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * @fn TZrBool ZrMath_Tensor_Construct(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 构造元方法读取 shape 与 data，分别浅复制两个外层数组容器，再调用 TensorPopulate 发布实例字段。
 *
 * ## Parameters
 * @param[in] context 当前同步构造调用；必须在该回调期间使用，接收者由构造目标 helper 解析。
 * @param[out] result 成功时接收 Tensor 对象值；该槽由 VM/native dispatcher 管理。
 *
 * ## Returns
 * @return 目标解析、副本创建及 Populate 普通校验成功时为 ZR_TRUE，否则为 ZR_FALSE；非数组参数会由 ReadArray 抛 VM 类型异常。Populate 的 void 字段写入软失败仍可能使本 callback 报成功。
 *
 * ## Constraints and lifecycle
 * shape/data 仅浅复制外层数组。BUG: shape 维度通过 ArrayReadInt 转为 int64，有限越界值如 1e300 可到达未检查的 float-to-int64 cast；正维度乘积也没有 signed-overflow 检查。data 只检查长度、不检查数值标签，见 TensorPopulate.numeric-data-block；copy_array 的单次 ARRAY pin 扩容软失败也可能漏轴，见本批 helper 单元。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMeta、ZrMath_ResolveConstructTarget、ZrMath_TensorPopulate、ZrMath_TensorTotalSize；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Clone(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 通过当前 receiver 的存储创建独立 Tensor，供调用者保留同一数据形状下的副本。
 *
 * ## Parameters
 * @param[in] context 无显式实参；从当前调用期 receiver 读取 Tensor 存储。
 * @param[out] result 成功时接收新 Tensor 对象值，由 VM 管理其生命周期。
 *
 * ## Returns
 * @return storage 可读且两个副本和新对象建立成功时为 ZR_TRUE；读取、复制或 TensorMake 普通失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为零，数组元素仍按 VM 值浅复制。BUG: copy_array 忽略 PushValue 普通失败；对 shape [1,2]，若首项 append 的一次 ARRAY pin 扩容软失败而后续 append 成功，副本可变为 [2]，元素积不变而 clone 返回 rank=1。已有非数值 data 会原样浅复制；其来源与消费缺陷归 TensorPopulate.numeric-data-block。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.clone、ZrMath_TensorGetStorage、ZrMath_TensorMake、zr_math_tensor_copy_array。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Reshape(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 以新的 shape 和原有数据建立 Tensor，不原地改写 receiver。
 *
 * ## Parameters
 * @param[in] context 一个显式 shape 数组；其维度乘积须等于当前 Tensor 的 size。
 * @param[out] result 成功时接收新 Tensor 对象值，由 VM 管理其生命周期。
 *
 * ## Returns
 * @return shape 乘积与现有 size 匹配且对象创建成功时为 ZR_TRUE；普通 shape/存储/复制失败为 ZR_FALSE。ReadArray 非数组参数会抛 VM 类型异常。
 *
 * ## Constraints and lifecycle
 * shape/data 外层数组复制，元素浅复制。BUG: shape 项经 ArrayReadInt→IntFromValue 转换，有限越界 float（如 1e300）会到达未检查的 float-to-int64 cast，正维度乘积也可能 signed overflow；小数会向零截断。copy_array 的一次 ARRAY pin 扩容失败可把 [1,2] 变为 [2] 并通过相同 size 校验；numeric data 标签缺陷沿用 TensorPopulate.numeric-data-block。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.reshape、ZrMath_TensorTotalSize、ZrMath_TensorMake、zr_math_tensor_copy_array。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Fill(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将 receiver 当前 data 中每个元素尝试写成同一数值，并返回 receiver 以支持链式调用。
 *
 * ## Parameters
 * @param[in] context 一个可由 ReadFloat 接受的数值参数；receiver 在当前回调内读取。
 * @param[out] result 成功分支接收 receiver 的对象值，随后由 dispatcher 同步回调用方槽位。
 *
 * ## Returns
 * @return storage 和参数读取成功、循环完成时为 ZR_TRUE；普通 storage/读取失败时为 ZR_FALSE。ZR_TRUE 不保证每次底层数组写入都成功。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为一；原地更新，不复制 Tensor。TODO: ZrMath_ArraySetValue 不传播底层 SetValue 状态；沿 common.c:359-364 与数组写入失败路径核实 Fill 是否需报告部分写入。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.fill、ZrMath_TensorGetStorage、ZrMath_ArraySetValue。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Get(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将完整多维索引解析为 row-major 偏移，再读取并转换对应 data 元素。
 *
 * ## Parameters
 * @param[in] context 一个 indices 数组；其长度和值须满足 shape 对应维度和边界。
 * @param[out] result 成功时写入 double 形式的 VM 浮点值。
 *
 * ## Returns
 * @return storage、offset 和目标数值转换成功时为 ZR_TRUE；普通越界/缺项/非数值 data 失败为 ZR_FALSE。非数组实参由 ReadArray 抛 VM 类型异常。
 *
 * ## Constraints and lifecycle
 * descriptor 只接受一个数组实参。BUG: 元素 1e300 可由 ArrayReadInt→IntFromValue 直接进入无范围检查的 float-to-int64 cast，发生在索引边界判断之前；这是有效数组参数的可达转换缺陷。TODO: 有限小数索引被向零截断，策略未定义。indices/receiver 只在同步 callback 内借用。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.get、ZrMath_TensorComputeOffset、ZrMath_NumberFromValue、ZrMath_ArrayReadFloat。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Set(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 依据多维 indices 计算偏移并尝试原地替换 data 元素，成功路径返回 receiver。
 *
 * ## Parameters
 * @param[in] context indices 数组和一个 ReadFloat 可接受的数值；descriptor 按此顺序提供两个实参。
 * @param[out] result 写入 receiver 的对象值以供链式调用，并由 dispatcher 同步。
 *
 * ## Returns
 * @return storage/offset/value 读取通过时为 ZR_TRUE，普通检查失败为 ZR_FALSE；ZR_TRUE 不保证 void setter 确实写入。非数组 indices 由 ReadArray、非数值 value 由 ReadFloat 抛 VM 类型异常。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为二，不复制 Tensor。BUG: indices 中有效 float 值 1e300 经 ArrayReadInt→IntFromValue 到达未检查的 float-to-int64 cast，且先于边界检查。TODO: 小数索引向零截断；另有 ZrMath_ArraySetValue 忽略 SetValue 失败状态，需核实是否需报告未写入。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.set、ZrMath_TensorComputeOffset、ZrMath_ArraySetValue。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Sum(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 遍历当前 data，将 ArrayReadFloat 成功读取的元素累加为一个浮点和。
 *
 * ## Parameters
 * @param[in] context 无显式实参；数据由当前 receiver 提供。
 * @param[out] result 接收累加和的 VM 浮点值。
 *
 * ## Returns
 * @return receiver 存储可读时为 ZR_TRUE；存储无效时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为零。BUG: Construct/Populate 接受 data=["x"] 这类长度正确的非数值数组；该 callback 把失败项跳过，因而可对合法构造路径交付不完整的和。此为 TensorPopulate.numeric-data-block 的消费端单元，不另计同根因。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.sum、ZrMath_TensorGetStorage、ZrMath_ArrayReadFloat；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Mean(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 累加可读数值后按 TensorStorage.size 计算平均值。
 *
 * ## Parameters
 * @param[in] context 无显式实参；数据和 size 来自当前 receiver。
 * @param[out] result 接收计算得到的 VM 浮点值。
 *
 * ## Returns
 * @return 存储有效且 size 大于零时为 ZR_TRUE；空/无效存储或非正 size 时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为零。BUG: 长度正确的 Tensor data 可含非数值项而通过 Populate；callback 跳过该项但仍除以全部 size，因此结果不等于有效数值的平均值。缺陷根因与 Sum 共用 TensorPopulate.numeric-data-block。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.mean、ZrMath_TensorGetStorage、ZrMath_ArrayReadFloat；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Transpose2D(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 对 rank=2 的 row-major Tensor 交换行列坐标并尝试建立新的二维实例。
 *
 * ## Parameters
 * @param[in] context 无显式实参；矩阵 shape 和 data 取自当前 receiver。
 * @param[out] result 成功构造后接收新 Tensor 对象值。
 *
 * ## Returns
 * @return storage/rank/shape 检查及 TensorMake 通过时为 ZR_TRUE；普通校验或创建失败为 ZR_FALSE。data 项读取失败会按局部 0.0 继续。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为零，只接受二维 Tensor。BUG: Populate 不拒绝非数值 data，读取失败会以零代替；另若输出首维（原 cols）为 1，且唯一一次 shape ARRAY ignored-root 扩容软失败而后续 append 成功，首维丢失仍可通过元素积校验，见本函数 output-shape-softfail-block。shape 项越界 float-to-int64 风险继承共享 IntFromValue。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.transpose2D、ZrMath_TensorMake、ZrMath_ArrayReadFloat；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr（本 fixture 未调用 transpose2D）。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Matmul(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 计算两个二维 Tensor 的 row-major 矩阵积，返回新的 Tensor。
 *
 * ## Parameters
 * @param[in] context 一个矩阵对象或数组标签的对象值；其 Tensor 存储须可读取。
 * @param[out] result 成功时接收新 Tensor 对象值，由 VM 管理。
 *
 * ## Returns
 * @return 双方均为 rank=2、左列等于右行且结果创建成功时为 ZR_TRUE；存储、维度、构造或长度校验失败时为 ZR_FALSE。非对象标签由 ReadObject 抛出类型错误。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为一，不支持广播。BUG: Populate 可接受非数值 data，失败读取以局部 0.0 继续；若左行数为 1 且唯一一次输出 shape ARRAY ignored-root 扩容软失败、后续 append 成功，首维丢失仍可能通过总元素数检查，见 output-shape-softfail-block。shape 越界 float-to-int64 风险继承公共转换 helper。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.matmul、ZrMath_TensorGetStorage、ZrMath_TensorMake、ZrMath_ArrayReadFloat；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Add(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将 receiver 与同 shape 的另一个 Tensor 逐项相加并返回新实例。
 *
 * ## Parameters
 * @param[in] context 一个可读取 Tensor 存储的 object/array 实参。
 * @param[out] result 成功时接收新的 Tensor 对象值。
 *
 * ## Returns
 * @return shape 相等且结果对象创建成功时为 ZR_TRUE；存储、shape 或分配校验失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为一。BUG: 二元 helper 将 ArrayReadFloat 失败留作零初值，合法构造的非数值 data 会参与错误计算；其 shape 副本也使用忽略 PushValue 失败的 copy_array，单次 ARRAY pin 软失败可改变 rank。TODO: ShapeEquals 将浮点维度截断后比较，例如 [1.1] 与 [1.9] 可判等。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.add、zr_math_tensor_binary、ZrMath_TensorShapeEquals；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_Sub(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将 receiver 与同 shape 的另一个 Tensor 逐项相减并返回新实例。
 *
 * ## Parameters
 * @param[in] context 一个可读取 Tensor 存储的 object/array 实参。
 * @param[out] result 成功时接收新的 Tensor 对象值。
 *
 * ## Returns
 * @return shape 相等且结果对象创建成功时为 ZR_TRUE；存储、shape 或分配校验失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为一，receiver 为被减数。BUG: 二元 helper 将 ArrayReadFloat 失败留作零初值，合法构造的非数值 data 会参与错误计算；shape 副本共用 copy_array，单次 ARRAY pin 软失败可改变 rank。TODO: ShapeEquals 将浮点维度截断后比较，分数维度政策待核实。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.sub、zr_math_tensor_binary、ZrMath_TensorShapeEquals。
 */
/**
 * @fn TZrBool ZrMath_Tensor_MulScalar(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 将 receiver 中可读取的各数值乘以一个标量，建立新 Tensor。
 *
 * ## Parameters
 * @param[in] context 一个 ReadFloat 可接受的标量实参。
 * @param[out] result 成功时接收新的 Tensor 对象值。
 *
 * ## Returns
 * @return 存储可读且结果实例成功创建时为 ZR_TRUE；普通存储/分配失败时为 ZR_FALSE，错误标量标签由 ReadFloat 抛出类型错误。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为一，不修改 receiver。BUG: nonnumeric data 读取失败时局部零初值仍进入结果；shape copy 也可能因 copy_array 的一次 ARRAY pin 扩容软失败而漏轴，元素积相同则可成功返回改变 rank 的实例。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.mulScalar、ZrMath_TensorGetStorage、ZrMath_TensorMake；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_ToArray(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 复制 Tensor data 的外层数组并将副本作为普通 VM array 返回，避免直接返回内部容器。
 *
 * ## Parameters
 * @param[in] context 无显式实参；读取当前 receiver 的 data。
 * @param[out] result 成功时接收新数组对象，值元素沿 VM 值语义浅复制。
 *
 * ## Returns
 * @return 存储可读且副本对象存在时为 ZR_TRUE；存储或数组创建失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为零；返回容器与内部 data 不同，不递归复制元素对象。BUG: copy_array 忽略单次 ARRAY ignored-root pin 扩容软失败，可能返回较短数组并仍报告成功。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMethods.toArray、ZrMath_TensorGetStorage、zr_math_tensor_copy_array；tests/fixtures/projects/native_numeric_pipeline/src/tensor_pipeline.zr。
 */
/**
 * @fn TZrBool ZrMath_Tensor_MetaToString(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * 在对象字符串化阶段将缓存的 rank 与 size 格式化为 Tensor 摘要。
 *
 * ## Parameters
 * @param[in] context 无显式实参；只读取当前 receiver 的 Tensor 字段。
 * @param[out] result 由公共字符串 helper 写入 VM string 值。
 *
 * ## Returns
 * @return storage 校验通过且格式化 helper 返回 true 时为 ZR_TRUE；storage/helper 普通失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * descriptor arity 为零；VM 管理返回字符串。BUG: 若该格式化结果进入短串缓存且 HASH_BUCKET 扩容软失败，公共 helper 可保留 null result 却返回 true。TODO: 数值格式长度变化时的长串缓存布局另由 MakeStringResult.long-cache-block 核查。
 *
 * ## References
 * ZrMath_TensorRegistry_GetType.kMeta.toString、ZrMath_TensorGetStorage、ZrMath_MakeStringResult。
 */

#endif // ZR_VM_LIB_MATH_TENSOR_H
