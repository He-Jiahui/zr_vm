/** @file
 * ## Summary
 * 为 zr.math.Complex 的构造、实例方法和 VM 运算协议声明 native 回调。
 * 注册表把同一组回调交给数学模块聚合器，脚本调用随后由 binding 同步派发。
 * Complex 以可读数值 real、imag 字段表示；普通运算交付新结果，不改写输入对象。
 *
 * ## Calling model and lifecycle
 * 应通过已注册的模块或 VM 元方法协议调用。context、state、参数视图和 result 槽
 * 由本次 binding 提供，仅在同步调用期间借用；不可跨调用缓存，也不建立长期 VM 根。
 * 返回对象和字符串归 VM 管理，保存裸对象仍须由宿主维持根。构造目标可能复用 self，
 * 其余对象运算按 Complex 类型创建新实例。线程与 GC 域沿用当前 binding 的准入约束。
 *
 * ## Failure model
 * 缺参或实参标签错误走不返回的 VM 错误入口；可读字段检查或对象 helper 正常失败
 * 可返回 ZR_FALSE。只在回调成功且结果契约满足时消费 result；GC 分配的异常退出不能
 * 作为普通 ZR_FALSE 推断。void 字段与字符串 setter 不提供逐项成功凭据。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:16 登记命名方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:26 登记构造和运算元方法。
 * - zr_vm_lib_math/src/zr_vm_lib_math/module.c:129 汇入类型；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202 与 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择回调。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274 进入 native GC 域，zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277 同步派发。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:219 选择构造目标；zr_vm_lib_math/src/zr_vm_lib_math/common.c:282 创建普通运算的新 Complex。
 */

#ifndef ZR_VM_LIB_MATH_COMPLEX_H
#define ZR_VM_LIB_MATH_COMPLEX_H

#include "zr_vm_lib_math/math_common.h"

/**
 * ## Summary
 * 为脚本 Complex(real, imag) 构造调用建立或初始化 binding 指定的目标，并交付该对象。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
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
 * void，成功返回不额外证明每个字段写入均完成。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:26 登记恰好两个实参；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:11 读取它们。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:211 复用兼容 self；zr_vm_lib_math/src/zr_vm_lib_math/common.c:215 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:219 选择新目标。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:249 写字段，zr_vm_lib_math/src/zr_vm_lib_math/common.c:252 交付对象；
 *   tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:5 是脚本调用。
 */
TZrBool ZrMath_Complex_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 magnitude() 调用方交付复数模长的 float 标量，供后续数值汇总使用。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；按值写入 float 标量，不转移对象所有权或创建 GC 根。
 *
 * ## Returns
 * @return 字段读取通过并写入 float 后为 ZR_TRUE；读取正常失败时为 ZR_FALSE，不消费结果。
 *
 * ## Constraints and lifecycle
 * 不改写 receiver，结果槽仅属于此次 binding。计算沿用宿主 double 的平方中间值，
 * 不是防溢出的范数实现，数值成功不代表结果有限。
 * BUG: 合法有限输入 (1e200, 0) 在 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:20 的平方中间值溢出，模长变为无穷。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:16 登记无参方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:19 和 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:20 读取并交付模长。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:323 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:324 定义分量读取约束。
 * - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:10 消费模长。 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1298 与 zr_vm_core/src/zr_vm_core/value.c:108 说明标量写入不建立 GC 对象。
 */
TZrBool ZrMath_Complex_Magnitude(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 phase() 调用方提供保留象限的相角，结果单位为弧度。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
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
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:18 登记无参方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:23 检查分量，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:24 写相角。
 * - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:7 消费结果。 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1298 与 zr_vm_core/src/zr_vm_core/value.c:108 说明标量写入不建立 GC 对象。
 */
TZrBool ZrMath_Complex_Phase(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 为 conjugate() 创建独立共轭对象，让调用方继续使用原 receiver。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 新对象发布后为 ZR_TRUE；字段读取或对象 helper 正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 不修改 receiver；新 Complex 归 VM 管理，不能当作由调用方释放的 C 对象。
 * 对象创建与字段写入可能进入分配路径；void 字段 setter 不提供逐项成功凭据。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:20 将方法交给 binding；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:202 选择命名方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:28 检查 receiver，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:29 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:31 写结果。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:27 和 zr_vm_lib_math/src/zr_vm_lib_math/common.c:33 说明类型实例与字段建立入口。
 */
TZrBool ZrMath_Complex_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 为 normalized() 创建归一化的新 Complex，原 receiver 保持不变。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 新对象发布后为 ZR_TRUE；字段读取或对象 helper 正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 模长不超过 ZR_MATH_EPSILON 时交付零复数，包括非零但接近零的输入；不能把
 * 每次成功视为单位模保证。新对象归 VM 管理，void 字段写入不提供逐项成功凭据。
 * BUG: 合法有限 (1e200, 0) 在 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:39 平方溢出，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:41 按无穷缩放得到零复数。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:22 登记方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:39 计算模长，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:40 选择零模策略。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:41 缩放非零路径，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:43 交付对象。
 * - tests/fixtures/projects/native_numeric_pipeline/src/signal.zr:6 是脚本调用。
 */
TZrBool ZrMath_Complex_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 的 + 运算协议映射为复数加法，创建独立结果并保持两输入不变。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
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
 * 保存裸 VM 对象须由上层保活；浮点中间值沿用宿主 double。void 字段 setter 不逐项
 * 报告成功；分配异常退出与 helper 的正常 NULL 返回应分别处理。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:27 登记一个显式实参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择元方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:48 检查输入，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:50 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:52 交付结果。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1224 处理缺参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1228 处理对象标签错误。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:323 与 zr_vm_lib_math/src/zr_vm_lib_math/common.c:324 仅核对可读数值字段。
 */
TZrBool ZrMath_Complex_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 的 二元 - 运算协议映射为左复数减右复数，创建独立结果并保持两输入不变。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
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
 * 保存裸 VM 对象须由上层保活；浮点中间值沿用宿主 double。void 字段 setter 不逐项
 * 报告成功；分配异常退出与 helper 的正常 NULL 返回应分别处理。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:28 登记一个显式实参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择元方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:57 检查输入，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:59 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:61 交付结果。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1224 处理缺参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1228 处理对象标签错误。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:323 与 zr_vm_lib_math/src/zr_vm_lib_math/common.c:324 仅核对可读数值字段。
 */
TZrBool ZrMath_Complex_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 的 * 运算协议映射为复数乘法，创建独立结果并保持两输入不变。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
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
 * 保存裸 VM 对象须由上层保活；浮点中间值沿用宿主 double。void 字段 setter 不逐项
 * 报告成功；分配异常退出与 helper 的正常 NULL 返回应分别处理。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:29 登记一个显式实参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择元方法回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:66 检查输入，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:68 建新对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:70 交付结果。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1224 处理缺参；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1228 处理对象标签错误。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:323 与 zr_vm_lib_math/src/zr_vm_lib_math/common.c:324 仅核对可读数值字段。
 */
TZrBool ZrMath_Complex_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 把 VM 一元 - 协议映射为符号相反的新 Complex，让原 receiver 保持不变。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 新对象发布后为 ZR_TRUE；字段读取或对象 helper 正常失败时为 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 结果是 VM 管理的新对象，不能由调用方释放或长期保存为未加根裸指针；void
 * 字段 setter 不逐项报告成功，分配异常退出沿用当前 binding 的 VM 恢复约束。
 *
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:30 登记无参元方法；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:205 选择回调。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:75 检查字段，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:76 建对象，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:78 交付结果。
 */
TZrBool ZrMath_Complex_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 按模长平方向 VM 比较协议提供 -1、0、1，不定义复数分量的字典序。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
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
 * NaN 或双方平方溢出也会使两个严格比较都不成立；使用方不能据此推断分量等价。
 * TODO: 结合 zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:89 的返回值与 VM compare 消费规则、数值边界用例核实
 *       unordered/双边溢出是否应与等模分开；当前策略意图尚未解明。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:31 登记比较元方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:86 检查输入。
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:88 计算平方模长，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:89 交付比较值。 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1291 与 zr_vm_core/src/zr_vm_core/value.c:84 说明标量写入不建立 GC 对象。
 */
TZrBool ZrMath_Complex_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * ## Summary
 * 向 VM toString 协议交付 Complex(real, imag) 调试文本，保留原 receiver。
 *
 * ## Parameters
 * @param[in] context 本次同步 binding 上下文；state 必须有效，参数视图与 self 只在此次
 *                    调用期借用。无显式实参；self 的 real、imag 必须可读为数值。
 * @param[out] result 本次 binding 提供的可写结果槽；成功结果归 VM 管理，调用方不释放。
 *
 * ## Returns
 * @return 字段读取正常失败为 ZR_FALSE；格式 helper 返回 ZR_TRUE。由于字符串 setter
 *         为 void，TRUE 本身不是已完成字符串分配或写入结果槽的额外证明。
 *
 * ## Constraints and lifecycle
 * 格式化使用有界临时 C 缓冲；返回字符串归 VM 管理，不借用该栈缓冲。result
 * 与 context 只借用本次调用；可能进入 GC 分配，异常退出不能视为普通返回成功。
 * BUG: 合法 receiver 生成未驻留短串时，若对象创建成功但短串哈希表扩容失败，
 *      setter 不写结果槽，helper 仍 TRUE；常规 VM 派发预置 NULL，因此成功状态可没有字符串结果。
 *      zr_vm_core/src/zr_vm_core/hash_set.c:181 经 zr_vm_core/include/zr_vm_core/hash_set.h:537、zr_vm_core/src/zr_vm_core/string.c:808 使 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1313 不写结果槽；常规派发在 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:362 预置 NULL，zr_vm_lib_math/src/zr_vm_lib_math/common.c:519 仍报告成功；静态证明。
 *
 * ## References
 * - zr_vm_lib_math/src/zr_vm_lib_math/complex/complex_registry.c:32 登记无参元方法；zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:95 检查分量，zr_vm_lib_math/src/zr_vm_lib_math/complex/complex.c:96 调用格式 helper。
 * - zr_vm_lib_math/src/zr_vm_lib_math/common.c:516 写临时文本，zr_vm_lib_math/src/zr_vm_lib_math/common.c:518 使用 void setter。
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1307 创建字符串；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1312 对 NULL 字符串直接返回。
 */
TZrBool ZrMath_Complex_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_COMPLEX_H
