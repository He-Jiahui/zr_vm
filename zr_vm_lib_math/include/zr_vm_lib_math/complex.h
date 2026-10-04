//
// Complex native callbacks.
//

#ifndef ZR_VM_LIB_MATH_COMPLEX_H
#define ZR_VM_LIB_MATH_COMPLEX_H

#include "zr_vm_lib_math/math_common.h"

/** @file
 *  Complex 的 VM 回调表接口。对象以实部、虚部字段表示；phase 返回弧度；
 *  归一化零复数时返回零复数，非构造运算的对象结果为新实例。
 *  context/state/result 由 binding 提供，仅借用本次调用；不可缓存无根 VM 对象。
 */
/**
 * @brief 为 Complex 构造调用初始化 binding 指定的目标。
 * context/state/result 仅借用本次调用；两个实参依次是数值 real、imag。
 * 兼容 self 可以复用；否则按 construct target/owner prototype 创建目标，不要求 receiver 已有可读分量。
 * 缺参或非数值由 ReadFloat 进入 VM 异常；目标无法建立时返回 false。
 * 结果归 VM 管理；字段 setter 为 void，本回调不额外确认每个字段写入成功。
 */
TZrBool ZrMath_Complex_Construct(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 返回 receiver 的模长 float，供脚本数值计算使用。
 * 无显式实参；self 的 real/imag 须可读为数值，失败返回 false。
 * 结果写入本次 binding 的 VM 槽，receiver 不变。
 * BUG: 有限 (1e200,0) 会使平方中间值溢出，模长错误地成为无穷。
 */
TZrBool ZrMath_Complex_Magnitude(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 返回 receiver 的相角 float，单位是弧度。
 * 无显式实参；real/imag 必须可读为数值，否则返回 false。
 * 不修改 receiver；结果写入本次 binding 槽，带符号零/非有限值沿用宿主 atan2 语义。
 */
TZrBool ZrMath_Complex_Phase(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 返回新的 Complex 共轭对象，保持 receiver 不变。
 * 无显式实参；real/imag 字段不可读或结果对象 helper 返回 NULL 时返回 false。
 * 新对象交给本次 binding 的结果槽并归 VM 管理，调用方不释放。
 */
TZrBool ZrMath_Complex_Conjugate(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 返回归一化的新 Complex；模长不超过 EPSILON 时选择零对象。
 * 无显式实参；数值字段读取或新对象 helper 失败时返回 false。
 * receiver 不变，结果归 VM 管理。
 * BUG: (1e200,0) 的平方范数溢出，非零复数会被缩放成零对象。
 */
TZrBool ZrMath_Complex_Normalized(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 为 VM 的 + 协议返回新的 Complex 和，保持两操作数不变。
 * self 与第一个对象实参均须有可读数值 real/imag；这里只核字段，不额外核 rhs 的名义原型。
 * 缺参/非对象实参走 VM 异常；字段读取或新对象 helper 失败返回 false。
 * 结果写入本次 binding 槽并归 VM 管理。
 */
TZrBool ZrMath_Complex_MetaAdd(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 为 VM 的二元 - 协议返回左减右的新 Complex，保持两操作数不变。
 * self 与第一个对象实参须有可读数值 real/imag；rhs 只经字段核查。
 * 缺参/非对象实参走 VM 异常；字段读取或新对象 helper 失败返回 false。
 * 新结果归 VM 管理，交给本次 binding 槽。
 */
TZrBool ZrMath_Complex_MetaSub(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 为 VM 的 * 协议返回复数乘积的新 Complex，保持两操作数不变。
 * self 与第一个对象实参须有可读数值 real/imag，rhs 不额外做名义原型核对。
 * 缺参/非对象实参走 VM 异常；字段读取或新对象 helper 失败返回 false。
 * 结果归 VM 管理；浮点中间值沿用宿主 double，不保证防溢出。
 */
TZrBool ZrMath_Complex_MetaMul(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 为 VM 一元 - 协议返回符号相反的新 Complex，receiver 不变。
 * 无显式实参；real/imag 读取或新对象 helper 失败返回 false。
 * 结果交给本次 binding 槽并归 VM 管理。
 */
TZrBool ZrMath_Complex_MetaNeg(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 为 VM 比较协议按模长平方返回 -1、0、1，不按分量字典序。
 * self 与第一个对象实参须有可读数值 real/imag；缺参/非对象走 VM 异常，字段失败返回 false。
 * receiver 不变；int 结果写入本次 binding 槽。
 * TODO: NaN 或双方平方溢出会落到 0；核对 VM compare 消费规则及边界用例是否需要区分 unordered。
 */
TZrBool ZrMath_Complex_MetaCompare(ZrLibCallContext *context, SZrTypeValue *result);
/**
 * @brief 为 VM toString 协议返回 Complex(real, imag) 调试字符串。
 * 无显式实参；数值字段不可读时返回 false；receiver 不变，字符串结果归 VM 管理。
 * TODO: 共用 helper 调用 void setter 后返回 true；需沿 CreateTryHitCache 的 OOM/结果槽路径确认分配失败是否抛错或留下旧结果。
 */
TZrBool ZrMath_Complex_MetaToString(ZrLibCallContext *context, SZrTypeValue *result);

#endif // ZR_VM_LIB_MATH_COMPLEX_H
