#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_PLUGIN_SHADOW_IO_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_PLUGIN_SHADOW_IO_H

#include "zr_vm_library/conf.h"
#include <stdio.h>

/* Private owning-stream boundary. Both valid, distinct streams are closed exactly
 * once, including after read, write, or either close failure. False reports any
 * such failure; the caller owns paths and removes only its incomplete target. */
TZrBool native_registry_copy_plugin_streams(FILE *source, FILE *target);

/* Create a shadow image under the platform temporary cache; no registry is published. */
TZrBool native_registry_prepare_shadow_plugin_path(const TZrChar *moduleName,
                                                   const TZrChar *sourcePath,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize);

#endif

/**
 * @fn TZrBool native_registry_copy_plugin_streams(FILE *source, FILE *target)
 *
 * ## Summary
 * 向复制路径层及专用错误夹具公开拥有关闭责任的双流边界，避免调用方在失败后再次关闭。
 *
 * ## Parameters
 * @param[in,out] source 有效可读且与 target 不同的 FILE；本调用接管 fclose 责任。
 * @param[in,out] target 有效可写且数据使用与源不冲突的 FILE；本调用接管 fclose 责任。
 *
 * ## Returns
 * @return 任何检测到的读、写或任一关闭错误返回 FALSE，否则 TRUE；两个 fclose 都会被执行一次，返回后调用方不得再使用流。
 *
 * ## Constraints and lifecycle
 * 不验证 NULL、别名、读写模式或路径安全；调用方确保有效流和串行使用。可能产生部分目标字节，函数不拥有路径且不删除文件；路径包装层在 FALSE 后尝试删除。FILE 关闭失败也不恢复为可再次安全关闭的对象；不提供持久化或可加载性证明。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h:10
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h:7
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:53
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:65
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:69
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:73
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:103
 * - tests/library/test_native_plugin_shadow_io.c:102
 */

/**
 * @fn TZrBool native_registry_prepare_shadow_plugin_path(const TZrChar *moduleName, const TZrChar *sourcePath, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 为装载器提供独立影子路径与文件副本，保持复制准备和注册记录发布两个阶段的所有权边界。
 *
 * ## Parameters
 * @param[in] moduleName 稳定 NUL 结尾名字；NULL 被拒绝，清洗只用于文件名，不证明模块身份。
 * @param[in] sourcePath 稳定源文件路径；不得被输出覆盖，调用方保有字符存储。
 * @param[out] buffer 独立可写且实际至少 ZR_LIBRARY_MAX_PATH_LENGTH 字节；FALSE 不保证内容不变或为空。
 * @param[in] bufferSize 真实容量，当前仅做非零检查，不能把参数当作任意小输出缓冲区支持。
 *
 * ## Returns
 * @return 普通 TRUE 表示复制 helper 成功并写出影子路径；FALSE 表示某个显式检查或复制失败。两者都不发布注册记录。
 *
 * ## Constraints and lifecycle
 * 必须串行调用以保护进程级非原子 shadowCounter，并保证缓存目录和源/目标文件使用不冲突。TRUE 后调用方负责把副本交给装载器失败清理或 handle record 生命周期；删除是后续 remove 尝试而非此接口自动保证。原始路径在复制失败时可由装载器继续作为候选，但本接口不负责作回退选择。
 *
 * ## References
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h:13
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:109
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:123
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:136
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c:151
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:219
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:227
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:234
 * - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c:17
 */
