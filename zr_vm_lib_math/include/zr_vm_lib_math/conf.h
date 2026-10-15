//
// Shared configuration for zr.math public headers.
//

#ifndef ZR_VM_LIB_MATH_CONF_H
#define ZR_VM_LIB_MATH_CONF_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_MATH_API ZR_API

#define ZR_MATH_PI 3.14159265358979323846
#define ZR_MATH_TAU (ZR_MATH_PI * 2.0)
#define ZR_MATH_E 2.71828182845904523536
/** 公开近零判定与默认 almostEqual 的共同绝对阈值；并非相对误差比例。 */
#define ZR_MATH_EPSILON 1.0e-9
#define ZR_MATH_FORMAT_BUFFER_LENGTH 512U

#endif // ZR_VM_LIB_MATH_CONF_H

/** @file
 * ## Summary
 * 提供 `zr.math` 公共头和实现共用的编译期导出别名、数学常量及格式缓冲区容量；不创建运行时状态。
 * `module.h` 直接包含本文件，其余 Math 公共头通过该模块入口获得这些配置。
 *
 * ## Constraints and lifecycle
 * 所有定义都是预处理期文本替换，不建立运行时对象；数值表达式由使用它的翻译单元求值。
 * 对外 `zr.math` 常量由 `module.c` 的静态描述符表另行发布，不能仅凭此头文件定义推断模块导入行为。
 *
 * ## References
 * `zr_vm_lib_math/include/zr_vm_lib_math/module.h` 引入配置；`zr_vm_lib_math/src/zr_vm_lib_math/module.c`
 * 聚合公开常量；数值算法和格式化 helper 按各自单元使用相关定义。
 */

/**
 * @def ZR_VM_LIB_MATH_API
 * ## Summary
 * 给 Math 公开声明预留的模块前缀导出标记，当前直接展开为公共依赖提供的 `ZR_API`。
 *
 * ## Value model
 * 仅为对象式预处理器别名；不产生独立符号或存储，也不自行定义导入/导出策略。
 *
 * ## Constraints and lifecycle
 * `ZR_API` 的平台配置来自 `zr_vm_common/include/zr_vm_common/zr_api_conf.h`。当前 Math 的 `zr_vm_lib_math/include/zr_vm_lib_math/module.h` 声明直接写 `ZR_API`，
 * 仓内未发现 `ZR_VM_LIB_MATH_API` 的展开点；不得据此声称本别名控制了现有符号可见性。
 * TODO: 若要替换或删除该别名，先核对仓外嵌入者及兼容头的实际使用。
 *
 * ## References
 * `zr_vm_library/include/zr_vm_library/conf.h` 引入共同 API 配置；`zr_vm_lib_math/include/zr_vm_lib_math/module.h` 展示当前声明写法。
 */

/**
 * @def ZR_MATH_PI
 * ## Summary
 * Math 共用的圆周率近似值；模块描述符以 `PI` 浮点常量发布它，Scalar 的角度转换也用它。
 *
 * ## Value model
 * 宏展开为十进制浮点常量 `3.14159265358979323846`，按 C 浮点字面量规则求值。
 *
 * ## Constraints and lifecycle
 * 这是编译期数值，不包含角度单位或归一化规则；调用方按自己的公式解释结果。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 的 `g_math_constants.PI` 发布入口；
 * `zr_vm_lib_math/src/zr_vm_lib_math/scalar/scalar.c` 的 `Degrees` 与 `Radians` 公式是仓内计算消费者。
 */

/**
 * @def ZR_MATH_TAU
 * ## Summary
 * 圆周率的两倍，用于公开的 `zr.math.TAU` 常量。
 *
 * ## Value model
 * 展开为 `ZR_MATH_PI * 2.0`，复用同一 PI 字面量，而不是另一份独立近似值。
 *
 * ## Constraints and lifecycle
 * 该宏只参与编译期算术；当前仓内消费者是模块常量描述符，未发现其他运行时算法直接引用它。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 的 `g_math_constants.TAU` 将计算值作为 float 描述符发布。
 */

/**
 * @def ZR_MATH_E
 * ## Summary
 * 自然对数底的十进制近似值，并作为 `zr.math.E` 模块常量发布。
 *
 * ## Value model
 * 展开为 `2.71828182845904523536` 浮点常量；不引用宿主 `<math.h>` 的非标准符号。
 *
 * ## Constraints and lifecycle
 * 这是纯编译期数值；仓内未发现除模块常量表以外的直接算法消费者。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 的 `g_math_constants.E`。
 */

/**
 * @def ZR_MATH_EPSILON
 * ## Summary
 * `zr.math.EPSILON` 的固定数值，并为未传第三个参数的 `almostEqual` 提供默认 epsilon。
 *
 * ## Value model
 * 宏值为 `1.0e-9`；`almostEqual` 将其作为绝对差上限，其它调用点分别用于局部阈值比较。
 *
 * ## Constraints and lifecycle
 * 这不是相对误差、ULP 距离或对所有算法统一生效的比较器；每个消费点仍由自己的比较式决定边界。
 *
 * ## References
 * 模块发布见 `module.c:g_math_constants.EPSILON`；默认值见 `scalar.c:ZrMath_Scalar_AlmostEqual`；
 * 其它已检索到的近零阈值包括 Vector、Complex、Quaternion 与 Matrix 实现。
 */

/**
 * @def ZR_MATH_FORMAT_BUFFER_LENGTH
 * ## Summary
 * `ZrMath_MakeStringResult` 使用的固定栈缓冲区容量，单位为字节。
 *
 * ## Value model
 * 值为 `512U`，包括结尾 NUL 的空间；配合 `vsnprintf` 时最多写入 511 个非 NUL 字节。
 *
 * ## Constraints and lifecycle
 * helper 忽略 `vsnprintf` 的所需长度并继续把缓冲区交给 VM 字符串 setter；超长格式结果因此只交付可容纳前缀。
 * 该容量不构成任意长输出保证，也不拥有动态内存。
 *
 * ## References
 * `zr_vm_lib_math/src/zr_vm_lib_math/common.c:ZrMath_MakeStringResult` 是仓内唯一缓冲区声明消费者。
 */
