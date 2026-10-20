/** @file
 * ## Summary
 * 为 zr.math.Complex 的构造、实例方法和 VM 运算协议声明 native 回调。
 * 注册表把同一组回调交给数学模块聚合器，脚本调用随后由 binding 同步派发。
 * Complex 以可读数值 real、imag 字段表示；产生 Complex 的实例方法与算术元运算交付新对象，
 * 标量方法交付值槽，toString 交付 VM 字符串，均不改写输入对象。
 *
 * ## Constraints and lifecycle
 * 应通过已注册的模块或 VM 元方法协议调用；context、state 与可写 result 必须有效。
 * 由本次 binding 提供，仅在同步调用期间借用；不可跨调用缓存，也不建立长期 VM 根。
 * 返回对象和字符串归 VM 管理，保存裸对象仍须由宿主维持根。构造目标可能复用 self，
 * 其余对象运算按 Complex 类型创建新实例。线程与 GC 域沿用当前 binding 的准入约束。
 *
 * ## Failure model
 * 缺参或实参标签错误走不返回的 VM 错误入口；可读字段检查或对象 helper 正常失败
 * 可返回 ZR_FALSE。只在回调成功且结果契约满足时消费 result；GC 分配的异常退出不能
 * 作为普通 ZR_FALSE 推断。字段 setter 的失败传播与字符串 OOM 均可能通过 Throw 非局部退出。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:22 登记命名方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:32 登记构造和运算元方法。
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:129 汇入类型；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202 与 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择回调。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274 进入 native GC 域，zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277 同步派发。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:218 复用兼容目标、zr_vm_lib_math/src/zr_vm_lib_math/common.c:222 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:226 选择新目标；zr_vm_lib_math/src/zr_vm_lib_math/common.c:286 创建运算结果的新 Complex。
 * 当前字段写入在有效 state 上失败会 Throw，不会沿旧 ignored-root 软失败前提继续成功；后段失败不回滚先前已发布字段。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:14 转抛失败；zr_vm_core/src/zr_vm_core/object/object_write.c:138 捕获单次写入、zr_vm_core/src/zr_vm_core/object/object_write.c:151 释放临时租约、zr_vm_core/src/zr_vm_core/object/object_write.c:161 发布状态。对象继续归 VM/GC 管理。
 */

#ifndef ZR_VM_LIB_MATH_COMPLEX_H
#define ZR_VM_LIB_MATH_COMPLEX_H

#include "zr_vm_lib_math/math_common.h"

/**
 * ## Summary
 * 为脚本 Complex(real, imag) 构造调用建立或初始化 binding 指定的目标，并交付该对象。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。位置 0、1 依次为 real、imag，均须为可读取的 VM 数值；不要求 self 预先有分量字段。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 目标建立并发布后为 ZR_TRUE；目标 helper 正常返回 NULL 时为 ZR_FALSE。缺参或
 *         非数值实参通过 VM 异常报告，不能按普通 false 处理。
 *
 * ## Constraints and lifecycle
 * 兼容 owner prototype 的 self 可复用；否则优先按 construct target、再按 owner
 * prototype 创建实例。该接口不承诺始终分配新对象。结果由 VM 管理；字段 setter 为
 * void；有效 state 上字段写入失败会 Throw，普通完成后才继续发布 result。
 * 复用目标的后段写入失败可保留前面字段；新对象归 VM/GC 管理，本回调不手动销毁或整体回滚。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:32 登记恰好两个实参；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:17 读取它们。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:218 复用兼容 self；zr_vm_lib_math/src/zr_vm_lib_math/common.c:222 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:226 选择新目标。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:256 逐字段写入，zr_vm_lib_math/src/zr_vm_lib_math/common.c:259 经 FinishConstructObject 交付对象；
 *   tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:5 是脚本调用。
 */
TZrBool ZrMath_Complex_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 magnitude() 调用方交付复数模长的 float 标量，供后续数值汇总使用。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；按值写入 float 标量，不转移对象所有权或创建 GC 根。
 *
 * ## Returns
 * @return 字段读取通过并写入 float 后为 ZR_TRUE；读取正常失败时为 ZR_FALSE，不消费结果。
 *
 * ## Constraints and lifecycle
 * 不改写 receiver，结果槽仅属于此次 binding。计算沿用宿主 double 的平方中间值，
 * 不是防溢出的范数实现，数值成功不代表结果有限。
 * BUG: 合法有限输入 (1e200, 0) 在 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:26 的平方中间值溢出，模长变为无穷。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:22 登记无参方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:25 和 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:26 读取并交付模长。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:330 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:331 定义分量读取约束。
 * - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:10 消费模长；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1304 与 zr_vm_core/src/zr_vm_core/value/value.c:103 定义标量写入。
 */
TZrBool ZrMath_Complex_Magnitude(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 phase() 调用方提供保留象限的相角，结果单位为弧度。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；按值写入 float 标量，不转移对象所有权或创建 GC 根。
 *
 * ## Returns
 * @return 字段读取通过并写入 float 后为 ZR_TRUE；读取正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * receiver 不变，result 仅借用本次调用；带符号零与非有限输入沿用宿主 atan2
 * 语义。接口不提供角度制转换，也不承诺对非有限输入归一化。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:24 登记无参方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:29 检查分量，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:30 写相角。
 * - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:7 消费结果；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1304 与 zr_vm_core/src/zr_vm_core/value/value.c:103 定义标量写入。
 */
TZrBool ZrMath_Complex_Phase(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 为 conjugate() 创建独立共轭对象，让调用方继续使用原 receiver。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 新对象发布后为 ZR_TRUE；字段读取或对象 helper 正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 不修改 receiver；新 Complex 归 VM 管理，不能当作由调用方释放的 C 对象。
 * 对象创建与字段写入可能分配；有效 state 上写入失败会 Throw，已写字段不整体回滚，正常对象结果归 VM/GC 管理。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:26 将方法交给 binding；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202 选择命名方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:34 检查 receiver，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:35 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:37 写结果。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:27 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:33 说明类型实例与字段建立入口。
 */
TZrBool ZrMath_Complex_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 为 normalized() 创建归一化的新 Complex，原 receiver 保持不变。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 新对象发布后为 ZR_TRUE；字段读取或对象 helper 正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 模长不超过 ZR_MATH_EPSILON 时交付零复数，包括非零但接近零的输入；不能把
 * 每次成功视为单位模保证。新对象归 VM 管理；字段写入失败会 Throw，不保证复合操作回滚。
 * BUG: 合法有限 (1e200, 0) 在 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:45 平方溢出，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:47 按无穷缩放得到零复数。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:28 登记方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:45 计算模长，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:46 选择零模策略。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:47 缩放非零路径，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:49 交付对象。
 * - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:6 是脚本调用。
 */
TZrBool ZrMath_Complex_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 的 + 运算协议映射为复数加法，创建独立结果并保持两输入不变。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。self 和位置 0 的对象或数组实参须有可读数值 real、imag；不额外要求 rhs
 *                    的名义 prototype 为 Complex。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 字段读取与新对象建立通过、结果发布后为 ZR_TRUE；其正常失败为 ZR_FALSE。
 *         缺参或非对象/数组实参进入 VM 异常，不作为正常 false 返回。
 *
 * ## Constraints and lifecycle
 * 结果按 Complex 类型创建并归 VM 管理，不复用 rhs。输入只借用本次调用，
 * 保存裸 VM 对象须由上层保活；浮点中间值沿用宿主 double。字段写入失败会 Throw，
 * 已写字段不整体回滚；分配异常退出与 helper 的正常 NULL 返回应分别处理。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:33 登记一个显式实参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择元方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:54 检查输入，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:56 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:58 交付结果。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1231 处理缺参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1235 处理对象标签错误。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:330 与 zr_vm_lib_math/src/zr_vm_lib_math/common.c:331 仅核对可读数值字段。
 */
TZrBool ZrMath_Complex_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 的 二元 - 运算协议映射为左复数减右复数，创建独立结果并保持两输入不变。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。self 和位置 0 的对象或数组实参须有可读数值 real、imag；不额外要求 rhs
 *                    的名义 prototype 为 Complex。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 字段读取与新对象建立通过、结果发布后为 ZR_TRUE；其正常失败为 ZR_FALSE。
 *         缺参或非对象/数组实参进入 VM 异常，不作为正常 false 返回。
 *
 * ## Constraints and lifecycle
 * 结果按 Complex 类型创建并归 VM 管理，不复用 rhs。输入只借用本次调用，
 * 保存裸 VM 对象须由上层保活；浮点中间值沿用宿主 double。字段写入失败会 Throw，
 * 已写字段不整体回滚；分配异常退出与 helper 的正常 NULL 返回应分别处理。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:34 登记一个显式实参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择元方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:63 检查输入，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:65 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:67 交付结果。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1231 处理缺参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1235 处理对象标签错误。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:330 与 zr_vm_lib_math/src/zr_vm_lib_math/common.c:331 仅核对可读数值字段。
 */
TZrBool ZrMath_Complex_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 的 * 运算协议映射为复数乘法，创建独立结果并保持两输入不变。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。self 和位置 0 的对象或数组实参须有可读数值 real、imag；不额外要求 rhs
 *                    的名义 prototype 为 Complex。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 字段读取与新对象建立通过、结果发布后为 ZR_TRUE；其正常失败为 ZR_FALSE。
 *         缺参或非对象/数组实参进入 VM 异常，不作为正常 false 返回。
 *
 * ## Constraints and lifecycle
 * 结果按 Complex 类型创建并归 VM 管理，不复用 rhs。输入只借用本次调用，
 * 保存裸 VM 对象须由上层保活；浮点中间值沿用宿主 double。字段写入失败会 Throw，
 * 已写字段不整体回滚；分配异常退出与 helper 的正常 NULL 返回应分别处理。
 * BUG: 对合法有限输入 lhs = rhs = (1e200, 1e200)，实部数学结果为 0；两次乘法分别溢出为 Inf，当前 Inf - Inf 写入 NaN。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:35 登记一个显式实参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择元方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:72 检查输入，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:74 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:76 交付结果。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1231 处理缺参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1235 处理对象标签错误。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:330 与 zr_vm_lib_math/src/zr_vm_lib_math/common.c:331 仅核对可读数值字段。
 */
TZrBool ZrMath_Complex_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 一元 - 协议映射为符号相反的新 Complex，让原 receiver 保持不变。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 新对象发布后为 ZR_TRUE；字段读取或对象 helper 正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 结果是 VM 管理的新对象，不能由调用方释放或长期保存为未加根裸指针；有效 state 上
 * 字段写入失败会 Throw，不保证复合字段回滚；异常退出沿用当前 binding 的 VM 恢复约束。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:36 登记无参元方法；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:81 检查字段，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:82 建对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:84 交付结果。
 */
TZrBool ZrMath_Complex_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按模长平方向 VM 比较协议提供 -1、0、1，不定义复数分量的字典序。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。self 与位置 0 的对象或数组实参须有可读数值 real、imag；rhs 不额外做
 *                    名义 prototype 核对。
 * @param[out] result 本次 binding 提供的可写结果槽；按值写入 int 标量，不转移对象所有权或创建 GC 根。
 *
 * ## Returns
 * @return 分量读取通过并发布 int 后为 ZR_TRUE；字段正常读取失败时为 ZR_FALSE。
 *         缺参或实参标签错误通过 VM 异常报告。结果 0 本身不证明两个复数相等。
 *
 * ## Constraints and lifecycle
 * 不修改输入、不建立新 Complex；result 只属于本次 binding。相同模长可返回 0，
 * BUG: 不同合法有限输入 (1e200, 0) 与 (2e200, 0) 的平方模长都溢出为 Inf，当前返回 0，错误折叠严格不同的模长。
 * TODO: 核实 NaN/unordered compare 的 VM 约定与此回调应返回的 tri-state；下一入口为 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:94-95 与 zr_vm_core/src/zr_vm_core/value/meta.c:29-31。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:37 登记比较元方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:92 检查输入。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:94 计算平方模长、zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:95 交付比较值；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1297 与 zr_vm_core/src/zr_vm_core/value/value.c:79 定义标量写入。
 */
TZrBool ZrMath_Complex_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 VM toString 协议交付 Complex(real, imag) 调试文本，保留原 receiver。
 *
 * ## Parameters
 * @param[in,out] context 本次同步 binding 上下文；读取 self/实参会刷新其栈视图，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 字段读取正常失败为 ZR_FALSE；有效 context/state/result 下格式 helper 正常完成
 *         时返回 ZR_TRUE 并已写入 VM 字符串；字符串分配失败通过 Throw 离开，不是普通成功。
 *
 * ## Constraints and lifecycle
 * 格式化使用有界临时 C 缓冲；返回字符串归 VM 管理，不借用该栈缓冲。result
 * 与 context 只借用本次调用；可能进入 GC 分配，异常退出不能视为普通返回成功。
 * 格式化使用 ZR_MATH_FORMAT_BUFFER_LENGTH 的栈缓冲；返回文本由 VM 字符串持有。
 * 当前 SetString 对创建结果 NULL 调用 Throw(MEMORY_ERROR)，旧短串缓存软失败的成功结论不再适用。
 * 非局部退出由上层恢复点处理；无恢复点时可能终止进程，不能把异常视作 FALSE 或保证普通 C 尾部清理。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:38 登记无参元方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:101 检查分量，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:102 调用格式 helper。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:530 写临时文本，zr_vm_lib_math/src/zr_vm_lib_math/common.c:532 使用字符串 setter。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1316 创建字符串、zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1318 在 NULL 时 Throw、zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1321 发布字符串结果。
 */
TZrBool ZrMath_Complex_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_COMPLEX_H
