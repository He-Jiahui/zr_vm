//
// Matrix4x4 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_MATRIX4X4_H
#define ZR_VM_LIB_MATH_MATRIX4X4_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 * ## Summary
 * Matrix4x4 的原生回调接口；分量按 row-major 存储，使用 column vector 相乘，平移位于 m03、m13、m23，旋转工厂接收弧度。
 * ## Constraints and lifecycle
 * 所有 callback 只在当前 VM native 调用期间借用 context 与 result；不得跨回调或跨线程保存临时指针。数值路径使用 TZrFloat64，不拒绝 NaN/Inf。
 * ## References
 * ZrMath_Matrix4x4Registry_GetType 注册本文件回调；ZrLib_CallContext_ReadFloat、ZrLib_CallContext_ReadObject、ZrMath_ReadMatrix4Object 提供输入读取边界。
 */

/**
 * ## Summary
 * 由元类型 constructor callback 初始化零参数单位矩阵或十六个行主序分量，并交付兼容的构造目标。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；不得在回调返回后保存。
 * @param[out] result VM 提供的结果槽；成功时接收构造目标，可能是 compiler 预置的兼容 receiver。
 *
 * ## Returns
 * @return 成功写入对象时返回 ZR_TRUE；无法建立目标时返回 ZR_FALSE。缺参/错数值类别由 ReadFloat/Arity Raise API 抛 VM 异常。
 *
 * ## Constraints and lifecycle
 * BUG: 当前只接受 0 或 16 个参数，但 descriptor 声明 0..16；1..15 会在本回调抛出范围误导的 ArityError。构造目标可复用 receiver；16 个字段经 void setter 写入，默认 allocator 对单次 HASH_BUCKET 扩容 realloc 软失败可静默漏字段而仍成功。自定义 allocator 的扩容失败契约待核，入口为 zr_vm_core/include/zr_vm_core/memory.h。
 *
 * ## References
 * ZrMath_ConstructFloatObject、ZrMath_ResolveConstructTarget、ZrMath_FinishConstructObject、ZrMath_Matrix4x4Registry_GetType。
 */
TZrBool ZrMath_Matrix4x4_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 静态 identity 方法创建新的单位矩阵对象，不依赖 receiver。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文。
 * @param[out] result VM 提供的结果槽；成功时接收 VM 管理的 Matrix4x4 对象。
 *
 * ## Returns
 * @return 对象创建成功时返回 ZR_TRUE；对象创建返回 null 时返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 无参数静态方法，不读取 self。BUG: 共享 MakeMatrix4x4 在 HASH_BUCKET 单次扩容失败时可能发布不完整对象；详细触发前提见 matrix4x4.c 的字段发布注释。
 *
 * ## References
 * zr_math_matrix4_identity、ZrMath_MakeMatrix4x4、ZrMath_Matrix4x4Registry_GetType。
 */
TZrBool ZrMath_Matrix4x4_Identity(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实例方法读取 self 的 16 个行主序字段并转置到新的 Matrix4x4。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文，含 receiver。
 * @param[out] result VM 提供的结果槽；成功时接收新对象。
 *
 * ## Returns
 * @return 字段读取或对象创建失败时返回 ZR_FALSE；成功写入对象时返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 不修改 receiver；字段读取使用局部快照，输出不与输入别名。缺失或非数值字段是普通读取失败；新对象共享字段工厂有窄化软失败风险。
 *
 * ## References
 * ZrMath_ReadMatrix4Object、ZrMath_MakeMatrix4x4、ZrMath_Matrix4x4Registry_GetType。
 */
TZrBool ZrMath_Matrix4x4_Transpose(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实例方法在局部副本上计算 4×4 行列式，并把标量写入 result。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文，含 receiver。
 * @param[out] result VM 提供的结果槽；成功时写入 float 值。
 *
 * ## Returns
 * @return 字段读取失败返回 ZR_FALSE；否则写入 helper 结果并返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * 实现用部分选主元和固定绝对阈值；该阈值会误判可逆矩阵，累计乘积亦可溢出，具体合法有限矩阵见私有 helper 注释。计算不改写 receiver。
 *
 * ## References
 * zr_math_matrix4_det、ZR_MATH_EPSILON、ZrMath_ReadMatrix4Object、matrix4x4_registry.c determinant descriptor。
 */
TZrBool ZrMath_Matrix4x4_Determinant(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 实例方法通过局部副本做 Gauss-Jordan 求逆，成功时返回新的 Matrix4x4。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；实例方法从中读取 receiver。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Matrix4x4 对象；失败时 callback 不写此槽，dispatcher 负责按 false 状态归一化。
 *
 * ## Returns
 * @return 字段读取、inverse helper 或对象 factory 普通失败时返回 ZR_FALSE；成功写入新对象后返回 ZR_TRUE。此方法无显式实参，缺字段不会由本 callback 主动 Raise。
 *
 * ## Constraints and lifecycle
 * 固定绝对阈值使条件数为 1 的 diag(1e-10,1e-10,1e-10,1e-10) 返回普通失败；dispatcher 在 VM 无异常时将 callback false 归一化为 null，而 registry 声明 Matrix4x4。TODO: 核实这一数值边界是否符合公开逆矩阵政策，并确认脚本是否接受 false 到 null 的结果约定；下一步检查 descriptor 返回声明与脚本调用路径。新对象字段工厂另有 HASH_BUCKET 单故障部分发布 BUG。
 *
 * ## References
 * zr_math_matrix4_inverse、ZR_MATH_EPSILON、ZrMath_ReadMatrix4Object、ZrMath_MakeMatrix4x4、kMethods.inverse。
 */
TZrBool ZrMath_Matrix4x4_Inverse(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实例方法将 self 行主序矩阵乘以列向量 Vector4，并创建新的 Vector4 结果。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；实例方法从中读取 receiver。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Vector4 对象。
 *
 * ## Returns
 * @return 完成结果写入返回 ZR_TRUE；普通字段/helper/factory 失败返回 ZR_FALSE。读取缺参或错误值类别时由 Raise API 抛出 VM 异常。
 *
 * ## Constraints and lifecycle
 * 读取一个 OBJECT/ARRAY 且四个向量字段可读的参数；结果用四个标量分量写入新 VM 对象。有限输入的乘积可溢出并产生 NaN/Inf；不做有限值验证。缺字段是普通 false，缺参/错类别由 ReadObject 抛 VM 异常。
 *
 * ## References
 * ZrMath_ReadMatrix4Object、ZrMath_ReadVector4Object、ZrMath_MakeVector4、kMethods.mulVector。
 */
TZrBool ZrMath_Matrix4x4_MulVector(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实例方法计算 self × 参数矩阵，按行主序创建新的 Matrix4x4。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；实例方法从中读取 receiver。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Matrix4x4 对象。
 *
 * ## Returns
 * @return 完成结果写入返回 ZR_TRUE；普通字段/helper/factory 失败返回 ZR_FALSE。读取缺参或错误值类别时由 Raise API 抛出 VM 异常。
 *
 * ## Constraints and lifecycle
 * 参数必须是可读取 m00..m33 的 OBJECT/ARRAY；矩阵乘法 helper 的输入和输出位于互不重叠的局部数组。浮点乘加可溢出；输出字段发布有 factory BUG。缺字段或 factory 返回 null 时普通 false。
 *
 * ## References
 * zr_math_matrix4_mul、ZrMath_ReadMatrix4Object、ZrMath_MakeMatrix4x4、kMethods.mulMatrix。
 */
TZrBool ZrMath_Matrix4x4_MulMatrix(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 静态工厂用三个平移分量构造齐次变换矩阵。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文，承载三个平移 float 参数；本静态方法不读取 self。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Matrix4x4 对象。
 *
 * ## Returns
 * @return 三个 ReadFloat 参数读取与对象 factory 成功时返回 ZR_TRUE；factory 返回 null 时返回 ZR_FALSE。ReadFloat 对缺参或错误类型使用 VM Raise API。
 *
 * ## Constraints and lifecycle
 * 三个参数依次写入 m03、m13、m23；其余值来自单位矩阵。参数缺失或类型不受支持时 ReadFloat 使用 VM Raise API。输出字段发布有 factory BUG。
 *
 * ## References
 * zr_math_matrix4_identity、ZrMath_MakeMatrix4x4、ZrLib_CallContext_ReadFloat、kMethods.translation。
 */
TZrBool ZrMath_Matrix4x4_Translation(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 静态工厂用 x/y/z 比例创建齐次缩放矩阵。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；静态方法不读取 self。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Matrix4x4 对象。
 *
 * ## Returns
 * @return ReadFloat 与对象创建成功时返回 ZR_TRUE；factory 返回 null 时返回 ZR_FALSE。ReadFloat 对缺参或非数值标签通过 VM Raise API 抛出异常。
 *
 * ## Constraints and lifecycle
 * 三个 float 参数依次写入 m00、m11、m22；m33 保持 1。参数缺失或错类型由 ReadFloat 的 VM Raise API 处理；返回对象由 VM 管理，字段发布有共享 factory BUG。
 *
 * ## References
 * ZrMath_MakeMatrix4x4、ZrLib_CallContext_ReadFloat、kMethods.scale、native_math_export_probe。
 */
TZrBool ZrMath_Matrix4x4_Scale(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 静态工厂构造绕 X 轴的齐次旋转矩阵。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；静态方法不读取 self。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Matrix4x4 对象。
 *
 * ## Returns
 * @return ReadFloat 与对象创建成功时返回 ZR_TRUE；factory 返回 null 时返回 ZR_FALSE。ReadFloat 对缺参或非数值标签通过 VM Raise API 抛出异常。
 *
 * ## Constraints and lifecycle
 * 接受 ReadFloat 支持的有符号/无符号整数及 float/double 标签；其他标签由 RaiseTypeError 处理。正规 descriptor arity 检查先拦截缺参；参数按弧度解释，不做角度换算或有限值验证。返回对象由 VM 管理，字段发布有共享 factory BUG。
 *
 * ## References
 * sin、cos、ZrMath_MakeMatrix4x4、kMethods.rotationX。
 */
TZrBool ZrMath_Matrix4x4_RotationX(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 静态工厂构造绕 Y 轴的齐次旋转矩阵。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；静态方法不读取 self。
 * @param[out] result VM 提供的结果槽；成功时接收新的 Matrix4x4 对象。
 *
 * ## Returns
 * @return ReadFloat 与对象创建成功时返回 ZR_TRUE；factory 返回 null 时返回 ZR_FALSE。ReadFloat 对缺参或非数值标签通过 VM Raise API 抛出异常。
 *
 * ## Constraints and lifecycle
 * 接受 ReadFloat 支持的有符号/无符号整数及 float/double 标签；其他标签由 RaiseTypeError 处理。正规 descriptor arity 检查先拦截缺参；参数按弧度解释，不做角度换算或有限值验证。返回对象由 VM 管理，字段发布有共享 factory BUG。
 *
 * ## References
 * sin、cos、ZrMath_MakeMatrix4x4、kMethods.rotationY。
 */
TZrBool ZrMath_Matrix4x4_RotationY(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 静态工厂构造绕 Z 轴的齐次旋转矩阵。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文，承载一个弧度 float 参数；本静态方法不读取 self。
 * @param[out] result VM 提供的结果槽；成功时接收 VM 管理的 Matrix4x4 对象。
 *
 * ## Returns
 * @return 参数读取或对象 factory 普通失败时返回 ZR_FALSE；成功写入新的 Matrix4x4 后返回 ZR_TRUE。ReadFloat 对缺参或错误类型使用 VM Raise API。
 *
 * ## Constraints and lifecycle
 * 单个 float 参数按弧度解释；不做角度换算或有限值验证。返回对象由 VM 管理，字段发布有共享 factory BUG。
 *
 * ## References
 * sin、cos、ZrMath_MakeMatrix4x4、kMethods.rotationZ。
 */
TZrBool ZrMath_Matrix4x4_RotationZ(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 实现 `*` 元方法；按右操作数 prototype name 分派到 Vector4 或 Matrix4x4 乘法。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；不得跨回调保存。
 * @param[out] result VM 提供的结果槽；成功时接收 Vector4 或 Matrix4x4 对象。
 *
 * ## Returns
 * @return ReadObject 缺参或类型错误时通过 VM 异常退出；字段读取或对象 factory 普通失败返回 ZR_FALSE；成功写入 Vector4/Matrix4x4 对象返回 ZR_TRUE。
 *
 * ## Constraints and lifecycle
 * ReadObject 接受 OBJECT/ARRAY；缺参或值类型错误会抛 VM 异常。只有 prototype name 精确等于 Vector4 才走向量分支，其余值需含可读矩阵字段。分支结果为不同对象类型，字段/helper失败普通返回 ZR_FALSE。
 *
 * ## References
 * ZrMath_ObjectTypeEquals、ZrMath_Matrix4x4_MulVector、ZrMath_Matrix4x4_MulMatrix、kMeta.mul。
 */
TZrBool ZrMath_Matrix4x4_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 实现 Matrix4x4 字符串元方法，将十六个分量按四行格式化。
 *
 * ## Parameters
 * @param[in] context 当前 VM native callback 的借用上下文；本方法无显式参数，不得跨回调保存。
 * @param[out] result VM 提供的结果槽；成功路径尝试写入 VM 管理的字符串；短串缓存扩容软失败时可能未写槽。
 *
 * ## Returns
 * @return 字段读取失败时返回 ZR_FALSE；MakeStringResult 的返回值直接作为 callback 结果，短串缓存扩容软失败时仍可能返回 ZR_TRUE 而未写结果槽。此方法无显式参数，不会因参数读取抛异常。
 *
 * ## Constraints and lifecycle
 * 读字段失败普通返回 ZR_FALSE；结果为 VM 管理字符串。BUG: 短串 cache miss 且 intern bucket 扩容 Memory_Allocate 软失败时，SetStringObject 不写结果而 MakeStringResult 仍报告成功。长输出仍进入 CreateTryHitCache，桶索引由栈局部缓冲区地址计算；命中同桶时查找比较经 ZR_CAST_STRING_TO_NATIVE 按短串内联布局读取缓存项。TODO: 核查长串/短串共桶及同地址复用是否可达并影响格式结果；下一步追踪 conversion.h:73-76 与 string.c:958-974 的 cache lookup/replace，不据此声称悬空引用。
 *
 * ## References
 * ZrMath_ReadMatrix4Object、ZrMath_MakeStringResult、ZrLib_Value_SetString、native_binding_create_string、ZrCore_String_CreateTryHitCache。
 */
TZrBool ZrMath_Matrix4x4_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_MATRIX4X4_H
