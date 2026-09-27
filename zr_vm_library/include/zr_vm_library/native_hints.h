//
// Native module hint export helpers.
//

#ifndef ZR_VM_LIBRARY_NATIVE_HINTS_H
#define ZR_VM_LIBRARY_NATIVE_HINTS_H

#include "zr_vm_library/native_binding.h"

/* 生成器和消费端共用的提示侧车协议版本。 */
#define ZR_VM_NATIVE_HINTS_SCHEMA_ID "zr.native.hints/v1"

/** @brief 暴露编译工具解析 Native 类型提示时使用的侧车协议版本。 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeHints_GetSchemaId(void);
/** @brief 借用描述符中已生成的 JSON 提示；返回值依赖描述符生命周期。 */
ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeHints_GetModuleJson(const ZrLibModuleDescriptor *descriptor);
/** @brief 将描述符提示写为 UTF-8 侧车文件，供构建和工具链读取。
 * @note BUG: fopen 成功后未检查 fwrite/fclose；磁盘写满等错误可留下截断文件却返回成功。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_NativeHints_WriteSidecar(const ZrLibModuleDescriptor *descriptor,
                                                          const TZrChar *outputPath);

#endif // ZR_VM_LIBRARY_NATIVE_HINTS_H
