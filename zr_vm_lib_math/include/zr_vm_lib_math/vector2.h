//
// Vector2 native callbacks.
//

#ifndef ZR_VM_LIB_MATH_VECTOR2_H
#define ZR_VM_LIB_MATH_VECTOR2_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 * Vector2 native constructor、instance methods 与 meta callbacks 的公开声明。
 *
 * ## Dispatch and error contract
 * `zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c` 为每个 callback 声明固定 arity；
 * `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:34,364` 在选择 lane 前检查数量，
 * 错误参数数目通过 `RaiseArityError` 在 callback 入口前报告。进入 callback 后，`ReadFloat` / `ReadObject`
 * 对错误标签走 VM `RaiseTypeError`（同文件 `:1152,1182,1221-1229`）；字段形状读取失败、构造 helper
 * 拒绝目标或实例未创建均可能作为普通 `ZR_FALSE` 返回。若 callback 返回 false 且线程状态仍为 FINE，
 * dispatcher 才把结果设为 null；非 FINE 状态保留错误并清理后传播（同文件 `:375-378,618-624,664-674`）。
 * result 在回调前初始化为 null（同文件 `:362`）。
 *
 * ## Constraints and lifecycle
 * callback 前提是 dispatcher 提供有效 context/state 与非空可写 result 槽，并在该 state 的 VM 调用域内执行。
 * context、receiver、实参及 result 槽仅在同步 callback 期间借用；不得保存到后续调用或异步任务。
 * 新对象和字符串由 VM 管理。注册表中的函数地址必须与 `zr.math` provider image 同寿命。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:16-36` 注册固定 arity 和 callback；
 * `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:34,364,618-624,664-674` 执行预检和结果处理。
 */

/**
 * ## Summary
 * 注册为 Vector2 constructor callback；读取两个数值实参，按 `x`、`y` 顺序初始化构造 receiver。
 *
 * ## Parameters
 * @param[in] context arity 为 2 的构造回调上下文；receiver 与两个数值实参由 VM 借用。
 * @param[out] result 成功时接收构造 receiver 或 fallback 实例；槽由 dispatcher 持有，callback 不释放对象。
 *
 * ## Returns
 * @return 通用构造 helper 接受上下文和目标时返回 `ZR_TRUE`；普通 helper 拒绝时返回 `ZR_FALSE`。
 * `ReadFloat` 的错误标签会抛 VM `TypeError`，不作为普通 false 返回；false 的后续处理见文件级约束。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=2；数量错误在 callback 前由 dispatcher 抛 `ArityError`，合法入口不以缺参方式进入。
 * `ConstructFloatObject` 复用与 owner prototype 兼容的 receiver，否则按 construct target/owner prototype 新建对象；
 * 字段名表为静态存储，数值 `values` 是仅供本次同步 helper 调用的栈数组。BUG:编译器生成的
 * CREATE_OBJECT/TO_STRUCT 后接 `__constructor` FUNCTION_CALL 可满足数值 fast lane；ignored-root 仅余一槽时，
 * setter 先 pin receiver，再 pin 未登记字段 key。扩表 RawMalloc 失败会静默漏写字段而 helper 仍发布对象。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:31`；调用生成：
 * `zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c:914-955,994-1003`；
 * receiver 选择、字段写入和发布：`zr_vm_lib_math/src/zr_vm_lib_math/common.c:200-219,231-252`。
 */
TZrBool ZrMath_Vector2_Construct(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 读取 receiver 的 `x`、`y`，计算欧氏长度并把 double 结果写入 dispatcher 的结果槽。
 *
 * ## Parameters
 * @param[in] context arity 为 0 的 `length()` 回调上下文；receiver 与 state 由 VM 借用至同步调用结束。
 * @param[out] result 接收浮点结果；结果槽归 VM/dispatcher 所有，callback 不释放。
 *
 * ## Returns
 * @return 两个字段可读时为 `ZR_TRUE`；字段缺失或非数值时为 `ZR_FALSE`，false 的处理见文件级约束。
 * BUG:有限输入 `(1e200, 0)` 的平方和溢出，当前实现返回正无穷。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0；receiver 不被修改，字段指针不跨回调保存。当前 `ZrMath_Dot` cast 的结构布局
 * 依赖项见 `math_common.h:20,54`。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:17`；
 * 计算：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；字段读取：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:297-300`；布局 TODO:
 * `zr_vm_lib_math/include/zr_vm_lib_math/math_common.h:20,54`。
 */
TZrBool ZrMath_Vector2_Length(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 读取 receiver 的 `x`、`y` 并返回分量平方和，供调用方直接取得平方范数。
 *
 * ## Parameters
 * @param[in] context arity 为 0 的 `lengthSquared()` 回调上下文；VM 对象引用仅借用至回调结束。
 * @param[out] result 接收 double 标量；结果槽由 dispatcher 管理。
 *
 * ## Returns
 * @return 两个字段可读时为 `ZR_TRUE`；字段缺失或非数值时为 `ZR_FALSE`，false 的处理见文件级约束。
 * IEEE 平方和溢出为无穷是本方法直接返回的平方结果。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0；不缓存对象或字段地址，也不修改 receiver。当前 `ZrMath_Dot` cast 的布局
 * 依赖项见 `math_common.h:20,54`。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:19`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；
 * 共享布局核查：`zr_vm_lib_math/include/zr_vm_lib_math/math_common.h:20,54`。
 */
TZrBool ZrMath_Vector2_LengthSquared(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 按 receiver 的长度缩放两个分量并返回新 `Vector2`；长度不大于 epsilon 时返回新零向量，receiver 不变。
 *
 * ## Parameters
 * @param[in] context arity 为 0 的 `normalized()` 回调上下文；receiver 及 state 为同步调用内的借用值。
 * @param[out] result 接收新建 VM 对象；成功后由 VM 管理，不由 callback 释放。
 *
 * ## Returns
 * @return 字段读取及实例分配成功时为 `ZR_TRUE`；普通读取/创建失败为 `ZR_FALSE`，后续处理见文件级约束。
 * BUG:有限输入 `(1e200, 0)` 的平方和溢出为无穷，当前结果成为零向量而非单位向量。
 * BUG:新对象字段 setter 的 ignored-root 扩表分配失败可静默省略 `x`、`y`，callback 仍发布对象。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0；结果由 `ZrMath_MakeVector2` 创建，字段 setter 是软失败且 helper 不校验；
 * 当前 `ZrMath_Dot` cast 的布局依赖项见 `math_common.h:20,54`。不保存 context 或字段指针。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:21`；
 * 回调及 factory：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；
 * `ZrMath_MakeVector2`：`zr_vm_lib_math/src/zr_vm_lib_math/common.c:22-35,255-259`；
 * 布局 TODO:`zr_vm_lib_math/include/zr_vm_lib_math/math_common.h:20,54`。
 */
TZrBool ZrMath_Vector2_Normalized(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 计算 receiver 与一个向量形状对象的点积；操作数按 `x`、`y` 字段读取，不要求相同名义原型。
 *
 * ## Parameters
 * @param[in] context arity 为 1 的 `dot(other)` 回调上下文；`other` 必须是对象或数组值。
 * @param[out] result 接收 double 结果，由 VM dispatcher 持有。
 *
 * ## Returns
 * @return 两个操作数的字段均可读时为 `ZR_TRUE`；字段形状不符时为 `ZR_FALSE`，后续处理见文件级约束。
 * 参数标签错误由 `ReadObject` 抛 `TypeError`；注册入口的数量错误在 callback 前抛 `ArityError`。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；object rhs 不满足 scalar fast-lane 条件，dispatcher 使用 pinned 或通用 lane。
 * 实现只读取快照、不修改操作数。TODO:核实 struct 地址转连续 double 数组的布局与 strict-aliasing 前提。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:23`；
 * 实现及 cast：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；
 * 共享布局核查：`zr_vm_lib_math/include/zr_vm_lib_math/math_common.h:20,54`。
 */
TZrBool ZrMath_Vector2_Dot(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 读取 receiver 与另一个向量形状对象的快照，返回两者欧氏距离。
 *
 * ## Parameters
 * @param[in] context arity 为 1 的 `distance(other)` 回调上下文；`other` 必须有数值 `x`、`y` 字段。
 * @param[out] result 接收 double 距离，由 dispatcher 管理。
 *
 * ## Returns
 * @return 两组字段均可读时为 `ZR_TRUE`；字段形状不符时为 `ZR_FALSE`，后续处理见文件级约束。
 * 错误参数标签由 `ReadObject` 抛 VM `TypeError`；参数数量在 callback 前由 dispatcher 检查。
 * BUG:`(1e200, 0)` 到 `(0, 0)` 的数学距离有限，但平方差中间值溢出并返回正无穷。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；object rhs 不满足 scalar fast-lane 条件，调用走 pinned 或通用 lane；不改操作数。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:24`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；字段读取：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:297-300`。
 */
TZrBool ZrMath_Vector2_Distance(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 按 `lhs + (rhs - lhs) * factor` 计算分量插值并创建新 `Vector2`；factor 可在 `[0,1]` 外外推。
 *
 * ## Parameters
 * @param[in] context arity 为 2 的 `lerp(other, factor)` 上下文；other 为对象/数组，factor 为 VM 数值。
 * @param[out] result 接收新建对象值，成功后归 VM 管理。
 *
 * ## Returns
 * @return 字段、factor 与新实例可用时为 `ZR_TRUE`；普通字段/创建失败为 `ZR_FALSE`，处理见文件级约束。
 * 错误标签由 `ReadObject` / `ReadFloat` 抛 `TypeError`；数量错误在 callback 前抛 `ArityError`。
 * BUG:有限端点 `(1e308, 0)`、`(-1e308, 0)` 和 `factor=0.5` 的中间差溢出为负无穷，数学中点为零。
 * BUG:新对象 setter 的 ignored-root 扩表分配失败可静默漏写字段，callback 仍发布对象。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=2；object rhs 不满足 scalar fast-lane 条件，dispatcher 使用 pinned 或通用 lane。
 * result 对象由 VM 管理；context、receiver 和临时字段不跨同步调用保存。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:26`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；factory：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:22-35,255-259`。
 */
TZrBool ZrMath_Vector2_Lerp(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 注册为加法 meta callback，读取 rhs 后创建分量相加的新 `Vector2`；不修改 receiver。
 *
 * ## Parameters
 * @param[in] context arity 为 1 的 operator callback 上下文；rhs 为含数值 `x`、`y` 的对象/数组。
 * @param[out] result 接收新对象值，成功后归 VM 管理。
 *
 * ## Returns
 * @return 两操作数字段可读且实例创建成功时为 `ZR_TRUE`；普通读取/分配失败为 `ZR_FALSE`，处理见文件级约束。
 * rhs 标签错误由 `ReadObject` 抛 `TypeError`；数量错误由 dispatcher 在 callback 前抛 `ArityError`。
 * BUG:新对象字段 setter 的 ignored-root 扩表分配失败可静默省略字段，但 callback 仍发布对象。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；object rhs 不满足 scalar fast-lane 条件；不保存输入引用或释放 VM 对象。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:32`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；factory：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:22-35,255-259`。
 */
TZrBool ZrMath_Vector2_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 注册为减法 meta callback，返回 receiver 减 rhs 的新 `Vector2`，不改写任一输入对象。
 *
 * ## Parameters
 * @param[in] context arity 为 1 的 operator callback 上下文；rhs 必须可读出数值 `x`、`y` 字段。
 * @param[out] result 接收新对象值，成功后由 VM 管理。
 *
 * ## Returns
 * @return 输入字段和实例创建成功时为 `ZR_TRUE`；普通字段读取/分配失败为 `ZR_FALSE`，处理见文件级约束。
 * rhs 标签错误由 `ReadObject` 抛 `TypeError`；数量错误在 callback 前由 dispatcher 抛 `ArityError`。
 * BUG:新对象字段 setter 的 ignored-root 扩表分配失败可静默省略字段，但 callback 仍发布对象。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；object rhs 不满足 scalar fast-lane 条件；context 和输入快照不跨调用保存。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:33`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；factory：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:22-35,255-259`。
 */
TZrBool ZrMath_Vector2_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 注册为一元取负 meta callback，读取 receiver 并创建两个分量取反的新 `Vector2`。
 *
 * ## Parameters
 * @param[in] context arity 为 0 的元方法上下文；receiver 由 VM 在同步 callback 期间借用。
 * @param[out] result 接收新对象值，成功后归 VM 管理。
 *
 * ## Returns
 * @return receiver 字段可读且实例创建成功时为 `ZR_TRUE`；普通读取/分配失败为 `ZR_FALSE`，处理见文件级约束。
 * BUG:新对象字段 setter 的 ignored-root 扩表分配失败可静默省略字段，但 callback 仍发布对象。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0；不修改 receiver，不释放或保存 VM 对象。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:34`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；factory：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:22-35,255-259`。
 */
TZrBool ZrMath_Vector2_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 按两个严格比较的结果返回 `-1`、`0` 或 `1`；返回 `0` 不证明分量相等，NaN/溢出策略见约束。
 *
 * ## Parameters
 * @param[in] context arity 为 1 的 compare meta callback 上下文；rhs 为可读 `x`、`y` 的对象/数组。
 * @param[out] result 接收有符号整型比较结果，由 dispatcher 管理。
 *
 * ## Returns
 * @return 两边字段可读时为 `ZR_TRUE` 并写入比较值；字段形状错误时为 `ZR_FALSE`，处理见文件级约束。
 * rhs 标签错误由 `ReadObject` 抛 `TypeError`；参数数量在 callback 前抛 `ArityError`。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=1；object rhs 不满足 scalar fast-lane 条件；比较平方和且不修改操作数。
 * TODO:对照 `docs/wiki/03-modules/math-api.md:296-308` 的 Vector2 compare 契约，为 NaN、双边平方和
 * 溢出为 Inf，以及当前 struct cast 的布局假设分别补核查/断言。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:35`；
 * 实现及比较表达式：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；布局 TODO:
 * `zr_vm_lib_math/include/zr_vm_lib_math/math_common.h:20,54`。
 */
TZrBool ZrMath_Vector2_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);

/**
 * ## Summary
 * 将 receiver 格式化为 `Vector2(%g, %g)` 并把 VM 字符串写入结果槽。
 *
 * ## Parameters
 * @param[in] context arity 为 0 的 toString meta callback 上下文；receiver/state 仅在同步调用内借用。
 * @param[out] result 接收 VM 字符串值，所有权由 VM 管理。
 *
 * ## Returns
 * @return 字段解析成功时转交格式化 helper 的 bool；普通字段读取失败为 `ZR_FALSE`，处理见文件级约束。
 * BUG:仅当格式结果是未驻留的短字符串且 short-intern 表需要 rehash 时，`HASH_BUCKET` RawMalloc 失败会
 * 令字符串创建返回 null；`SetString` 不写 result，而 helper 仍返回 true，dispatcher 因 true 保留初始 null。
 *
 * ## Constraints and lifecycle
 * descriptor 固定 arity=0；格式临时缓冲仅在 callback 中使用，返回字符串不得由 callback 释放。
 *
 * ## References
 * 注册：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2_registry.c:36`；
 * 实现：`zr_vm_lib_math/src/zr_vm_lib_math/vector/vector2.c`；精确字符串链：
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:512-519`、
 * `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1303-1316`、
 * `zr_vm_core/src/zr_vm_core/string/string.c:767-817,958-973`。
 */
TZrBool ZrMath_Vector2_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_VECTOR2_H
